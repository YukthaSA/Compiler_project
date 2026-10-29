#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"

/* Returns 0 on success, or non-zero error count on semantic failure */
int perform_semantic_analysis(ASTNode *program);

#endif /* SEMANTIC_H */
