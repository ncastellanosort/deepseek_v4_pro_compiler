#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token current;

static void advance(void) { current = next_token(); }
static int check(TokenType t) { return current.type == t; }
static int match(TokenType t) { if (check(t)) { advance(); return 1; } return 0; }

static void expect(TokenType t) {
    if (!match(t)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "esperaba '%s', encontró '%s'",
                 token_type_name(t), token_type_name(current.type));
        lexer_error(buf, current.line, current.col);
        exit(1);
    }
}

/* fwd */
static ASTNode *parse_lor(void);
static ASTNode *parse_land(void);
static ASTNode *parse_bitor(void);
static ASTNode *parse_bitxor(void);
static ASTNode *parse_bitand(void);
static ASTNode *parse_eq(void);
static ASTNode *parse_cmp(void);
static ASTNode *parse_shift(void);
static ASTNode *parse_addsub(void);
static ASTNode *parse_muldivmod(void);
static ASTNode *parse_unary(void);
static ASTNode *parse_factor(void);
static ASTNode *parse_block(void);
static ASTNode *parse_stmt(void);

/* ── program ────────────────────────────────────────────────────── */

ASTNode *parse_program(void) {
    advance();
    ASTNode *program = ast_make_program(NULL);
    if (check(TOK_DEF)) {
        while (!check(TOK_EOF)) {
            advance();
            if (!check(TOK_IDENT)) { lexer_error("nombre función", current.line, current.col); exit(1); }
            char fn[MAX_LEXEME]; strncpy(fn, current.lexeme, MAX_LEXEME-1); fn[MAX_LEXEME-1]='\0';
            advance(); expect(TOK_LPAREN);
            ASTNode *params=NULL, *pt=NULL;
            if (!check(TOK_RPAREN)) {
                do { if(match(TOK_COMMA)){}
                    if (!check(TOK_IDENT)) { lexer_error("parámetro",current.line,current.col);exit(1);}
                    ASTNode *p=ast_make_param(current.lexeme); advance();
                    if(!params) params=pt=p; else {pt->next=p; pt=p;}
                } while(match(TOK_COMMA));
            }
            expect(TOK_RPAREN);
            ast_append_stmt(program, ast_make_func(fn, params, parse_block()));
        }
        return program;
    }
    while (!check(TOK_EOF)) ast_append_stmt(program, parse_stmt());
    return program;
}

/* ── block ──────────────────────────────────────────────────────── */

static ASTNode *parse_block(void) {
    expect(TOK_LBRACE);
    ASTNode *b = ast_make_block(NULL);
    while (!check(TOK_RBRACE) && !check(TOK_EOF)) ast_append_stmt(b, parse_stmt());
    expect(TOK_RBRACE);
    return b;
}

/* ── target ─────────────────────────────────────────────────────── */

static ASTNode *parse_target(void) {
    if (!check(TOK_IDENT)) { lexer_error("identificador", current.line, current.col); exit(1); }
    char name[MAX_LEXEME]; strncpy(name, current.lexeme, MAX_LEXEME-1); name[MAX_LEXEME-1]='\0';
    advance();
    if (check(TOK_LBRACK)) { advance(); ASTNode *idx=parse_lor(); expect(TOK_RBRACK); return ast_make_index(name,idx); }
    return ast_make_variable(name);
}

/* ── stmt ───────────────────────────────────────────────────────── */

