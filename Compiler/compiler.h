#ifndef COMPILER_H
#define COMPILER_H

#include "../Parser/parser.h"
#include <setjmp.h>

extern jmp_buf return_buf;
extern double return_value;

void interpret(ASTNode *node);

#endif // COMPILER_H
