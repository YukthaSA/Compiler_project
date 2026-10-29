#include "tac.h"

TACProgram g_tac;

void init_tac(void) {
    g_tac.count = 0;
    g_tac.temp_count = 0;
    g_tac.label_count = 0;
}

static void add_quad(const char *op, const char *arg1, const char *arg2, const char *res) {
    if (g_tac.count >= MAX_QUADS) return;
    int idx = g_tac.count++;
    snprintf(g_tac.quads[idx].op, sizeof(g_tac.quads[idx].op), "%s", op ? op : "-");
    snprintf(g_tac.quads[idx].arg1, sizeof(g_tac.quads[idx].arg1), "%s", arg1 ? arg1 : "-");
    snprintf(g_tac.quads[idx].arg2, sizeof(g_tac.quads[idx].arg2), "%s", arg2 ? arg2 : "-");
    snprintf(g_tac.quads[idx].result, sizeof(g_tac.quads[idx].result), "%s", res ? res : "-");
}

static void new_temp(char *buf) {
    sprintf(buf, "t%d", g_tac.temp_count++);
}

static void generate_expr_tac(ASTNode *expr, char *res_buf) {
    if (!expr) {
        strcpy(res_buf, "-");
        return;
    }

    switch (expr->type) {
        case NODE_EXPR_IDENT:
            strncpy(res_buf, expr->ident_expr.name, 63);
            res_buf[63] = '\0';
            break;

        case NODE_EXPR_INT:
            sprintf(res_buf, "%d", expr->int_expr.val);
            break;

        case NODE_EXPR_FLOAT:
            sprintf(res_buf, "%g", expr->float_expr.val);
            break;

        case NODE_EXPR_STRING:
            sprintf(res_buf, "\"%s\"", expr->string_expr.val);
            break;

        case NODE_EXPR_BINARY: {
            char left_res[64];
            char right_res[64];
            generate_expr_tac(expr->binary_expr.left, left_res);
            generate_expr_tac(expr->binary_expr.right, right_res);

            new_temp(res_buf);
            const char *op_str = "CMP";
            switch (expr->binary_expr.op) {
                case OP_EQ:  op_str = "CMP_EQ"; break;
                case OP_NEQ: op_str = "CMP_NE"; break;
                case OP_GT:  op_str = "CMP_GT"; break;
                case OP_LT:  op_str = "CMP_LT"; break;
                case OP_GTE: op_str = "CMP_GE"; break;
                case OP_LTE: op_str = "CMP_LE"; break;
                case OP_AND: op_str = "LOG_AND"; break;
                case OP_OR:  op_str = "LOG_OR"; break;
            }
            add_quad(op_str, left_res, right_res, res_buf);
            break;
        }

        default:
            strcpy(res_buf, "-");
            break;
    }
}

void generate_tac(ASTNode *program) {
    if (!program || program->type != NODE_PROGRAM) return;

    init_tac();

    ASTNode *load = program->program.load_stmt;
    ASTNode *select = program->program.select_stmt;
    ASTNode *filter = program->program.filter_stmt;
    ASTNode *agg = program->program.agg_stmt;

    char current_dataset[64] = "DATA";

    // 1. LOAD
    if (load) {
        char quoted_file[256];
        sprintf(quoted_file, "\"%s\"", load->load_stmt.file_path);
        add_quad("LOAD_CSV", quoted_file, "-", load->load_stmt.alias);
        strncpy(current_dataset, load->load_stmt.alias, 63);
    }

    // 2. SCAN
    char scan_temp[64];
    new_temp(scan_temp);
    add_quad("SCAN", current_dataset, "-", scan_temp);
    char filtered_temp[64];
    strcpy(filtered_temp, scan_temp);

    // 3. FILTER
    if (filter && filter->filter_stmt.condition) {
        char cond_res[64];
        generate_expr_tac(filter->filter_stmt.condition, cond_res);

        new_temp(filtered_temp);
        add_quad("FILTER_ROWS", scan_temp, cond_res, filtered_temp);
    }

    // 4. PROJECT (SELECT)
    char proj_temp[64];
    if (select) {
        char col_list[512] = "";
        for (int i = 0; i < select->select_stmt.col_count; i++) {
            if (i > 0) strcat(col_list, ", ");
            strcat(col_list, select->select_stmt.columns[i]);
        }
        new_temp(proj_temp);
        add_quad("PROJECT", col_list, filtered_temp, proj_temp);
    }

    // 5. AGGREGATE
    char agg_temp[64];
    if (agg && agg->agg_stmt.func != AGG_NONE) {
        char agg_op[32];
        sprintf(agg_op, "AGG_%s", aggfunc_to_string(agg->agg_stmt.func));
        new_temp(agg_temp);
        add_quad(agg_op, agg->agg_stmt.column ? agg->agg_stmt.column : "*", filtered_temp, agg_temp);
        add_quad("OUTPUT", proj_temp, agg_temp, "-");
    } else {
        add_quad("OUTPUT", proj_temp, "-", "-");
    }
}

