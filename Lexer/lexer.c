#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "../errors/error.h"

char *arr[99999];
int i = 0;
int line = 1;

void add_token(const char *token) {
    arr[i++] = strdup(token);
}

void lex(const char *src) {
    const char *p = src;
    static char last_token[32] = "";

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace(*p)) { p++; continue; }

        // Handle comments
        if (*p == '/' && *(p+1) == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        // Compound operators
        if (*p == '=' && *(p + 1) == '=') {
            add_token("EQ");
            strcpy(last_token, "EQ");
            p += 2;
            continue;
        }

        // Single-character tokens
        if (*p == ';') { add_token("SEMICOLON"); strcpy(last_token, "SEMICOLON"); p++; continue; }
        if (*p == '=') { add_token("ASSIGN"); strcpy(last_token, "ASSIGN"); p++; continue; }
        if (*p == '(') { add_token("LPAREN"); strcpy(last_token, "LPAREN"); p++; continue; }
        if (*p == ')') { add_token("RPAREN"); strcpy(last_token, "RPAREN"); p++; continue; }
        if (*p == '{') { add_token("LBRACE"); strcpy(last_token, "LBRACE"); p++; continue; }
        if (*p == '}') { add_token("RBRACE"); strcpy(last_token, "RBRACE"); p++; continue; }
        if (*p == '+') { add_token("PLUS"); strcpy(last_token, "PLUS"); p++; continue; }
        if (*p == '-') { add_token("MINUS"); strcpy(last_token, "MINUS"); p++; continue; }
        if (*p == '*') { add_token("MULTIPLY"); strcpy(last_token, "MULTIPLY"); p++; continue; }
        if (*p == '/') { add_token("DIVIDE"); strcpy(last_token, "DIVIDE"); p++; continue; }
        if (*p == '<') { add_token("LT"); strcpy(last_token, "LT"); p++; continue; }
        if (*p == '>') { add_token("GT"); strcpy(last_token, "GT"); p++; continue; }
        if (*p == ',') { add_token("COMMA"); strcpy(last_token, "COMMA"); p++; continue; }
        if (*p == '.') { add_token("DOT"); strcpy(last_token, "DOT"); p++; continue; }

        // Strings
        if (*p == '"') {
            const char *start = ++p;
            while (*p && *p != '"') p++;
            if (*p != '"') error(line, "Unterminated string literal at line %d", line);
            size_t len = p - start;
            char *buf = malloc(len + 1);
            strncpy(buf, start, len);
            buf[len] = '\0';
            char *tok = malloc(len + 20);
            if (strcmp(last_token, "IMPORT") == 0) {
                sprintf(tok, "IMSTRING \"%s\"", buf);
                add_token(tok);
                strcpy(last_token, "IMSTRING");
            } else {
                sprintf(tok, "STRING \"%s\"", buf);
                add_token(tok);
                strcpy(last_token, "STRING");
            }
            free(buf);
            free(tok);
            p++; // Skip closing quote
            continue;
        }

        // Numbers
        if (isdigit(*p)) {
            const char *start = p;
            while (isdigit(*p)) p++;
            if (*p == '.') {
                p++;
                while (isdigit(*p)) p++;
            }
            size_t len = p - start;
            char *buf = malloc(len + 1);
            strncpy(buf, start, len);
            buf[len] = '\0';
            char *tok = malloc(len + 10);
            sprintf(tok, "NUMBER %s", buf);
            add_token(tok);
            free(buf);
            free(tok);
            continue;
        }

        // Identifiers / Keywords
        if (isalpha(*p) || *p == '_') {
            const char *start = p;
            while (isalnum(*p) || *p == '_') p++;
            size_t len = p - start;
            char *buf = malloc(len + 1);
            strncpy(buf, start, len);
            buf[len] = '\0';

            if (strcmp(buf, "let") == 0) { add_token("LET"); strcpy(last_token, "LET"); }
            else if (strcmp(buf, "print") == 0) { add_token("PRINT"); strcpy(last_token, "PRINT"); }
            else if (strcmp(buf, "raise") == 0) { add_token("RAISE"); strcpy(last_token, "RAISE"); }
            else if (strcmp(buf, "warn") == 0) { add_token("WARN"); strcpy(last_token, "WARN"); }
            else if (strcmp(buf, "info") == 0) { add_token("INFO"); strcpy(last_token, "RETURN"); }
            else if (strcmp(buf, "while") == 0) { add_token("WHILE"); strcpy(last_token, "WHILE"); }
            else if (strcmp(buf, "if") == 0) { add_token("IF"); strcpy(last_token, "IF"); }
            else if (strcmp(buf, "else") == 0) { add_token("ELSE"); strcpy(last_token, "ELSE"); }
            else if (strcmp(buf, "true") == 0) { add_token("TRUE"); strcpy(last_token, "TRUE"); }
            else if (strcmp(buf, "false") == 0) { add_token("FALSE"); strcpy(last_token, "FALSE"); }
            else if (strcmp(buf, "and") == 0) { add_token("AND"); strcpy(last_token, "AND"); }
            else if (strcmp(buf, "or") == 0) { add_token("OR"); strcpy(last_token, "OR"); }
            else if (strcmp(buf, "not") == 0) { add_token("NOT"); strcpy(last_token, "NOT"); }
            else if (strcmp(buf, "func") == 0) { add_token("FUNC"); strcpy(last_token, "FUNC"); }
            else if (strcmp(buf, "return") == 0) { add_token("RETURN"); strcpy(last_token, "RETURN"); }
            else if (strcmp(buf, "import") == 0) { add_token("IMPORT"); strcpy(last_token, "IMPORT"); }
            else if (strcmp(buf, "for") == 0) { add_token("FOR"); strcpy(last_token, "FOR"); }
            else if (strcmp(buf, "in") == 0) { add_token("IN"); strcpy(last_token, "IN"); }
            // comments
            else if (strcmp(buf, "//") == 0) {
                while (*p && *p != '\n') p++;
                continue; // Skip the rest of the line
            }
            else {
                char *tok = malloc(len + 20);
                sprintf(tok, "IDENTIFIER %s", buf);
                add_token(tok);
                strcpy(last_token, "IDENTIFIER");
                free(tok);
            }
            free(buf);
            continue;
        }

        // Unknown characters
        error(line, "Unknown character '%c' at line %d", *p, line);
        p++;
    }
}
