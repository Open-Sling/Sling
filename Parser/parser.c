#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "../errors/error.h"
#include "../Compiler/compiler.h"
#include "../import.h"
extern char *arr[99999];
extern int i;
extern int line;

int current = 0;

int match(const char *token) {
    if (current < i && strcmp(arr[current], token) == 0) {
        current++;
        return 1;
    }
    return 0;
}

int is_statement_start(const char *token) {
    return
        strcmp(token, "LET") == 0 ||
        strcmp(token, "PRINT") == 0 ||
        strcmp(token, "WHILE") == 0 ||
        strcmp(token, "IF") == 0 ||
        strcmp(token, "FUNC") == 0 ||
        strcmp(token, "RETURN") == 0 ||
        strcmp(token, "IMPORT") == 0 ||
        strcmp(token, "FOR") == 0 ||
        (strncmp(token, "IDENTIFIER ", 11) == 0);
}

// Forward declarations
ASTNode *parse_expression();
ASTNode *parse_logical();
ASTNode *parse_comparison();
ASTNode *parse_term();
ASTNode *parse_factor();

ASTNode *parse_factor() {
    if (current >= i) error(line, "Unexpected end of input");
    if (match("LPAREN")) {
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' ");
        return expr;
    } else if (current < i && strncmp(arr[current], "NUMBER ", 7) == 0) {
        double value = strtod(arr[current] + 7, NULL);
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_NUMBER;
        node->number = value;
        current++;
        return node;
    } else if (current < i && strncmp(arr[current], "STRING ", 7) == 0) {
        const char *start = arr[current] + 8;
        size_t len = strlen(start);
        if (len > 0 && start[len-1] == '"') len--;
        char *str = malloc(len + 1);
        strncpy(str, start, len);
        str[len] = '\0';
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_STRING;
        node->string = str;
        current++;
        return node;
    } else if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
        // Function call or variable
        const char *name = arr[current] + 11;
        current++;
        if (current < i && strcmp(arr[current], "LPAREN") == 0) {
            // Function call
            current++; // skip LPAREN
            ASTNode **args = malloc(sizeof(ASTNode*) * i);
            int arg_count = 0;
            if (current < i && strcmp(arr[current], "RPAREN") != 0) {
                while (1) {
                    args[arg_count++] = parse_expression();
                    if (current < i && strcmp(arr[current], "COMMA") == 0) current++;
                    else break;
                }
            }
            if (!match("RPAREN")) error(line, "Expected ')' after function call arguments");
            ASTNode *node = malloc(sizeof(ASTNode));
            node->type = AST_FUNCTION_CALL;
            node->funccall.funcname = strdup(name);
            node->funccall.args = args;
            node->funccall.arg_count = arg_count;
            return node;
        } else {
            ASTNode *node = malloc(sizeof(ASTNode));
            node->type = AST_IDENTIFIER;
            node->string = strdup(name);
            return node;
        }
    } else if (current < i && strcmp(arr[current], "TRUE") == 0) {
        current++;
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_NUMBER;
        node->number = 1;
        return node;
    } else if (current < i && strcmp(arr[current], "FALSE") == 0) {
        current++;
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_NUMBER;
        node->number = 0;
        return node;
    } else if (current < i && strcmp(arr[current], "NOT") == 0) {
        current++;
        ASTNode *expr = parse_factor();
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, "!");
        node->binop.left = expr;
        node->binop.right = NULL;
        return node;
    } else {
        error(line, "Expected number, identifier, string, boolean, or '(' or function call");
        return NULL;
    }
}

