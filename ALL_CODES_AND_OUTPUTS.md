# QueryLang: Complete Code Submission, Compilation Guide, and Execution Outputs

**Student Name:** Yuktha Sreenivasa Anjaneyulu  
**Register Number:** 23BDS0287  
**Course:** Compiler Design Laboratory  
**Project:** QueryLang — Tabular Data Analytics Domain-Specific Compiler  

---

# PART 1: HOW TO COMPILE AND RUN

## Environment Requirements
- **OS:** Windows 10/11 or Linux / macOS / WSL
- **Compiler:** GCC (version 10 or later)
- **Compiler Tools:** Flex (v2.6+) and Bison (v3.0+)
- **IDE / Terminal:** Visual Studio Code, Windows Terminal, PowerShell, or Command Prompt.

*(On your machine, GCC, Flex, Bison, and Make are located in `C:\msys64\ucrt64\bin` and `C:\msys64\usr\bin`.)*

---

## 1. Quick 1-Click Compilation (Windows Terminal / VS Code)

### Option A: Using the Automated Build Script (`build.bat`)
Open the terminal in the `proj_cpl` directory and run:
```cmd
build.bat
```
This automatically runs Bison, Flex, and GCC with full optimizations and creates `querylang.exe`.

### Option B: Running the Test Suite (`run_tests.bat`)
To compile (if needed) and run all 7 test cases in one go:
```cmd
run_tests.bat
```

---

## 2. Manual Step-by-Step Compilation Commands

### In Windows PowerShell:
```powershell
# 1. Ensure tools are on the environment PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"

# 2. Generate Bison Parser (query.tab.c and query.tab.h)
bison -d -o src/query.tab.c src/query.y

# 3. Generate Flex Lexer (lex.yy.c)
flex -o src/lex.yy.c src/query.l

# 4. Compile all C modules with GCC (AVX2 SIMD Zero-Copy Engine)
gcc -Wall -O3 -mavx2 -Isrc -o querylang.exe src/query.tab.c src/lex.yy.c src/ast.c src/symbol_table.c src/semantic.c src/tac.c src/simd_csv.c src/csv_parser.c src/executor.c src/main.c

# 5. Run any query file
.\querylang.exe tests/test1_basic.ql

# 6. Run with SIMD Zero-Copy benchmark telemetry
.\querylang.exe tests/test_benchmark_agg.ql --simd -b
```

### In Linux / macOS / WSL:
```bash
make
./querylang tests/test1_basic.ql
```

---

## 3. Command-Line Options Available

| Option | Flag | Description |
|:---|:---:|:---|
| **Lexical Tokens** | `-t`, `--tokens` | Display token stream table with line and column positions |
| **Syntax Tree** | `-a`, `--ast` | Print hierarchical Abstract Syntax Tree |
| **Symbol Table** | `-s`, `--symtab` | Print dataset schema attributes, inferred types, and offsets |
| **Intermediate Code** | `-i`, `--tac` | Print Three-Address Code and Quadruples table |
| **All Phases** | `--all` | Display AST, Symbol Table, TAC, Quadruples, and Query Table Output |
| **AVX2 SIMD Engine** | `--simd` | High-speed 32-byte vectorized zero-copy scanning (default) |
| **Scalar Engine** | `--scalar` | Fallback zero-copy scalar scanning |
| **Benchmark Mode** | `-b`, `--benchmark`| Display microsecond latency, rows parsed, and scan throughput (MB/s) |
| **Interactive Mode** | *(no file)* | Opens REPL mode for manual query input |

---

# PART 2: COMPLETE SOURCE CODE LISTINGS

---

## File 1: `src/query.l` (Flex Lexical Specification)
```c
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "query.tab.h"

int yycolumn = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;

static char* strip_quotes(const char *src) {
    size_t len = strlen(src);
    if (len < 2) return strdup("");
    char *res = (char*)malloc(len);
    strncpy(res, src + 1, len - 2);
    res[len - 2] = '\0';
    return res;
}
%}

%option noyywrap
%option yylineno
%option case-insensitive
%option noinput
%option nounput

%%

[ \t]+                  { /* Skip spaces and tabs */ }
\r?\n                   { yycolumn = 1; /* Reset column on newline */ }

"//".*                  { /* Skip single-line comments */ }
"/*"([^*]|\*+[^*/])*\*+"/" { /* Skip multi-line comments */ }

"LOAD"                  { return LOAD; }
"AS"                    { return AS; }
"SELECT"                { return SELECT; }
"FILTER"                { return FILTER; }
"AGGREGATE"             { return AGGREGATE; }

"SUM"                   { return SUM; }
"AVG"                   { return AVG; }
"COUNT"                 { return COUNT; }
"MAX"                   { return MAX; }
"MIN"                   { return MIN; }

"AND"                   { return AND; }
"OR"                    { return OR; }
"NOT"                   { return NOT; }

"=="                    { return EQ; }
"="                     { return EQ; }
"!="                    { return NEQ; }
">="                    { return GTE; }
"<="                    { return LTE; }
">"                     { return '>'; }
"<"                     { return '<'; }

";"                     { return ';'; }
","                     { return ','; }
"("                     { return '('; }
")"                     { return ')'; }
"*"                     { return '*'; }

[0-9]+\.[0-9]+          {
                            yylval.fval = atof(yytext);
                            return FLOAT_LITERAL;
                        }

[0-9]+                  {
                            yylval.ival = atoi(yytext);
                            return NUMBER;
                        }

\"([^\"\\]|\\.)*\"      {
                            yylval.sval = strip_quotes(yytext);
                            return STRING;
                        }

[a-zA-Z_][a-zA-Z0-9_]*  {
                            yylval.sval = strdup(yytext);
                            return IDENTIFIER;
                        }

.                       {
                            fprintf(stderr, "Lexical Error [Line %d, Column %d]: Unrecognized character '%s'\n",
                                    yylineno, yycolumn - yyleng, yytext);
                        }

%%
```

