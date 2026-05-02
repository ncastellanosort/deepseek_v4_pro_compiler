#ifndef COMPILER_H
#define COMPILER_H

#include <stddef.h>

/* ── Token ───────────────────────────────────────────────────────── */

typedef enum {
    TOK_EOF, TOK_IDENT, TOK_NUMBER, TOK_STRING,
    TOK_DEF, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_BREAK, TOK_CONTINUE,
    TOK_RETURN, TOK_PRINT, TOK_ARRAY,
    TOK_ASSIGN, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACK, TOK_RBRACK, TOK_SEMICOLON, TOK_COMMA,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_MOD,
    TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_NOT,
    TOK_LSHIFT, TOK_RSHIFT,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_LAND, TOK_LOR,
    TOK_ERROR
} TokenType;

#define MAX_LEXEME 64

typedef struct {
    TokenType type;
    char lexeme[MAX_LEXEME];
    int int_value;
    int line;
    int col;
} Token;

const char *token_type_name(TokenType type);

/* ── AST ─────────────────────────────────────────────────────────── */

typedef enum {
    AST_PROGRAM, AST_FUNC, AST_PARAM, AST_BLOCK,
    AST_IF, AST_ELSE, AST_WHILE, AST_FOR, AST_BREAK, AST_CONTINUE, AST_RETURN,
    AST_ASSIGN, AST_PRINT, AST_CALL,
    AST_BINARY, AST_UNARY,
    AST_NUMBER, AST_STRING, AST_VARIABLE,
    AST_INDEX, AST_DEREF, AST_ADDR,
    AST_ARRAY_DECL   /* array a[size];  → var_name=a, num_value=size */
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    int num_value;
    char var_name[MAX_LEXEME];
    char op;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *next;
} ASTNode;

#define OP_LSHIFT 'L'
#define OP_RSHIFT 'R'
#define OP_EQ     'E'
#define OP_NE     'N'
#define OP_LE     'e'
#define OP_GE     'g'
#define OP_LAND   'A'
#define OP_LOR    'O'
#define OP_NOT    '!'
#define OP_DEREF  'd'
#define OP_ADDR   'a'

ASTNode *ast_make_program(ASTNode *f);
ASTNode *ast_make_func(const char *n, ASTNode *p, ASTNode *b);
ASTNode *ast_make_param(const char *n);
ASTNode *ast_make_block(ASTNode *f);
ASTNode *ast_make_if(ASTNode *c, ASTNode *t);
ASTNode *ast_make_else(ASTNode *e);
ASTNode *ast_make_while(ASTNode *c, ASTNode *b);
ASTNode *ast_make_for(ASTNode *init, ASTNode *cond, ASTNode *step, ASTNode *body);
ASTNode *ast_make_break(void);
ASTNode *ast_make_continue(void);
ASTNode *ast_make_return(ASTNode *e);
ASTNode *ast_make_assign(ASTNode *t, ASTNode *v);
ASTNode *ast_make_print(ASTNode *e);
ASTNode *ast_make_call(const char *n, ASTNode *a);
ASTNode *ast_make_binary(char op, ASTNode *l, ASTNode *r);
ASTNode *ast_make_unary(char op, ASTNode *o);
ASTNode *ast_make_number(int v);
ASTNode *ast_make_string(const char *s);
ASTNode *ast_make_variable(const char *n);
ASTNode *ast_make_index(const char *arr, ASTNode *idx);
ASTNode *ast_make_deref(ASTNode *p);
ASTNode *ast_make_addr(ASTNode *lv);
ASTNode *ast_make_array_decl(const char *name, int size);
void ast_append_stmt(ASTNode *list, ASTNode *s);
void ast_free(ASTNode *n);
void ast_print(ASTNode *n, int ind);

#endif
