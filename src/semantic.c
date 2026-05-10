#include "semantic.h"
#include "struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Scope *global_scope;
static Scope *current_scope;
static int loop_depth;
static int switch_depth;
static int func_depth;
static int error_count;

static void sem_error(const char *msg) {
    fprintf(stderr, "Error semántico: %s\n", msg);
    error_count++;
}

static Scope *scope_create(Scope *parent) {
    Scope *s = malloc(sizeof(Scope));
    s->parent = parent;
    s->symbols = NULL;
    return s;
}

static void scope_destroy(Scope *s) {
    Symbol *sym = s->symbols;
    while (sym) {
        Symbol *next = sym->next;
        free(sym);
        sym = next;
    }
    free(s);
}

static Symbol *scope_lookup(Scope *scope, const char *name) {
    for (; scope; scope = scope->parent) {
        for (Symbol *s = scope->symbols; s; s = s->next)
            if (!strcmp(s->name, name)) return s;
    }
    return NULL;
}

static Symbol *scope_lookup_current(Scope *scope, const char *name) {
    for (Symbol *s = scope->symbols; s; s = s->next)
        if (!strcmp(s->name, name)) return s;
    return NULL;
}

static Symbol *scope_add(Scope *scope, const char *name, SymKind kind, int type) {
    Symbol *s = malloc(sizeof(Symbol));
    strncpy(s->name, name, MAX_LEXEME - 1);
    s->name[MAX_LEXEME - 1] = '\0';
    s->kind = kind;
    s->type = type;
    s->param_count = 0;
    s->next = scope->symbols;
    scope->symbols = s;
    return s;
}

static void check_stmt_list(ASTNode *first);
static void check_expr(ASTNode *n);

static void check_stmt(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
    case AST_IF:
        check_expr(n->left);
        check_stmt_list(n->right->next);
        if (n->next && n->next->type == AST_ELSE)
            check_stmt_list(n->next->right->next);
        break;
    case AST_WHILE:
        check_expr(n->left);
        loop_depth++;
        check_stmt_list(n->right->next);
        loop_depth--;
        break;
    case AST_DOWHILE:
        loop_depth++;
        check_stmt_list(n->right->next);
        loop_depth--;
        check_expr(n->left);
        break;
    case AST_FOR:
        if (n->left) check_expr(n->left);
        if (n->right->left) check_expr(n->right->left);
        loop_depth++;
        check_stmt_list(n->right->next);
        loop_depth--;
        break;
    case AST_SWITCH:
        check_expr(n->left);
        switch_depth++;
        for (ASTNode *c = n->right; c; c = c->next)
            check_stmt_list(c->left);
        switch_depth--;
        break;
    case AST_BREAK:
        if (loop_depth == 0 && switch_depth == 0)
            sem_error("'break' fuera de un bucle o switch");
        break;
    case AST_CONTINUE:
        if (loop_depth == 0) sem_error("'continue' fuera de un bucle");
        break;
    case AST_RETURN:
        if (func_depth == 0) sem_error("'return' fuera de una función");
        check_expr(n->left);
        break;
    case AST_ARRAY_DECL: {
        Symbol *existing = scope_lookup_current(current_scope, n->var_name);
        if (existing) {
            char buf[128];
            snprintf(buf, sizeof(buf), "redeclaración del array '%s'", n->var_name);
            sem_error(buf);
        } else {
            scope_add(current_scope, n->var_name, SYM_VARIABLE, TYPE_LONG);
        }
        break;
    }
    case AST_ARRAY_TYPED: {
        Symbol *existing = scope_lookup_current(current_scope, n->var_name);
        if (existing) {
            char buf[128];
            snprintf(buf, sizeof(buf), "redeclaración del array '%s'", n->var_name);
            sem_error(buf);
        } else {
            scope_add(current_scope, n->var_name, SYM_VARIABLE, (int)(unsigned char)n->op);
        }
        break;
    }
    case AST_DECL: {
        Symbol *existing = scope_lookup_current(current_scope, n->var_name);
        if (existing) {
            char buf[128];
            snprintf(buf, sizeof(buf), "redeclaración de la variable '%s'", n->var_name);
            sem_error(buf);
        } else {
            scope_add(current_scope, n->var_name, SYM_VARIABLE, n->num_value);
        }
        if (n->left) check_expr(n->left);
        break;
    }
    default:
        check_expr(n);
        break;
    }
}