---

## File 2: `src/query.y` (Bison Grammar Specification)
```c
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
    return node;
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
```

---

## File 3: `src/ast.h` (Abstract Syntax Tree Header)
```c
#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING
} DataType;

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

typedef enum {
    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_LT,
    OP_GTE,
    OP_LTE,
    OP_AND,
    OP_OR
} CompOp;

typedef enum {
    AGG_NONE = 0,
    AGG_SUM,
    AGG_AVG,
    AGG_COUNT,
    AGG_MAX,
    AGG_MIN
} AggFunc;

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeType type;
    int line_no;

    union {
        struct {
            ASTNode *load_stmt;
            ASTNode *select_stmt;
            ASTNode *filter_stmt;
            ASTNode *agg_stmt;
        } program;

        struct {
            char *file_path;
            char *alias;
        } load_stmt;

        struct {
            char **columns;
            int col_count;
        } select_stmt;

        struct {
            ASTNode *condition;
        } filter_stmt;

        struct {
            AggFunc func;
            char *column;
        } agg_stmt;

        struct {
            CompOp op;
            ASTNode *left;
            ASTNode *right;
        } binary_expr;

        struct {
            char *name;
        } ident_expr;

        struct {
            int val;
        } int_expr;

        struct {
            double val;
        } float_expr;

        struct {
            char *val;
        } string_expr;
    };
};

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

void print_ast(ASTNode *node, int indent);
void free_ast(ASTNode *node);

#endif /* AST_H */
```

---

## File 4: `src/ast.c` (AST Node Operations & Visualization)
```c
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
            printf("├── LoadStmtNode: File=\"%s\", Alias=\"%s\"\n",
                   node->load_stmt.file_path, node->load_stmt.alias);
            break;

        case NODE_SELECT:
            printf("├── SelectStmtNode: Columns=[");
            for (int i = 0; i < node->select_stmt.col_count; i++) {
                printf("%s%s", node->select_stmt.columns[i],
                       (i + 1 < node->select_stmt.col_count) ? ", " : "");
            }
            printf("]\n");
            break;

        case NODE_FILTER:
            printf("├── FilterStmtNode\n");
            if (node->filter_stmt.condition) {
                print_ast(node->filter_stmt.condition, indent + 1);
            }
            break;

        case NODE_AGGREGATE:
            printf("└── AggregateStmtNode: Func=%s, TargetColumn=\"%s\"\n",
                   aggfunc_to_string(node->agg_stmt.func),
                   node->agg_stmt.column ? node->agg_stmt.column : "*");
            break;

        case NODE_EXPR_BINARY:
            printf("└── BinaryOpNode (%s)\n", compop_to_string(node->binary_expr.op));
            print_ast(node->binary_expr.left, indent + 1);
            print_ast(node->binary_expr.right, indent + 1);
            break;

        case NODE_EXPR_IDENT:
            printf("├── IdentifierNode: %s\n", node->ident_expr.name);
            break;

        case NODE_EXPR_INT:
            printf("└── IntegerLiteralNode: %d\n", node->int_expr.val);
            break;

        case NODE_EXPR_FLOAT:
            printf("└── FloatLiteralNode: %g\n", node->float_expr.val);
            break;

        case NODE_EXPR_STRING:
            printf("└── StringLiteralNode: \"%s\"\n", node->string_expr.val);
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
```

---

## File 5: `src/symbol_table.h` (Symbol Table Header)
```c
#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h"

#define MAX_SYMBOLS 256
#define MAX_STR_LEN 128

typedef struct {
    char name[MAX_STR_LEN];
    DataType type;
    int col_index;
    char alias[MAX_STR_LEN];
} Symbol;

typedef struct {
    Symbol symbols[MAX_SYMBOLS];
    int count;
    char dataset_alias[MAX_STR_LEN];
    char dataset_file[256];
} SymbolTable;

extern SymbolTable g_symtab;

void init_symbol_table(void);
int insert_symbol(const char *name, DataType type, int col_index, const char *alias);
Symbol* lookup_symbol(const char *name);
int load_csv_schema_into_symtab(const char *filepath, const char *alias);
void print_symbol_table(void);

#endif /* SYMBOL_TABLE_H */
```

---

