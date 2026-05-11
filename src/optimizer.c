#include "optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────────────────── */

static int eval_binary(char op, int a, int b, int *result) {
    switch (op) {
        case '+': *result = a + b; return 1;
        case '-': *result = a - b; return 1;
        case '*': *result = a * b; return 1;
        case '/': if (b == 0) return 0; *result = a / b; return 1;
        case '%': if (b == 0) return 0; *result = a % b; return 1;
        case '&': *result = a & b; return 1;
        case '|': *result = a | b; return 1;
        case '^': *result = a ^ b; return 1;
        case OP_LSHIFT: *result = a << b; return 1;
        case OP_RSHIFT: *result = a >> b; return 1;
        case OP_EQ:  *result = (a == b) ? 1 : 0; return 1;
        case OP_NE:  *result = (a != b) ? 1 : 0; return 1;
        case '<':  *result = (a < b) ? 1 : 0; return 1;
        case '>':  *result = (a > b) ? 1 : 0; return 1;
        case OP_LE: *result = (a <= b) ? 1 : 0; return 1;
        case OP_GE: *result = (a >= b) ? 1 : 0; return 1;
        case OP_LAND: *result = (a && b) ? 1 : 0; return 1;
        case OP_LOR:  *result = (a || b) ? 1 : 0; return 1;
        default: return 0;
    }
}

static void node_overwrite(ASTNode *dst, ASTNode *src) {
    dst->type = src->type; dst->num_value = src->num_value;
    strncpy(dst->var_name, src->var_name, MAX_LEXEME - 1);
    dst->var_name[MAX_LEXEME - 1] = '\0';
    dst->op = src->op;
    dst->left = src->left; dst->right = src->right; dst->next = src->next;
    free(src);
}

static void node_set_number(ASTNode *n, int value) {
    ast_free(n->left); n->left = NULL;
    ast_free(n->right); n->right = NULL;
    ast_free(n->next); n->next = NULL;
    n->type = AST_NUMBER;
    n->num_value = value;
}

/* ── forward declarations ────────────────────────────────────────── */

static int fold_node(ASTNode *n);
static int fold_in_block(ASTNode *block);
static int copy_prop_block(ASTNode *block);
static int eliminate_dead_block(ASTNode *block);

/* ── 1. constant folding ─────────────────────────────────────────── */

