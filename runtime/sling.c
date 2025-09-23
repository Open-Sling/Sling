#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "sling.h"

static char *read_file_or_null(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_code|file.sling>\n", argv[0]);
        return 1;
    }

    const char *arg = argv[1];
    if (arg && strcmp(arg, "-v") == 0) {
        printf("Sling Version 3.0.0\n");
        return 0;
    }

    char *owned_src = NULL;
    const char *src = NULL;
    if (arg && access(arg, R_OK) == 0) {
        owned_src = read_file_or_null(arg);
        if (!owned_src) { error(0, "Could not read file: %s", arg); return 1; }
        src = owned_src;
    } else {
        src = arg;
    }
    if (!src || !*src) { if (owned_src) free(owned_src); error(0, "No source code provided"); return 1; }

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
    if (owned_src) free(owned_src);
    return 0;
}