#include "executor.h"
#include "symbol_table.h"
#include <float.h>

typedef struct {
    DataType type;
    int ival;
    double fval;
    char sval[256];
} EvalValue;

static EvalValue eval_expr(ASTNode *expr, CSVData *csv, CSVRecord *row) {
    EvalValue res;
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
            strncpy(res.sval, expr->string_expr.val, sizeof(res.sval) - 1);
            return res;

        case NODE_EXPR_IDENT: {
            int c_idx = find_col_index(csv, expr->ident_expr.name);
            if (c_idx < 0 || c_idx >= row->field_count) {
                res.type = TYPE_UNKNOWN;
                return res;
            }
            char *raw_val = row->fields[c_idx];
            Symbol *sym = lookup_symbol(expr->ident_expr.name);
            DataType t = sym ? sym->type : TYPE_STRING;
            res.type = t;
            if (t == TYPE_INT) {
                res.ival = atoi(raw_val);
                res.fval = (double)res.ival;
            } else if (t == TYPE_FLOAT) {
                res.fval = atof(raw_val);
                res.ival = (int)res.fval;
            } else {
                strncpy(res.sval, raw_val, sizeof(res.sval) - 1);
            }
            return res;
        }

        case NODE_EXPR_BINARY: {
            if (expr->binary_expr.op == OP_AND) {
                EvalValue left = eval_expr(expr->binary_expr.left, csv, row);
                if (left.ival == 0) {
                    res.type = TYPE_INT;
                    res.ival = 0;
                    return res;
                }
                EvalValue right = eval_expr(expr->binary_expr.right, csv, row);
                res.type = TYPE_INT;
                res.ival = (right.ival != 0) ? 1 : 0;
                return res;
            }
            if (expr->binary_expr.op == OP_OR) {
                EvalValue left = eval_expr(expr->binary_expr.left, csv, row);
                if (left.ival != 0) {
                    res.type = TYPE_INT;
                    res.ival = 1;
                    return res;
                }
                EvalValue right = eval_expr(expr->binary_expr.right, csv, row);
                res.type = TYPE_INT;
                res.ival = (right.ival != 0) ? 1 : 0;
                return res;
            }

            EvalValue left = eval_expr(expr->binary_expr.left, csv, row);
            EvalValue right = eval_expr(expr->binary_expr.right, csv, row);

            int cmp_result = 0;
            if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
                int scmp = strcmp(left.sval, right.sval);
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

int execute_query(ASTNode *program) {
    if (!program || program->type != NODE_PROGRAM) return -1;

    ASTNode *load = program->program.load_stmt;
    ASTNode *select = program->program.select_stmt;
    ASTNode *filter = program->program.filter_stmt;
    ASTNode *agg = program->program.agg_stmt;

    CSVData *csv = parse_csv_file(load->load_stmt.file_path);
    if (!csv) {
        fprintf(stderr, "Runtime Error: Unable to open CSV dataset at '%s'\n", load->load_stmt.file_path);
        return -1;
    }

    int proj_count = 0;
    int *proj_indices = NULL;
    char **proj_names = NULL;

    if (select) {
        if (select->select_stmt.col_count == 1 && strcmp(select->select_stmt.columns[0], "*") == 0) {
            proj_count = csv->col_count;
            proj_indices = (int*)malloc(sizeof(int) * proj_count);
            proj_names = (char**)malloc(sizeof(char*) * proj_count);
            for (int i = 0; i < proj_count; i++) {
                proj_indices[i] = i;
                proj_names[i] = csv->headers[i];
            }
        } else {
            proj_count = select->select_stmt.col_count;
            proj_indices = (int*)malloc(sizeof(int) * proj_count);
            proj_names = (char**)malloc(sizeof(char*) * proj_count);
            for (int i = 0; i < proj_count; i++) {
                proj_names[i] = select->select_stmt.columns[i];
                proj_indices[i] = find_col_index(csv, proj_names[i]);
            }
        }
    }

    if (proj_count <= 0) {
        free_csv_data(csv);
        return 0;
    }

    CSVRecord **matched_rows = (CSVRecord**)malloc(sizeof(CSVRecord*) * csv->record_count);
    int match_count = 0;

    for (int r = 0; r < csv->record_count; r++) {
        CSVRecord *row = &csv->records[r];
        int keep = 1;
        if (filter && filter->filter_stmt.condition) {
            EvalValue ev = eval_expr(filter->filter_stmt.condition, csv, row);
            keep = (ev.ival != 0);
        }
        if (keep) {
            matched_rows[match_count++] = row;
        }
    }

    int *col_widths = (int*)calloc(proj_count, sizeof(int));
    for (int i = 0; i < proj_count; i++) {
        col_widths[i] = (int)strlen(proj_names[i]);
        for (int m = 0; m < match_count; m++) {
            int c_idx = proj_indices[i];
            if (c_idx >= 0 && c_idx < matched_rows[m]->field_count) {
                int flen = (int)strlen(matched_rows[m]->fields[c_idx]);
                if (flen > col_widths[i]) {
                    col_widths[i] = flen;
                }
            }
        }
    }

    for (int i = 0; i < proj_count; i++) {
        col_widths[i] += 2;
    }

    printf("\n--- Query Result (%d of %d rows) ---\n", match_count, csv->record_count);

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
    for (int m = 0; m < match_count; m++) {
        CSVRecord *row = matched_rows[m];
        printf("|");
        for (int i = 0; i < proj_count; i++) {
            int c_idx = proj_indices[i];
            if (c_idx >= 0 && c_idx < row->field_count) {
                printf(" %-*s |", col_widths[i], row->fields[c_idx]);
            } else {
                printf(" %-*s |", col_widths[i], "NULL");
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

    // Aggregations
    if (agg && agg->agg_stmt.func != AGG_NONE) {
        printf("\n--- Aggregation ---\n");
        AggFunc fn = agg->agg_stmt.func;
        const char *col_name = agg->agg_stmt.column;

        if (fn == AGG_COUNT) {
            printf("COUNT(%s) = %d\n", col_name ? col_name : "*", match_count);
        } else {
            int c_idx = find_col_index(csv, col_name);
            double sum = 0.0;
            double min_val = DBL_MAX;
            double max_val = -DBL_MAX;
            int valid_nums = 0;

            for (int m = 0; m < match_count; m++) {
                if (c_idx >= 0 && c_idx < matched_rows[m]->field_count) {
                    double v = atof(matched_rows[m]->fields[c_idx]);
                    sum += v;
                    if (v < min_val) min_val = v;
                    if (v > max_val) max_val = v;
                    valid_nums++;
                }
            }

            if (fn == AGG_SUM) {
                printf("SUM(%s) = %.2f\n", col_name, sum);
            } else if (fn == AGG_AVG) {
                double avg = valid_nums > 0 ? (sum / valid_nums) : 0.0;
                printf("AVG(%s) = %.2f\n", col_name, avg);
            } else if (fn == AGG_MIN) {
                printf("MIN(%s) = %.2f\n", col_name, (valid_nums > 0) ? min_val : 0.0);
            } else if (fn == AGG_MAX) {
                printf("MAX(%s) = %.2f\n", col_name, (valid_nums > 0) ? max_val : 0.0);
            }
        }
        printf("\n");
    }

    free(col_widths);
    free(proj_indices);
    free(proj_names);
    free(matched_rows);
    free_csv_data(csv);

    return 0;
}
