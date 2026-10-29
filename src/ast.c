#include "ast.h"

ASTNode* create_program_node(ASTNode *load, ASTNode *select, ASTNode *filter, ASTNode *agg) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_PROGRAM;
    n->line_no = load ? load->line_no : 1;
    n->program.load_stmt = load;
    n->program.select_stmt = select;
    n->program.filter_stmt = filter;
    n->program.agg_stmt = agg;
    return n;
}

ASTNode* create_load_node(char *file_path, char *alias, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_LOAD;
    n->line_no = line;
    n->load_stmt.file_path = file_path ? strdup(file_path) : NULL;
    n->load_stmt.alias = alias ? strdup(alias) : NULL;
    return n;
}

ASTNode* create_select_node(char **columns, int col_count, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_SELECT;
    n->line_no = line;
    n->select_stmt.col_count = col_count;
    n->select_stmt.columns = (char**)malloc(sizeof(char*) * col_count);
    for (int i = 0; i < col_count; i++) {
        n->select_stmt.columns[i] = strdup(columns[i]);
    }
    return n;
}

ASTNode* create_filter_node(ASTNode *condition, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_FILTER;
    n->line_no = line;
    n->filter_stmt.condition = condition;
    return n;
}

ASTNode* create_agg_node(AggFunc func, char *column, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_AGGREGATE;
    n->line_no = line;
    n->agg_stmt.func = func;
    n->agg_stmt.column = column ? strdup(column) : NULL;
    return n;
}

ASTNode* create_binary_expr(CompOp op, ASTNode *left, ASTNode *right, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_EXPR_BINARY;
    n->line_no = line;
    n->binary_expr.op = op;
    n->binary_expr.left = left;
    n->binary_expr.right = right;
    return n;
}

ASTNode* create_ident_expr(char *name, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_EXPR_IDENT;
    n->line_no = line;
    n->ident_expr.name = name ? strdup(name) : NULL;
    return n;
}

ASTNode* create_int_expr(int val, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_EXPR_INT;
    n->line_no = line;
    n->int_expr.val = val;
    return n;
}

ASTNode* create_float_expr(double val, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_EXPR_FLOAT;
    n->line_no = line;
    n->float_expr.val = val;
    return n;
}

ASTNode* create_string_expr(char *val, int line) {
    ASTNode *n = (ASTNode*)calloc(1, sizeof(ASTNode));
    n->type = NODE_EXPR_STRING;
    n->line_no = line;
    n->string_expr.val = val ? strdup(val) : NULL;
    return n;
}

const char* aggfunc_to_string(AggFunc func) {
    switch (func) {
        case AGG_SUM: return "SUM";
        case AGG_AVG: return "AVG";
        case AGG_COUNT: return "COUNT";
        case AGG_MAX: return "MAX";
        case AGG_MIN: return "MIN";
        default: return "NONE";
    }
}

const char* compop_to_string(CompOp op) {
    switch (op) {
        case OP_EQ: return "==";
        case OP_NEQ: return "!=";
        case OP_GT: return ">";
        case OP_LT: return "<";
        case OP_GTE: return ">=";
        case OP_LTE: return "<=";
        case OP_AND: return "AND";
        case OP_OR: return "OR";
        default: return "?";
    }
}

const char* datatype_to_string(DataType type) {
    switch (type) {
        case TYPE_INT: return "INTEGER";
        case TYPE_FLOAT: return "FLOAT";
        case TYPE_STRING: return "STRING";
        default: return "UNKNOWN";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;

    print_indent(indent);

    switch (node->type) {
        case NODE_PROGRAM:
            printf("ProgramNode\n");
            if (node->program.load_stmt) print_ast(node->program.load_stmt, indent + 1);
            if (node->program.select_stmt) print_ast(node->program.select_stmt, indent + 1);
            if (node->program.filter_stmt) print_ast(node->program.filter_stmt, indent + 1);
            if (node->program.agg_stmt) print_ast(node->program.agg_stmt, indent + 1);
            break;

        case NODE_LOAD:
            printf("|-- LoadStmtNode: File=\"%s\", Alias=\"%s\"\n",
                   node->load_stmt.file_path, node->load_stmt.alias);
            break;

        case NODE_SELECT:
            printf("|-- SelectStmtNode: Columns=[");
            for (int i = 0; i < node->select_stmt.col_count; i++) {
                printf("%s%s", node->select_stmt.columns[i],
                       (i + 1 < node->select_stmt.col_count) ? ", " : "");
            }
            printf("]\n");
            break;

        case NODE_FILTER:
            printf("|-- FilterStmtNode\n");
            if (node->filter_stmt.condition) {
                print_ast(node->filter_stmt.condition, indent + 1);
            }
            break;

        case NODE_AGGREGATE:
            printf("\\-- AggregateStmtNode: Func=%s, TargetColumn=\"%s\"\n",
                   aggfunc_to_string(node->agg_stmt.func),
                   node->agg_stmt.column ? node->agg_stmt.column : "*");
            break;

        case NODE_EXPR_BINARY:
            printf("\\-- BinaryOpNode (%s)\n", compop_to_string(node->binary_expr.op));
            print_ast(node->binary_expr.left, indent + 1);
            print_ast(node->binary_expr.right, indent + 1);
            break;

        case NODE_EXPR_IDENT:
            printf("|-- IdentifierNode: %s\n", node->ident_expr.name);
            break;

        case NODE_EXPR_INT:
            printf("\\-- IntegerLiteralNode: %d\n", node->int_expr.val);
            break;

        case NODE_EXPR_FLOAT:
            printf("\\-- FloatLiteralNode: %g\n", node->float_expr.val);
            break;

        case NODE_EXPR_STRING:
            printf("\\-- StringLiteralNode: \"%s\"\n", node->string_expr.val);
            break;
    }
}

void free_ast(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
            free_ast(node->program.load_stmt);
            free_ast(node->program.select_stmt);
            free_ast(node->program.filter_stmt);
            free_ast(node->program.agg_stmt);
            break;

        case NODE_LOAD:
            free(node->load_stmt.file_path);
            free(node->load_stmt.alias);
            break;

        case NODE_SELECT:
            for (int i = 0; i < node->select_stmt.col_count; i++) {
                free(node->select_stmt.columns[i]);
            }
            free(node->select_stmt.columns);
            break;

        case NODE_FILTER:
            free_ast(node->filter_stmt.condition);
            break;

        case NODE_AGGREGATE:
            free(node->agg_stmt.column);
            break;

        case NODE_EXPR_BINARY:
            free_ast(node->binary_expr.left);
            free_ast(node->binary_expr.right);
            break;

        case NODE_EXPR_IDENT:
            free(node->ident_expr.name);
            break;

        case NODE_EXPR_STRING:
            free(node->string_expr.val);
            break;

        default:
            break;
    }
    free(node);
}
