#ifndef TAC_H
#define TAC_H

#include "ast.h"

#define MAX_QUADS 128

typedef struct {
    char op[32];
    char arg1[256];
    char arg2[256];
    char result[128];
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
void print_tac_textbook(void);
void print_quadruples(void);

#endif /* TAC_H */
