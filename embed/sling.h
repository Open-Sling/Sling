#ifndef SLING_H
#define SLING_H
/* Header-only Sling: lexer + parser + interpreter + runtime
 * All globals are static; all functions are static inline.
 * No external .c files are required. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#include <dlfcn.h>


typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_OBJECT
} ValueType;

typedef struct Value {
    ValueType type;
    double number;
    char *string;
    void *object;
} Value;

/* Generic object types used by the runtime (arrays etc) */
typedef enum { OBJ_ARRAY } ObjectType;
typedef struct {
    ObjectType type;
    int count;
    int capacity;
    Value *items;
} ObjArray;


#include "sdk.h"   // now sdk.h can use Value


/* ============================================================
   Globals / Tokens buffer
   ============================================================ */
static char *arr[99999];
static int   i = 0;
static int   line = 1;
static int   current = 0;

/* ============================================================
   Diagnostics (runtime logging)
   ============================================================ */
static inline void enable_ansi_support(void) { /* no-op for embedded */ }

static inline void error(int ln, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERROR] line %d: ", ln);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    /* For embedded, you might prefer abort(); or a longjmp to a toplevel. */
    exit(1);
}
static inline void warn(int ln, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[WARN]  line %d: ", ln);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
static inline void info(int ln, const char *fmt, ...) {
    (void)ln;
    va_list ap; va_start(ap, fmt);
    fprintf(stdout, "[INFO] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}
static inline void success(int ln, const char *fmt, ...) {
    (void)ln;
    va_list ap; va_start(ap, fmt);
    fprintf(stdout, "[SUCCESS]   ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

/* ============================================================
   AST types
   ============================================================ */
typedef enum {
    AST_NUMBER,
    AST_STRING,
    AST_IDENTIFIER,
    AST_BINARY_OP,
    AST_LET,
    AST_ASSIGN,
    AST_PRINT,
    AST_RAISE,
    AST_WARN,
    AST_INFO,
    AST_WHILE,
    AST_IF,
    AST_STATEMENTS,
    AST_FUNC_DEF,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_IMPORT,
    AST_FOR
    ,AST_ARRAY_LITERAL
    ,AST_INDEX_EXPR
    ,AST_INDEX_ASSIGN
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        double number;
        char *string;

        struct {
            char op[4];
            struct ASTNode *left;
            struct ASTNode *right; /* may be NULL for unary NOT */
        } binop;

        struct {
            char *varname;
            struct ASTNode *value;
        } var;

        struct { struct ASTNode *expr; } print;
        struct { struct ASTNode *expr; } raise;
        struct { struct ASTNode *expr; } warn;
        struct { struct ASTNode *expr; } info;

        struct { struct ASTNode *cond; struct ASTNode *body; } whilestmt;

        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } ifstmt;

        struct {
            struct ASTNode **stmts;
            int count;
        } statements;

        struct {
            char *funcname;
            char **params;
            int param_count;
            struct ASTNode *body;
        } funcdef;

        struct {
            char *funcname;
            struct ASTNode **args;
            int arg_count;
        } funccall;

        struct { struct ASTNode *expr; } retstmt;

        struct { char *filename; } importstmt;

        struct {
            char *for_var;
            struct ASTNode *for_start;
            struct ASTNode *for_end;
            struct ASTNode *for_body;
        } forstmt;

        /* array literal: [expr, expr, ...] */
        struct {
            struct ASTNode **elements;
            int count;
        } arraylit;

        /* index expression: target[index] */
        struct {
            struct ASTNode *target;
            struct ASTNode *index;
        } indexexpr;

        /* index assignment: target[index] = value */
        struct {
            struct ASTNode *target; /* typically an identifier */
            struct ASTNode *index;
            struct ASTNode *value;
        } indexassign;
    };
} ASTNode;

/* ============================================================
   Values, returns, and environment
   ============================================================ */


static jmp_buf return_buf;
static Value   return_value;

/* Simple variable table */
typedef enum { VAR_NUMBER, VAR_STRING, VAR_OBJECT } VarType;
typedef struct {
    char  *name;
    VarType type;
    double value;
    char  *str;
    void  *obj; /* for VAR_OBJECT */
} Var;

static Var vars[1024];
static int var_count = 0;

static inline Var *get_var(const char *name) {
    for (int j = 0; j < var_count; ++j)
        if (strcmp(vars[j].name, name) == 0) return &vars[j];
    return NULL;
}
static inline void free_object(void *obj) {
    if (!obj) return;
    ObjArray *oa = (ObjArray*)obj;
    if (oa->type == OBJ_ARRAY) {
        for (int j = 0; j < oa->count; ++j) {
            if (oa->items[j].type == VAL_STRING) free(oa->items[j].string);
        }
        free(oa->items);
    }
    free(oa);
}

static inline void set_var_object(const char *name, void *obj) {
    for (int j = 0; j < var_count; ++j) {
        if (strcmp(vars[j].name, name) == 0) {
            if (vars[j].type == VAR_OBJECT) free_object(vars[j].obj);
            vars[j].type = VAR_OBJECT;
            vars[j].obj = obj;
            free(vars[j].str); vars[j].str = NULL;
            vars[j].value = 0;
            return;
        }
    }
    vars[var_count].name = strdup(name);
    vars[var_count].type = VAR_OBJECT;
    vars[var_count].obj  = obj;
    vars[var_count].value = 0;
    vars[var_count].str = NULL;
    var_count++;
}
static inline void set_var(const char *name, VarType type, double value, const char *str) {
    for (int j = 0; j < var_count; ++j) {
        if (strcmp(vars[j].name, name) == 0) {
            vars[j].type = type;
            if (type == VAR_STRING) {
                free(vars[j].str);
                vars[j].str = strdup(str ? str : "");
            } else {
                vars[j].value = value;
                free(vars[j].str);
                vars[j].str = NULL;
            }
            return;
        }
    }
    vars[var_count].name = strdup(name);
    vars[var_count].type = type;
    vars[var_count].value = (type == VAR_NUMBER) ? value : 0;
    vars[var_count].str   = (type == VAR_STRING) ? strdup(str ? str : "") : NULL;
    var_count++;
}

/* Simple function table */
#define MAX_FUNCS 16384
typedef struct {
    char    *name;
    ASTNode *def;   /* points to AST_FUNC_DEF node */
} Func;

static Func funcs[MAX_FUNCS];
static int  func_count = 0;

static inline void add_func(const char *name, ASTNode *def) {
    for (int j = 0; j < func_count; ++j)
        if (strcmp(funcs[j].name, name) == 0) { funcs[j].def = def; return; }
    funcs[func_count].name = strdup(name);
    funcs[func_count].def  = def;
    func_count++;
}
static inline ASTNode *get_func(const char *name) {
    for (int j = 0; j < func_count; ++j)
        if (strcmp(funcs[j].name, name) == 0) return funcs[j].def;
    return NULL;
}

/* ============================================================
   Forward declarations (all static inline)
   ============================================================ */
static inline void     add_token(const char *token);
static inline void     lex(const char *src);
static inline int      match(const char *token);
static inline int      is_statement_start(const char *token);
static inline ASTNode *parse_expression(void);
static inline ASTNode *parse_logical(void);
static inline ASTNode *parse_comparison(void);
static inline ASTNode *parse_term(void);
static inline ASTNode *parse_factor(void);
static inline ASTNode *parse_statement(void);
static inline ASTNode *parse_statements(void);
static inline void     free_ast(ASTNode *node);
static inline void     Parse(void);
static inline Value    eval(ASTNode *node);
static inline char    *eval_to_string(ASTNode *node);
static inline void     interpret(ASTNode *node);
static inline void     handle_import(const char *filename);
static inline void     run_script(const char *src);

/* ============================================================
   Lexer
   ============================================================ */
static inline void add_token(const char *token) {
    arr[i++] = strdup(token);
}

static inline void lex(const char *src) {
    const char *p = src;
    static char last_token[32] = "";

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* line comment //... */
        if (*p == '/' && *(p+1) == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* compound operators */
        if (*p == '=' && *(p+1) == '=') { add_token("EQ");  strcpy(last_token,"EQ");  p+=2; continue; }
        if (*p == '!' && *(p+1) == '=') { add_token("NEQ"); strcpy(last_token,"NEQ"); p+=2; continue; }
        if (*p == '<' && *(p+1) == '=') { add_token("LE");  strcpy(last_token,"LE");  p+=2; continue; }
        if (*p == '>' && *(p+1) == '=') { add_token("GE");  strcpy(last_token,"GE");  p+=2; continue; }

        /* single char tokens */
        if (*p == ';') { add_token("SEMICOLON"); strcpy(last_token,"SEMICOLON"); p++; continue; }
        if (*p == '=') { add_token("ASSIGN");    strcpy(last_token,"ASSIGN");    p++; continue; }
        if (*p == '(') { add_token("LPAREN");    strcpy(last_token,"LPAREN");    p++; continue; }
        if (*p == ')') { add_token("RPAREN");    strcpy(last_token,"RPAREN");    p++; continue; }
        if (*p == '{') { add_token("LBRACE");    strcpy(last_token,"LBRACE");    p++; continue; }
        if (*p == '}') { add_token("RBRACE");    strcpy(last_token,"RBRACE");    p++; continue; }
        if (*p == '+') { add_token("PLUS");      strcpy(last_token,"PLUS");      p++; continue; }
        if (*p == '-') { add_token("MINUS");     strcpy(last_token,"MINUS");     p++; continue; }
        if (*p == '*') { add_token("MULTIPLY");  strcpy(last_token,"MULTIPLY");  p++; continue; }
        if (*p == '/') { add_token("DIVIDE");    strcpy(last_token,"DIVIDE");    p++; continue; }
        if (*p == '<') { add_token("LT");        strcpy(last_token,"LT");        p++; continue; }
        if (*p == '>') { add_token("GT");        strcpy(last_token,"GT");        p++; continue; }
        if (*p == ',') { add_token("COMMA");     strcpy(last_token,"COMMA");     p++; continue; }
        if (*p == '[') { add_token("LBRACKET"); strcpy(last_token,"LBRACKET"); p++; continue; }
        if (*p == ']') { add_token("RBRACKET"); strcpy(last_token,"RBRACKET"); p++; continue; }
        if (*p == '.') { add_token("DOT");       strcpy(last_token,"DOT");       p++; continue; }

        /* string literal */
        if (*p == '"') {
            const char *start = ++p;
            while (*p && *p != '"') p++;
            if (*p != '"') error(line, "Unterminated string literal");
            size_t len = (size_t)(p - start);
            char *buf = (char*)malloc(len + 1);
            memcpy(buf, start, len);
            buf[len] = '\0';

            char *tok = (char*)malloc(len + 32);
            if (strcmp(last_token, "IMPORT") == 0) {
                snprintf(tok, len + 32, "IMSTRING \"%s\"", buf);
                add_token(tok); strcpy(last_token, "IMSTRING");
            } else {
                snprintf(tok, len + 32, "STRING \"%s\"", buf);
                add_token(tok); strcpy(last_token, "STRING");
            }
            free(buf);
            free(tok);
            p++; /* consume closing " */
            continue;
        }

        /* number literal */
        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            while (isdigit((unsigned char)*p)) p++;
            if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
            size_t len = (size_t)(p - start);
            char *buf = (char*)malloc(len + 1);
            memcpy(buf, start, len); buf[len] = '\0';
            char *tok = (char*)malloc(len + 16);
            snprintf(tok, len + 16, "NUMBER %s", buf);
            add_token(tok);
            free(buf); free(tok);
            continue;
        }

        /* identifier/keyword */
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t len = (size_t)(p - start);
            char *buf = (char*)malloc(len + 1);
            memcpy(buf, start, len); buf[len] = '\0';

            if (strcmp(buf, "let") == 0)      { add_token("LET");    strcpy(last_token,"LET"); }
            else if (strcmp(buf, "print") == 0){ add_token("PRINT"); strcpy(last_token,"PRINT"); }
            else if (strcmp(buf, "raise") == 0){ add_token("RAISE"); strcpy(last_token,"RAISE"); }
            else if (strcmp(buf, "warn") == 0) { add_token("WARN");  strcpy(last_token,"WARN"); }
            else if (strcmp(buf, "info") == 0) { add_token("INFO");  strcpy(last_token,"INFO"); } /* fixed */
            else if (strcmp(buf, "while") == 0){ add_token("WHILE"); strcpy(last_token,"WHILE"); }
            else if (strcmp(buf, "if") == 0)   { add_token("IF");    strcpy(last_token,"IF"); }
            else if (strcmp(buf, "else") == 0) { add_token("ELSE");  strcpy(last_token,"ELSE"); }
            else if (strcmp(buf, "true") == 0) { add_token("TRUE");  strcpy(last_token,"TRUE"); }
            else if (strcmp(buf, "false") == 0){ add_token("FALSE"); strcpy(last_token,"FALSE"); }
            else if (strcmp(buf, "and") == 0)  { add_token("AND");   strcpy(last_token,"AND"); }
            else if (strcmp(buf, "or") == 0)   { add_token("OR");    strcpy(last_token,"OR"); }
            else if (strcmp(buf, "not") == 0)  { add_token("NOT");   strcpy(last_token,"NOT"); }
            else if (strcmp(buf, "func") == 0) { add_token("FUNC");  strcpy(last_token,"FUNC"); }
            else if (strcmp(buf, "return") == 0){ add_token("RETURN"); strcpy(last_token,"RETURN"); }
            else if (strcmp(buf, "import") == 0){ add_token("IMPORT"); strcpy(last_token,"IMPORT"); }
            else if (strcmp(buf, "for") == 0)   { add_token("FOR");    strcpy(last_token,"FOR"); }
            else if (strcmp(buf, "in") == 0)    { add_token("IN");     strcpy(last_token,"IN"); }
            else {
                char *tok = (char*)malloc(len + 20);
                snprintf(tok, len + 20, "IDENTIFIER %s", buf);
                add_token(tok);
                strcpy(last_token,"IDENTIFIER");
                free(tok);
            }
            free(buf);
            continue;
        }

        error(line, "Unknown character '%c'", *p);
        p++;
    }
}

/* ============================================================
   Parser
   ============================================================ */
static inline int match(const char *token) {
    if (current < i && strcmp(arr[current], token) == 0) { current++; return 1; }
    return 0;
}
static inline int is_statement_start(const char *token) {
    return
        strcmp(token, "LET") == 0 ||
        strcmp(token, "PRINT") == 0 ||
        strcmp(token, "RAISE") == 0 ||
        strcmp(token, "WARN") == 0 ||
        strcmp(token, "INFO") == 0 ||
        strcmp(token, "WHILE") == 0 ||
        strcmp(token, "IF") == 0 ||
        strcmp(token, "FUNC") == 0 ||
        strcmp(token, "RETURN") == 0 ||
        strcmp(token, "IMPORT") == 0 ||
        strcmp(token, "FOR") == 0 ||
        (strncmp(token, "IDENTIFIER ", 11) == 0);
}

/* fwd decl bodies (needed due to mutual recursion) */
static inline ASTNode *parse_statement(void);

static inline ASTNode *parse_factor(void) {
    if (current >= i) error(line, "Unexpected end of input");

    if (match("MINUS")) {
        ASTNode *right = parse_factor();
        ASTNode *negate = (ASTNode*)malloc(sizeof(ASTNode));
        negate->type = AST_BINARY_OP;
        strcpy(negate->binop.op, "*");
        ASTNode *minus_one = (ASTNode*)malloc(sizeof(ASTNode));
        minus_one->type = AST_NUMBER;
        minus_one->number = -1.0;
        negate->binop.left = minus_one;
        negate->binop.right = right;
        return negate;
    }

    if (match("LPAREN")) {
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')'");
        return expr;
    }

    if (current < i && strcmp(arr[current], "LBRACKET") == 0) {
        /* array literal (as expression) */
        current++; /* skip LBRACKET */
        ASTNode **elems = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
        int ec = 0;
        if (current < i && strcmp(arr[current], "RBRACKET") != 0) {
            while (1) {
                elems[ec++] = parse_expression();
                if (current < i && strcmp(arr[current], "COMMA") == 0) { current++; continue; }
                break;
            }
        }
        if (!match("RBRACKET")) error(line, "Expected ']' after array literal");
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_ARRAY_LITERAL;
        node->arraylit.elements = elems;
        node->arraylit.count = ec;
        return node;
    } else if (current < i && strncmp(arr[current], "NUMBER ", 7) == 0) {
        double value = strtod(arr[current] + 7, NULL);
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_NUMBER;
        node->number = value;
        current++;
        return node;

    } else if (current < i && strncmp(arr[current], "STRING ", 7) == 0) {
        const char *start = arr[current] + 8; /* skip space and opening " */
        size_t len = strlen(start);
        if (len > 0 && start[len - 1] == '"') len--;
        char *str = (char*)malloc(len + 1);
        memcpy(str, start, len);
        str[len] = '\0';
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_STRING;
        node->string = str;
        current++;
        return node;

    } else if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
        const char *name = arr[current] + 11;
        current++;
        if (current < i && strcmp(arr[current], "LPAREN") == 0) {
            current++; /* skip LPAREN */
            ASTNode **args = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
            int arg_count = 0;
            if (current < i && strcmp(arr[current], "RPAREN") != 0) {
                while (1) {
                    args[arg_count++] = parse_expression();
                    if (current < i && strcmp(arr[current], "COMMA") == 0) current++;
                    else break;
                }
            }
            if (!match("RPAREN")) error(line, "Expected ')' after function call arguments");
            ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
            node->type = AST_FUNCTION_CALL;
            node->funccall.funcname = strdup(name);
            node->funccall.args = args;
            node->funccall.arg_count = arg_count;
            return node;
        } else {
            ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
            node->type = AST_IDENTIFIER;
            node->string = strdup(name);
            /* handle indexing like ident[expr] */
            if (current < i && strcmp(arr[current], "LBRACKET") == 0) {
                ASTNode *idx = NULL;
                current++; /* skip LBRACKET */
                idx = parse_expression();
                if (!match("RBRACKET")) error(line, "Expected ']' after index");
                ASTNode *nidx = (ASTNode*)malloc(sizeof(ASTNode));
                nidx->type = AST_INDEX_EXPR;
                nidx->indexexpr.target = node;
                nidx->indexexpr.index = idx;
                return nidx;
            }
            return node;
        }

    } else if (current < i && strcmp(arr[current], "TRUE") == 0) {
        current++;
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_NUMBER; node->number = 1;
        return node;

    } else if (current < i && strcmp(arr[current], "FALSE") == 0) {
        current++;
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_NUMBER; node->number = 0;
        return node;

    } else if (current < i && strcmp(arr[current], "NOT") == 0) {
        current++;
        ASTNode *expr = parse_factor();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, "!");
        node->binop.left = expr;
        node->binop.right = NULL;
        return node;

    } else {
        error(line, "Expected expression");
        return NULL;
    }
}

