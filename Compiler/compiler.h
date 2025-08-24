#ifndef COMPILER_H
#define COMPILER_H

#include "../Parser/parser.h"
#include <setjmp.h>

// Extend ValueType to support objects for class instances
// VAL_OBJECT: represents an instance of a class. When a class is instantiated
// the returned value will have type VAL_OBJECT and its `object` pointer set.
typedef enum { VAL_NUMBER, VAL_STRING, VAL_OBJECT } ValueType;
// A Value can now hold either a numeric value, a string or an object pointer.
// When type == VAL_NUMBER only the `number` field is valid.
// When type == VAL_STRING only the `string` field is valid. The caller is
// responsible for freeing the returned string.
// When type == VAL_OBJECT only the `object` field is valid and points to
// a ClassInstance structure defined in compiler.c.
typedef struct {
    ValueType type;
    double number;
    char *string;
    void *object;
} Value;

extern jmp_buf return_buf;
extern Value return_value;

Value eval(ASTNode *node);
void interpret(ASTNode *node);
char *eval_to_string(ASTNode *node);
char *get_input_result();

#endif
