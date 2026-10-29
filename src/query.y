%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylineno;
extern int yycolumn;
extern int yylex(void);
void yyerror(const char *s);

/* Root AST pointer */
ASTNode *g_ast_root = NULL;

/* Temporary list structure for parsing column_list */
typedef struct ColNode {
    char *name;
    struct ColNode *next;
} ColNode;

static ColNode* add_col(ColNode *head, char *name) {
    ColNode *node = (ColNode*)malloc(sizeof(ColNode));
    node->name = strdup(name);
    node->next = NULL;
    if (!head) return node;
    ColNode *curr = head;
    while (curr->next) curr = curr->next;
    curr->next = node;
    return head;
}

static void free_cols(ColNode *head) {
    while (head) {
        ColNode *tmp = head->next;
        free(head->name);
        free(head);
        head = tmp;
    }
}
%}

%union {
    int ival;
    double fval;
    char *sval;
    struct ASTNode *node;
    struct ColNode *col_list;
    int op;
    int agg_fn;
}

/* Tokens */
%token LOAD AS SELECT FILTER AGGREGATE
%token SUM AVG COUNT MAX MIN
%token AND OR NOT
%token EQ NEQ GTE LTE

%token <sval> IDENTIFIER STRING
%token <ival> NUMBER
%token <fval> FLOAT_LITERAL

/* Non-terminals typing */
%type <node> program load_stmt select_stmt filter_stmt agg_stmt condition expr primary_expr
%type <col_list> column_list
%type <agg_fn> agg_function
%type <sval> agg_target

/* Operator Precedence & Associativity */
%left OR
%left AND
%left EQ NEQ '<' '>' GTE LTE

%%

program:
    load_stmt select_stmt filter_stmt agg_stmt {
        g_ast_root = create_program_node($1, $2, $3, $4);
        $$ = g_ast_root;
    }
    ;

load_stmt:
    LOAD STRING AS IDENTIFIER ';' {
        $$ = create_load_node($2, $4, @1.first_line ? @1.first_line : yylineno);
        free($2);
        free($4);
    }
    ;

select_stmt:
    SELECT column_list ';' {
        // Count columns
        int count = 0;
        ColNode *curr = $2;
        while (curr) { count++; curr = curr->next; }
        
        char **cols = (char**)malloc(sizeof(char*) * count);
        curr = $2;
        for (int i = 0; i < count; i++) {
            cols[i] = curr->name;
            curr = curr->next;
        }
        $$ = create_select_node(cols, count, yylineno);
        free(cols);
        free_cols($2);
    }
    ;

column_list:
    IDENTIFIER {
        $$ = add_col(NULL, $1);
        free($1);
    }
    | '*' {
        $$ = add_col(NULL, "*");
    }
    | IDENTIFIER ',' column_list {
        ColNode *node = (ColNode*)malloc(sizeof(ColNode));
        node->name = $1;
        node->next = $3;
        $$ = node;
    }
    ;

filter_stmt:
    FILTER condition ';' {
        $$ = create_filter_node($2, yylineno);
    }
    | /* epsilon */ {
        $$ = NULL;
    }
    ;

condition:
    expr {
        $$ = $1;
    }
    ;

expr:
    expr AND expr {
        $$ = create_binary_expr(OP_AND, $1, $3, yylineno);
    }
    | expr OR expr {
        $$ = create_binary_expr(OP_OR, $1, $3, yylineno);
    }
    | expr EQ expr {
        $$ = create_binary_expr(OP_EQ, $1, $3, yylineno);
    }
    | expr NEQ expr {
        $$ = create_binary_expr(OP_NEQ, $1, $3, yylineno);
    }
    | expr '>' expr {
        $$ = create_binary_expr(OP_GT, $1, $3, yylineno);
    }
    | expr '<' expr {
        $$ = create_binary_expr(OP_LT, $1, $3, yylineno);
    }
    | expr GTE expr {
        $$ = create_binary_expr(OP_GTE, $1, $3, yylineno);
    }
    | expr LTE expr {
        $$ = create_binary_expr(OP_LTE, $1, $3, yylineno);
    }
    | '(' expr ')' {
        $$ = $2;
    }
    | primary_expr {
        $$ = $1;
    }
    ;

primary_expr:
    IDENTIFIER {
        $$ = create_ident_expr($1, yylineno);
        free($1);
    }
    | NUMBER {
        $$ = create_int_expr($1, yylineno);
    }
    | FLOAT_LITERAL {
        $$ = create_float_expr($1, yylineno);
    }
    | STRING {
        $$ = create_string_expr($1, yylineno);
        free($1);
    }
    ;

agg_stmt:
    AGGREGATE agg_function '(' agg_target ')' ';' {
        $$ = create_agg_node((AggFunc)$2, $4, yylineno);
        free($4);
    }
    | /* epsilon */ {
        $$ = NULL;
    }
    ;

agg_target:
    IDENTIFIER { $$ = $1; }
    | '*'      { $$ = strdup("*"); }
    ;

agg_function:
    SUM   { $$ = AGG_SUM; }
    | AVG { $$ = AGG_AVG; }
    | COUNT { $$ = AGG_COUNT; }
    | MAX { $$ = AGG_MAX; }
    | MIN { $$ = AGG_MIN; }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Syntax Error [Line %d, Column %d]: %s near current token\n", yylineno, yycolumn, s);
}