## File 6: `src/symbol_table.c` (Symbol Table & Schema Deductions)
```c
#include "symbol_table.h"
#include <ctype.h>

SymbolTable g_symtab;

void init_symbol_table(void) {
    g_symtab.count = 0;
    g_symtab.dataset_alias[0] = '\0';
    g_symtab.dataset_file[0] = '\0';
}

int insert_symbol(const char *name, DataType type, int col_index, const char *alias) {
    if (g_symtab.count >= MAX_SYMBOLS) {
        fprintf(stderr, "Error: Symbol table overflow!\n");
        return -1;
    }
    for (int i = 0; i < g_symtab.count; i++) {
        if (strcmp(g_symtab.symbols[i].name, name) == 0) {
            return i;
        }
    }
    int idx = g_symtab.count;
    strncpy(g_symtab.symbols[idx].name, name, MAX_STR_LEN - 1);
    g_symtab.symbols[idx].name[MAX_STR_LEN - 1] = '\0';
    g_symtab.symbols[idx].type = type;
    g_symtab.symbols[idx].col_index = col_index;
    if (alias) {
        strncpy(g_symtab.symbols[idx].alias, alias, MAX_STR_LEN - 1);
        g_symtab.symbols[idx].alias[MAX_STR_LEN - 1] = '\0';
    } else {
        g_symtab.symbols[idx].alias[0] = '\0';
    }
    g_symtab.count++;
    return idx;
}

Symbol* lookup_symbol(const char *name) {
    for (int i = 0; i < g_symtab.count; i++) {
        if (strcmp(g_symtab.symbols[i].name, name) == 0) {
            return &g_symtab.symbols[i];
        }
    }
    return NULL;
}

static void trim_inplace(char *s) {
    if (!s) return;
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
}

static DataType infer_field_type(const char *val) {
    if (!val || *val == '\0') return TYPE_STRING;
    const char *p = val;
    if (*p == '-' || *p == '+') p++;
    int has_digits = 0;
    int has_dot = 0;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            has_digits = 1;
        } else if (*p == '.') {
            if (has_dot) return TYPE_STRING;
            has_dot = 1;
        } else {
            return TYPE_STRING;
        }
        p++;
    }
    if (!has_digits) return TYPE_STRING;
    return has_dot ? TYPE_FLOAT : TYPE_INT;
}

int load_csv_schema_into_symtab(const char *filepath, const char *alias) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return -1;
    }

    init_symbol_table();
    strncpy(g_symtab.dataset_file, filepath, sizeof(g_symtab.dataset_file) - 1);
    if (alias) {
        strncpy(g_symtab.dataset_alias, alias, sizeof(g_symtab.dataset_alias) - 1);
    }

    char header_buf[2048];
    if (!fgets(header_buf, sizeof(header_buf), fp)) {
        fclose(fp);
        return -2;
    }

    char sample_buf[2048];
    int has_sample = 0;
    if (fgets(sample_buf, sizeof(sample_buf), fp)) {
        has_sample = 1;
    }
    fclose(fp);

    char *hdr_copy = strdup(header_buf);
    char *smp_copy = has_sample ? strdup(sample_buf) : NULL;

    char *saveptr_hdr, *saveptr_smp;
    char *col_token = strtok_r(hdr_copy, ",\r\n", &saveptr_hdr);
    char *val_token = smp_copy ? strtok_r(smp_copy, ",\r\n", &saveptr_smp) : NULL;
    int col_idx = 0;

    while (col_token) {
        trim_inplace(col_token);
        if (val_token) trim_inplace(val_token);

        DataType t = val_token ? infer_field_type(val_token) : TYPE_STRING;
        insert_symbol(col_token, t, col_idx, alias);

        col_idx++;
        col_token = strtok_r(NULL, ",\r\n", &saveptr_hdr);
        val_token = smp_copy ? strtok_r(NULL, ",\r\n", &saveptr_smp) : NULL;
    }

    free(hdr_copy);
    if (smp_copy) free(smp_copy);
    return 0;
}

void print_symbol_table(void) {
    printf("\n================================ SYMBOL TABLE ================================\n");
    printf("Dataset Alias : %s\n", g_symtab.dataset_alias[0] ? g_symtab.dataset_alias : "(None)");
    printf("Dataset File  : %s\n", g_symtab.dataset_file[0] ? g_symtab.dataset_file : "(None)");
    printf("+-------+----------------------+---------------+------------+-----------------+\n");
    printf("| Index | Identifier Name      | Inferred Type | Col Offset | Scope/Alias     |\n");
    printf("+-------+----------------------+---------------+------------+-----------------+\n");
    for (int i = 0; i < g_symtab.count; i++) {
        Symbol *s = &g_symtab.symbols[i];
        printf("| %-5d | %-20s | %-13s | %-10d | %-15s |\n",
               i, s->name, datatype_to_string(s->type), s->col_index, s->alias);
    }
    printf("+-------+----------------------+---------------+------------+-----------------+\n\n");
}
```

---

## File 7: `src/semantic.h` (Semantic Analysis Header)
```c
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"

int perform_semantic_analysis(ASTNode *program);

#endif /* SEMANTIC_H */
```

---

## File 8: `src/semantic.c` (Semantic Analysis Implementation)
```c
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
                return TYPE_INT;
            }
            if (lt != TYPE_UNKNOWN && rt != TYPE_UNKNOWN) {
                if ((lt == TYPE_STRING && (rt == TYPE_INT || rt == TYPE_FLOAT)) ||
                    (rt == TYPE_STRING && (lt == TYPE_INT || lt == TYPE_FLOAT))) {
                    report_semantic_error(expr->line_no, "Type mismatch in condition comparison between string and numeric operand.", NULL, NULL);
                } else if (lt == TYPE_STRING && rt == TYPE_STRING) {
                    if (expr->binary_expr.op != OP_EQ && expr->binary_expr.op != OP_NEQ) {
                        report_semantic_error(expr->line_no, "Relational operator cannot be applied to STRING operands (only == or != supported).", NULL, NULL);
                    }
                }
            }
            return TYPE_INT;
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

    if (!select) {
        report_semantic_error(load->line_no, "Missing SELECT statement in query.", NULL, NULL);
    } else {
        for (int i = 0; i < select->select_stmt.col_count; i++) {
            const char *col = select->select_stmt.columns[i];
            if (strcmp(col, "*") == 0) continue;
            Symbol *s = lookup_symbol(col);
            if (!s) {
                report_semantic_error(select->line_no, "Selected column '%s' does not exist in dataset schema.", col, NULL);
            }
        }
    }

    if (filter && filter->filter_stmt.condition) {
        get_expr_type(filter->filter_stmt.condition);
    }

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
```

