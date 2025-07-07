#ifndef PARSER_H
#define PARSER_H

typedef enum {
    AST_NUMBER,
    AST_STRING,
    AST_IDENTIFIER,
    AST_BINARY_OP,
    AST_LET,
    AST_ASSIGN,
    AST_PRINT,
    AST_WHILE,
    AST_IF,
    AST_STATEMENTS,
    AST_FUNC_DEF,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_IMPORT,
    AST_FOR
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        double number;
        char *string;

        struct {
            char op[4];
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;

        struct {
            char *varname;
            struct ASTNode *value;
        } var;

        struct {
            struct ASTNode *expr;
        } print;

        struct {
            struct ASTNode *cond;
            struct ASTNode *body;
        } whilestmt;

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

        struct {
            struct ASTNode *expr;
        } retstmt;

        struct {
            char *filename;
        } importstmt;

        struct {
            char *for_var;
            struct ASTNode *for_start;
            struct ASTNode *for_end;
            struct ASTNode *for_body;
        } forstmt;
    };
} ASTNode;

void Parse(void);
extern int current;
ASTNode *parse_statements();
void free_ast(ASTNode *node);

#endif // PARSER_H
