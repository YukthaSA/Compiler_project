#include "simd_csv.h"
#include <time.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static double get_time_ms(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return ((double)counter.QuadPart * 1000.0) / (double)freq.QuadPart;
}
#else
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif

int cpu_supports_avx2(void) {
#if defined(__AVX2__)
    return 1;
#else
    #if defined(__GNUC__) || defined(__clang__)
    return __builtin_cpu_supports("avx2");
    #else
    return 0;
    #endif
#endif
}

int fast_strview_to_int(StringView sv) {
    const char *p = sv.ptr;
    size_t len = sv.len;
    while (len > 0 && isspace((unsigned char)*p)) { p++; len--; }
    if (len == 0) return 0;

    int sign = 1;
    if (*p == '-') { sign = -1; p++; len--; }
    else if (*p == '+') { p++; len--; }

    int val = 0;
    while (len > 0 && isdigit((unsigned char)*p)) {
        val = val * 10 + (*p - '0');
        p++;
        len--;
    }
    return val * sign;
}

double fast_strview_to_float(StringView sv) {
    const char *p = sv.ptr;
    size_t len = sv.len;
    while (len > 0 && isspace((unsigned char)*p)) { p++; len--; }
    if (len == 0) return 0.0;

    double sign = 1.0;
    if (*p == '-') { sign = -1.0; p++; len--; }
    else if (*p == '+') { p++; len--; }

    double val = 0.0;
    while (len > 0 && isdigit((unsigned char)*p)) {
        val = val * 10.0 + (*p - '0');
        p++;
        len--;
    }
    if (len > 0 && *p == '.') {
        p++;
        len--;
        double frac = 0.1;
        while (len > 0 && isdigit((unsigned char)*p)) {
            val += (*p - '0') * frac;
            frac *= 0.1;
            p++;
            len--;
        }
    }
    return val * sign;
}

int strview_equals_str(StringView sv, const char *str) {
    if (!str) return 0;
    size_t slen = strlen(str);
    if (sv.len != slen) return 0;
    return (strncmp(sv.ptr, str, slen) == 0);
}

static char* read_entire_file(const char *filepath, size_t *out_size) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        return NULL;
    }

    // Allocate buffer with 64 extra bytes for SIMD over-read padding
    char *buf = (char*)malloc((size_t)sz + 64);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    // Null-terminate and pad
    memset(buf + read_bytes, 0, 64);
    *out_size = read_bytes;
    return buf;
}

static StringView trim_view(StringView sv) {
    while (sv.len > 0 && isspace((unsigned char)*sv.ptr)) {
        sv.ptr++;
        sv.len--;
    }
    while (sv.len > 0 && (isspace((unsigned char)sv.ptr[sv.len - 1]) || sv.ptr[sv.len - 1] == '\r')) {
        sv.len--;
    }
    // Strip surrounding quotes
    if (sv.len >= 2 && sv.ptr[0] == '"' && sv.ptr[sv.len - 1] == '"') {
        sv.ptr++;
        sv.len -= 2;
    }
    return sv;
}

