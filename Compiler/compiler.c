#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Parser/parser.h"
#include <setjmp.h>
#include "../Lexer/lexer.h"
#include "../import.h"
#include "../errors/error.h"

jmp_buf return_buf;
double return_value;

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

double eval(ASTNode *node);
void interpret(ASTNode *node);

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
            if (type == VAR_NUMBER) {
                vars[j].value = value;
            } else {
                free(vars[j].str);
                vars[j].str = strdup(str);
            }
            return;
        }
    }
    vars[var_count].name = strdup(name);
    vars[var_count].type = type;
    if (type == VAR_NUMBER) {
        vars[var_count].value = value;
        vars[var_count].str = NULL;
    } else {
        vars[var_count].str = strdup(str);
        vars[var_count].value = 0;
    }
    var_count++;
}

void print_ast(ASTNode *node, int depth) {
    if (!node) return;
    for (int d = 0; d < depth; ++d) printf("  ");
    printf("Node type: %d\n", node->type);
    switch (node->type) {
        case AST_FUNC_DEF:
            printf("Function: %s\n", node->funcdef.funcname);
            print_ast(node->funcdef.body, depth + 1);
            break;
        case AST_STATEMENTS:
            for (int j = 0; j < node->statements.count; ++j)
                print_ast(node->statements.stmts[j], depth + 1);
            break;
        case AST_RETURN:
            printf("Return statement\n");
            print_ast(node->retstmt.expr, depth + 1);
            break;
        case AST_BINARY_OP:
            printf("Binary op: %s\n", node->binop.op);
            print_ast(node->binop.left, depth + 1);
            print_ast(node->binop.right, depth + 1);
            break;
        case AST_NUMBER:
            printf("Number: %g\n", node->number);
            break;
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->string);
            break;
        default:
            break;
    }
}

