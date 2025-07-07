#ifndef ERROR_H
#define ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

void enable_ansi_support(void);

void error(int line, const char *fmt, ...);
void warn(int line, const char *fmt, ...);
void info(int line, const char *fmt, ...);
void success(int line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // ERROR_H