ASTNode *parse_term() {
    ASTNode *left = parse_factor();
    while (current < i && (strcmp(arr[current], "MULTIPLY") == 0 || strcmp(arr[current], "DIVIDE") == 0)) {
        char op[4];
        strcpy(op, strcmp(arr[current], "MULTIPLY") == 0 ? "*" : "/");
        match(arr[current]);
        ASTNode *right = parse_factor();
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

ASTNode *parse_comparison() {
    ASTNode *left = parse_term();
    while (current < i &&
        (strcmp(arr[current], "LT") == 0 ||
         strcmp(arr[current], "GT") == 0 ||
         strcmp(arr[current], "LE") == 0 ||
         strcmp(arr[current], "GE") == 0 ||
         strcmp(arr[current], "EQ") == 0 ||
         strcmp(arr[current], "NEQ") == 0)) {

        char op[4];
        if (strcmp(arr[current], "LT") == 0) strcpy(op, "<");
        else if (strcmp(arr[current], "GT") == 0) strcpy(op, ">");
        else if (strcmp(arr[current], "LE") == 0) strcpy(op, "<=");
        else if (strcmp(arr[current], "GE") == 0) strcpy(op, ">=");
        else if (strcmp(arr[current], "EQ") == 0) strcpy(op, "==");
        else if (strcmp(arr[current], "NEQ") == 0) strcpy(op, "!=");
        else error(line, "Unknown comparison operator");

        current++;
        ASTNode *right = parse_term();
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

ASTNode *parse_logical() {
    ASTNode *left = parse_comparison();
    while (current < i && (strcmp(arr[current], "AND") == 0 || strcmp(arr[current], "OR") == 0)) {
        char op[4];
        if (strcmp(arr[current], "AND") == 0) strcpy(op, "&&");
        else strcpy(op, "||");
        current++;
        ASTNode *right = parse_comparison();
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

ASTNode *parse_expression() {
    ASTNode *left = parse_logical();
    while (current < i && (strcmp(arr[current], "PLUS") == 0 || strcmp(arr[current], "MINUS") == 0)) {
        char op[4];
        strcpy(op, strcmp(arr[current], "PLUS") == 0 ? "+" : "-");
        match(arr[current]);
        ASTNode *right = parse_logical();
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_BINARY_OP;
        strcpy(node->binop.op, op);
        node->binop.left = left;
        node->binop.right = right;
        left = node;
    }
    return left;
}

ASTNode *parse_statement() {
    if (current >= i) return NULL;
    if (strncmp(arr[current], "IMPORT", 6) == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IMSTRING ", 9) == 0) {
            // Extract filename from IMSTRING
            const char *start = arr[current] + 9;
            if (*start == '"') start++;
            size_t len = strlen(start);
            if (len > 0 && start[len-1] == '"') len--;
            char *filename = malloc(len + 1);
            strncpy(filename, start, len);
            filename[len] = '\0';
            handle_import(filename);
            free(filename);
            current++; // Skip IMSTRING
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++; // Skip SEMICOLON
            return NULL; // Import is not an AST node
        } else {
            error(line, "Expected string after import");
            return NULL;
        }
    } else if (strcmp(arr[current], "FUNC") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *funcname = strdup(arr[current] + 11);
            current++;
            if (!match("LPAREN")) error(line, "Expected '(' after func name");
            char **params = malloc(sizeof(char*) * i);
            int param_count = 0;
            if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
                while (1) {
                    params[param_count++] = strdup(arr[current] + 11);
                    current++;
                    if (current < i && strcmp(arr[current], "COMMA") == 0) current++;
                    else break;
                }
            }
            if (!match("RPAREN")) error(line, "Expected ')' after func parameters");
            if (!match("LBRACE")) error(line, "Expected '{' after func parameters");
            ASTNode **stmts = malloc(sizeof(ASTNode*) * i);
            int count = 0;
            while (current < i && is_statement_start(arr[current])) {
                stmts[count++] = parse_statement();
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after func body");
            ASTNode *body = malloc(sizeof(ASTNode));
            body->type = AST_STATEMENTS;
            body->statements.stmts = stmts;
            body->statements.count = count;
            ASTNode *node = malloc(sizeof(ASTNode));
            node->type = AST_FUNC_DEF;
            node->funcdef.funcname = funcname;
            node->funcdef.params = params;
            node->funcdef.param_count = param_count;
            node->funcdef.body = body;
            return node;
        }
        error(line, "Expected identifier after func keyword");
        return NULL;
    } else if (strcmp(arr[current], "RETURN") == 0) {
        current++;
        ASTNode *expr = NULL;
        if (current < i && strcmp(arr[current], "SEMICOLON") != 0 && strcmp(arr[current], "RBRACE") != 0) {
            expr = parse_expression();
        }
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_RETURN;
        node->retstmt.expr = expr;
        return node;
    } else if (strcmp(arr[current], "LET") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *varname = strdup(arr[current] + 11);
            current++;
            if (match("ASSIGN")) {
                ASTNode *value = parse_expression();
                ASTNode *node = malloc(sizeof(ASTNode));
                node->type = AST_LET;
                node->var.varname = varname;
                node->var.value = value;
                return node;
            }
        }
        error(line, "Expected identifier after let");
        return NULL;
    } else if (strcmp(arr[current], "PRINT") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after print");
        ASTNode *expr = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after print expression");
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_PRINT;
        node->print.expr = expr;
        return node;
    } else if (strcmp(arr[current], "WHILE") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after while");
        ASTNode *cond = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after while condition");
        if (!match("LBRACE")) error(line, "Expected '{' after while condition");
        ASTNode **stmts = malloc(sizeof(ASTNode*) * i);
        int count = 0;
        while (current < i && is_statement_start(arr[current])) {
            stmts[count++] = parse_statement();
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        }
        if (!match("RBRACE")) error(line, "Expected '}' after while body");
        ASTNode *body = malloc(sizeof(ASTNode));
        body->type = AST_STATEMENTS;
        body->statements.stmts = stmts;
        body->statements.count = count;
        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_WHILE;
        node->whilestmt.cond = cond;
        node->whilestmt.body = body;
        return node;
    } else if (strcmp(arr[current], "IF") == 0) {
        current++;
        if (!match("LPAREN")) error(line, "Expected '(' after if");
        ASTNode *cond = parse_expression();
        if (!match("RPAREN")) error(line, "Expected ')' after if condition");
        if (!match("LBRACE")) error(line, "Expected '{' after if condition");
        ASTNode **then_stmts = malloc(sizeof(ASTNode*) * i);
        int then_count = 0;
        while (current < i && is_statement_start(arr[current])) {
            then_stmts[then_count++] = parse_statement();
            if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        }
        if (!match("RBRACE")) error(line, "Expected '}' after if body");
        ASTNode *then_body = malloc(sizeof(ASTNode));
        then_body->type = AST_STATEMENTS;
        then_body->statements.stmts = then_stmts;
        then_body->statements.count = then_count;

        ASTNode *else_body = NULL;
        if (current < i && strcmp(arr[current], "ELSE") == 0) {
            current++;
            if (!match("LBRACE")) error(line, "Expected '{' after else");
            ASTNode **else_stmts = malloc(sizeof(ASTNode*) * i);
            int else_count = 0;
            while (current < i && is_statement_start(arr[current])) {
                else_stmts[else_count++] = parse_statement();
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after else body");
            else_body = malloc(sizeof(ASTNode));
            else_body->type = AST_STATEMENTS;
            else_body->statements.stmts = else_stmts;
            else_body->statements.count = else_count;
        }

        ASTNode *node = malloc(sizeof(ASTNode));
        node->type = AST_IF;
        node->ifstmt.condition = cond;
        node->ifstmt.then_branch = then_body;
        node->ifstmt.else_branch = else_body;
        return node;
    } else if (strcmp(arr[current], "FOR") == 0) {
        current++;
        if (current < i && strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
            char *varname = strdup(arr[current] + 11);
            current++;
            if (!match("IN")) error(line, "Expected 'in' after for variable");
            ASTNode *start = parse_expression();
            if (!match("DOT")) error(line, "Expected '.' in for range");
            ASTNode *end = parse_expression();
            if (!match("LBRACE")) error(line, "Expected '{' after for range");
            ASTNode **stmts = malloc(sizeof(ASTNode*) * i);
            int count = 0;
            while (current < i && is_statement_start(arr[current])) {
                stmts[count++] = parse_statement();
                if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
            }
            if (!match("RBRACE")) error(line, "Expected '}' after for body");
            ASTNode *body = malloc(sizeof(ASTNode));
            body->type = AST_STATEMENTS;
            body->statements.stmts = stmts;
            body->statements.count = count;
            ASTNode *node = malloc(sizeof(ASTNode));
            node->type = AST_FOR;
            node->forstmt.for_var = varname;
            node->forstmt.for_start = start;
            node->forstmt.for_end = end;
            node->forstmt.for_body = body;
            return node;
        }
        error(line, "Expected identifier after for");
        return NULL;
    } else if (strncmp(arr[current], "IDENTIFIER ", 11) == 0) {
        char *varname = strdup(arr[current] + 11);
        current++;
        if (current < i && strcmp(arr[current], "ASSIGN") == 0) {
            current++;
            ASTNode *value = parse_expression();
            ASTNode *node = malloc(sizeof(ASTNode));
            node->type = AST_ASSIGN;
            node->var.varname = varname;
            node->var.value = value;
            return node;
        } else if (current < i && strcmp(arr[current], "LPAREN") == 0) {
            // Function call as statement
            current--; // step back so parse_factor sees IDENTIFIER
            ASTNode *call = parse_factor();
            return call;
        } else {
            error(line, "Expected '=' or '(' after identifier");
            return NULL;
        }
    } else {
        ASTNode *expr = parse_expression();
        return expr;
    }
}

ASTNode *parse_statements() {
    ASTNode **stmts = malloc(sizeof(ASTNode*) * i);
    int count = 0;
    while (current < i) {
        if (!is_statement_start(arr[current])) break;
        ASTNode *stmt = parse_statement();
        if (stmt) stmts[count++] = stmt; // Only add non-NULL statements
        if (current < i && strcmp(arr[current], "SEMICOLON") == 0) current++;
        if (current >= i) break;
    }
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = AST_STATEMENTS;
    node->statements.stmts = stmts;
    node->statements.count = count;
    return node;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_STRING:
        case AST_IDENTIFIER:
            free(node->string);
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
        case AST_PRINT:
            free_ast(node->print.expr);
            break;
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
        default: break;
    }
    free(node);
}

void Parse(void) {
    current = 0;
    ASTNode *root = parse_statements();
    interpret(root);
    free_ast(root);
}
