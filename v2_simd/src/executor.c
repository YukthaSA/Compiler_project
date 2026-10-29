#include "executor.h"
#include "symbol_table.h"
#include <float.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static double get_now_ms(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return ((double)counter.QuadPart * 1000.0) / (double)freq.QuadPart;
}
#else
#include <time.h>
static double get_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif

typedef struct {
    DataType type;
    int ival;
    double fval;
    StringView sval;
} EvalValueZC;

static EvalValueZC eval_expr_zerocopy(ASTNode *expr, ZeroCopyTable *table, ZeroCopyRow *row) {
    EvalValueZC res;
    memset(&res, 0, sizeof(res));
    res.type = TYPE_UNKNOWN;

    if (!expr) return res;

    switch (expr->type) {
        case NODE_EXPR_INT:
            res.type = TYPE_INT;
            res.ival = expr->int_expr.val;
            res.fval = (double)expr->int_expr.val;
            return res;

        case NODE_EXPR_FLOAT:
            res.type = TYPE_FLOAT;
            res.fval = expr->float_expr.val;
            res.ival = (int)expr->float_expr.val;
            return res;

        case NODE_EXPR_STRING:
            res.type = TYPE_STRING;
            res.sval.ptr = expr->string_expr.val;
            res.sval.len = strlen(expr->string_expr.val);
            return res;

        case NODE_EXPR_IDENT: {
            int c_idx = find_col_index_zerocopy(table, expr->ident_expr.name);
            if (c_idx < 0 || (uint32_t)c_idx >= row->field_count) {
                res.type = TYPE_UNKNOWN;
                return res;
            }
            StringView raw_view = row->fields[c_idx];
            Symbol *sym = lookup_symbol(expr->ident_expr.name);
            DataType t = sym ? sym->type : TYPE_STRING;
            res.type = t;
            if (t == TYPE_INT) {
                res.ival = fast_strview_to_int(raw_view);
                res.fval = (double)res.ival;
            } else if (t == TYPE_FLOAT) {
                res.fval = fast_strview_to_float(raw_view);
                res.ival = (int)res.fval;
            } else {
                res.sval = raw_view;
            }
            return res;
        }

        case NODE_EXPR_BINARY: {
            if (expr->binary_expr.op == OP_AND) {
                EvalValueZC left = eval_expr_zerocopy(expr->binary_expr.left, table, row);
                if (left.ival == 0) {
                    res.type = TYPE_INT;
                    res.ival = 0;
                    return res;
                }
                EvalValueZC right = eval_expr_zerocopy(expr->binary_expr.right, table, row);
                res.type = TYPE_INT;
                res.ival = (right.ival != 0) ? 1 : 0;
                return res;
            }
            if (expr->binary_expr.op == OP_OR) {
                EvalValueZC left = eval_expr_zerocopy(expr->binary_expr.left, table, row);
                if (left.ival != 0) {
                    res.type = TYPE_INT;
                    res.ival = 1;
                    return res;
                }
                EvalValueZC right = eval_expr_zerocopy(expr->binary_expr.right, table, row);
                res.type = TYPE_INT;
                res.ival = (right.ival != 0) ? 1 : 0;
                return res;
            }

            EvalValueZC left = eval_expr_zerocopy(expr->binary_expr.left, table, row);
            EvalValueZC right = eval_expr_zerocopy(expr->binary_expr.right, table, row);

            int cmp_result = 0;
            if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
                // String comparison
                size_t min_len = left.sval.len < right.sval.len ? left.sval.len : right.sval.len;
                int scmp = strncmp(left.sval.ptr, right.sval.ptr, min_len);
                if (scmp == 0) {
                    if (left.sval.len < right.sval.len) scmp = -1;
                    else if (left.sval.len > right.sval.len) scmp = 1;
                }
                if (expr->binary_expr.op == OP_EQ) cmp_result = (scmp == 0);
                else if (expr->binary_expr.op == OP_NEQ) cmp_result = (scmp != 0);
            } else {
                double lv = (left.type == TYPE_FLOAT) ? left.fval : left.ival;
                double rv = (right.type == TYPE_FLOAT) ? right.fval : right.ival;

                switch (expr->binary_expr.op) {
                    case OP_EQ:  cmp_result = (lv == rv); break;
                    case OP_NEQ: cmp_result = (lv != rv); break;
                    case OP_GT:  cmp_result = (lv > rv); break;
                    case OP_LT:  cmp_result = (lv < rv); break;
                    case OP_GTE: cmp_result = (lv >= rv); break;
                    case OP_LTE: cmp_result = (lv <= rv); break;
                    default: cmp_result = 0; break;
                }
            }
            res.type = TYPE_INT;
            res.ival = cmp_result ? 1 : 0;
            return res;
        }

        default:
            return res;
    }
}

