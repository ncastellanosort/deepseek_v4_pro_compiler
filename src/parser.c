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

static int is_compound_assign(TokenType t) {
    return t == TOK_PLUS_ASSIGN || t == TOK_MINUS_ASSIGN || t == TOK_STAR_ASSIGN ||
           t == TOK_SLASH_ASSIGN || t == TOK_MOD_ASSIGN || t == TOK_AND_ASSIGN ||
           t == TOK_OR_ASSIGN || t == TOK_XOR_ASSIGN || t == TOK_LS_ASSIGN || t == TOK_RS_ASSIGN;
}

static char compound_op(TokenType t) {
    switch(t) {
        case TOK_PLUS_ASSIGN: return '+';
        case TOK_MINUS_ASSIGN: return '-';
        case TOK_STAR_ASSIGN: return '*';
        case TOK_SLASH_ASSIGN: return '/';
        case TOK_MOD_ASSIGN: return '%';
        case TOK_AND_ASSIGN: return '&';
        case TOK_OR_ASSIGN: return '|';
        case TOK_XOR_ASSIGN: return '^';
        case TOK_LS_ASSIGN: return OP_LSHIFT;
        case TOK_RS_ASSIGN: return OP_RSHIFT;
        default: return '?';
    }
}

static int is_type_keyword(TokenType t) {
    return t == TOK_INT || t == TOK_CHAR || t == TOK_SHORT || t == TOK_LONG;
}

static int type_keyword_to_code(TokenType t) {
    switch(t) {
        case TOK_INT: return TYPE_INT;
        case TOK_CHAR: return TYPE_CHAR;
        case TOK_SHORT: return TYPE_SHORT;
        case TOK_LONG: return TYPE_LONG;
        default: return TYPE_INT;
    }
}