static int fold_node(ASTNode *n) {
    if (!n) return 0;
    int c = 0;

    switch (n->type) {
    case AST_BINARY:
        c |= fold_node(n->left);
        c |= fold_node(n->right);
        if (n->left && n->left->type == AST_NUMBER &&
            n->right && n->right->type == AST_NUMBER) {
            int r;
            if (eval_binary(n->op, n->left->num_value, n->right->num_value, &r)) {
                node_set_number(n, r); c = 1;
            }
        }
        break;
    case AST_UNARY:
        c |= fold_node(n->left);
        if (n->op == 'm' && n->left && n->left->type == AST_NUMBER) {
            node_set_number(n, -n->left->num_value); c = 1;
        } else if (n->op == OP_NOT && n->left && n->left->type == AST_NUMBER) {
            node_set_number(n, n->left->num_value ? 0 : 1); c = 1;
        }
        break;
    case AST_TERNARY:
        c |= fold_node(n->left);
        c |= fold_node(n->right);
        c |= fold_node(n->next);
        if (n->left && n->left->type == AST_NUMBER) {
            ASTNode *keep = n->left->num_value ? n->right : n->next;
            ASTNode *drop = n->left->num_value ? n->next : n->right;
            ast_free(n->left); ast_free(drop);
            ASTNode *cpy = ast_clone(keep);
            node_overwrite(n, cpy); c = 1;
        }
        break;
    case AST_CAST:
        c |= fold_node(n->left);
        if (n->left && n->left->type == AST_NUMBER) {
            int v = n->left->num_value;
            switch (n->num_value) {
                case TYPE_CHAR: v = (char)v; break;
                case TYPE_SHORT: v = (short)v; break;
                case TYPE_INT: v = (int)v; break;
                default: break;
            }
            node_set_number(n, v); c = 1;
        }
        break;
    case AST_RETURN:
    case AST_PRINT:
    case AST_DEREF:
    case AST_ADDR:
        c |= fold_node(n->left);
        break;
    case AST_ASSIGN:
    case AST_INDEX:
    case AST_MEMBER:
        c |= fold_node(n->left);
        c |= fold_node(n->right);
        break;
    case AST_IF:
        c |= fold_node(n->left);
        c |= fold_in_block(n->right);
        if (n->next && n->next->type == AST_ELSE)
            c |= fold_in_block(n->next->right);
        break;
    case AST_WHILE:
    case AST_DOWHILE:
        c |= fold_node(n->left);
        c |= fold_in_block(n->right);
        break;
    case AST_FOR:
        c |= fold_node(n->left);
        if (n->right) {
            c |= fold_node(n->right->left);
            c |= fold_in_block(n->right);
        }
        break;
    case AST_SWITCH:
        c |= fold_node(n->left);
        for (ASTNode *cs = n->right; cs; cs = cs->next)
            c |= fold_in_block(cs);
        break;
    case AST_CALL:
        for (ASTNode *a = n->left; a; a = a->next)
            c |= fold_node(a);
        break;
    case AST_CALL_INDIRECT:
        c |= fold_node(n->left);
        for (ASTNode *a = n->right; a; a = a->next)
            c |= fold_node(a);
        break;
    default:
        break;
    }
    return c;
}

static int fold_in_block(ASTNode *block) {
    if (!block) return 0;
    int c = 0;
    for (ASTNode *s = block->next; s; s = s->next)
        c |= fold_node(s);
    return c;
}

/* ── 2. copy propagation ─────────────────────────────────────────── */

#define CP_MAX 64
static struct { char name[MAX_LEXEME]; ASTNode *val; } cp_map[CP_MAX];
static int cp_cnt;

static void cp_set(const char *n, ASTNode *v) {
    for (int i = 0; i < cp_cnt; i++) {
        if (!strcmp(cp_map[i].name, n)) { ast_free(cp_map[i].val); cp_map[i].val = v; return; }
    }
    if (cp_cnt < CP_MAX) {
        strncpy(cp_map[cp_cnt].name, n, MAX_LEXEME - 1);
        cp_map[cp_cnt].name[MAX_LEXEME - 1] = '\0';
        cp_map[cp_cnt].val = v; cp_cnt++;
    }
}

static ASTNode *cp_get(const char *n) {
    for (int i = 0; i < cp_cnt; i++)
        if (!strcmp(cp_map[i].name, n)) return cp_map[i].val;
    return NULL;
}

static void cp_kill(const char *n) {
    for (int i = 0; i < cp_cnt; i++) {
        if (!strcmp(cp_map[i].name, n)) {
            ast_free(cp_map[i].val);
            for (int j = i; j < cp_cnt - 1; j++) cp_map[j] = cp_map[j + 1];
            cp_cnt--; return;
        }
    }
}

static void cp_clear(void) {
    for (int i = 0; i < cp_cnt; i++) ast_free(cp_map[i].val);
    cp_cnt = 0;
}

static int cp_expr(ASTNode *n) {
    if (!n) return 0;
    int c = 0;
    switch (n->type) {
    case AST_VARIABLE: {
        ASTNode *v = cp_get(n->var_name);
        if (v) { ASTNode *cpy = ast_clone(v); node_overwrite(n, cpy); c = 1; }
        break;
    }
    case AST_BINARY: c |= cp_expr(n->left); c |= cp_expr(n->right); break;
    case AST_UNARY: case AST_CAST: case AST_DEREF: case AST_ADDR:
        c |= cp_expr(n->left); break;
    case AST_TERNARY:
        c |= cp_expr(n->left); c |= cp_expr(n->right); c |= cp_expr(n->next); break;
    case AST_INDEX: case AST_MEMBER:
        c |= cp_expr(n->left); break;
    default: break;
    }
    return c;
}

