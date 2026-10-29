#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Data Types for attributes and expressions */
typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING
} DataType;

/* AST Node types */
typedef enum {
    NODE_PROGRAM,
    NODE_LOAD,
    NODE_SELECT,
    NODE_FILTER,
    NODE_AGGREGATE,
    NODE_EXPR_BINARY,
    NODE_EXPR_IDENT,
    NODE_EXPR_INT,
    NODE_EXPR_FLOAT,
    NODE_EXPR_STRING
} NodeType;

/* Comparison and logical operators */
typedef enum {
    OP_EQ,      /* == or = */
    OP_NEQ,     /* != */
    OP_GT,      /* > */
    OP_LT,      /* < */
    OP_GTE,     /* >= */
    OP_LTE,     /* <= */
    OP_AND,     /* AND */
    OP_OR       /* OR */
} CompOp;

/* Aggregate functions */
typedef enum {
    AGG_NONE = 0,
    AGG_SUM,
    AGG_AVG,
    AGG_COUNT,
    AGG_MAX,
    AGG_MIN
} AggFunc;

/* Forward declaration */
typedef struct ASTNode ASTNode;

/* AST Node structure */
struct ASTNode {
    NodeType type;
    int line_no;

    union {
        /* NODE_PROGRAM */
        struct {
            ASTNode *load_stmt;
            ASTNode *select_stmt;
            ASTNode *filter_stmt;
            ASTNode *agg_stmt;
        } program;

        /* NODE_LOAD */
        struct {
            char *file_path;
            char *alias;
        } load_stmt;

        /* NODE_SELECT */
        struct {
            char **columns;
            int col_count;
        } select_stmt;

        /* NODE_FILTER */
        struct {
            ASTNode *condition;
        } filter_stmt;

        /* NODE_AGGREGATE */
        struct {
            AggFunc func;
            char *column;
        } agg_stmt;

        /* NODE_EXPR_BINARY */
        struct {
            CompOp op;
            ASTNode *left;
            ASTNode *right;
        } binary_expr;

        /* NODE_EXPR_IDENT */
        struct {
            char *name;
        } ident_expr;

        /* NODE_EXPR_INT */
        struct {
            int val;
        } int_expr;

        /* NODE_EXPR_FLOAT */
        struct {
            double val;
        } float_expr;

        /* NODE_EXPR_STRING */
        struct {
            char *val;
        } string_expr;
    };
};

/* AST creation helper functions */
ASTNode* create_program_node(ASTNode *load, ASTNode *select, ASTNode *filter, ASTNode *agg);
ASTNode* create_load_node(char *file_path, char *alias, int line);
ASTNode* create_select_node(char **columns, int col_count, int line);
ASTNode* create_filter_node(ASTNode *condition, int line);
ASTNode* create_agg_node(AggFunc func, char *column, int line);

ASTNode* create_binary_expr(CompOp op, ASTNode *left, ASTNode *right, int line);
ASTNode* create_ident_expr(char *name, int line);
ASTNode* create_int_expr(int val, int line);
ASTNode* create_float_expr(double val, int line);
ASTNode* create_string_expr(char *val, int line);

const char* aggfunc_to_string(AggFunc func);
const char* compop_to_string(CompOp op);
const char* datatype_to_string(DataType type);

/* Visualization and cleanup */
void print_ast(ASTNode *node, int indent);
void free_ast(ASTNode *node);

#endif /* AST_H */
