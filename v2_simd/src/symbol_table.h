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

/* Global Symbol Table instance */
extern SymbolTable g_symtab;

void init_symbol_table(void);
int insert_symbol(const char *name, DataType type, int col_index, const char *alias);
Symbol* lookup_symbol(const char *name);
int load_csv_schema_into_symtab(const char *filepath, const char *alias);
void print_symbol_table(void);

#endif /* SYMBOL_TABLE_H */