static void check_stmt_list(ASTNode *first) {
    while (first) {
        if (first->type == AST_IF && first->next && first->next->type == AST_ELSE) {
            check_stmt(first);
            first = first->next->next;
        } else if (first->type == AST_ELSE) {
            first = first->next;
        } else {
            check_stmt(first);
            first = first->next;
        }
    }
}

static void check_expr(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
    case AST_VARIABLE: {
        Symbol *sym = scope_lookup(current_scope, n->var_name);
        if (!sym) {
            char buf[128];
            snprintf(buf, sizeof(buf), "variable '%s' no declarada", n->var_name);
            sem_error(buf);
        }
        break;
    }
    case AST_ASSIGN: {
        ASTNode *target = n->left;
        if (target->type == AST_VARIABLE) {
            Symbol *sym = scope_lookup(current_scope, target->var_name);
            if (!sym) {
                scope_add(current_scope, target->var_name, SYM_VARIABLE, TYPE_INT);
            }
        } else if (target->type == AST_INDEX) {
            Symbol *sym = scope_lookup(current_scope, target->var_name);
            if (!sym) {
                char buf[128];
                snprintf(buf, sizeof(buf), "array '%s' no declarado", target->var_name);
                sem_error(buf);
            }
            check_expr(target->left);
        } else if (target->type == AST_DEREF) {
            check_expr(target->left);
        } else if (target->type == AST_MEMBER) {
            check_expr(target);
        }
        check_expr(n->right);
        break;
    }
    case AST_INDEX: {
        Symbol *sym = scope_lookup(current_scope, n->var_name);
        if (!sym) {
            char buf[128];
            snprintf(buf, sizeof(buf), "variable/array '%s' no declarado", n->var_name);
            sem_error(buf);
        }
        check_expr(n->left);
        break;
    }
    case AST_DEREF:
        check_expr(n->left);
        break;
    case AST_CAST:
        check_expr(n->left);
        break;
    case AST_MEMBER: {
        check_expr(n->left);
        int stype = TYPE_LONG;
        if (n->left->type == AST_VARIABLE) {
            Symbol *sym = scope_lookup(current_scope, n->left->var_name);
            if (sym) stype = sym->type;
        } else if (n->left->type == AST_MEMBER) {
            stype = n->left->op;
        }
        if (TYPE_IS_STRUCT(stype)) {
            int sid = TYPE_STRUCT_ID(stype);
            int midx = struct_member_index(sid, n->var_name);
            if (midx < 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "struct '%s' no tiene miembro '%s'",
                         struct_table[sid].name, n->var_name);
                sem_error(buf);
            } else {
                n->num_value = struct_table[sid].members[midx].offset;
                n->op = (char)struct_table[sid].members[midx].type;
            }
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "acceso a miembro en tipo no-struct");
            sem_error(buf);
        }
        break;
    }
    case AST_ADDR:
        check_expr(n->left);
        break;
    case AST_CALL: {
        Symbol *sym = scope_lookup(current_scope, n->var_name);
        if (!sym) {
            char buf[128];
            snprintf(buf, sizeof(buf), "función '%s' no declarada", n->var_name);
            sem_error(buf);
        } else if (sym->kind == SYM_VARIABLE || sym->kind == SYM_PARAMETER) {
            /* indirect call through variable/parameter — OK */
        } else if (sym->kind != SYM_FUNCTION) {
            char buf[128];
            snprintf(buf, sizeof(buf), "'%s' no es una función", n->var_name);
            sem_error(buf);
        } else {
            int arg_count = 0;
            for (ASTNode *a = n->left; a; a = a->next) arg_count++;
            if (arg_count != sym->param_count) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "función '%s' espera %d argumento(s), recibió %d",
                         n->var_name, sym->param_count, arg_count);
                sem_error(buf);
            }
        }
        for (ASTNode *a = n->left; a; a = a->next) check_expr(a);
        break;
    }
    case AST_CALL_INDIRECT:
        check_expr(n->left);
        for (ASTNode *a = n->right; a; a = a->next) check_expr(a);
        break;
    case AST_PRINT:
        check_expr(n->left);
        break;
    case AST_BINARY:
        check_expr(n->left);
        check_expr(n->right);
        break;
    case AST_UNARY:
        check_expr(n->left);
        break;
    case AST_TERNARY:
        check_expr(n->left);
        check_expr(n->right);
        check_expr(n->next);
        break;
    case AST_DECL: {
        Symbol *existing = scope_lookup_current(current_scope, n->var_name);
        if (existing) {
            char buf[128];
            snprintf(buf, sizeof(buf), "redeclaración de la variable '%s'", n->var_name);
            sem_error(buf);
        } else {
            scope_add(current_scope, n->var_name, SYM_VARIABLE, n->num_value);
        }
        if (n->left) check_expr(n->left);
        break;
    }
    case AST_RETURN:
        check_expr(n->left);
        break;
    case AST_NUMBER:
    case AST_STRING:
        break;
    default:
        break;
    }
}