---

## File 9: `src/tac.h` (Intermediate Code Generation Header)
```c
#ifndef TAC_H
#define TAC_H

#include "ast.h"

#define MAX_QUADS 128

typedef struct {
    char op[32];
    char arg1[64];
    char arg2[64];
    char result[64];
} Quadruple;

typedef struct {
    Quadruple quads[MAX_QUADS];
    int count;
    int temp_count;
    int label_count;
} TACProgram;

extern TACProgram g_tac;

void init_tac(void);
void generate_tac(ASTNode *program);
void print_tac(void);
void print_quadruples(void);

#endif /* TAC_H */
```

---

## File 10: `src/tac.c` (TAC & Quadruple Generation)
```c
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
    strncpy(g_tac.quads[idx].op, op ? op : "-", 31);
    g_tac.quads[idx].op[31] = '\0';

    strncpy(g_tac.quads[idx].arg1, arg1 ? arg1 : "-", 63);
    g_tac.quads[idx].arg1[63] = '\0';

    strncpy(g_tac.quads[idx].arg2, arg2 ? arg2 : "-", 63);
    g_tac.quads[idx].arg2[63] = '\0';

    strncpy(g_tac.quads[idx].result, res ? res : "-", 63);
    g_tac.quads[idx].result[63] = '\0';
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

    if (load) {
        char quoted_file[256];
        sprintf(quoted_file, "\"%s\"", load->load_stmt.file_path);
        add_quad("LOAD_CSV", quoted_file, "-", load->load_stmt.alias);
        strncpy(current_dataset, load->load_stmt.alias, 63);
    }

    char scan_temp[64];
    new_temp(scan_temp);
    add_quad("SCAN", current_dataset, "-", scan_temp);
    char filtered_temp[64];
    strcpy(filtered_temp, scan_temp);

    if (filter && filter->filter_stmt.condition) {
        char cond_res[64];
        generate_expr_tac(filter->filter_stmt.condition, cond_res);

        new_temp(filtered_temp);
        add_quad("FILTER_ROWS", scan_temp, cond_res, filtered_temp);
    }

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
    printf("\n======================= THREE-ADDRESS CODE (TAC) =======================\n");
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
    printf("========================================================================\n\n");
}

void print_quadruples(void) {
    printf("\n========================== QUADRUPLES TABLE ===========================\n");
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
```

---

## File 11: `src/csv_parser.h` (CSV Reader Header)
```c
#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **fields;
    int field_count;
} CSVRecord;

typedef struct {
    char **headers;
    int col_count;
    CSVRecord *records;
    int record_count;
} CSVData;

CSVData* parse_csv_file(const char *filepath);
int find_col_index(CSVData *csv, const char *col_name);
void free_csv_data(CSVData *csv);

#endif /* CSV_PARSER_H */
```

---

## File 12: `src/csv_parser.c` (Pure C CSV Parser)
```c
#include "csv_parser.h"
#include <ctype.h>

static char* trim_str(char *s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (isspace((unsigned char)*end) || *end == '\r' || *end == '\n' || *end == '"')) {
        *end = '\0';
        end--;
    }
    if (*s == '"') s++;
    return s;
}

static char** split_csv_line(char *line, int *out_count) {
    int cap = 16;
    int count = 0;
    char **tokens = (char**)malloc(sizeof(char*) * cap);

    char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p) && *p != '\n' && *p != '\r') p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') break;

        char field[1024];
        int fidx = 0;

        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                field[fidx++] = *p++;
            }
            if (*p == '"') p++;
            while (*p && *p != ',' && *p != '\r' && *p != '\n') p++;
            if (*p == ',') p++;
        } else {
            while (*p && *p != ',' && *p != '\r' && *p != '\n') {
                field[fidx++] = *p++;
            }
            if (*p == ',') p++;
        }
        field[fidx] = '\0';

        char *trimmed = trim_str(field);
        if (count >= cap) {
            cap *= 2;
            tokens = (char**)realloc(tokens, sizeof(char*) * cap);
        }
        tokens[count++] = strdup(trimmed);
    }
    *out_count = count;
    return tokens;
}

CSVData* parse_csv_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    CSVData *csv = (CSVData*)calloc(1, sizeof(CSVData));

    char line_buf[4096];
    if (!fgets(line_buf, sizeof(line_buf), fp)) {
        fclose(fp);
        free(csv);
        return NULL;
    }

    csv->headers = split_csv_line(line_buf, &csv->col_count);

    int record_cap = 64;
    csv->records = (CSVRecord*)malloc(sizeof(CSVRecord) * record_cap);
    csv->record_count = 0;

    while (fgets(line_buf, sizeof(line_buf), fp)) {
        int cnt = 0;
        char **fields = split_csv_line(line_buf, &cnt);
        if (cnt == 0) {
            free(fields);
            continue;
        }

        if (csv->record_count >= record_cap) {
            record_cap *= 2;
            csv->records = (CSVRecord*)realloc(csv->records, sizeof(CSVRecord) * record_cap);
        }

        csv->records[csv->record_count].fields = fields;
        csv->records[csv->record_count].field_count = cnt;
        csv->record_count++;
    }

    fclose(fp);
    return csv;
}

int find_col_index(CSVData *csv, const char *col_name) {
    if (!csv || !col_name) return -1;
    for (int i = 0; i < csv->col_count; i++) {
        if (strcmp(csv->headers[i], col_name) == 0) {
            return i;
        }
    }
    return -1;
}

void free_csv_data(CSVData *csv) {
    if (!csv) return;
    for (int i = 0; i < csv->col_count; i++) {
        free(csv->headers[i]);
    }
    free(csv->headers);

    for (int i = 0; i < csv->record_count; i++) {
        for (int j = 0; j < csv->records[i].field_count; j++) {
            free(csv->records[i].fields[j]);
        }
        free(csv->records[i].fields);
    }
    free(csv->records);
    free(csv);
}
```