static int is_ctrl(ASTNode *n) {
    return n && (n->type == AST_IF || n->type == AST_WHILE || n->type == AST_FOR ||
                 n->type == AST_DOWHILE || n->type == AST_SWITCH ||
                 n->type == AST_CALL || n->type == AST_CALL_INDIRECT);
}

static int cp_stmt(ASTNode *s) {
    if (!s) return 0;
    int c = 0;

    if (is_ctrl(s)) { cp_clear(); }

    switch (s->type) {
    case AST_ASSIGN:
        c |= cp_expr(s->right);
        if (s->left && s->left->type == AST_VARIABLE) {
            if (s->right->type == AST_NUMBER)
                cp_set(s->left->var_name, ast_clone(s->right));
            else if (s->right->type == AST_VARIABLE && cp_get(s->right->var_name))
                cp_set(s->left->var_name, ast_clone(cp_get(s->right->var_name)));
            else
                cp_kill(s->left->var_name);
        }
        break;
    case AST_RETURN: case AST_PRINT:
        c |= cp_expr(s->left); break;
    case AST_IF:
        c |= cp_expr(s->left);
        c |= copy_prop_block(s->right);
        if (s->next && s->next->type == AST_ELSE) c |= copy_prop_block(s->next->right);
        break;
    case AST_WHILE: case AST_DOWHILE:
        c |= cp_expr(s->left);
        c |= copy_prop_block(s->right);
        break;
    case AST_FOR:
        c |= cp_expr(s->left);
        if (s->right) { c |= cp_expr(s->right->left); c |= copy_prop_block(s->right); }
        break;
    case AST_SWITCH:
        c |= cp_expr(s->left);
        for (ASTNode *cs = s->right; cs; cs = cs->next) c |= copy_prop_block(cs);
        break;
    default: break;
    }
    return c;
}

static int copy_prop_block(ASTNode *block) {
    if (!block) return 0;
    int c = 0;
    for (ASTNode *s = block->next; s; s = s->next)
        c |= cp_stmt(s);
    return c;
}

/* ── 3. dead code elimination ────────────────────────────────────── */

static void free_stmt_chain(ASTNode *s) {
    while (s) { ASTNode *nx = s->next; s->next = NULL; ast_free(s); s = nx; }
}

/* Replace node at *pos with the unwrapped contents of block_node.
 * block_node must be a BLOCK. Returns the new last statement in the chain. */
static ASTNode *splice_block(ASTNode **pos, ASTNode *block_node) {
    ASTNode *first = block_node->next;
    block_node->next = NULL; ast_free(block_node);
    if (!first) { *pos = NULL; return NULL; }
    ASTNode *last = first;
    while (last->next) last = last->next;
    last->next = (*pos)->next;
    (*pos)->next = NULL;
    ast_free(*pos);
    *pos = first;
    return last;
}