static void check_func(ASTNode *func) {
    if (func->right == NULL) return; // forward decl or extern

    Symbol *func_sym = scope_lookup(global_scope, func->var_name);
    if (!func_sym) {
        sem_error("error interno: función no registrada");
        return;
    }

    current_scope = scope_create(global_scope);

    int pi = 0;
    for (ASTNode *p = func->left; p; p = p->next) {
        if (scope_lookup_current(current_scope, p->var_name)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "parámetro duplicado '%s'", p->var_name);
            sem_error(buf);
        } else {
            scope_add(current_scope, p->var_name, SYM_PARAMETER, TYPE_INT);
        }
        pi++;
    }

    func_depth++;
    check_stmt_list(func->right->next);
    func_depth--;

    scope_destroy(current_scope);
    current_scope = global_scope;
}

static void register_functions(ASTNode *program) {
    for (ASTNode *s = program->next; s; s = s->next) {
        if (s->type != AST_FUNC) continue;
        Symbol *existing = scope_lookup_current(global_scope, s->var_name);
        int pc = 0;
        for (ASTNode *p = s->left; p; p = p->next) pc++;

        if (existing) {
            if (s->right == NULL) continue; // forward decl after anything is OK
            // current is definition; check if a previous definition exists
            int prev_def = 0;
            for (ASTNode *prev = program->next; prev != s; prev = prev->next) {
                if (prev->type == AST_FUNC && !strcmp(prev->var_name, s->var_name)
                    && prev->right != NULL) { prev_def = 1; break; }
            }
            if (prev_def) {
                char buf[128];
                snprintf(buf, sizeof(buf), "redeclaración de la función '%s'", s->var_name);
                sem_error(buf);
            } else {
                existing->param_count = pc;
            }
        } else {
            Symbol *sym = scope_add(global_scope, s->var_name, SYM_FUNCTION, TYPE_INT);
            sym->param_count = pc;
        }
    }
}

void semantic_check(ASTNode *ast) {
    error_count = 0;
    loop_depth = 0;
    switch_depth = 0;
    func_depth = 0;

    global_scope = scope_create(NULL);
    current_scope = global_scope;

    Symbol *ps = scope_add(global_scope, "print", SYM_FUNCTION, TYPE_INT);
    ps->param_count = 1;

    int func_mode = 0;
    for (ASTNode *s = ast->next; s; s = s->next) {
        if (s->type == AST_FUNC) { func_mode = 1; break; }
    }

    if (func_mode) {
        register_functions(ast);
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC) {
                check_func(s);
            } else {
                check_stmt(s);
            }
        }
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC && s->right == NULL && s->num_value != 1) {
                int found = 0;
                for (ASTNode *d = ast->next; d; d = d->next) {
                    if (d->type == AST_FUNC && !strcmp(d->var_name, s->var_name)
                        && d->right != NULL) { found = 1; break; }
                }
                if (!found) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "función '%s' declarada pero no definida", s->var_name);
                    sem_error(buf);
                }
            }
        }
    } else {
        check_stmt_list(ast->next);
    }

    scope_destroy(global_scope);

    if (error_count > 0) {
        fprintf(stderr, "Se encontraron %d error(es) semántico(s)\n", error_count);
        exit(1);
    }
}