void print_tac(void) {
    printf("\n--- Three-Address Code (TAC) ---\n");
    for (int i = 0; i < g_tac.count; i++) {
        Quadruple *q = &g_tac.quads[i];
        if (strcmp(q->op, "LOAD_CSV") == 0) {
            printf("[%02d] %s = LOAD_CSV(%s)\n", i, q->result, q->arg1);
        } else if (strcmp(q->op, "SCAN") == 0) {
            printf("[%02d] %s = SCAN(%s)\n", i, q->result, q->arg1);
        } else if (strncmp(q->op, "CMP_", 4) == 0 || strncmp(q->op, "LOG_", 4) == 0) {
            printf("[%02d] %s = %s %s %s\n", i, q->result, q->arg1, q->op + 4, q->arg2);
        } else if (strcmp(q->op, "FILTER_ROWS") == 0) {
            printf("[%02d] %s = FILTER_ROWS(%s, condition=%s)\n", i, q->result, q->arg1, q->arg2);
        } else if (strcmp(q->op, "PROJECT") == 0) {
            printf("[%02d] %s = PROJECT(%s, from=%s)\n", i, q->result, q->arg1, q->arg2);
        } else if (strncmp(q->op, "AGG_", 4) == 0) {
            printf("[%02d] %s = %s(%s, from=%s)\n", i, q->result, q->op, q->arg1, q->arg2);
        } else if (strcmp(q->op, "OUTPUT") == 0) {
            if (strcmp(q->arg2, "-") != 0) {
                printf("[%02d] EMIT_RESULT(%s, %s)\n", i, q->arg1, q->arg2);
            } else {
                printf("[%02d] EMIT_RESULT(%s)\n", i, q->arg1);
            }
        } else {
            printf("[%02d] %s = %s(%s, %s)\n", i, q->result, q->op, q->arg1, q->arg2);
        }
    }
    printf("\n");
}

static const char *get_textbook_op(const char *op) {
    if (strcmp(op, "CMP_EQ") == 0) return "==";
    if (strcmp(op, "CMP_NE") == 0) return "!=";
    if (strcmp(op, "CMP_GT") == 0) return ">";
    if (strcmp(op, "CMP_LT") == 0) return "<";
    if (strcmp(op, "CMP_GE") == 0) return ">=";
    if (strcmp(op, "CMP_LE") == 0) return "<=";
    if (strcmp(op, "LOG_AND") == 0) return "and";
    if (strcmp(op, "LOG_OR") == 0) return "or";
    return op;
}

void print_tac_textbook(void) {
    printf("--- TAC (Three Address Code) ---\n");
    for (int i = 0; i < g_tac.count; i++) {
        Quadruple *q = &g_tac.quads[i];
        if (strcmp(q->op, "LOAD_CSV") == 0) {
            printf("%s = load %s\n", q->result, q->arg1);
        } else if (strcmp(q->op, "SCAN") == 0) {
            printf("%s = scan %s\n", q->result, q->arg1);
        } else if (strncmp(q->op, "CMP_", 4) == 0 || strncmp(q->op, "LOG_", 4) == 0) {
            printf("%s = %s %s %s\n", q->result, q->arg1, get_textbook_op(q->op), q->arg2);
        } else if (strcmp(q->op, "FILTER_ROWS") == 0) {
            printf("%s = filter %s by %s\n", q->result, q->arg1, q->arg2);
        } else if (strcmp(q->op, "PROJECT") == 0) {
            printf("%s = project %s from %s\n", q->result, q->arg1, q->arg2);
        } else if (strncmp(q->op, "AGG_", 4) == 0) {
            printf("%s = %s %s from %s\n", q->result, q->op + 4, q->arg1, q->arg2);
        } else if (strcmp(q->op, "OUTPUT") == 0) {
            if (strcmp(q->arg2, "-") != 0) {
                printf("emit %s, %s\n", q->arg1, q->arg2);
            } else {
                printf("emit %s\n", q->arg1);
            }
        } else {
            printf("%s = %s %s %s\n", q->result, q->arg1, q->op, q->arg2);
        }
    }
    printf("\n");
}

void print_quadruples(void) {
    printf("--- Quadruples Table ---\n");
    printf("+-------+---------------+----------------------+---------------+--------+\n");
    printf("| Quad# | Operator      | Argument 1           | Argument 2    | Result |\n");
    printf("+-------+---------------+----------------------+---------------+--------+\n");
    for (int i = 0; i < g_tac.count; i++) {
        Quadruple *q = &g_tac.quads[i];
        printf("| %-5d | %-13s | %-20s | %-13s | %-6s |\n",
               i, q->op, q->arg1, q->arg2, q->result);
    }
    printf("+-------+---------------+----------------------+---------------+--------+\n\n");
}
