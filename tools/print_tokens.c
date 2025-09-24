#include <stdio.h>
#include <stdlib.h>
#include "../runtime/sling.h"

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

    /* reset globals used by lex */
    for (int t = 0; t < i; ++t) { free(arr[t]); arr[t] = NULL; }
    i = 0; current = 0; line = 1;

    lex(src);
    printf("Tokens (i=%d):\n", i);
    for (int k = 0; k < i; ++k) {
        printf("%3d: %s\n", k, arr[k]);
    }
    free(src);
    return 0;
}