static ASTNode *parse_stmt(void) {
    if (check(TOK_IF)) {
        advance(); expect(TOK_LPAREN);
        ASTNode *c=parse_lor(); expect(TOK_RPAREN);
        ASTNode *n=ast_make_if(c, parse_block());
        if (match(TOK_ELSE)) n->next = ast_make_else(parse_block());
        return n;
    }
    if (check(TOK_WHILE)) {
        advance(); expect(TOK_LPAREN);
        ASTNode *c=parse_lor(); expect(TOK_RPAREN);
        return ast_make_while(c, parse_block());
    }
    if (check(TOK_FOR)) {
        advance(); expect(TOK_LPAREN);
        /* init */
        ASTNode *init = NULL;
        if (!check(TOK_SEMICOLON)) init = parse_stmt();
        else advance();
        /* cond */
        ASTNode *cond = NULL;
        if (!check(TOK_SEMICOLON)) { cond = parse_lor(); }
        advance(); /* skip ; */
        /* step */
        ASTNode *step = NULL;
        if (!check(TOK_RPAREN)) {
            if (check(TOK_IDENT)) {
                ASTNode *tg = parse_target();
                if (check(TOK_ASSIGN)) {
                    advance();
                    step = ast_make_assign(tg, parse_lor());
                } else {
                    step = tg;
                }
            } else if (check(TOK_STAR)) {
                advance();
                ASTNode *ptr = parse_unary();
                expect(TOK_ASSIGN);
                step = ast_make_assign(ast_make_deref(ptr), parse_lor());
            } else {
                step = parse_lor();
            }
        }
        expect(TOK_RPAREN);
        ASTNode *body = parse_block();
        return ast_make_for(init, cond, step, body);
    }
    if (check(TOK_ARRAY)) {
        advance();
        if (!check(TOK_IDENT)) { lexer_error("nombre de array", current.line, current.col); exit(1); }
        char an[MAX_LEXEME]; strncpy(an, current.lexeme, MAX_LEXEME-1); an[MAX_LEXEME-1]='\0';
        advance(); expect(TOK_LBRACK);
        if (!check(TOK_NUMBER)) { lexer_error("tamaño del array", current.line, current.col); exit(1); }
        int sz = current.int_value; advance();
        expect(TOK_RBRACK); expect(TOK_SEMICOLON);
        return ast_make_array_decl(an, sz);
    }
    if (match(TOK_BREAK))    { expect(TOK_SEMICOLON); return ast_make_break(); }
    if (match(TOK_CONTINUE)) { expect(TOK_SEMICOLON); return ast_make_continue(); }
    if (check(TOK_RETURN))   { advance(); ASTNode *e=parse_lor(); expect(TOK_SEMICOLON); return ast_make_return(e); }
    if (check(TOK_PRINT))    { advance(); expect(TOK_LPAREN); ASTNode *e=parse_lor(); expect(TOK_RPAREN); expect(TOK_SEMICOLON); return ast_make_print(e); }
    if (check(TOK_STAR))     { advance(); ASTNode *p=parse_unary(); expect(TOK_ASSIGN); ASTNode *v=parse_lor(); expect(TOK_SEMICOLON); return ast_make_assign(ast_make_deref(p), v); }
    if (check(TOK_IDENT))    {
        ASTNode *tgt = parse_target();
        if (check(TOK_LPAREN)) { advance();
            ASTNode *args=NULL,*at=NULL;
            if (!check(TOK_RPAREN)) { do { if(match(TOK_COMMA)){} ASTNode *a=parse_lor(); if(!args){args=at=a;}else{at->next=a;at=a;} } while(match(TOK_COMMA)); }
            expect(TOK_RPAREN); expect(TOK_SEMICOLON);
            return ast_make_call(tgt->var_name, args);
        }
        expect(TOK_ASSIGN); ASTNode *v=parse_lor(); expect(TOK_SEMICOLON);
        return ast_make_assign(tgt, v);
    }
    char buf[100]; snprintf(buf, sizeof(buf), "sentencia inesperada '%s'", token_type_name(current.type));
    lexer_error(buf, current.line, current.col);
    exit(1);
}

/* ── expression parsers ─────────────────────────────────────────── */

