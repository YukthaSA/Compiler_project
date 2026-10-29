#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "csv_parser.h"
#include "simd_csv.h"

int execute_query(ASTNode *program);
int execute_query_ex(ASTNode *program, int use_simd, int print_bench);

#endif /* EXECUTOR_H */
