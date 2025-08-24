#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "../Parser/parser.h"
#include "../Lexer/lexer.h"
#include "../import.h"
#include "../errors/error.h"
#include "compiler.h"
#include "math.h"

jmp_buf return_buf;
Value return_value;

typedef enum { VAR_NUMBER, VAR_STRING } VarType;
typedef struct {
    char *name;
    VarType type;
    double value;
    char *str;
} Var;

Var vars[1024];
int var_count = 0;

#define MAX_FUNCS 128
typedef struct {
    char *name;
    ASTNode *def;
} Func;

Func funcs[MAX_FUNCS];
int func_count = 0;

Var *get_var(const char *name) {
    for (int j = 0; j < var_count; ++j)
        if (strcmp(vars[j].name, name) == 0)
            return &vars[j];
    return NULL;
}

void set_var(const char *name, VarType type, double value, const char *str) {
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
    vars[var_count].str = (type == VAR_STRING) ? strdup(str ? str : "") : NULL;
    var_count++;
}

void add_func(const char *name, ASTNode *def) {
    for (int j = 0; j < func_count; ++j)
        if (strcmp(funcs[j].name, name) == 0) {
            funcs[j].def = def;
            return;
        }
    funcs[func_count].name = strdup(name);
    funcs[func_count].def = def;
    func_count++;
}

ASTNode *get_func(const char *name) {
    for (int j = 0; j < func_count; ++j)
        if (strcmp(funcs[j].name, name) == 0)
            return funcs[j].def;
    return NULL;
}

char *eval_to_string(ASTNode *node);
void interpret(ASTNode *node);

Value eval(ASTNode *node) {
    Value v = {0};
    if (!node) return (Value){.type = VAL_NUMBER, .number = 0};

    switch (node->type) {
        case AST_NUMBER:
            return (Value){.type = VAL_NUMBER, .number = node->number};

        case AST_STRING:
            return (Value){.type = VAL_STRING, .string = strdup(node->string)};

        case AST_IDENTIFIER: {
            Var *var = get_var(node->string);
            if (var) {
                if (var->type == VAR_STRING)
                    return (Value){.type = VAL_STRING, .string = strdup(var->str)};
                return (Value){.type = VAL_NUMBER, .number = var->value};
            }
            return (Value){.type = VAL_NUMBER, .number = 0};
        }

        case AST_BINARY_OP: {
            Value left = eval(node->binop.left);
            Value right = node->binop.right ? eval(node->binop.right) : (Value){.type=VAL_NUMBER,.number=0};

            if (strcmp(node->binop.op, "+") == 0 &&
                (left.type == VAL_STRING || right.type == VAL_STRING)) {

                char buf[64];
                char *lstr = (left.type == VAL_STRING) ? left.string :
                             (snprintf(buf, sizeof(buf), "%g", left.number), strdup(buf));
                char *rstr = (right.type == VAL_STRING) ? right.string :
                             (snprintf(buf, sizeof(buf), "%g", right.number), strdup(buf));

                char *result = malloc(strlen(lstr) + strlen(rstr) + 1);
                strcpy(result, lstr);
                strcat(result, rstr);

                if (left.type != VAL_STRING) free(lstr);
                if (right.type != VAL_STRING) free(rstr);
                if (left.type == VAL_STRING) free(left.string);
                if (right.type == VAL_STRING) free(right.string);

                return (Value){.type = VAL_STRING, .string = result};
            }

            double lnum = (left.type == VAL_NUMBER) ? left.number : strtod(left.string, NULL);
            double rnum = (right.type == VAL_NUMBER) ? right.number : strtod(right.string, NULL);

            if (left.type == VAL_STRING) free(left.string);
            if (right.type == VAL_STRING) free(right.string);

            double result = 0;
            if (strcmp(node->binop.op, "+") == 0) result = lnum + rnum;
            else if (strcmp(node->binop.op, "-") == 0) result = lnum - rnum;
            else if (strcmp(node->binop.op, "*") == 0) result = lnum * rnum;
            else if (strcmp(node->binop.op, "/") == 0) result = lnum / rnum;
            else if (strcmp(node->binop.op, "<") == 0) result = lnum < rnum;
            else if (strcmp(node->binop.op, ">") == 0) result = lnum > rnum;
            else if (strcmp(node->binop.op, "<=") == 0) result = lnum <= rnum;
            else if (strcmp(node->binop.op, ">=") == 0) result = lnum >= rnum;
            else if (strcmp(node->binop.op, "==") == 0) result = lnum == rnum;
            else if (strcmp(node->binop.op, "!=") == 0) result = lnum != rnum;

            return (Value){.type = VAL_NUMBER, .number = result};
        }

        case AST_FUNCTION_CALL: {
            if (strcmp(node->funccall.funcname, "input") == 0) {
                char *prompt = node->funccall.arg_count > 0 ? eval_to_string(node->funccall.args[0]) : strdup("");
                if (*prompt) { printf("%s", prompt); fflush(stdout); }
                free(prompt);

                char buffer[1024];
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                return (Value){.type = VAL_STRING, .string = strdup(buffer)};
            }

            ASTNode *func = get_func(node->funccall.funcname);
            if (!func) {
                error(0, "Undefined function: %s", node->funccall.funcname);
                return (Value){.type = VAL_NUMBER, .number = 0};
            }

            int saved_var_count = var_count;
            for (int j = 0; j < func->funcdef.param_count; ++j) {
                Value argval = (j < node->funccall.arg_count) ? eval(node->funccall.args[j]) : (Value){.type=VAL_NUMBER,.number=0};
                if (argval.type == VAL_STRING)
                    set_var(func->funcdef.params[j], VAR_STRING, 0, argval.string);
                else
                    set_var(func->funcdef.params[j], VAR_NUMBER, argval.number, NULL);
                if (argval.type == VAL_STRING) free(argval.string);
            }

            Value result = {.type = VAL_NUMBER, .number = 0};
            if (setjmp(return_buf) == 0)
                interpret(func->funcdef.body);
            else
                result = return_value;

            var_count = saved_var_count;
            return result;
        }

        default:
            return (Value){.type = VAL_NUMBER, .number = 0};
    }
}

char *eval_to_string(ASTNode *node) {
    Value v = eval(node);
    if (v.type == VAL_STRING) return v.string;

    char buf[64];
    // If number is effectively an integer, print as integer
    if (fabs(v.number - (long long)v.number) < 1e-9) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v.number);
    } else {
        snprintf(buf, sizeof(buf), "%.15g", v.number);
    }
    return strdup(buf);
}


void interpret(ASTNode *node) {
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

        case AST_ASSIGN:
        case AST_LET: {
            Value val = eval(node->var.value);
            if (val.type == VAL_STRING) {
                set_var(node->var.varname, VAR_STRING, 0, val.string);
                free(val.string);
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
            int end = (int)eval(node->forstmt.for_end).number;
            for (int i = start; i <= end; ++i) {
                set_var(node->forstmt.for_var, VAR_NUMBER, i, NULL);
                interpret(node->forstmt.for_body);
            }
            break;
        }

        case AST_FUNCTION_CALL:
            eval(node);
            break;

        default:
            break;
    }
}