int execute_query_ex(ASTNode *program, int use_simd, int print_bench) {
    if (!program || program->type != NODE_PROGRAM) return -1;

    ASTNode *load = program->program.load_stmt;
    ASTNode *select = program->program.select_stmt;
    ASTNode *filter = program->program.filter_stmt;
    ASTNode *agg = program->program.agg_stmt;

    double t_start = get_now_ms();

    // 1. High-Performance Zero-Copy CSV Ingestion
    ZeroCopyTable *tbl = load_csv_zero_copy(load->load_stmt.file_path, use_simd);
    if (!tbl) {
        fprintf(stderr, "Runtime Error: Failed to open dataset '%s'\n", load->load_stmt.file_path);
        return -1;
    }

    double t_parsed = get_now_ms();

    // 2. Identify projections
    int proj_count = 0;
    int *proj_indices = NULL;
    char **proj_names = NULL;

    if (select) {
        if (select->select_stmt.col_count == 1 && strcmp(select->select_stmt.columns[0], "*") == 0) {
            proj_count = tbl->col_count;
            proj_indices = (int*)malloc(sizeof(int) * proj_count);
            proj_names = (char**)malloc(sizeof(char*) * proj_count);
            for (int i = 0; i < proj_count; i++) {
                proj_indices[i] = i;
                proj_names[i] = tbl->headers[i];
            }
        } else {
            proj_count = select->select_stmt.col_count;
            proj_indices = (int*)malloc(sizeof(int) * proj_count);
            proj_names = (char**)malloc(sizeof(char*) * proj_count);
            for (int i = 0; i < proj_count; i++) {
                proj_names[i] = select->select_stmt.columns[i];
                proj_indices[i] = find_col_index_zerocopy(tbl, proj_names[i]);
            }
        }
    }

    if (proj_count <= 0) {
        free_zero_copy_table(tbl);
        return 0;
    }

    // 3. Row-by-Row Predicate Evaluation (Zero Allocation)
    size_t *matched_indices = (size_t*)malloc(sizeof(size_t) * tbl->row_count);
    size_t match_count = 0;

    for (size_t r = 0; r < tbl->row_count; r++) {
        int keep = 1;
        if (filter && filter->filter_stmt.condition) {
            EvalValueZC ev = eval_expr_zerocopy(filter->filter_stmt.condition, tbl, &tbl->rows[r]);
            keep = (ev.ival != 0);
        }
        if (keep) {
            matched_indices[match_count++] = r;
        }
    }

    // 4. Aggregations (Vectorized/Zero-Copy Math)
    double agg_sum = 0.0;
    double agg_min = DBL_MAX;
    double agg_max = -DBL_MAX;
    int agg_valid_nums = 0;
    int agg_col_idx = -1;

    if (agg && agg->agg_stmt.func != AGG_NONE && agg->agg_stmt.func != AGG_COUNT) {
        agg_col_idx = find_col_index_zerocopy(tbl, agg->agg_stmt.column);
        if (agg_col_idx >= 0) {
            for (size_t m = 0; m < match_count; m++) {
                size_t r = matched_indices[m];
                ZeroCopyRow *row = &tbl->rows[r];
                if ((uint32_t)agg_col_idx < row->field_count) {
                    double v = fast_strview_to_float(row->fields[agg_col_idx]);
                    agg_sum += v;
                    if (v < agg_min) agg_min = v;
                    if (v > agg_max) agg_max = v;
                    agg_valid_nums++;
                }
            }
        }
    }

    double t_end = get_now_ms();
    double total_time_ms = t_end - t_start;
    double eval_time_ms = t_end - t_parsed;

    // 5. Output Presentation
    size_t display_limit = 25;
    size_t display_count = match_count < display_limit ? match_count : display_limit;

    // Calculate column widths
    int *col_widths = (int*)calloc((size_t)proj_count, sizeof(int));
    for (int i = 0; i < proj_count; i++) {
        col_widths[i] = (int)strlen(proj_names[i]);
    }

    for (size_t m = 0; m < display_count; m++) {
        size_t r = matched_indices[m];
        ZeroCopyRow *row = &tbl->rows[r];
        for (int i = 0; i < proj_count; i++) {
            int c_idx = proj_indices[i];
            if (c_idx >= 0 && (uint32_t)c_idx < row->field_count) {
                int flen = (int)row->fields[c_idx].len;
                if (flen > col_widths[i]) {
                    col_widths[i] = flen;
                }
            }
        }
    }

    for (int i = 0; i < proj_count; i++) {
        col_widths[i] += 2;
    }

    printf("\n--- Query Result (%zu of %zu rows) ---\n", match_count, tbl->row_count);

    // Top border
    printf("+");
    for (int i = 0; i < proj_count; i++) {
        for (int w = 0; w < col_widths[i] + 2; w++) printf("-");
        printf("+");
    }
    printf("\n");

    // Headers
    printf("|");
    for (int i = 0; i < proj_count; i++) {
        printf(" %-*s |", col_widths[i], proj_names[i]);
    }
    printf("\n");

    // Divider
    printf("+");
    for (int i = 0; i < proj_count; i++) {
        for (int w = 0; w < col_widths[i] + 2; w++) printf("-");
        printf("+");
    }
    printf("\n");

    // Rows
    for (size_t m = 0; m < display_count; m++) {
        size_t r = matched_indices[m];
        ZeroCopyRow *row = &tbl->rows[r];
        printf("|");
        for (int i = 0; i < proj_count; i++) {
            int c_idx = proj_indices[i];
            if (c_idx >= 0 && (uint32_t)c_idx < row->field_count) {
                StringView sv = row->fields[c_idx];
                printf(" %.*s%-*s |", (int)sv.len, sv.ptr, (int)(col_widths[i] - sv.len), "");
            } else {
                printf(" %-*s |", col_widths[i], "");
            }
        }
        printf("\n");
    }

    // Bottom border
    printf("+");
    for (int i = 0; i < proj_count; i++) {
        for (int w = 0; w < col_widths[i] + 2; w++) printf("-");
        printf("+");
    }
    printf("\n");

    if (match_count > display_limit) {
        printf("... (%zu more rows not shown in console) ...\n", match_count - display_limit);
    }

    // Print Aggregations
    if (agg && agg->agg_stmt.func != AGG_NONE) {
        printf("\n--- Aggregation ---\n");
        AggFunc fn = agg->agg_stmt.func;
        const char *col_name = agg->agg_stmt.column;

        if (fn == AGG_COUNT) {
            printf("COUNT(%s) = %zu\n", col_name ? col_name : "*", match_count);
        } else if (fn == AGG_SUM) {
            printf("SUM(%s) = %.2f\n", col_name, agg_sum);
        } else if (fn == AGG_AVG) {
            double avg = agg_valid_nums > 0 ? (agg_sum / agg_valid_nums) : 0.0;
            printf("AVG(%s) = %.2f\n", col_name, avg);
        } else if (fn == AGG_MIN) {
            printf("MIN(%s) = %.2f\n", col_name, (agg_valid_nums > 0) ? agg_min : 0.0);
        } else if (fn == AGG_MAX) {
            printf("MAX(%s) = %.2f\n", col_name, (agg_valid_nums > 0) ? agg_max : 0.0);
        }
        printf("\n");
    }

    // Benchmark Telemetry Output
    if (print_bench) {
        double fsize_mb = (double)tbl->buffer_size / (1024.0 * 1024.0);
        double throughput_mb_s = (total_time_ms > 0) ? (fsize_mb / (total_time_ms / 1000.0)) : 0.0;
        double throughput_rows_s = (total_time_ms > 0) ? ((double)tbl->row_count / (total_time_ms / 1000.0)) : 0.0;

        printf("--- Execution Telemetry (Benchmark) ---\n");
        printf("Engine Mode        : %s\n", tbl->used_simd ? "AVX2 SIMD Zero-Copy" : "Scalar Zero-Copy");
        printf("Dataset File Size  : %.2f MB (%zu bytes)\n", fsize_mb, tbl->buffer_size);
        printf("Total Rows Parsed  : %zu rows\n", tbl->row_count);
        printf("Parse Time         : %.3f ms\n", tbl->parse_time_ms);
        printf("Query Filter Time  : %.3f ms\n", eval_time_ms);
        printf("Total Latency      : %.3f ms\n", total_time_ms);
        printf("Scan Throughput    : %.2f MB/s (%.2f million rows/sec)\n", throughput_mb_s, throughput_rows_s / 1e6);
        printf("\n");
    }

    // Clean up
    free(col_widths);
    free(proj_indices);
    free(proj_names);
    free(matched_indices);
    free_zero_copy_table(tbl);

    return 0;
}

int execute_query(ASTNode *program) {
    return execute_query_ex(program, 1, 0);
}
