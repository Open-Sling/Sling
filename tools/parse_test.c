#include <stdio.h>
#include <stdlib.h>
#include "../runtime/sling.h"

/* stub for module registration used when linking test program */
void sling_register_module_hello(void) {}

int main(void) {
    const char *path = "../examples/array.sl";
    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *src = (char*)malloc(len+1);
    fread(src,1,len,f);
    src[len] = '\0';
    fclose(f);

    for (int t = 0; t < i; ++t) { free(arr[t]); arr[t] = NULL; }
    i = 0; current = 0; line = 1;

    lex(src);
    ASTNode *root = parse_statements();
    if (!root) { printf("parse returned NULL\n"); return 1; }
    printf("parse OK, statements=%d\n", root->statements.count);
    free_ast(root);
    free(src);
    return 0;
}