ZeroCopyTable* load_csv_zero_copy(const char *filepath, int force_simd) {
    size_t fsize = 0;
    char *raw = read_entire_file(filepath, &fsize);
    if (!raw) return NULL;

    double t0 = get_time_ms();

    ZeroCopyTable *tbl = (ZeroCopyTable*)calloc(1, sizeof(ZeroCopyTable));
    tbl->raw_buffer = raw;
    tbl->buffer_size = fsize;

    int use_simd = force_simd ? cpu_supports_avx2() : 0;
    tbl->used_simd = use_simd;

    // 1. Parse header row
    size_t pos = 0;
    while (pos < fsize && raw[pos] != '\n') pos++;
    size_t header_len = pos;
    if (pos < fsize && raw[pos] == '\n') pos++;

    // Split headers
    int hdr_cap = 16;
    tbl->headers = (char**)malloc(sizeof(char*) * hdr_cap);
    tbl->col_count = 0;

    size_t h_start = 0;
    for (size_t i = 0; i <= header_len; i++) {
        if (i == header_len || raw[i] == ',') {
            size_t flen = i - h_start;
            StringView sv = trim_view((StringView){ .ptr = raw + h_start, .len = flen });
            char *col_str = (char*)malloc(sv.len + 1);
            memcpy(col_str, sv.ptr, sv.len);
            col_str[sv.len] = '\0';

            if (tbl->col_count >= (uint32_t)hdr_cap) {
                hdr_cap *= 2;
                tbl->headers = (char**)realloc(tbl->headers, sizeof(char*) * hdr_cap);
            }
            tbl->headers[tbl->col_count++] = col_str;
            h_start = i + 1;
        }
    }

    uint32_t cols = tbl->col_count;
    if (cols == 0) {
        free_zero_copy_table(tbl);
        return NULL;
    }

    // 2. Parse data rows using SIMD or optimized scalar scanner
    size_t estimated_rows = (fsize / 32) + 1024;
    tbl->row_capacity = (estimated_rows < 1024) ? 1024 : estimated_rows;
    tbl->rows = (ZeroCopyRow*)malloc(sizeof(ZeroCopyRow) * tbl->row_capacity);
    tbl->row_count = 0;

    tbl->field_pool_capacity = tbl->row_capacity * cols;
    tbl->field_pool = (StringView*)malloc(sizeof(StringView) * tbl->field_pool_capacity);
    size_t total_fields = 0;

    uint32_t field_idx = 0;
    size_t field_start = pos;
    int in_quotes = 0;

#if defined(__AVX2__)
    if (use_simd) {
        const __m256i v_comma = _mm256_set1_epi8(',');
        const __m256i v_nl    = _mm256_set1_epi8('\n');
        const __m256i v_quote = _mm256_set1_epi8('"');

        while (pos < fsize) {
            if (!in_quotes && (pos + 32 <= fsize)) {
                __m256i chunk = _mm256_loadu_si256((const __m256i*)(raw + pos));
                __m256i m_comma = _mm256_cmpeq_epi8(chunk, v_comma);
                __m256i m_nl    = _mm256_cmpeq_epi8(chunk, v_nl);
                __m256i m_quote = _mm256_cmpeq_epi8(chunk, v_quote);
                __m256i m_delims = _mm256_or_si256(_mm256_or_si256(m_comma, m_nl), m_quote);

                uint32_t mask = (uint32_t)_mm256_movemask_epi8(m_delims);
                if (mask == 0) {
                    pos += 32;
                    continue;
                } else {
                    uint32_t offset = __builtin_ctz(mask);
                    pos += offset;
                }
            }

            char c = raw[pos];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (!in_quotes && (c == ',' || c == '\n')) {
                if (field_idx < cols) {
                    size_t flen = pos - field_start;
                    tbl->field_pool[total_fields++] = trim_view((StringView){ .ptr = raw + field_start, .len = flen });
                    field_idx++;
                }

                if (c == '\n') {
                    if (field_idx > 0) {
                        while (field_idx < cols) {
                            tbl->field_pool[total_fields++] = (StringView){ .ptr = "", .len = 0 };
                            field_idx++;
                        }
                        if (tbl->row_count >= tbl->row_capacity) {
                            tbl->row_capacity *= 2;
                            tbl->rows = (ZeroCopyRow*)realloc(tbl->rows, sizeof(ZeroCopyRow) * tbl->row_capacity);
                            tbl->field_pool_capacity = tbl->row_capacity * cols;
                            tbl->field_pool = (StringView*)realloc(tbl->field_pool, sizeof(StringView) * tbl->field_pool_capacity);
                        }
                        tbl->row_count++;
                        field_idx = 0;
                    }
                }
                field_start = pos + 1;
            }
            pos++;
        }
    } else {
#endif
        // Scalar fallback loop
        while (pos < fsize) {
            char c = raw[pos];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (!in_quotes && (c == ',' || c == '\n')) {
                if (field_idx < cols) {
                    size_t flen = pos - field_start;
                    tbl->field_pool[total_fields++] = trim_view((StringView){ .ptr = raw + field_start, .len = flen });
                    field_idx++;
                }

                if (c == '\n') {
                    if (field_idx > 0) {
                        while (field_idx < cols) {
                            tbl->field_pool[total_fields++] = (StringView){ .ptr = "", .len = 0 };
                            field_idx++;
                        }
                        if (tbl->row_count >= tbl->row_capacity) {
                            tbl->row_capacity *= 2;
                            tbl->rows = (ZeroCopyRow*)realloc(tbl->rows, sizeof(ZeroCopyRow) * tbl->row_capacity);
                            tbl->field_pool_capacity = tbl->row_capacity * cols;
                            tbl->field_pool = (StringView*)realloc(tbl->field_pool, sizeof(StringView) * tbl->field_pool_capacity);
                        }
                        tbl->row_count++;
                        field_idx = 0;
                    }
                }
                field_start = pos + 1;
            }
            pos++;
        }
#if defined(__AVX2__)
    }
#endif

    // Handle last trailing row if not terminated by newline
    if (field_idx > 0 || (pos > field_start)) {
        if (field_idx < cols) {
            size_t flen = pos - field_start;
            tbl->field_pool[total_fields++] = trim_view((StringView){ .ptr = raw + field_start, .len = flen });
            field_idx++;
        }
        while (field_idx < cols) {
            tbl->field_pool[total_fields++] = (StringView){ .ptr = "", .len = 0 };
            field_idx++;
        }
        if (tbl->row_count >= tbl->row_capacity) {
            tbl->row_capacity += 16;
            tbl->rows = (ZeroCopyRow*)realloc(tbl->rows, sizeof(ZeroCopyRow) * tbl->row_capacity);
        }
        tbl->row_count++;
    }

    // Connect contiguous field pool to row pointers in O(N)
    for (size_t r = 0; r < tbl->row_count; r++) {
        tbl->rows[r].fields = &tbl->field_pool[r * cols];
        tbl->rows[r].field_count = cols;
    }

    double t1 = get_time_ms();
    tbl->parse_time_ms = t1 - t0;
    return tbl;
}

int find_col_index_zerocopy(ZeroCopyTable *table, const char *col_name) {
    if (!table || !col_name) return -1;
    for (uint32_t i = 0; i < table->col_count; i++) {
        if (strcmp(table->headers[i], col_name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void free_zero_copy_table(ZeroCopyTable *table) {
    if (!table) return;

    for (uint32_t i = 0; i < table->col_count; i++) {
        free(table->headers[i]);
    }
    free(table->headers);

    free(table->field_pool);
    free(table->rows);

    free(table->raw_buffer);
    free(table);
}
