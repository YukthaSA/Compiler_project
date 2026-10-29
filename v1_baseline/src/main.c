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
    printf("Module-by-Module Verification Options:\n");
    printf("  --lex, -t       : Run Module 1 - Lexical Analysis (Print token stream table)\n");
    printf("  --syntax        : Run Module 2 - Syntactic Analysis (Bison LALR(1) parser validation)\n");
    printf("  --ast, -a       : Run Module 3 - Abstract Syntax Tree (AST node graph)\n");
    printf("  --symtab, -s    : Run Module 4 - Symbol Table (CSV dynamic schema inference)\n");
    printf("  --semantic      : Run Module 5 - Semantic Analysis (Type safety & scope verification)\n");
    printf("  --tac, --codegen: Run Module 6 - Intermediate Code Generation (TAC & Quadruples)\n");
    printf("  --exec          : Run Module 7 - Execution Engine (Tabular query result)\n");
    printf("  --all           : Full Pipeline - Print AST, Symbol Table, TAC, and Execution\n");
    printf("  -h, --help      : Show this help message\n");
}

static void dump_tokens(FILE *fp) {
    yyin = fp;
    printf("\n--- Module 1: Lexical Analysis (Token Stream) ---\n");
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
    int mode_lex = 0;
    int mode_syntax = 0;
    int mode_ast = 0;
    int mode_symtab = 0;
    int mode_semantic = 0;
    int mode_tac = 0;
    int mode_exec = 0;
    int mode_all = 0;
    const char *filepath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lex") == 0 || strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tokens") == 0) {
            mode_lex = 1;
        } else if (strcmp(argv[i], "--syntax") == 0 || strcmp(argv[i], "--parse") == 0) {
            mode_syntax = 1;
        } else if (strcmp(argv[i], "--ast") == 0 || strcmp(argv[i], "-a") == 0) {
            mode_ast = 1;
        } else if (strcmp(argv[i], "--symtab") == 0 || strcmp(argv[i], "-s") == 0) {
            mode_symtab = 1;
        } else if (strcmp(argv[i], "--semantic") == 0 || strcmp(argv[i], "--check") == 0) {
            mode_semantic = 1;
        } else if (strcmp(argv[i], "--tac") == 0 || strcmp(argv[i], "--codegen") == 0 || strcmp(argv[i], "-i") == 0) {
            mode_tac = 1;
        } else if (strcmp(argv[i], "--exec") == 0) {
            mode_exec = 1;
        } else if (strcmp(argv[i], "--all") == 0) {
            mode_all = 1;
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

    // Module 1: Lexical Analysis
    if (mode_lex) {
        dump_tokens(input_file);
        if (input_file != stdin) fclose(input_file);
        return 0;
    }

    yyin = input_file;

    // Module 2: Syntactic Analysis (Parser)
    int parse_rc = yyparse();
    if (parse_rc != 0) {
        fprintf(stderr, "Syntax Analysis: FAILED with syntax errors.\n");
        if (input_file != stdin) fclose(input_file);
        return 2;
    }

    if (mode_syntax) {
        printf("\n--- Module 2: Syntactic Analysis (Bison LALR(1) Parser) ---\n");
        printf("Status     : PASSED (0 syntax errors)\n");
        printf("Grammar    : ProgramNode -> LoadStmt SelectStmt [FilterStmt] [AggregateStmt]\n");
        printf("Validation : Query statements successfully conform to Context-Free Grammar rules.\n\n");
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return 0;
    }

    // Module 3: Abstract Syntax Tree Construction
    if (mode_ast) {
        printf("\n--- Module 3: Abstract Syntax Tree (AST Node Graph) ---\n");
        print_ast(g_ast_root, 0);
        printf("\n");
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return 0;
    }

    // Module 4: Dynamic Symbol Table Schema Ingestion
    // We populate the symbol table from the LOAD statement in AST
    int sem_errors = perform_semantic_analysis(g_ast_root);
    if (mode_symtab) {
        printf("\n--- Module 4: Dynamic Symbol Table (Dataset Schema Ingestion) ---\n");
        print_symbol_table();
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return (sem_errors == 0) ? 0 : 3;
    }

    // Module 5: Semantic Analysis & Type Safety
    if (mode_semantic) {
        printf("\n--- Module 5: Semantic Analysis & Type Safety Verification ---\n");
        if (sem_errors == 0) {
            printf("Dataset Schema Access : PASSED (Source file exists and is readable)\n");
            printf("Identifier Resolution : PASSED (All referenced columns exist in schema)\n");
            printf("Type Compatibility    : PASSED (Filter conditions and aggregations are type-safe)\n");
            printf("Semantic Status       : PASSED (0 semantic errors detected)\n\n");
        } else {
            printf("Semantic Status       : FAILED (%d semantic errors detected)\n\n", sem_errors);
        }
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return (sem_errors == 0) ? 0 : 3;
    }

    if (sem_errors > 0) {
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return 3;
    }

    // Module 6: Intermediate Code Generation (TAC & Quadruples)
    generate_tac(g_ast_root);

    if (mode_tac) {
        printf("\n--- Module 6: Intermediate Code Generation (TAC & Quadruples) ---\n");
        print_tac();
        print_tac_textbook();
        print_quadruples();
        free_ast(g_ast_root);
        if (input_file != stdin) fclose(input_file);
        return 0;
    }

    // Full Pipeline Mode (--all)
    if (mode_all) {
        printf("\n--- Abstract Syntax Tree (AST) ---\n");
        print_ast(g_ast_root, 0);
        print_symbol_table();
        print_tac();
        print_tac_textbook();
        print_quadruples();
    }

    (void)mode_exec;

    // Module 7: Query Execution Engine (Scalar Baseline)
    int exec_rc = execute_query(g_ast_root);

    free_ast(g_ast_root);
    if (input_file != stdin) fclose(input_file);

    return exec_rc;
}