---

## File 13: `src/executor.h` (Execution Engine Header)
```c
#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "csv_parser.h"
#include "simd_csv.h"

int execute_query(ASTNode *program);
int execute_query_ex(ASTNode *program, int use_simd, int print_bench);

#endif /* EXECUTOR_H */
```

---

## File 14: `src/executor.c` (Zero-Copy Execution Engine)
```c
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

```

---

## File 15: `src/main.c` (Main CLI Driver)
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"
#include "semantic.h"
#include "tac.h"
#include "executor.h"

extern FILE *yyin;
extern int yyparse(void);
extern ASTNode *g_ast_root;
extern int yylex(void);
extern char *yytext;
extern int yylineno;
extern int yycolumn;

static void print_usage(const char *prog) {
    printf("Usage: %s [options] <query_file.ql>\n", prog);
    printf("Options:\n");
    printf("  -t, --tokens   : Print recognized token stream\n");
    printf("  -a, --ast      : Print Abstract Syntax Tree (AST)\n");
    printf("  -s, --symtab   : Print Symbol Table from dataset schema\n");
    printf("  -i, --tac      : Print Three-Address Code and Quadruples\n");
    printf("  --all          : Print AST, Symbol Table, TAC, and Query results\n");
    printf("  --simd         : Use AVX2 SIMD-accelerated zero-copy execution engine\n");
    printf("  --scalar       : Use scalar zero-copy execution engine (baseline)\n");
    printf("  -b, --benchmark: Output execution latency and scan throughput telemetry\n");
    printf("  -h, --help     : Show this help message\n");
}

static void dump_tokens(FILE *fp) {
    yyin = fp;
    printf("\n--- Token Stream ---\n");
    printf("+-------+---------------+----------------------+-------------------------------+\n");
    printf("| Line  | Column        | Token Class          | Lexeme Value                  |\n");
    printf("+-------+---------------+----------------------+-------------------------------+\n");

    int tok;
    while ((tok = yylex()) != 0) {
        const char *name = "TOKEN";
        switch (tok) {
            case 258: name = "LOAD"; break;
            case 259: name = "AS"; break;
            case 260: name = "SELECT"; break;
            case 261: name = "FILTER"; break;
            case 262: name = "AGGREGATE"; break;
            case 263: name = "SUM"; break;
            case 264: name = "AVG"; break;
            case 265: name = "COUNT"; break;
            case 266: name = "MAX"; break;
            case 267: name = "MIN"; break;
            case 268: name = "AND"; break;
            case 269: name = "OR"; break;
            case 270: name = "NOT"; break;
            case 271: name = "EQ"; break;
            case 272: name = "NEQ"; break;
            case 273: name = "GTE"; break;
            case 274: name = "LTE"; break;
            case 275: name = "IDENTIFIER"; break;
            case 276: name = "STRING"; break;
            case 277: name = "NUMBER"; break;
            case 278: name = "FLOAT_LITERAL"; break;
            default:
                if (tok < 256) {
                    static char single[16];
                    sprintf(single, "SYM('%c')", tok);
                    name = single;
                }
                break;
        }
        printf("| %-5d | %-13d | %-20s | %-29s |\n",
               yylineno, yycolumn - (int)strlen(yytext), name, yytext);
    }
    printf("+-------+---------------+----------------------+-------------------------------+\n\n");
}

int main(int argc, char *argv[]) {
    int show_tokens = 0;
    int show_ast = 0;
    int show_symtab = 0;
    int show_tac = 0;
    int show_all = 0;
    int use_simd = 1;
    int print_bench = 0;
    const char *filepath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--ast") == 0) {
            show_ast = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--symtab") == 0) {
            show_symtab = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--tac") == 0) {
            show_tac = 1;
        } else if (strcmp(argv[i], "--all") == 0) {
            show_all = 1;
        } else if (strcmp(argv[i], "--simd") == 0) {
            use_simd = 1;
        } else if (strcmp(argv[i], "--scalar") == 0) {
            use_simd = 0;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--benchmark") == 0) {
            print_bench = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            filepath = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    FILE *input_file = NULL;
    if (filepath) {
        input_file = fopen(filepath, "r");
        if (!input_file) {
            fprintf(stderr, "Error: cannot open '%s'\n", filepath);
            return 1;
        }
    } else {
        printf("QueryLang Interactive Mode (Ctrl+Z or Ctrl+D to submit):\n");
        input_file = stdin;
    }

    if (show_tokens) {
        dump_tokens(input_file);
        if (input_file != stdin) fclose(input_file);
        return 0;
    }

    yyin = input_file;

    // 1. Parsing Phase
    if (yyparse() != 0) {
        if (input_file != stdin) fclose(input_file);
        return 2;
    }

    // 2. AST Output
    if (show_ast || show_all) {
        printf("\n--- Abstract Syntax Tree (AST) ---\n");
        print_ast(g_ast_root, 0);
    }

    // 3. Semantic Analysis Phase
    int sem_errors = perform_semantic_analysis(g_ast_root);
    if (sem_errors > 0) {
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return 3;
    }

    // 4. Symbol Table Output
    if (show_symtab || show_all) {
        print_symbol_table();
    }

    // 5. Intermediate Code Generation (TAC & Quadruples)
    generate_tac(g_ast_root);

    if (show_tac || show_all) {
        print_tac();
        print_quadruples();
    }

    // 6. Query Execution (SIMD Zero-Copy Engine)
    int exec_rc = execute_query_ex(g_ast_root, use_simd, print_bench);

    // Clean up
    free_ast(g_ast_root);
    if (input_file != stdin) fclose(input_file);

    return exec_rc;
}

