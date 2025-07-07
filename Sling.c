// === Sling.c ===
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "errors/error.h"
#include "Lexer/lexer.h"
#include "Parser/parser.h"

extern char *arr[99999];
extern int i;

int main(int argc, char *argv[]) {
    if (argc != 2) { error(0, "Usage: Sling <filename>"); }

    if (strcmp(argv[1], "--credits") == 0) {
        printf("\u00a9 2025 Sinha Ltd. All rights reserved.\n");
        printf("\u00a9 2025 Reyaansh Sinha. All rights reserved.\n");
        printf("\u00a9 2025 Sling Programming Language. All rights reserved.\n");
        return 0;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) { error(0, "Sling/File Error: Error opening file"); }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *code = malloc(length + 1);
    fread(code, 1, length, file);
    code[length] = '\0';
    fclose(file);

    lex(code);
    free(code);

    Parse();
    success(0, "Code ran successfully");
    return 0;
}
