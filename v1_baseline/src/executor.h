#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"
#include "csv_parser.h"

int execute_query(ASTNode *program);

#endif /* EXECUTOR_H */
