#include "semantic.h"

static int g_semantic_errors = 0;

static void report_semantic_error(int line, const char *msg, const char *arg1, const char *arg2) {
    g_semantic_errors++;
    fprintf(stderr, "Semantic Error [Line %d]: ", line);
    if (arg1 && arg2) {
        fprintf(stderr, msg, arg1, arg2);
    } else if (arg1) {
        fprintf(stderr, msg, arg1);
    } else {
        fprintf(stderr, "%s", msg);
    }
    fprintf(stderr, "\n");
}

static DataType get_expr_type(ASTNode *expr) {
    if (!expr) return TYPE_UNKNOWN;

    switch (expr->type) {
        case NODE_EXPR_IDENT: {
            Symbol *s = lookup_symbol(expr->ident_expr.name);
            if (!s) {
                report_semantic_error(expr->line_no, "Column '%s' in condition is not defined in dataset schema.", expr->ident_expr.name, NULL);
                return TYPE_UNKNOWN;
            }
            return s->type;
        }
        case NODE_EXPR_INT:
            return TYPE_INT;
        case NODE_EXPR_FLOAT:
            return TYPE_FLOAT;
        case NODE_EXPR_STRING:
            return TYPE_STRING;
        case NODE_EXPR_BINARY: {
            DataType lt = get_expr_type(expr->binary_expr.left);
            DataType rt = get_expr_type(expr->binary_expr.right);
            if (expr->binary_expr.op == OP_AND || expr->binary_expr.op == OP_OR) {
                return TYPE_INT; // Boolean result
            }
            // Comparison
            if (lt != TYPE_UNKNOWN && rt != TYPE_UNKNOWN) {
                // If one is string and one is numeric -> mismatch!
                if ((lt == TYPE_STRING && (rt == TYPE_INT || rt == TYPE_FLOAT)) ||
                    (rt == TYPE_STRING && (lt == TYPE_INT || lt == TYPE_FLOAT))) {
                    report_semantic_error(expr->line_no, "Type mismatch in condition comparison between string and numeric operand.", NULL, NULL);
                } else if (lt == TYPE_STRING && rt == TYPE_STRING) {
                    if (expr->binary_expr.op != OP_EQ && expr->binary_expr.op != OP_NEQ) {
                        report_semantic_error(expr->line_no, "Relational operator cannot be applied to STRING operands (only == or != supported).", NULL, NULL);
                    }
                }
            }
            return TYPE_INT; // Condition evaluates to boolean
        }
        default:
            return TYPE_UNKNOWN;
    }
}

int perform_semantic_analysis(ASTNode *program) {
    if (!program || program->type != NODE_PROGRAM) {
        return -1;
    }

    g_semantic_errors = 0;
    ASTNode *load = program->program.load_stmt;
    ASTNode *select = program->program.select_stmt;
    ASTNode *filter = program->program.filter_stmt;
    ASTNode *agg = program->program.agg_stmt;

    // 1. Verify LOAD statement
    if (!load) {
        report_semantic_error(1, "Missing LOAD statement. Query must begin with LOAD \"file.csv\" AS alias;", NULL, NULL);
        return g_semantic_errors;
    }

    const char *csv_file = load->load_stmt.file_path;
    const char *alias = load->load_stmt.alias;

    int rc = load_csv_schema_into_symtab(csv_file, alias);
    if (rc != 0) {
        report_semantic_error(load->line_no, "Cannot load dataset '%s'. File not found or empty.", csv_file, NULL);
        return g_semantic_errors;
    }

    // 2. Verify SELECT statement
    if (!select) {
        report_semantic_error(load->line_no, "Missing SELECT statement in query.", NULL, NULL);
    } else {
        for (int i = 0; i < select->select_stmt.col_count; i++) {
            const char *col = select->select_stmt.columns[i];
            if (strcmp(col, "*") == 0) continue; // Wildcard
            Symbol *s = lookup_symbol(col);
            if (!s) {
                report_semantic_error(select->line_no, "Selected column '%s' does not exist in dataset schema.", col, NULL);
            }
        }
    }

    // 3. Verify FILTER statement (if present)
    if (filter && filter->filter_stmt.condition) {
        get_expr_type(filter->filter_stmt.condition);
    }

    // 4. Verify AGGREGATE statement (if present)
    if (agg && agg->agg_stmt.func != AGG_NONE) {
        const char *col = agg->agg_stmt.column;
        if (!col || strcmp(col, "*") == 0) {
            if (agg->agg_stmt.func != AGG_COUNT) {
                report_semantic_error(agg->line_no, "Aggregate function '%s' cannot be applied to wildcard '*'. Only COUNT(*) is valid.", aggfunc_to_string(agg->agg_stmt.func), NULL);
            }
        } else {
            Symbol *s = lookup_symbol(col);
            if (!s) {
                report_semantic_error(agg->line_no, "Aggregate column '%s' does not exist in dataset schema.", col, NULL);
            } else {
                if (agg->agg_stmt.func != AGG_COUNT) {
                    if (s->type != TYPE_INT && s->type != TYPE_FLOAT) {
                        report_semantic_error(agg->line_no, "Aggregate function '%s' requires a numeric column, but '%s' is of type STRING.",
                                              aggfunc_to_string(agg->agg_stmt.func), col);
                    }
                }
            }
        }
    }

    return g_semantic_errors;
}
