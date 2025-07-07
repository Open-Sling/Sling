#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
void enable_ansi_support(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}
#else
void enable_ansi_support(void) {}
#endif

static void print_colored(const char *prefix, const char *color, int line, const char *fmt, va_list args) {
    if (line > 0)
        fprintf(stderr, "%s%s\033[0m (line %d) ", color, prefix, line);
    else
        fprintf(stderr, "%s%s\033[0m ", color, prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void error(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_colored("[ERROR]", "\033[1;31m", line, fmt, args);
    va_end(args);
    fflush(stdout);
    fflush(stderr);
    exit(1);
}

void warn(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_colored("[WARN]", "\033[1;33m", line, fmt, args);
    va_end(args);
}

void info(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_colored("[INFO]", "\033[1;34m", line, fmt, args);
    va_end(args);
}

void success(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_colored("[SUCESS]", "\033[1;32m", line, fmt, args);
    va_end(args);
}