/* parse index assignment like a[expr] = value */
static inline ASTNode *try_parse_index_assignment(char *identname) {
    /* current is at LBRACKET already consumed by caller */
    ASTNode *idx = parse_expression();
    if (!match("RBRACKET")) error(line, "Expected ']' after index");
    if (!match("ASSIGN")) { /* not an assignment */ return NULL; }
    ASTNode *val = parse_expression();
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = AST_INDEX_ASSIGN;
    ASTNode *target = (ASTNode*)malloc(sizeof(ASTNode));
    target->type = AST_IDENTIFIER; target->string = strdup(identname);
    node->indexassign.target = target;
    node->indexassign.index = idx;
    node->indexassign.value = val;
    return node;
}

static inline ASTNode *parse_term(void) {
    ASTNode *left = parse_factor();
    while (current < i && (strcmp(arr[current], "MULTIPLY") == 0 || strcmp(arr[current], "DIVIDE") == 0)) {
        char op[4];
        strcpy(op, strcmp(arr[current], "MULTIPLY") == 0 ? "*" : "/");
        current++;
        ASTNode *right = parse_factor();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

static inline ASTNode *parse_comparison(void) {
    ASTNode *left = parse_term();
    while (current < i &&
          (strcmp(arr[current], "LT") == 0 ||
           strcmp(arr[current], "GT") == 0 ||
           strcmp(arr[current], "LE") == 0 ||
           strcmp(arr[current], "GE") == 0 ||
           strcmp(arr[current], "EQ") == 0 ||
           strcmp(arr[current], "NEQ") == 0)) {

        char op[4] = {0};
        if      (strcmp(arr[current], "LT")  == 0) strcpy(op, "<");
        else if (strcmp(arr[current], "GT")  == 0) strcpy(op, ">");
        else if (strcmp(arr[current], "LE")  == 0) strcpy(op, "<=");
        else if (strcmp(arr[current], "GE")  == 0) strcpy(op, ">=");
        else if (strcmp(arr[current], "EQ")  == 0) strcpy(op, "==");
        else if (strcmp(arr[current], "NEQ") == 0) strcpy(op, "!=");
        current++;
        ASTNode *right = parse_term();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

static inline ASTNode *parse_logical(void) {
    ASTNode *left = parse_comparison();
    while (current < i && (strcmp(arr[current], "AND") == 0 || strcmp(arr[current], "OR") == 0)) {
        char op[4];
        strcpy(op, strcmp(arr[current], "AND") == 0 ? "&&" : "||");
        current++;
        ASTNode *right = parse_comparison();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

static inline ASTNode *parse_expression(void) {
    ASTNode *left = parse_logical();
    while (current < i && (strcmp(arr[current], "PLUS") == 0 || strcmp(arr[current], "MINUS") == 0)) {
        char op[4];
        strcpy(op, strcmp(arr[current], "PLUS") == 0 ? "+" : "-");
        current++;
        ASTNode *right = parse_logical();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

static inline ASTNode *parse_statement(void) {
    if (current >= i) return NULL;

    if (strncmp(arr[current], "IMPORT", 6) == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IMSTRING ", 9) == 0) {
            const char *start = arr[current] + 9;
            if (*start == '"') start++;
            size_t len = strlen(start);
            if (len > 0 && start[len-1] == '"') len--;
            char *filename = (char*)malloc(len + 1);
            memcpy(filename, start, len);
            filename[len] = '\0';
            handle_import(filename);
            free(filename);
            current++; /* IMSTRING */
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            return NULL;
        } else {
            error(line, "Expected string after import");
            return NULL;
        }
    }

    if (strcmp(arr[current], "FUNC") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *funcname = strdup(arr[current] + 11);
            current++;
            if (!match("LPAREN")) error(line, "Expected '(' after func name");
            char **params = (char**)malloc(sizeof(char*) * (size_t)i);
            int param_count = 0;
            if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
                while (1) {
                    params[param_count++] = strdup(arr[current] + 11);
                    current++;
                    if (current < i && strcmp(arr[current], "COMMA") == 0) current++; else break;
                }
            }
            if (!match("RPAREN")) error(line, "Expected ')' after func parameters");
            if (!match("LBRACE")) error(line, "Expected '{' after func parameters");
            ASTNode **stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
            int count = 0;
            while (current < i && is_statement_start(arr[current])) {
                ASTNode *st = parse_statement();
                if (st) stmts[count++] = st;
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after func body");
            ASTNode *body = (ASTNode*)malloc(sizeof(ASTNode));
            body->type = AST_STATEMENTS;
            body->statements.stmts = stmts;
            body->statements.count = count;

            ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
            node->type = AST_FUNC_DEF;
            node->funcdef.funcname = funcname;
            node->funcdef.params = params;
            node->funcdef.param_count = param_count;
            node->funcdef.body = body;
            return node;
        }
        error(line, "Expected identifier after func");
        return NULL;
    }

    if (strcmp(arr[current], "RETURN") == 0) {
        current++;
        ASTNode *expr = NULL;
        if (current < i && strcmp(arr[current], "SEMICOLON") != 0 && strcmp(arr[current], "RBRACE") != 0)
            expr = parse_expression();
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_RETURN;
        node->retstmt.expr = expr;
        return node;
    }

    if (strcmp(arr[current], "LET") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *varname = strdup(arr[current] + 11);
            current++;
            if (match("ASSIGN")) {
                ASTNode *value = parse_expression();
                ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
                node->type = AST_LET;
                node->var.varname = varname;
                node->var.value = value;
                return node;
            }
        }
        error(line, "Expected identifier after let");
        return NULL;
    }

    

    if (strcmp(arr[current], "PRINT") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after print");
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after print expression");
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_PRINT;
        node->print.expr = expr;
        return node;
    }

    if (strcmp(arr[current], "RAISE") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after raise");
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after raise expression");
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_RAISE;
        node->raise.expr = expr;
        return node;
    }

    if (strcmp(arr[current], "WARN") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after warn");
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after warn expression");
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_WARN;
        node->warn.expr = expr;
        return node;
    }

    if (strcmp(arr[current], "INFO") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after info");
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after info expression");
        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_INFO;
        node->info.expr = expr;
        return node;
    }

    if (strcmp(arr[current], "WHILE") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after while");
        ASTNode *cond = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after while condition");
        if (!match("LBRACE")) error(line, "Expected '{' after while condition");
        ASTNode **stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
        int count = 0;
        while (current < i && is_statement_start(arr[current])) {
            ASTNode *s = parse_statement();
            if (s) stmts[count++] = s;
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        }
        if (!match("RBRACE")) error(line, "Expected '}' after while body");
        ASTNode *body = (ASTNode*)malloc(sizeof(ASTNode));
        body->type = AST_STATEMENTS;
        body->statements.stmts = stmts;
        body->statements.count = count;

        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_WHILE;
        node->whilestmt.cond = cond;
        node->whilestmt.body = body;
        return node;
    }

    if (strcmp(arr[current], "IF") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after if");
        ASTNode *cond = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after if condition");
        if (!match("LBRACE")) error(line, "Expected '{' after if condition");
        ASTNode **then_stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
        int then_count = 0;
        while (current < i && is_statement_start(arr[current])) {
            ASTNode *s = parse_statement();
            if (s) then_stmts[then_count++] = s;
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        }
        if (!match("RBRACE")) error(line, "Expected '}' after if body");
        ASTNode *then_body = (ASTNode*)malloc(sizeof(ASTNode));
        then_body->type = AST_STATEMENTS;
        then_body->statements.stmts = then_stmts;
        then_body->statements.count = then_count;

        ASTNode *else_body = NULL;
        if (current < i && strcmp(arr[current], "ELSE") == 0) {
            current++;
            if (!match("LBRACE")) error(line, "Expected '{' after else");
            ASTNode **else_stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
            int else_count = 0;
            while (current < i && is_statement_start(arr[current])) {
                ASTNode *s = parse_statement();
                if (s) else_stmts[else_count++] = s;
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after else body");
            else_body = (ASTNode*)malloc(sizeof(ASTNode));
            else_body->type = AST_STATEMENTS;
            else_body->statements.stmts = else_stmts;
            else_body->statements.count = else_count;
        }

        ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
        node->type = AST_IF;
        node->ifstmt.condition  = cond;
        node->ifstmt.then_branch = then_body;
        node->ifstmt.else_branch = else_body;
        return node;
    }

    if (strcmp(arr[current], "FOR") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *varname = strdup(arr[current] + 11);
            current++;
            if (!match("IN")) error(line, "Expected 'in' after for variable");
            ASTNode *start = parse_expression();
            if (!match("DOT")) error(line, "Expected '.' in for range");
            ASTNode *end = parse_expression();
            if (!match("LBRACE")) error(line, "Expected '{' after for range");
            ASTNode **stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
            int count = 0;
            while (current < i && is_statement_start(arr[current])) {
                ASTNode *s = parse_statement();
                if (s) stmts[count++] = s;
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after for body");
            ASTNode *body = (ASTNode*)malloc(sizeof(ASTNode));
            body->type = AST_STATEMENTS;
            body->statements.stmts = stmts;
            body->statements.count = count;

            ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
            node->type = AST_FOR;
            node->forstmt.for_var   = varname;
            node->forstmt.for_start = start;
            node->forstmt.for_end   = end;
            node->forstmt.for_body  = body;
            return node;
        }
        error(line, "Expected identifier after for");
        return NULL;
    }

    if (strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
        char *varname = strdup(arr[current] + 11);
        current++;
        if (current < i && strcmp(arr[current], "ASSIGN") == 0) {
            current++;
            ASTNode *value = parse_expression();
            ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
            node->type = AST_ASSIGN;
            node->var.varname = varname;
            node->var.value   = value;
            return node;
        } else if (current < i && strcmp(arr[current], "LPAREN") == 0) {
            /* function call as statement */
            current--; /* step back so parse_factor sees IDENTIFIER */
            ASTNode *call = parse_factor();
            return call;
        } else if (current < i && strcmp(arr[current], "LBRACKET") == 0) {
            /* possibly index assignment like a[expr] = value */
            /* consume LBRACKET */
            current++;
            ASTNode *idx = parse_expression();
            if (!match("RBRACKET")) error(line, "Expected ']' after index");
            if (current < i && strcmp(arr[current], "ASSIGN") == 0) {
                current++; /* consume ASSIGN */
                ASTNode *val = parse_expression();
                ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
                node->type = AST_INDEX_ASSIGN;
                ASTNode *target = (ASTNode*)malloc(sizeof(ASTNode));
                target->type = AST_IDENTIFIER; target->string = varname;
                node->indexassign.target = target;
                node->indexassign.index = idx;
                node->indexassign.value = val;
                return node;
            }
            error(line, "Expected '=' after index assignment");
            return NULL;
        } else {
            error(line, "Expected '=' or '(' after identifier");
            return NULL;
        }
    }

    /* Fallback: expression statement */
    return parse_expression();
}

static inline ASTNode *parse_statements(void) {
    ASTNode **stmts = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)i);
    int count = 0;
    while (current < i) {
        if (!is_statement_start(arr[current])) break;
        ASTNode *stmt = parse_statement();
        if (stmt) stmts[count++] = stmt;
        if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        if (current >= i) break;
    }
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = AST_STATEMENTS;
    node->statements.stmts = stmts;
    node->statements.count = count;
    return node;
}

static inline void free_ast(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_STRING:
            free(node->string);
            break;
        case AST_IDENTIFIER:
            free(node->string);
            break;
        case AST_ARRAY_LITERAL:
            for (int j = 0; j < node->arraylit.count; ++j) free_ast(node->arraylit.elements[j]);
            free(node->arraylit.elements);
            break;
        case AST_INDEX_EXPR:
            free_ast(node->indexexpr.target);
            free_ast(node->indexexpr.index);
            break;
        case AST_INDEX_ASSIGN:
            free_ast(node->indexassign.target);
            free_ast(node->indexassign.index);
            free_ast(node->indexassign.value);
            break;
        case AST_LET:
        case AST_ASSIGN:
            free(node->var.varname);
            if (node->var.value) free_ast(node->var.value);
            break;
        case AST_BINARY_OP:
            free_ast(node->binop.left);
            if (node->binop.right) free_ast(node->binop.right);
            break;
        case AST_PRINT:       free_ast(node->print.expr); break;
        case AST_RAISE:       free_ast(node->raise.expr); break;
        case AST_WARN:        free_ast(node->warn.expr); break;
        case AST_INFO:        free_ast(node->info.expr); break;
        case AST_WHILE:
            free_ast(node->whilestmt.cond);
            free_ast(node->whilestmt.body);
            break;
        case AST_IF:
            free_ast(node->ifstmt.condition);
            free_ast(node->ifstmt.then_branch);
            if (node->ifstmt.else_branch) free_ast(node->ifstmt.else_branch);
            break;
        case AST_STATEMENTS:
            for (int j = 0; j < node->statements.count; ++j)
                free_ast(node->statements.stmts[j]);
            free(node->statements.stmts);
            break;
        case AST_FUNC_DEF:
            free(node->funcdef.funcname);
            for (int j = 0; j < node->funcdef.param_count; ++j) free(node->funcdef.params[j]);
            free(node->funcdef.params);
            free_ast(node->funcdef.body);
            break;
        case AST_FUNCTION_CALL:
            free(node->funccall.funcname);
            for (int j = 0; j < node->funccall.arg_count; ++j) free_ast(node->funccall.args[j]);
            free(node->funccall.args);
            break;
        case AST_RETURN:
            if (node->retstmt.expr) free_ast(node->retstmt.expr);
            break;
        case AST_FOR:
            free(node->forstmt.for_var);
            free_ast(node->forstmt.for_start);
            free_ast(node->forstmt.for_end);
            free_ast(node->forstmt.for_body);
            break;
        default: break;
    }
    free(node);
}

static inline void Parse(void) {
    current = 0;
    ASTNode *root = parse_statements();
    interpret(root);
    free_ast(root);
}

/* ============================================================
   Evaluation / Interpreter
   ============================================================ */


/* ============================================================
   Helper: normalize function names
   ============================================================ */


/* ============================================================
   Evaluation / Interpreter
   ============================================================ */
static inline char *eval_to_string(ASTNode *node);

static inline Value eval(ASTNode *node) {
    if (!node) return (Value){ .type = VAL_NUMBER, .number = 0 };

    switch (node->type) {
        case AST_NUMBER:
            return (Value){ .type = VAL_NUMBER, .number = node->number };

        case AST_STRING:
            return (Value){ .type = VAL_STRING, .string = strdup(node->string) };

        case AST_IDENTIFIER: {
            Var *v = get_var(node->string);
            if (v) {
                if (v->type == VAR_STRING)
                    return (Value){ .type = VAL_STRING, .string = strdup(v->str) };
                else if (v->type == VAR_OBJECT)
                    return (Value){ .type = VAL_OBJECT, .object = v->obj };
                else
                    return (Value){ .type = VAL_NUMBER, .number = v->value };
            }
            return (Value){ .type = VAL_NUMBER, .number = 0 };
        }

        case AST_ARRAY_LITERAL: {
            ObjArray *oa = (ObjArray*)malloc(sizeof(ObjArray));
            oa->type = OBJ_ARRAY;
            oa->count = node->arraylit.count;
            oa->capacity = node->arraylit.count;
            oa->items = (Value*)malloc(sizeof(Value) * (oa->capacity ? oa->capacity : 0));
            for (int j = 0; j < node->arraylit.count; ++j) {
                oa->items[j] = eval(node->arraylit.elements[j]);
            }
            return (Value){ .type = VAL_OBJECT, .object = oa };
        }

        case AST_INDEX_EXPR: {
            Value target = eval(node->indexexpr.target);
            Value idxv = eval(node->indexexpr.index);
            int idx = (int)idxv.number;
            if (target.type != VAL_OBJECT) { error(0, "index: target is not array"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
            ObjArray *oa = (ObjArray*)target.object;
            if (!oa || oa->type != OBJ_ARRAY) { error(0, "index: not an array"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
            if (idx < 0 || idx >= oa->count) return (Value){ .type = VAL_NUMBER, .number = 0 };
            return oa->items[idx];
        }

        case AST_INDEX_ASSIGN: {
            /* target must be identifier */
            if (node->indexassign.target->type != AST_IDENTIFIER) { error(0, "index assign: target must be identifier"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
            Var *v = get_var(node->indexassign.target->string);
            if (!v || v->type != VAR_OBJECT) { error(0, "index assign: variable is not array"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
            ObjArray *oa = (ObjArray*)v->obj;
            Value idxv = eval(node->indexassign.index);
            int idx = (int)idxv.number;
            Value val = eval(node->indexassign.value);
            if (idx < 0) error(0, "index assign: negative index");
            if (idx >= oa->count) {
                /* extend array to fit index */
                while (idx >= oa->capacity) {
                    int newcap = oa->capacity ? oa->capacity * 2 : 4;
                    oa->items = (Value*)realloc(oa->items, sizeof(Value) * newcap);
                    oa->capacity = newcap;
                }
                for (int k = oa->count; k <= idx; ++k) { oa->items[k].type = VAL_NUMBER; oa->items[k].number = 0; }
                oa->count = idx + 1;
            }
            /* free previous string if needed */
            if (oa->items[idx].type == VAL_STRING) free(oa->items[idx].string);
            oa->items[idx] = val;
            return (Value){ .type = VAL_NUMBER, .number = 1 };
        }

    case AST_BINARY_OP: {
            Value left  = eval(node->binop.left);
            Value right = node->binop.right ? eval(node->binop.right)
                                            : (Value){ .type = VAL_NUMBER, .number = 0 };

            // string concatenation for '+'
            if (strcmp(node->binop.op, "+") == 0 &&
                (left.type == VAL_STRING || right.type == VAL_STRING)) {
                char buf[64];
                char *lstr = (left.type == VAL_STRING) ? left.string :
                             (snprintf(buf, sizeof(buf), "%g", left.number), strdup(buf));
                char *rstr = (right.type == VAL_STRING) ? right.string :
                             (snprintf(buf, sizeof(buf), "%g", right.number), strdup(buf));

                char *result = (char*)malloc(strlen(lstr) + strlen(rstr) + 1);
                strcpy(result, lstr);
                strcat(result, rstr);

                if (left.type  != VAL_STRING) free(lstr);
                if (right.type != VAL_STRING) free(rstr);
                if (left.type  == VAL_STRING) free(left.string);
                if (right.type == VAL_STRING) free(right.string);

                return (Value){ .type = VAL_STRING, .string = result };
            }

            double lnum = (left.type == VAL_NUMBER) ? left.number : strtod(left.string, NULL);
            double rnum = (right.type == VAL_NUMBER) ? right.number : strtod(right.string, NULL);

            double result = 0;

            /* If either side is a string and we're doing equality/inequality,
               compare as strings rather than numeric conversion. For other
               ops, fall back to numeric semantics. */
            if (strcmp(node->binop.op, "==") == 0) {
                if (left.type == VAL_STRING || right.type == VAL_STRING) {
                    char lb[64], rb[64];
                    const char *ls = (left.type == VAL_STRING) ? left.string : (snprintf(lb, sizeof(lb), "%g", lnum), lb);
                    const char *rs = (right.type == VAL_STRING) ? right.string : (snprintf(rb, sizeof(rb), "%g", rnum), rb);
                    result = (strcmp(ls, rs) == 0);
                } else {
                    result = (lnum == rnum);
                }
            } else if (strcmp(node->binop.op, "!=") == 0) {
                if (left.type == VAL_STRING || right.type == VAL_STRING) {
                    char lb[64], rb[64];
                    const char *ls = (left.type == VAL_STRING) ? left.string : (snprintf(lb, sizeof(lb), "%g", lnum), lb);
                    const char *rs = (right.type == VAL_STRING) ? right.string : (snprintf(rb, sizeof(rb), "%g", rnum), rb);
                    result = (strcmp(ls, rs) != 0);
                } else {
                    result = (lnum != rnum);
                }
            } else {
                if      (strcmp(node->binop.op, "+")  == 0) result = lnum + rnum;
                else if (strcmp(node->binop.op, "-")  == 0) result = lnum - rnum;
                else if (strcmp(node->binop.op, "*")  == 0) result = lnum * rnum;
                else if (strcmp(node->binop.op, "/")  == 0) result = lnum / rnum;
                else if (strcmp(node->binop.op, "<")  == 0) result = (lnum <  rnum);
                else if (strcmp(node->binop.op, ">")  == 0) result = (lnum >  rnum);
                else if (strcmp(node->binop.op, "<=") == 0) result = (lnum <= rnum);
                else if (strcmp(node->binop.op, ">=") == 0) result = (lnum >= rnum);
                else if (strcmp(node->binop.op, "&&") == 0) result = ((lnum != 0) && (rnum != 0));
                else if (strcmp(node->binop.op, "||") == 0) result = ((lnum != 0) || (rnum != 0));
                else if (strcmp(node->binop.op, "!")  == 0) result = (lnum == 0);
            }

            if (left.type == VAL_STRING)  free(left.string);
            if (right.type == VAL_STRING) free(right.string);

            return (Value){ .type = VAL_NUMBER, .number = result };
        }

        case AST_FUNCTION_CALL: {
            // Built-ins: append(array, value) and len(array)
            if (strcmp(node->funccall.funcname, "append") == 0) {
                if (node->funccall.arg_count != 2) { error(0, "append requires 2 args"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
                Value a = eval(node->funccall.args[0]);
                Value v = eval(node->funccall.args[1]);
                if (a.type != VAL_OBJECT) { error(0, "append: first arg must be array"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
                ObjArray *oa = (ObjArray*)a.object;
                if (!oa || oa->type != OBJ_ARRAY) { error(0, "append: not an array"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
                if (oa->count >= oa->capacity) {
                    int newcap = oa->capacity ? oa->capacity * 2 : 4;
                    oa->items = (Value*)realloc(oa->items, sizeof(Value) * newcap);
                    oa->capacity = newcap;
                }
                /* transfer ownership of string if present */
                oa->items[oa->count++] = v;
                return (Value){ .type = VAL_NUMBER, .number = 1 };
            }
            if (strcmp(node->funccall.funcname, "len") == 0) {
                if (node->funccall.arg_count != 1) { error(0, "len requires 1 arg"); return (Value){ .type = VAL_NUMBER, .number = 0 }; }
                Value a = eval(node->funccall.args[0]);
                if (a.type != VAL_OBJECT) return (Value){ .type = VAL_NUMBER, .number = 0 };
                ObjArray *oa = (ObjArray*)a.object;
                if (!oa || oa->type != OBJ_ARRAY) return (Value){ .type = VAL_NUMBER, .number = 0 };
                return (Value){ .type = VAL_NUMBER, .number = (double)oa->count };
            }

            // let sling_get_native() normalize
            SlingCFunc native = sling_get_native(node->funccall.funcname);

            if (native) {;

                Value *args = (Value*) malloc(sizeof(Value) * node->funccall.arg_count);
                for (int j = 0; j < node->funccall.arg_count; j++)
                    args[j] = eval(node->funccall.args[j]);

                Value result = native(node->funccall.arg_count, args);

                for (int j = 0; j < node->funccall.arg_count; j++)
                    if (args[j].type == VAL_STRING) free(args[j].string);
                free(args);

                return result;
            }

            // === Built-in: input() ===
            if (strcmp(node->funccall.funcname, "input") == 0) {
                char *prompt = node->funccall.arg_count > 0
                               ? eval_to_string(node->funccall.args[0])
                               : strdup("");
                if (*prompt) { printf("%s", prompt); fflush(stdout); }
                free(prompt);

                char buffer[1024];
                if (!fgets(buffer, sizeof(buffer), stdin)) buffer[0] = 0;
                buffer[strcspn(buffer, "\n")] = 0;
                return (Value){ .type = VAL_STRING, .string = strdup(buffer) };
            }

            // === User-defined Sling function ===
            ASTNode *func = get_func(node->funccall.funcname);
            if (!func) {
                error(0, "Undefined function: %s", node->funccall.funcname);
                return (Value){ .type = VAL_NUMBER, .number = 0 };
            }

            int saved_var_count = var_count;
            for (int j = 0; j < func->funcdef.param_count; ++j) {
                Value argval = (j < node->funccall.arg_count)
                    ? eval(node->funccall.args[j])
                    : (Value){ .type = VAL_NUMBER, .number = 0 };

                if (argval.type == VAL_STRING) {
                    set_var(func->funcdef.params[j], VAR_STRING, 0, argval.string);
                    free(argval.string);
                } else {
                    set_var(func->funcdef.params[j], VAR_NUMBER, argval.number, NULL);
                }
            }

            Value result = { .type = VAL_NUMBER, .number = 0 };
            if (setjmp(return_buf) == 0)
                interpret(func->funcdef.body);
            else
                result = return_value;

            var_count = saved_var_count;
            return result;
        }

        default:
            return (Value){ .type = VAL_NUMBER, .number = 0 };
    }
}



/* ============================================================
   String conversion
   ============================================================ */
static inline char *eval_to_string(ASTNode *node) {
    Value v = eval(node);
    if (v.type == VAL_STRING) return v.string;
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v.number);
    return strdup(buf);
}


static inline void interpret(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_STATEMENTS:
            for (int j = 0; j < node->statements.count; ++j)
                interpret(node->statements.stmts[j]);
            break;

        case AST_PRINT: {
            char *s = eval_to_string(node->print.expr);
            printf("%s\n", s);
            free(s);
            break;
        }

        case AST_FUNC_DEF:
            add_func(node->funcdef.funcname, node);
            break;

        case AST_RETURN:
            return_value = eval(node->retstmt.expr);
            longjmp(return_buf, 1);
            break;

        case AST_ASSIGN:
        case AST_LET: {
            Value val = eval(node->var.value);
            if (val.type == VAL_STRING) {
                set_var(node->var.varname, VAR_STRING, 0, val.string);
                free(val.string);
            } else if (val.type == VAL_OBJECT) {
                /* transfer ownership of object to variable */
                set_var_object(node->var.varname, val.object);
            } else {
                set_var(node->var.varname, VAR_NUMBER, val.number, NULL);
            }
            break;
        }

        case AST_IF:
            if (eval(node->ifstmt.condition).number)
                interpret(node->ifstmt.then_branch);
            else if (node->ifstmt.else_branch)
                interpret(node->ifstmt.else_branch);
            break;

        case AST_WHILE:
            while (eval(node->whilestmt.cond).number)
                interpret(node->whilestmt.body);
            break;

        case AST_FOR: {
            int start = (int)eval(node->forstmt.for_start).number;
            int end   = (int)eval(node->forstmt.for_end).number;
            for (int k = start; k <= end; ++k) {
                set_var(node->forstmt.for_var, VAR_NUMBER, k, NULL);
                interpret(node->forstmt.for_body);
            }
            break;
        }
        case AST_FUNCTION_CALL:
        case AST_ARRAY_LITERAL:
        case AST_INDEX_EXPR:
        case AST_INDEX_ASSIGN:
            (void)eval(node); /* result ignored in statement context */
            break;

        case AST_RAISE: {
            char *msg = eval_to_string(node->raise.expr);
            error(line, "%s", msg);
            free(msg);
            break; /* unreachable if error() exits */
        }
        case AST_WARN: {
            char *msg = eval_to_string(node->warn.expr);
            warn(line, "%s", msg);
            free(msg);
            break;
        }
        case AST_INFO: {
            char *msg = eval_to_string(node->info.expr);
            info(line, "%s", msg);
            free(msg);
            break;
        }

        default: (void)0; break;
    }
}

/* ============================================================
   Imports and toplevel driver
   ============================================================ */
static inline void handle_import(const char *filename) {
    const char *ext = strrchr(filename, '.');

    // --- Case 1: C header module (.h) ---
    if (ext && strcmp(ext, ".h") == 0) {
        /* Dynamically look up a registration function named
           sling_register_module_<basename> where <basename> is the
           header filename without directory or .h, normalized to
           alphanumerics/underscores. This allows any compiled-in
           native module to register itself without special-casing.
        */
        const char *base = strrchr(filename, '/');
        base = base ? base + 1 : filename;
        size_t blen = strlen(base);
        char mod[256];
        if (blen >= sizeof(mod)) blen = sizeof(mod) - 1;
        memcpy(mod, base, blen);
        mod[blen] = '\0';
        /* strip trailing .h */
        if (blen > 2 && strcmp(mod + blen - 2, ".h") == 0) mod[blen - 2] = '\0';

        /* normalize into identifier characters */
        char clean[256];
        size_t ci = 0;
        for (size_t j = 0; j < strlen(mod) && ci + 1 < sizeof(clean); ++j) {
            char c = mod[j];
            clean[ci++] = (isalnum((unsigned char)c) ? c : '_');
        }
        clean[ci] = '\0';

        char sym[512];
        snprintf(sym, sizeof(sym), "sling_register_module_%s", clean);

        void (*init_func)(void) = (void(*)(void)) dlsym(RTLD_DEFAULT, sym);
        if (init_func) { init_func(); return; }

        /* Fallback: try the raw mod name (some modules use dots or other chars) */
        snprintf(sym, sizeof(sym), "sling_register_module_%s", mod);
        init_func = (void(*)(void)) dlsym(RTLD_DEFAULT, sym);
        if (init_func) { init_func(); return; }

        error(0, "Unknown native module: %s", filename);
        return;
    }

    // --- Case 2: Sling source (.sling) ---
    FILE *file = fopen(filename, "r");
    if (!file) {
        error(0, "Could not import file: %s", filename);
        return;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *code = (char*)malloc((size_t)length + 1);
    if (!code) { fclose(file); error(0, "Out of memory reading %s", filename); }
    fread(code, 1, (size_t)length, file);
    code[length] = '\0';
    fclose(file);

    int old_i = i, old_current = current, old_line = line;
    lex(code);
    free(code);

    int import_start = old_i, import_end = i;

    current = import_start;
    ASTNode *root = parse_statements();

    // Only keep function definitions
    if (root && root->type == AST_STATEMENTS) {
        for (int j = 0; j < root->statements.count; ++j) {
            ASTNode *stmt = root->statements.stmts[j];
            if (stmt && stmt->type == AST_FUNC_DEF) {
                add_func(stmt->funcdef.funcname, stmt);
                root->statements.stmts[j] = NULL;
            }
        }
    }
    free_ast(root);

    current = old_current;
    line    = old_line;

    for (int j = import_start; j < import_end; ++j) {
        free(arr[j]); arr[j] = NULL;
    }
    i = old_i;
}



#endif /* SLING_H */