static int eliminate_dead_block(ASTNode *block) {
    if (!block) return 0;
    int c = 0;
    ASTNode **prev = &block->next;

    while (*prev) {
        ASTNode *s = *prev;

        /* Remove code after return/break/continue */
        if (s->type == AST_RETURN || s->type == AST_BREAK || s->type == AST_CONTINUE) {
            if (s->next) { free_stmt_chain(s->next); s->next = NULL; c = 1; }
        }

        /* if (0) → remove / if (1) → unwrap */
        if (s->type == AST_IF && s->left && s->left->type == AST_NUMBER) {
            c = 1;
            int cond = s->left->num_value;
            ASTNode *else_n = (s->next && s->next->type == AST_ELSE) ? s->next : NULL;

            if (cond != 0) {
                /* keep then, discard else */
                ASTNode *then_blk = s->right;
                s->right = NULL;
                if (else_n) {
                    s->next = else_n->next;
                    else_n->next = NULL; else_n->right = NULL;
                    ast_free(else_n);
                }
                splice_block(prev, then_blk);
                /* prev stays — next iteration processes *prev naturally */
                continue;
            } else {
                /* cond == 0: keep else, discard then */
                if (else_n) {
                    ASTNode *else_blk = else_n->right;
                    ASTNode *after = else_n->next;
                    else_n->right = NULL; else_n->next = NULL;
                    s->next = NULL;
                    ast_free(s); ast_free(else_n);
                    if (else_blk && else_blk->next) {
                        *prev = else_blk->next;
                        else_blk->next = NULL;  /* detach before free */
                        ast_free(else_blk);
                        /* now link the extracted statements to the rest */
                        ASTNode *tail = *prev;
                        while (tail->next) tail = tail->next;
                        tail->next = after;
                        /* prev stays — next iteration processes *prev naturally */
                    } else {
                        ast_free(else_blk);
                        *prev = after;
                    }
                    continue;
                }
                /* no else: remove if */
                { ASTNode *nx = s->next; s->next = NULL; ast_free(s); *prev = nx; }
                continue;
            }
        }

        /* while(0) → remove */
        if (s->type == AST_WHILE && s->left && s->left->type == AST_NUMBER && s->left->num_value == 0) {
            c = 1;
            ASTNode *nx = s->next;
            s->next = NULL; ast_free(s);
            *prev = nx;
            continue;
        }

        /* recurse */
        if (s->type == AST_IF) {
            c |= eliminate_dead_block(s->right);
            if (s->next && s->next->type == AST_ELSE)
                c |= eliminate_dead_block(s->next->right);
        } else if (s->type == AST_WHILE || s->type == AST_DOWHILE) {
            c |= eliminate_dead_block(s->right);
        } else if (s->type == AST_FOR && s->right) {
            c |= eliminate_dead_block(s->right);
        } else if (s->type == AST_SWITCH) {
            for (ASTNode *cs = s->right; cs; cs = cs->next)
                c |= eliminate_dead_block(cs);
        }

        prev = &(*prev)->next;
    }
    return c;
}

/* ── 4. function inlining ────────────────────────────────────────── */

static int inline_in_expr(ASTNode **pn, ASTNode *prog);
static int inline_in_block(ASTNode *block, ASTNode *prog);

/* Check if expression tree contains a call to func_name (recursion guard) */
static int contains_recursive_call(ASTNode *n, const char *fname) {
    if (!n) return 0;
    if (n->type == AST_CALL && !strcmp(n->var_name, fname)) return 1;
    if (contains_recursive_call(n->left, fname)) return 1;
    if (contains_recursive_call(n->right, fname)) return 1;
    if (contains_recursive_call(n->next, fname)) return 1;
    return 0;
}

static ASTNode *subst_tree(ASTNode *tree, ASTNode *params, ASTNode *args) {
    if (!tree) return NULL;

    if (tree->type == AST_VARIABLE) {
        ASTNode *p = params, *a = args;
        while (p && a) {
            if (!strcmp(tree->var_name, p->var_name))
                return ast_clone(a);
            p = p->next; a = a->next;
        }
        return ast_clone(tree);
    }

    ASTNode *c = calloc(1, sizeof(ASTNode));
    if (!c) { fprintf(stderr, "Error: sin memoria\n"); exit(1); }
    c->type = tree->type;
    c->num_value = tree->num_value;
    strncpy(c->var_name, tree->var_name, MAX_LEXEME - 1);
    c->var_name[MAX_LEXEME - 1] = '\0';
    c->op = tree->op;
    c->left = subst_tree(tree->left, params, args);
    c->right = subst_tree(tree->right, params, args);
    c->next = subst_tree(tree->next, params, args);
    return c;
}