static ASTNode *parse_lor(void) {
    ASTNode *l=parse_land(); while(check(TOK_LOR)){advance();l=ast_make_binary(OP_LOR,l,parse_land());} return l;
}
static ASTNode *parse_land(void) {
    ASTNode *l=parse_bitor(); while(check(TOK_LAND)){advance();l=ast_make_binary(OP_LAND,l,parse_bitor());} return l;
}
static ASTNode *parse_bitor(void) {
    ASTNode *l=parse_bitxor(); while(check(TOK_BITOR)){advance();l=ast_make_binary('|',l,parse_bitxor());} return l;
}
static ASTNode *parse_bitxor(void) {
    ASTNode *l=parse_bitand(); while(check(TOK_BITXOR)){advance();l=ast_make_binary('^',l,parse_bitand());} return l;
}
static ASTNode *parse_bitand(void) {
    ASTNode *l=parse_eq(); while(check(TOK_BITAND)){advance();l=ast_make_binary('&',l,parse_eq());} return l;
}
static ASTNode *parse_eq(void) {
    ASTNode *l=parse_cmp(); while(check(TOK_EQ)||check(TOK_NE)){char op=(current.type==TOK_EQ)?OP_EQ:OP_NE;advance();l=ast_make_binary(op,l,parse_cmp());} return l;
}
static ASTNode *parse_cmp(void) {
    ASTNode *l=parse_shift(); while(check(TOK_LT)||check(TOK_GT)||check(TOK_LE)||check(TOK_GE)){char op;switch(current.type){case TOK_LT:op='<';break;case TOK_GT:op='>';break;case TOK_LE:op=OP_LE;break;case TOK_GE:op=OP_GE;break;default:op='?';break;}advance();l=ast_make_binary(op,l,parse_shift());} return l;
}
static ASTNode *parse_shift(void) {
    ASTNode *l=parse_addsub(); while(check(TOK_LSHIFT)||check(TOK_RSHIFT)){char op=(current.type==TOK_LSHIFT)?OP_LSHIFT:OP_RSHIFT;advance();l=ast_make_binary(op,l,parse_addsub());} return l;
}
static ASTNode *parse_addsub(void) {
    ASTNode *l=parse_muldivmod(); while(check(TOK_PLUS)||check(TOK_MINUS)){char op=(current.type==TOK_PLUS)?'+':'-';advance();l=ast_make_binary(op,l,parse_muldivmod());} return l;
}
static ASTNode *parse_muldivmod(void) {
    ASTNode *l=parse_unary(); while(check(TOK_STAR)||check(TOK_SLASH)||check(TOK_MOD)){char op=(current.type==TOK_STAR)?'*':(current.type==TOK_SLASH)?'/':'%';advance();l=ast_make_binary(op,l,parse_unary());} return l;
}
static ASTNode *parse_unary(void) {
    if(check(TOK_MINUS)){advance();return ast_make_unary('m',parse_unary());}
    if(check(TOK_NOT)){advance();return ast_make_unary(OP_NOT,parse_unary());}
    if(check(TOK_STAR)){advance();return ast_make_unary(OP_DEREF,parse_unary());}
    if(check(TOK_BITAND)){advance();return ast_make_unary(OP_ADDR,parse_unary());}
    return parse_factor();
}
static ASTNode *parse_factor(void) {
    if(check(TOK_NUMBER)){int v=current.int_value;advance();return ast_make_number(v);}
    if(check(TOK_STRING)){ASTNode *n=ast_make_string(current.lexeme);advance();return n;}
    if(check(TOK_IDENT)){
        ASTNode *tgt=parse_target();
        if(check(TOK_LPAREN)){advance();
            ASTNode *args=NULL,*at=NULL;
            if(!check(TOK_RPAREN)){do{if(match(TOK_COMMA)){}ASTNode *a=parse_lor();if(!args){args=at=a;}else{at->next=a;at=a;}}while(match(TOK_COMMA));}
            expect(TOK_RPAREN); return ast_make_call(tgt->var_name,args);
        }
        return tgt;
    }
    if(match(TOK_LPAREN)){ASTNode *n=parse_lor();expect(TOK_RPAREN);return n;}
    lexer_error("esperaba número, id, string o '('", current.line, current.col); exit(1);
}
