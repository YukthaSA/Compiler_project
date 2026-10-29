#ifndef SIMD_CSV_H
#define SIMD_CSV_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

/* Zero-Copy String Slice: points directly into mapped/contiguous memory buffer */
typedef struct {
    const char *ptr;
    size_t len;
} StringView;

/* A row represented as an array of zero-copy StringView fields */
typedef struct {
    StringView *fields;
    uint32_t field_count;
} ZeroCopyRow;

/* Zero-copy table holding the contiguous file buffer and row pointers */
typedef struct {
    char *raw_buffer;
    size_t buffer_size;

    char **headers;
    uint32_t col_count;

    ZeroCopyRow *rows;
    size_t row_count;
    size_t row_capacity;

    StringView *field_pool;
    size_t field_pool_capacity;

    double parse_time_ms;
    int used_simd;
} ZeroCopyTable;

/* Fast string-to-number parsing from StringView without allocations */
int fast_strview_to_int(StringView sv);
double fast_strview_to_float(StringView sv);
int strview_equals_str(StringView sv, const char *str);

/* High-performance CSV loaders */
ZeroCopyTable* load_csv_zero_copy(const char *filepath, int force_simd);
int find_col_index_zerocopy(ZeroCopyTable *table, const char *col_name);
void free_zero_copy_table(ZeroCopyTable *table);

/* Check CPU SIMD capability at runtime */
int cpu_supports_avx2(void);

#endif /* SIMD_CSV_H */
