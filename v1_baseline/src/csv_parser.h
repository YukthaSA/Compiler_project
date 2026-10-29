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
