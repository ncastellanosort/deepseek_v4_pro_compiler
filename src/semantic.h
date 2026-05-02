#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "compiler.h"

typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_PARAMETER
} SymKind;

typedef struct Symbol {
    char name[MAX_LEXEME];
    SymKind kind;
    int type;
    int param_count;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    struct Scope *parent;
    Symbol *symbols;
} Scope;

void semantic_check(ASTNode *ast);

#endif
