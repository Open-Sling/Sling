#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sling.h"
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_code>\n", argv[0]);
        return 1;
    }
    const char *src = argv[1];
    if (!src || !*src) { error(0, "No source code provided"); return 1; }
    if (strcmp(src, "-v") == 0) {
        printf("Sling Version 3.0.0\n");
        return 0;
    }

    /* reset global token stream */
    for (int t = 0; t < i; ++t) { free(arr[t]); arr[t] = NULL; }
    i = 0; current = 0; line = 1;

    lex(src);
    if (i == 0) { error(0, "No tokens generated"); return 1; }

    ASTNode *root = parse_statements();
    if (!root) { error(0, "Failed to parse source"); return 1; }

    interpret(root);
    free_ast(root);
    success(0, "Code ran successfully");
    return 0;
}