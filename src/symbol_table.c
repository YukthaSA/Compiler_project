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
    // Check if already exists
    for (int i = 0; i < g_symtab.count; i++) {
        if (strcmp(g_symtab.symbols[i].name, name) == 0) {
            return i; // already present
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
        return -1; // File not found or inaccessible
    }

    init_symbol_table();
    strncpy(g_symtab.dataset_file, filepath, sizeof(g_symtab.dataset_file) - 1);
    if (alias) {
        strncpy(g_symtab.dataset_alias, alias, sizeof(g_symtab.dataset_alias) - 1);
    }

    char header_buf[2048];
    if (!fgets(header_buf, sizeof(header_buf), fp)) {
        fclose(fp);
        return -2; // Empty file
    }

    char sample_buf[2048];
    int has_sample = 0;
    if (fgets(sample_buf, sizeof(sample_buf), fp)) {
        has_sample = 1;
    }
    fclose(fp);

    // Parse header tokens
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
    printf("\n--- Symbol Table (%s) ---\n", g_symtab.dataset_file[0] ? g_symtab.dataset_file : "schema");
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
