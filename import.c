#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "import.h"
#include "Parser/parser.h"
#include "Lexer/lexer.h"
#include "errors/error.h"

extern int i;
extern int current;
void lex(const char *src);
ASTNode *parse_statements();
void free_ast(ASTNode *node);
void interpret(ASTNode *node);

void handle_import(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        error(0, "Could not import file: %s\n", filename);
        return;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *code = malloc(length + 1);
    fread(code, 1, length, file);
    code[length] = '\0';
    fclose(file);
    int old_i = i;
    int old_current = current;
    lex(code);
    free(code);
    int import_start = old_i;
    int import_end = i;
    current = import_start;
    ASTNode *root = parse_statements();
    // Only register function definitions from the imported AST
    if (root && root->type == AST_STATEMENTS) {
        for (int j = 0; j < root->statements.count; ++j) {
            ASTNode *stmt = root->statements.stmts[j];
            if (stmt && stmt->type == AST_FUNC_DEF) {
                extern void add_func(const char *, ASTNode *);
                add_func(stmt->funcdef.funcname, stmt);
                root->statements.stmts[j] = NULL;
            }
        }
    }
    free_ast(root);
    current = old_current;
    for (int j = import_start; j < import_end; ++j) {
        free(arr[j]);
        arr[j] = NULL;
    }
    i = old_i;
} 