static int is_inlineable(ASTNode *func) {
    if (func->num_value == 1) return 0; /* extern */
    if (!func->right || !func->right->next) return 0;
    ASTNode *first = func->right->next;
    if (!first || first->next || first->type != AST_RETURN || !first->left) return 0;
    int pc = 0;
    for (ASTNode *p = func->left; p; p = p->next) pc++;
    if (pc > 3) return 0;
    if (contains_recursive_call(first->left, func->var_name)) return 0;
    return 1;
}

static int inline_in_stmt(ASTNode *s, ASTNode *prog) {
    if (!s) return 0;
    int c = 0;
    switch (s->type) {
    case AST_ASSIGN: c |= inline_in_expr(&s->right, prog); break;
    case AST_RETURN: case AST_PRINT: c |= inline_in_expr(&s->left, prog); break;
    case AST_IF:
        c |= inline_in_expr(&s->left, prog);
        c |= inline_in_block(s->right, prog);
        if (s->next && s->next->type == AST_ELSE)
            c |= inline_in_block(s->next->right, prog);
        break;
    case AST_WHILE: case AST_DOWHILE:
        c |= inline_in_expr(&s->left, prog);
        c |= inline_in_block(s->right, prog);
        break;
    case AST_FOR:
        if (s->left) c |= inline_in_expr(&s->left, prog);
        if (s->right) { c |= inline_in_expr(&s->right->left, prog); c |= inline_in_block(s->right, prog); }
        break;
    case AST_SWITCH:
        c |= inline_in_expr(&s->left, prog);
        for (ASTNode *cs = s->right; cs; cs = cs->next) c |= inline_in_block(cs, prog);
        break;
    default: break;
    }
    return c;
}

static int inline_in_block(ASTNode *block, ASTNode *prog) {
    if (!block) return 0;
    int c = 0;
    for (ASTNode *s = block->next; s; s = s->next)
        c |= inline_in_stmt(s, prog);
    return c;
}

static int inline_in_expr(ASTNode **pn, ASTNode *prog) {
    ASTNode *n = *pn;
    if (!n) return 0;
    int c = 0;

    if (n->type == AST_CALL) {
        ASTNode *func = NULL;
        for (ASTNode *s = prog->next; s; s = s->next) {
            if (s->type == AST_FUNC && !strcmp(s->var_name, n->var_name)) { func = s; break; }
        }
        if (func && is_inlineable(func)) {
            c = 1;
            ASTNode *ret_expr = func->right->next->left;
            ASTNode *inlined = subst_tree(ret_expr, func->left, n->left);
            n->left = NULL;
            ast_free(n);
            *pn = inlined;
            return 1;
        }
    }

    c |= inline_in_expr(&n->left, prog);
    c |= inline_in_expr(&n->right, prog);
    for (ASTNode *a = n->next; a; a = a->next)
        c |= inline_in_expr(&a, prog);
    return c;
}

/* ── public API ──────────────────────────────────────────────────── */

void optimize(ASTNode *ast) {
    if (!ast) return;
    cp_cnt = 0;

    for (int iter = 0; iter < 10; iter++) {
        int changed = 0;

        /* Constant folding */
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC && s->right)
                changed |= fold_in_block(s->right);
            else
                changed |= fold_node(s);
        }

        /* Copy propagation */
        cp_clear();
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC && s->right) {
                cp_clear();
                changed |= copy_prop_block(s->right);
            }
        }

        /* Dead code elimination */
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC && s->right)
                changed |= eliminate_dead_block(s->right);
        }

        /* Inlining */
        for (ASTNode *s = ast->next; s; s = s->next) {
            if (s->type == AST_FUNC && s->right)
                changed |= inline_in_block(s->right, ast);
        }

        if (!changed) break;
    }

    cp_clear();
}