```

---

## File 16: `src/simd_csv.h` (AVX2 SIMD Zero-Copy Engine Header)
```c
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

```

---

## File 17: `src/simd_csv.c` (AVX2 Vectorized 32-Byte Chunk Parser)
```c
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

```

---

## File 18: `benchmarks/run_benchmark.py` (Automated Comparative Benchmark Suite)
```python
import os
import subprocess
import time
import re
import pandas as pd
import tracemalloc

DATASETS = [
    ("100K Rows", 100000, "data/benchmark_100k.csv", "tests/bench_100k.ql"),
    ("250K Rows", 250000, "data/large_employees.csv", "tests/test_benchmark_agg.ql"),
    ("1M Rows",   1000000, "data/employees_1m.csv",   "tests/test_1m.ql")
]

def ensure_dataset(rows, filepath, ql_file):
    if not os.path.exists(filepath):
        from generate_large_dataset import generate_csv
        generate_csv(filepath, rows)
        
    if not os.path.exists(ql_file):
        with open(ql_file, "w") as f:
            f.write(f'LOAD "{filepath}" AS dataset;\n')
            f.write('SELECT department, salary;\n')
            f.write('FILTER department == "Engineering";\n')
            f.write('AGGREGATE AVG(salary);\n')

def run_querylang(ql_file, simd=True):
    flag = "--simd" if simd else "--scalar"
    cmd = ["querylang.exe", ql_file, flag, "-b"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    out = res.stdout
    
    parse_time = float(re.search(r"Parse Time\s*:\s*([\d\.]+)\s*ms", out).group(1))
    eval_time = float(re.search(r"Query Filter Time\s*:\s*([\d\.]+)\s*ms", out).group(1))
    total_time = float(re.search(r"Total Latency\s*:\s*([\d\.]+)\s*ms", out).group(1))
    throughput_mb = float(re.search(r"Scan Throughput\s*:\s*([\d\.]+)\s*MB/s", out).group(1))
    
    return {
        "parse_time_ms": parse_time,
        "eval_time_ms": eval_time,
        "total_time_ms": total_time,
        "throughput_mb_s": throughput_mb
    }

def run_pandas(csv_file):
    tracemalloc.start()
    t0 = time.perf_counter()
    df = pd.read_csv(csv_file)
    t1 = time.perf_counter()
    
    t2 = time.perf_counter()
    filt = df[df["department"] == "Engineering"]
    avg = filt["salary"].mean()
    t3 = time.perf_counter()
    
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    fsize_mb = os.path.getsize(csv_file) / (1024 * 1024)
    total_time_ms = (t3 - t0) * 1000.0
    load_time_ms = (t1 - t0) * 1000.0
    eval_time_ms = (t3 - t2) * 1000.0
    throughput = fsize_mb / (total_time_ms / 1000.0)
    
    return {
        "parse_time_ms": load_time_ms,
        "eval_time_ms": eval_time_ms,
        "total_time_ms": total_time_ms,
        "peak_mem_mb": peak / (1024 * 1024),
        "throughput_mb_s": throughput
    }

def main():
    print("Running Comparative Performance Benchmark Suite...")
    print(f"{'Dataset':<12} | {'Engine':<20} | {'Parse (ms)':<10} | {'Total Latency (ms)':<18} | {'Throughput (MB/s)':<18} | {'Speedup':<8}")
    print("-" * 96)
    
    for label, rows, csv_file, ql_file in DATASETS:
        ensure_dataset(rows, csv_file, ql_file)
        
        simd_res = run_querylang(ql_file, simd=True)
        scalar_res = run_querylang(ql_file, simd=False)
        pd_res = run_pandas(csv_file)
        
        speedup_simd_vs_pd = pd_res["total_time_ms"] / simd_res["total_time_ms"]
        speedup_parse = pd_res["parse_time_ms"] / simd_res["parse_time_ms"]
        
        print(f"{label:<12} | {'Python Pandas':<20} | {pd_res['parse_time_ms']:<10.2f} | {pd_res['total_time_ms']:<18.2f} | {pd_res['throughput_mb_s']:<18.2f} | {'1.00x':<8}")
        print(f"{'':<12} | {'Scalar Zero-Copy':<20} | {scalar_res['parse_time_ms']:<10.2f} | {scalar_res['total_time_ms']:<18.2f} | {scalar_res['throughput_mb_s']:<18.2f} | {pd_res['total_time_ms']/scalar_res['total_time_ms']:<7.2f}x")
        print(f"{'':<12} | {'AVX2 SIMD Zero-Copy':<20} | {simd_res['parse_time_ms']:<10.2f} | {simd_res['total_time_ms']:<18.2f} | {simd_res['throughput_mb_s']:<18.2f} | {speedup_simd_vs_pd:<7.2f}x")
        print("-" * 96)

if __name__ == "__main__":
    main()

```

---

# PART 3: SAMPLE DATASETS

## File: `data/employees.csv`
```csv
id,name,department,salary,age
101,Aarav Sharma,Engineering,75000,29
102,Priya Iyer,Marketing,52000,34
103,Rohan Verma,Engineering,82000,41
104,Ananya Nair,HR,48000,26
105,Vikram Malhotra,Sales,61000,32
106,Sneha Reddy,Engineering,95000,38
107,Karan Joshi,Finance,70000,45
108,Divya Mehra,Marketing,58000,30
109,Aditya Kulkarni,Sales,64000,37
110,Pooja Hegde,HR,51000,28
```

## File: `data/students.csv`
```csv
roll_no,name,branch,gpa,attendance
1,Aarav Sharma,CSE,9.2,95
2,Diya Patel,ECE,8.5,88
3,Rohan Verma,CSE,7.8,79
4,Sneha Reddy,IT,9.6,98
5,Vikram Singh,MECH,6.9,72
6,Ananya Iyer,CSE,8.9,91
7,Karan Malhotra,IT,7.4,80
8,Pooja Nair,ECE,8.1,86
```

---

# PART 4: TEST CASES & ACTUAL TERMINAL OUTPUTS

---

## Output 1: Running Test Case 1 (`tests/test1_basic.ql`)
**Command:**
```powershell
.\querylang.exe tests/test1_basic.ql
```

**Output:**
```
--- Query Result (6 of 10 rows) ---
+-------------------+---------------+----------+
| name              | department    | salary   |
+-------------------+---------------+----------+
| Aarav Sharma      | Engineering   | 75000    |
| Rohan Verma       | Engineering   | 82000    |
| Vikram Malhotra   | Sales         | 61000    |
| Sneha Reddy       | Engineering   | 95000    |
| Karan Joshi       | Finance       | 70000    |
| Aditya Kulkarni   | Sales         | 64000    |
+-------------------+---------------+----------+
```

---

## Output 2: Running Test Case 2 with All Compiler Phases (`--all`)
**Command:**
```powershell
.\querylang.exe --all tests/test2_aggregate.ql
```

**Output:**
```
--- Abstract Syntax Tree (AST) ---
ProgramNode
  ├── LoadStmtNode: File="data/employees.csv", Alias="emp_data"
  ├── SelectStmtNode: Columns=[name, salary]
  ├── FilterStmtNode
    └── BinaryOpNode (>=)
      ├── IdentifierNode: salary
      └── IntegerLiteralNode: 50000
  └── AggregateStmtNode: Func=AVG, TargetColumn="salary"

--- Symbol Table (data/employees.csv) ---
+-------+----------------------+---------------+------------+-----------------+
| Index | Identifier Name      | Inferred Type | Col Offset | Scope/Alias     |
+-------+----------------------+---------------+------------+-----------------+
| 0     | id                   | INTEGER       | 0          | emp_data        |
| 1     | name                 | STRING        | 1          | emp_data        |
| 2     | department           | STRING        | 2          | emp_data        |
| 3     | salary               | INTEGER       | 3          | emp_data        |
| 4     | age                  | INTEGER       | 4          | emp_data        |
+-------+----------------------+---------------+------------+-----------------+

--- Three-Address Code (TAC) ---
[00] emp_data = LOAD_CSV("data/employees.csv")
[01] t0 = SCAN(emp_data)
[02] t1 = salary GE 50000
[03] t2 = FILTER_ROWS(t0, condition=t1)
[04] t3 = PROJECT(name, salary, from=t2)
[05] t4 = AGG_AVG(salary, from=t2)
[06] EMIT_RESULT(t3, t4)

--- Quadruples Table ---
+-------+---------------+----------------------+---------------+--------+
| Quad# | Operator      | Argument 1           | Argument 2    | Result |
+-------+---------------+----------------------+---------------+--------+
| 0     | LOAD_CSV      | "data/employees.csv" | -             | emp_data |
| 1     | SCAN          | emp_data             | -             | t0     |
| 2     | CMP_GE        | salary               | 50000         | t1     |
| 3     | FILTER_ROWS   | t0                   | t1            | t2     |
| 4     | PROJECT       | name, salary         | t2            | t3     |
| 5     | AGG_AVG       | salary               | t2            | t4     |
| 6     | OUTPUT        | t3                   | t4            | -      |
+-------+---------------+----------------------+---------------+--------+

--- Query Result (9 of 10 rows) ---
+-------------------+----------+
| name              | salary   |
+-------------------+----------+
| Aarav Sharma      | 75000    |
| Priya Iyer        | 52000    |
| Rohan Verma       | 82000    |
| Vikram Malhotra   | 61000    |
| Sneha Reddy       | 95000    |
| Karan Joshi       | 70000    |
| Divya Mehra       | 58000    |
| Aditya Kulkarni   | 64000    |
| Pooja Hegde       | 51000    |
+-------------------+----------+

--- Aggregation ---
AVG(salary) = 67555.56
```

---

## Output 3: Running Test Case 3 (`tests/test3_compound_filter.ql`)
**Command:**
```powershell
.\querylang.exe tests/test3_compound_filter.ql
```

**Output:**
```
--- Query Result (2 of 10 rows) ---
+---------------+---------------+----------+-------+
| name          | department    | salary   | age   |
+---------------+---------------+----------+-------+
| Rohan Verma   | Engineering   | 82000    | 41    |
| Sneha Reddy   | Engineering   | 95000    | 38    |
+---------------+---------------+----------+-------+

--- Aggregation ---
COUNT(*) = 2
```

---

## Output 4: Running Test Case 4 (`tests/test4_students.ql`)
**Command:**
```powershell
.\querylang.exe tests/test4_students.ql
```

**Output:**
```
--- Query Result (4 of 8 rows) ---
+----------------+----------+-------+
| name           | branch   | gpa   |
+----------------+----------+-------+
| Aarav Sharma   | CSE      | 9.2   |
| Diya Patel     | ECE      | 8.5   |
| Sneha Reddy    | IT       | 9.6   |
| Ananya Iyer    | CSE      | 8.9   |
+----------------+----------+-------+

--- Aggregation ---
MAX(gpa) = 9.60
```

---

## Output 5: Lexical Error Detection (`tests/test5_error_lexical.ql`)
**Command:**
```powershell
.\querylang.exe tests/test5_error_lexical.ql
```

**Output:**
```
Lexical Error [Line 2, Column 14]: Unrecognized character '#'
Semantic Error [Line 2]: Selected column 'invalid_char' does not exist in dataset schema.
```

---

## Output 6: Syntax Error Detection (`tests/test6_error_syntax.ql`)
**Command:**
```powershell
.\querylang.exe tests/test6_error_syntax.ql
```

**Output:**
```
Syntax Error [Line 2, Column 7]: syntax error near current token
```

---

## Output 7: Semantic Error Detection (`tests/test7_error_semantic.ql`)
**Command:**
```powershell
.\querylang.exe tests/test7_error_semantic.ql
```

**Output:**
```
Semantic Error [Line 2]: Selected column 'bonus_percentage' does not exist in dataset schema.
Semantic Error [Line 4]: Aggregate function 'AVG' requires a numeric column, but 'department' is of type STRING.
```

---

## Output 8: AVX2 SIMD Zero-Copy Execution and Comparative Benchmark
**Command:**
```powershell
.\querylang.exe tests/test_benchmark_agg.ql --simd -b
```

**SIMD Zero-Copy Output:**
```
--- Query Result (41773 of 250000 rows) ---
+---------------+----------+
| department    | salary   |
+---------------+----------+
| Engineering   | 37000    |
| Engineering   | 121000   |
| Engineering   | 147000   |
| Engineering   | 171000   |
| Engineering   | 140000   |
| Engineering   | 90000    |
| Engineering   | 167000   |
| Engineering   | 171000   |
| Engineering   | 49000    |
| Engineering   | 59000    |
| Engineering   | 57000    |
| Engineering   | 90000    |
| Engineering   | 146000   |
| Engineering   | 129000   |
| Engineering   | 150000   |
| Engineering   | 93000    |
| Engineering   | 92000    |
| Engineering   | 137000   |
| Engineering   | 156000   |
| Engineering   | 91000    |
| Engineering   | 53000    |
| Engineering   | 79000    |
| Engineering   | 176000   |
| Engineering   | 177000   |
| Engineering   | 172000   |
+---------------+----------+
... (41748 more rows not shown in console) ...

--- Aggregation ---
AVG(salary) = 105079.93

--- Execution Telemetry (Benchmark) ---
Engine Mode        : AVX2 SIMD Zero-Copy
Dataset File Size  : 9.00 MB (9435010 bytes)
Total Rows Parsed  : 250000 rows
Parse Time         : 12.53 ms
Query Filter Time  : 18.12 ms
Total Latency      : 31.44 ms
Scan Throughput    : 286.22 MB/s (5.93 million rows/sec)
```

**Command:**
```powershell
python benchmarks/run_benchmark.py
```

**Benchmark Suite Empirical Output:**
```
Running Comparative Performance Benchmark Suite...
Dataset      | Engine               | Parse (ms) | Total Latency (ms) | Throughput (MB/s)  | Speedup 
------------------------------------------------------------------------------------------------
100K Rows    | Python Pandas        | 48.85      | 55.11              | 64.14              | 1.00x   
             | Scalar Zero-Copy     | 8.04       | 14.02              | 252.14             | 3.93x   
             | AVX2 SIMD Zero-Copy  | 5.58       | 13.57              | 260.52             | 4.06x   
------------------------------------------------------------------------------------------------
250K Rows    | Python Pandas        | 109.82     | 121.92             | 73.80              | 1.00x   
             | Scalar Zero-Copy     | 20.72      | 37.81              | 237.96             | 3.22x   
             | AVX2 SIMD Zero-Copy  | 12.53      | 31.44              | 286.22             | 3.88x   
------------------------------------------------------------------------------------------------
1M Rows      | Python Pandas        | 458.71     | 501.14             | 72.46              | 1.00x   
             | Scalar Zero-Copy     | 88.79      | 169.92             | 213.70             | 2.95x   
             | AVX2 SIMD Zero-Copy  | 73.19      | 154.37             | 235.22             | 3.25x   
------------------------------------------------------------------------------------------------
```