/* fwd */
static ASTNode *parse_ternary(void);
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
        ASTNode *c=parse_ternary(); expect(TOK_RPAREN);
        ASTNode *n=ast_make_if(c, parse_block());
        if (match(TOK_ELSE)) n->next = ast_make_else(parse_block());
        return n;
    }
    if (check(TOK_WHILE)) {
        advance(); expect(TOK_LPAREN);
        ASTNode *c=parse_ternary(); expect(TOK_RPAREN);
        return ast_make_while(c, parse_block());
    }
    if (check(TOK_DO)) {
        advance();
        ASTNode *body = parse_block();
        expect(TOK_WHILE);
        expect(TOK_LPAREN);
        ASTNode *cond = parse_ternary();
        expect(TOK_RPAREN);
        expect(TOK_SEMICOLON);
        return ast_make_dowhile(body, cond);
    }
    if (check(TOK_SWITCH)) {
        advance(); expect(TOK_LPAREN);
        ASTNode *expr = parse_ternary();
        expect(TOK_RPAREN);
        expect(TOK_LBRACE);
        ASTNode *cases = NULL, *last_case = NULL;
        while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
            if (check(TOK_CASE)) {
                advance();
                int val = 0;
                if (check(TOK_NUMBER) || check(TOK_CHAR_LITERAL)) {
                    val = current.int_value; advance();
                } else {
                    lexer_error("esperaba número o literal char después de 'case'", current.line, current.col);
                    exit(1);
                }
                expect(TOK_COLON);
                ASTNode *body = NULL, *body_last = NULL;
                while (!check(TOK_CASE) && !check(TOK_DEFAULT) && !check(TOK_RBRACE) && !check(TOK_EOF)) {
                    ASTNode *s = parse_stmt();
                    if (!body) body = body_last = s;
                    else { body_last->next = s; body_last = s; }
                }
                ASTNode *cn = ast_make_case(val, body);
                if (!cases) cases = last_case = cn;
                else { last_case->next = cn; last_case = cn; }
            } else if (check(TOK_DEFAULT)) {
                advance();
                expect(TOK_COLON);
                ASTNode *body = NULL, *body_last = NULL;
                while (!check(TOK_CASE) && !check(TOK_DEFAULT) && !check(TOK_RBRACE) && !check(TOK_EOF)) {
                    ASTNode *s = parse_stmt();
                    if (!body) body = body_last = s;
                    else { body_last->next = s; body_last = s; }
                }
                ASTNode *dn = ast_make_default(body);
                if (!cases) cases = last_case = dn;
                else { last_case->next = dn; last_case = dn; }
            } else {
                lexer_error("esperaba 'case' o 'default'", current.line, current.col);
                exit(1);
            }
        }
        expect(TOK_RBRACE);
        return ast_make_switch(expr, cases);
    }
    if (check(TOK_FOR)) {
        advance(); expect(TOK_LPAREN);
        /* init */
        ASTNode *init = NULL;
        if (!check(TOK_SEMICOLON)) init = parse_stmt();
        else advance();
        /* cond */
        ASTNode *cond = NULL;
        if (!check(TOK_SEMICOLON)) { cond = parse_ternary(); }
        advance(); /* skip ; */
        /* step */
        ASTNode *step = NULL;
        if (!check(TOK_RPAREN)) {
            if (check(TOK_IDENT)) {
                ASTNode *tg = parse_target();
                if (check(TOK_INC)) {
                    advance();
                    step = ast_make_assign(tg, ast_make_binary('+', ast_clone(tg), ast_make_number(1)));
                } else if (check(TOK_DEC)) {
                    advance();
                    step = ast_make_assign(tg, ast_make_binary('-', ast_clone(tg), ast_make_number(1)));
                } else if (check(TOK_ASSIGN)) {
                    advance();
                    step = ast_make_assign(tg, parse_ternary());
                } else {
                    step = tg;
                }
            } else if (check(TOK_INC)) {
                advance();
                if (!check(TOK_IDENT)) { lexer_error("identificador", current.line, current.col); exit(1); }
                ASTNode *v = ast_make_variable(current.lexeme); advance();
                step = ast_make_assign(v, ast_make_binary('+', ast_clone(v), ast_make_number(1)));
            } else if (check(TOK_DEC)) {
                advance();
                if (!check(TOK_IDENT)) { lexer_error("identificador", current.line, current.col); exit(1); }
                ASTNode *v = ast_make_variable(current.lexeme); advance();
                step = ast_make_assign(v, ast_make_binary('-', ast_clone(v), ast_make_number(1)));
            } else if (check(TOK_STAR)) {
                advance();
                ASTNode *ptr = parse_unary();
                expect(TOK_ASSIGN);
                step = ast_make_assign(ast_make_deref(ptr), parse_ternary());
            } else {
                step = parse_ternary();
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
    if (check(TOK_INC)) {
        advance();
        if (!check(TOK_IDENT)) { lexer_error("identificador", current.line, current.col); exit(1); }
        ASTNode *v = ast_make_variable(current.lexeme); advance();
        expect(TOK_SEMICOLON);
        return ast_make_assign(v, ast_make_binary('+', ast_clone(v), ast_make_number(1)));
    }
    if (check(TOK_DEC)) {
        advance();
        if (!check(TOK_IDENT)) { lexer_error("identificador", current.line, current.col); exit(1); }
        ASTNode *v = ast_make_variable(current.lexeme); advance();
        expect(TOK_SEMICOLON);
        return ast_make_assign(v, ast_make_binary('-', ast_clone(v), ast_make_number(1)));
    }
    if (check(TOK_RETURN))   { advance(); ASTNode *e=parse_ternary(); expect(TOK_SEMICOLON); return ast_make_return(e); }
    if (is_type_keyword(current.type)) {
        int tc = type_keyword_to_code(current.type); advance();
        if (!check(TOK_IDENT)) { lexer_error("nombre de variable", current.line, current.col); exit(1); }
        char name[MAX_LEXEME]; strncpy(name, current.lexeme, MAX_LEXEME-1); name[MAX_LEXEME-1]='\0';
        advance();
        ASTNode *init = NULL;
        if (match(TOK_ASSIGN)) init = parse_ternary();
        expect(TOK_SEMICOLON);
        return ast_make_decl(tc, name, init);
    }
    if (check(TOK_PRINT))    { advance(); expect(TOK_LPAREN); ASTNode *e=parse_ternary(); expect(TOK_RPAREN); expect(TOK_SEMICOLON); return ast_make_print(e); }
    if (check(TOK_STAR))     {
        advance(); ASTNode *p=parse_unary();
        if (is_compound_assign(current.type)) {
            TokenType op = current.type; advance();
            ASTNode *v = parse_ternary(); expect(TOK_SEMICOLON);
            ASTNode *deref = ast_make_deref(p);
            return ast_make_assign(deref, ast_make_binary(compound_op(op), ast_make_deref(ast_clone(p)), v));
        }
        expect(TOK_ASSIGN); ASTNode *v=parse_ternary(); expect(TOK_SEMICOLON);
        return ast_make_assign(ast_make_deref(p), v);
    }
    if (check(TOK_IDENT))    {
        ASTNode *tgt = parse_target();
        if (check(TOK_LPAREN)) { advance();
            ASTNode *args=NULL,*at=NULL;
            if (!check(TOK_RPAREN)) { do { if(match(TOK_COMMA)){} ASTNode *a=parse_ternary(); if(!args){args=at=a;}else{at->next=a;at=a;} } while(match(TOK_COMMA)); }
            expect(TOK_RPAREN); expect(TOK_SEMICOLON);
            return ast_make_call(tgt->var_name, args);
        }
        if (check(TOK_INC)) {
            advance(); expect(TOK_SEMICOLON);
            return ast_make_assign(tgt, ast_make_binary('+', ast_clone(tgt), ast_make_number(1)));
        }
        if (check(TOK_DEC)) {
            advance(); expect(TOK_SEMICOLON);
            return ast_make_assign(tgt, ast_make_binary('-', ast_clone(tgt), ast_make_number(1)));
        }
        if (is_compound_assign(current.type)) {
            TokenType op = current.type; advance();
            ASTNode *v = parse_ternary(); expect(TOK_SEMICOLON);
            return ast_make_assign(tgt, ast_make_binary(compound_op(op), ast_clone(tgt), v));
        }
        expect(TOK_ASSIGN); ASTNode *v=parse_ternary(); expect(TOK_SEMICOLON);
        return ast_make_assign(tgt, v);
    }
    char buf[100]; snprintf(buf, sizeof(buf), "sentencia inesperada '%s'", token_type_name(current.type));
    lexer_error(buf, current.line, current.col);
    exit(1);
}

/* ── expression parsers ─────────────────────────────────────────── */

static ASTNode *parse_ternary(void) {
    ASTNode *cond = parse_lor();
    if (check(TOK_QUESTION)) {
        advance();
        ASTNode *t = parse_ternary();
        expect(TOK_COLON);
        ASTNode *e = parse_ternary();
        return ast_make_ternary(cond, t, e);
    }
    return cond;
}

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
    if(check(TOK_CHAR_LITERAL)){int v=current.int_value;advance();return ast_make_number(v);}
    if(check(TOK_STRING)){ASTNode *n=ast_make_string(current.lexeme);advance();return n;}
    if(check(TOK_IDENT)){
        ASTNode *tgt=parse_target();
        if(check(TOK_LPAREN)){advance();
            ASTNode *args=NULL,*at=NULL;
            if(!check(TOK_RPAREN)){do{if(match(TOK_COMMA)){}ASTNode *a=parse_ternary();if(!args){args=at=a;}else{at->next=a;at=a;}}while(match(TOK_COMMA));}
            expect(TOK_RPAREN); return ast_make_call(tgt->var_name,args);
        }
        return tgt;
    }
    if(match(TOK_LPAREN)){
        if(is_type_keyword(current.type)){
            int ct=type_keyword_to_code(current.type);advance();
            expect(TOK_RPAREN);
            return ast_make_cast(ct,parse_unary());
        }
        ASTNode *n=parse_ternary();expect(TOK_RPAREN);return n;
    }
    lexer_error("esperaba número, id, string o '('", current.line, current.col); exit(1);
}
