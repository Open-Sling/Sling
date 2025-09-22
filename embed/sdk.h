#ifndef SDK_H
#define SDK_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>   // tolower, isspace

/* ============================================================
   Value forward declaration (from sling.h)
   ============================================================ */
struct Value;
typedef struct Value Value;

/* ============================================================
   Native function pointer type
   ============================================================ */
typedef Value (*SlingCFunc)(int argc, Value *argv);

typedef struct {
    char *name;       // normalized name
    SlingCFunc func;
} SlingCFuncEntry;

/* ============================================================
   Globals (ONE copy across all .c files)
   ============================================================ */
#define MAX_NATIVE_FUNCS 256
#define MAX_MODULES      64

// Declare as extern by default
extern SlingCFuncEntry sling_native_funcs[MAX_NATIVE_FUNCS];
extern int sling_native_func_count;

typedef void (*SlingModuleInit)(void);
extern SlingModuleInit sling_modules[MAX_MODULES];
extern int sling_module_count;

/* ============================================================
   Definitions (only once if SDK_IMPLEMENTATION defined)
   ============================================================ */
#ifdef SDK_IMPLEMENTATION
SlingCFuncEntry sling_native_funcs[MAX_NATIVE_FUNCS];
int sling_native_func_count = 0;

SlingModuleInit sling_modules[MAX_MODULES];
int sling_module_count = 0;
#endif

/* ============================================================
   Helper: normalize names
   ============================================================ */
static inline char *sling_normalize_name(const char *raw) {
    while (isspace((unsigned char)*raw)) raw++;
    size_t len = strlen(raw);
    while (len > 0 && isspace((unsigned char)raw[len-1])) len--;

    char *clean = (char*)malloc(len + 1);
    for (size_t j = 0; j < len; j++)
        clean[j] = (char)tolower((unsigned char)raw[j]);
    clean[len] = '\0';
    return clean;
}

/* ============================================================
   Register / Lookup natives
   ============================================================ */
static inline void sling_register_native(const char *name, SlingCFunc func) {
    if (sling_native_func_count < MAX_NATIVE_FUNCS) {
        char *clean = sling_normalize_name(name);
        sling_native_funcs[sling_native_func_count].name = clean;
        sling_native_funcs[sling_native_func_count].func = func;
        sling_native_func_count++;
    }
}

static inline SlingCFunc sling_get_native(const char *name) {
    char *clean = sling_normalize_name(name);

    for (int i = 0; i < sling_native_func_count; i++) {


        if (strcmp(sling_native_funcs[i].name, clean) == 0) {
            SlingCFunc fn = sling_native_funcs[i].func;
            free(clean);
            return fn;
        }
    }

    free(clean);
    return NULL;
}

/* ============================================================
   Module registration
   ============================================================ */
static inline void sling_register_module(SlingModuleInit init) {
    if (sling_module_count < MAX_MODULES) {
        sling_modules[sling_module_count++] = init;
    }
}

static inline void sling_init_all_modules(void) {
    for (int i = 0; i < sling_module_count; i++) {
        sling_modules[i]();
    }
}

#endif /* SDK_H */
