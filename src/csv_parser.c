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
        // Skip leading space
        while (isspace((unsigned char)*p) && *p != '\n' && *p != '\r') p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') break;

        char field[1024];
        int fidx = 0;

        if (*p == '"') {
            p++; // skip leading quote
            while (*p && *p != '"') {
                field[fidx++] = *p++;
            }
            if (*p == '"') p++; // skip trailing quote
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
    // Read header line
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
            continue; // skip blank lines
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