void add_func(const char *name, ASTNode *def) {
    for (int j = 0; j < func_count; ++j) {
        if (strcmp(funcs[j].name, name) == 0) {
            funcs[j].def = def;
            return;
        }
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

// Save/restore variable state for function calls
typedef struct {
    char *name;
    double value;
} VarSnapshot;

void push_vars(VarSnapshot *snap, int *count) {
    *count = var_count;
    for (int j = 0; j < var_count; ++j) {
        snap[j].name = vars[j].name;
        snap[j].value = vars[j].value;
    }
}
void pop_vars(VarSnapshot *snap, int count) {
    for (int j = 0; j < count; ++j) {
        vars[j].name = snap[j].name;
        vars[j].value = snap[j].value;
    }
    var_count = count;
}

void process_import(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        error(0, "Could not import file: %s", filename);
        return;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *code = malloc(length + 1);
    fread(code, 1, length, file);
    code[length] = '\0';
    fclose(file);
    int old_i = i;
    lex(code);
    free(code);
    int import_start = old_i;
    int old_current = current;
    current = import_start;
    ASTNode *root = parse_statements();
    interpret(root);
    free_ast(root);
    current = old_current;
    i = old_i;
}

double eval(ASTNode *node) {
    if (!node) return 0;
    switch (node->type) {
        case AST_NUMBER:
            return node->number;
        case AST_STRING:
            return 0; // Strings are not numbers
        case AST_IDENTIFIER: {
            Var *v = get_var(node->string);
            if (v && v->type == VAR_NUMBER) return v->value;
            return 0;
        }
        case AST_BINARY_OP: {
            // --- Begin string concatenation logic ---
            ASTNode *leftNode = node->binop.left;
            ASTNode *rightNode = node->binop.right;
            int leftIsString = leftNode && (leftNode->type == AST_STRING || (leftNode->type == AST_IDENTIFIER && get_var(leftNode->string) && get_var(leftNode->string)->type == VAR_STRING));
            int rightIsString = rightNode && (rightNode->type == AST_STRING || (rightNode->type == AST_IDENTIFIER && get_var(rightNode->string) && get_var(rightNode->string)->type == VAR_STRING));
            if (strcmp(node->binop.op, "+") == 0 && (leftIsString || rightIsString)) {
                // Get left string
                char *leftStr = NULL;
                if (leftIsString) {
                    if (leftNode->type == AST_STRING) leftStr = leftNode->string;
                    else if (leftNode->type == AST_IDENTIFIER) leftStr = get_var(leftNode->string)->str;
                } else {
                    // Convert left number to string
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%g", eval(leftNode));
                    leftStr = strdup(buf);
                }
                // Get right string
                char *rightStr = NULL;
                if (rightIsString) {
                    if (rightNode->type == AST_STRING) rightStr = rightNode->string;
                    else if (rightNode->type == AST_IDENTIFIER) rightStr = get_var(rightNode->string)->str;
                } else {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%g", eval(rightNode));
                    rightStr = strdup(buf);
                }
                // Concatenate
                size_t len = strlen(leftStr) + strlen(rightStr) + 1;
                char *result = malloc(len);
                strcpy(result, leftStr);
                strcat(result, rightStr);
                // Store result in a static buffer for printing
                static char static_result[1024];
                strncpy(static_result, result, sizeof(static_result)-1);
                static_result[sizeof(static_result)-1] = '\0';
                free(result);
                if (!leftIsString) free(leftStr);
                if (!rightIsString) free(rightStr);
                // Hack: set node type to AST_STRING for interpret
                node->type = AST_STRING;
                node->string = static_result;
                return 0; // Not used for numbers
            }
            // --- End string concatenation logic ---
            double left = eval(leftNode);
            double right = rightNode ? eval(rightNode) : 0;
            if (strcmp(node->binop.op, "+") == 0) return left + right;
            if (strcmp(node->binop.op, "-") == 0) return left - right;
            if (strcmp(node->binop.op, "*") == 0) return left * right;
            if (strcmp(node->binop.op, "/") == 0) return left / right;
            if (strcmp(node->binop.op, "<") == 0) return left < right;
            if (strcmp(node->binop.op, ">") == 0) return left > right;
            if (strcmp(node->binop.op, "<=") == 0) return left <= right;
            if (strcmp(node->binop.op, ">=") == 0) return left >= right;
            if (strcmp(node->binop.op, "==") == 0) return left == right;
            if (strcmp(node->binop.op, "!=") == 0) return left != right;
            if (strcmp(node->binop.op, "&&") == 0) return left && right;
            if (strcmp(node->binop.op, "||") == 0) return left || right;
            if (strcmp(node->binop.op, "!") == 0) return !left;
            return 0;
        }
        case AST_FUNCTION_CALL: {
            ASTNode *func = get_func(node->funccall.funcname);
            if (!func) {
                error(0, "Undefined function: %s", node->funccall.funcname);
                return 0;
            }
            VarSnapshot snap[1024];
            int snap_count;
            push_vars(snap, &snap_count);
            for (int j = 0; j < func->funcdef.param_count; ++j) {
                ASTNode *arg = (j < node->funccall.arg_count) ? node->funccall.args[j] : NULL;
                if (arg && arg->type == AST_STRING) {
                    set_var(func->funcdef.params[j], VAR_STRING, 0, arg->string);
                } else {
                    set_var(func->funcdef.params[j], VAR_NUMBER, arg ? eval(arg) : 0, NULL);
                }
            }
            double result = 0;
            if (setjmp(return_buf) == 0) {
                interpret(func->funcdef.body);
                result = 0;
            } else {
                result = return_value;
            }
            pop_vars(snap, snap_count);
            return result;
        }
        default:
            return 0;
    }
}

// Helper to evaluate an ASTNode to a string (caller must free)
char *eval_to_string(ASTNode *node) {
    if (!node) return strdup("");
    switch (node->type) {
        case AST_STRING:
            return strdup(node->string);
        case AST_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", node->number);
            return strdup(buf);
        }
        case AST_IDENTIFIER: {
            Var *v = get_var(node->string);
            if (v) {
                if (v->type == VAR_STRING) return strdup(v->str);
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", v->value);
                return strdup(buf);
            }
            return strdup("0");
        }
        case AST_BINARY_OP: {
            int leftIsString = node->binop.left && (node->binop.left->type == AST_STRING || (node->binop.left->type == AST_IDENTIFIER && get_var(node->binop.left->string) && get_var(node->binop.left->string)->type == VAR_STRING));
            int rightIsString = node->binop.right && (node->binop.right->type == AST_STRING || (node->binop.right->type == AST_IDENTIFIER && get_var(node->binop.right->string) && get_var(node->binop.right->string)->type == VAR_STRING));
            if (strcmp(node->binop.op, "+") == 0 && (leftIsString || rightIsString)) {
                char *leftStr = eval_to_string(node->binop.left);
                char *rightStr = eval_to_string(node->binop.right);
                size_t len = strlen(leftStr) + strlen(rightStr) + 1;
                char *result = malloc(len);
                strcpy(result, leftStr);
                strcat(result, rightStr);
                free(leftStr);
                free(rightStr);
                return result;
            }
            // For non-string +, fallback to number
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", eval(node));
            return strdup(buf);
        }
        default: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", eval(node));
            return strdup(buf);
        }
    }
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
        case AST_STRING:
            printf("%s\n", node->string);
            break;
        case AST_LET:
        case AST_ASSIGN:
            if (node->var.value->type == AST_STRING) {
                set_var(node->var.varname, VAR_STRING, 0, node->var.value->string);
            } else {
                set_var(node->var.varname, VAR_NUMBER, eval(node->var.value), NULL);
            }
            break;
        case AST_WHILE:
            while (eval(node->whilestmt.cond)) {
                interpret(node->whilestmt.body);
            }
            break;
        case AST_IF:
            if (eval(node->ifstmt.condition)) {
                interpret(node->ifstmt.then_branch);
            } else if (node->ifstmt.else_branch) {
                interpret(node->ifstmt.else_branch);
            }
            break;
        case AST_FUNC_DEF:
            add_func(node->funcdef.funcname, node);
            break;
        case AST_RETURN:
            if (node->retstmt.expr && node->retstmt.expr->type == AST_STRING) {
                set_var("__return_str__", VAR_STRING, 0, node->retstmt.expr->string);
                return_value = 0;
            } else {
                set_var("__return_str__", VAR_NUMBER, 0, NULL);
                return_value = node->retstmt.expr ? eval(node->retstmt.expr) : 0;
            }
            longjmp(return_buf, 1);
            break;
        case AST_FUNCTION_CALL:
            eval(node);
            break;
        case AST_IMPORT:
            // Do nothing here; imports are handled in the parser
            break;
        case AST_FOR: {
            int for_start = (int)eval(node->forstmt.for_start);
            int for_end = (int)eval(node->forstmt.for_end);
            for (int v = for_start; v <= for_end; ++v) {
                set_var(node->forstmt.for_var, VAR_NUMBER, v, NULL);
                interpret(node->forstmt.for_body);
            }
            break;
        }
        default:
            break;
    }
}
