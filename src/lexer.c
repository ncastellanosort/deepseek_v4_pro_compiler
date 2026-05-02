#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static FILE *source;
static int line, col, ch, next_ch;

/* source line cache for error messages */
#define MAX_LINES 512
static char src_lines[MAX_LINES][256];
static int src_line_count;

static void read_char(void) {
    ch = next_ch;
    if (ch == '\n') { line++; col = 1; }
    else if (ch != EOF) col++;
    next_ch = fgetc(source);
}

static void skip_ws_comments(void) {
    while (1) {
        while (ch != EOF && isspace(ch)) read_char();
        if (ch == '/' && next_ch == '/') {
            while (ch != EOF && ch != '\n') read_char();
            continue;
        }
        if (ch == '/' && next_ch == '*') {
            read_char(); read_char();
            while (ch != EOF && !(ch == '*' && next_ch == '/')) read_char();
            if (ch != EOF) { read_char(); read_char(); }
            continue;
        }
        break;
    }
}

static Token make_token(TokenType t, const char *l) {
    Token tok; tok.type = t; tok.int_value = 0; tok.line = line; tok.col = col;
    strncpy(tok.lexeme, l, MAX_LEXEME - 1);
    tok.lexeme[MAX_LEXEME - 1] = '\0';
    return tok;
}

static TokenType keyword_type(const char *s) {
    if (strcmp(s, "def")==0) return TOK_DEF;
    if (strcmp(s, "if")==0) return TOK_IF;
    if (strcmp(s, "else")==0) return TOK_ELSE;
    if (strcmp(s, "while")==0) return TOK_WHILE;
    if (strcmp(s, "for")==0) return TOK_FOR;
    if (strcmp(s, "break")==0) return TOK_BREAK;
    if (strcmp(s, "continue")==0) return TOK_CONTINUE;
    if (strcmp(s, "return")==0) return TOK_RETURN;
    if (strcmp(s, "print")==0) return TOK_PRINT;
    if (strcmp(s, "array")==0) return TOK_ARRAY;
    return TOK_IDENT;
}

static Token read_number(void) {
    char b[MAX_LEXEME]; int i = 0;
    b[i++]=(char)ch; read_char();
    while (ch!=EOF&&isdigit(ch)) { if(i<MAX_LEXEME-1)b[i++]=(char)ch; read_char(); }
    b[i]='\0';
    Token t=make_token(TOK_NUMBER,b); t.int_value=atoi(b); return t;
}

static Token read_ident(void) {
    char b[MAX_LEXEME]; int i = 0;
    b[i++]=(char)ch; read_char();
    while (ch!=EOF&&(isalnum(ch)||ch=='_')) { if(i<MAX_LEXEME-1)b[i++]=(char)ch; read_char(); }
    b[i]='\0'; return make_token(keyword_type(b), b);
}

static Token read_string(void) {
    char b[MAX_LEXEME]; int i = 0;
    read_char();
    while (ch != EOF && ch != '"') {
        if (ch == '\\') { read_char();
            switch (ch) { case 'n':ch='\n';break; case 't':ch='\t';break;
                          case '"':ch='"';break; case '\\':ch='\\';break; default:break; }
        }
        if (i < MAX_LEXEME - 1) b[i++] = (char)ch;
        read_char();
    }
    b[i] = '\0';
    if (ch != '"') { fprintf(stderr, "Error: string sin cerrar\n"); exit(1); }
    read_char();
    return make_token(TOK_STRING, b);
}

static Token read_twochar(TokenType t2, const char *l2,
                           TokenType t1, const char *l1, int ex) {
    if (next_ch == ex) { read_char(); read_char(); return make_token(t2, l2); }
    read_char(); return make_token(t1, l1);
}

static void cache_source_lines(void) {
    rewind(source);
    src_line_count = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), source) && src_line_count < MAX_LINES) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        strncpy(src_lines[src_line_count], buf, 255);
        src_lines[src_line_count][255] = '\0';
        src_line_count++;
    }
    rewind(source);
}

void lexer_init(const char *fn) {
    source = fopen(fn, "r");
    if (!source) { fprintf(stderr, "Error: '%s'\n", fn); exit(1); }
    cache_source_lines();
    line = 1; col = 0;
    ch = fgetc(source); next_ch = fgetc(source);
}

const char *get_source_line(int lineno) {
    if (lineno >= 1 && lineno <= src_line_count)
        return src_lines[lineno - 1];
    return NULL;
}

void lexer_error(const char *msg, int line, int col) {
    const char *sl = get_source_line(line);
    fprintf(stderr, "Error línea %d col %d: %s\n", line, col, msg);
    if (sl) {
        fprintf(stderr, "  %s\n", sl);
        fprintf(stderr, "  ");
        for (int i = 0; sl[i] && i < col - 1; i++)
            fputc(sl[i] == '\t' ? '\t' : ' ', stderr);
        fprintf(stderr, "^\n");
    }
}

Token next_token(void) {
    skip_ws_comments();
    if (ch == EOF) { Token t=make_token(TOK_EOF,""); t.line=line; return t; }
    if (ch == '"') return read_string();
    if (isdigit(ch)) return read_number();
    if (isalpha(ch) || ch == '_') return read_ident();

    Token tok; tok.line=line; tok.col=col; tok.int_value=0;
    switch (ch) {
        case '=': tok=read_twochar(TOK_EQ,"==",TOK_ASSIGN,"=",'='); break;
        case '!': tok=read_twochar(TOK_NE,"!=",TOK_NOT,"!",'=');   break;
        case '<':
            if (next_ch=='<') {read_char();read_char();tok=make_token(TOK_LSHIFT,"<<");}
            else if (next_ch=='='){read_char();read_char();tok=make_token(TOK_LE,"<=");}
            else {read_char();tok=make_token(TOK_LT,"<");} break;
        case '>':
            if (next_ch=='>') {read_char();read_char();tok=make_token(TOK_RSHIFT,">>");}
            else if (next_ch=='='){read_char();read_char();tok=make_token(TOK_GE,">=");}
            else {read_char();tok=make_token(TOK_GT,">");} break;
        case '&': tok=read_twochar(TOK_LAND,"&&",TOK_BITAND,"&",'&'); break;
        case '|': tok=read_twochar(TOK_LOR,"||",TOK_BITOR,"|",'|');  break;
        case '^': read_char(); tok=make_token(TOK_BITXOR,"^"); break;
        case '+': read_char(); tok=make_token(TOK_PLUS,"+");   break;
        case '-': read_char(); tok=make_token(TOK_MINUS,"-");  break;
        case '*': read_char(); tok=make_token(TOK_STAR,"*");   break;
        case '/': read_char(); tok=make_token(TOK_SLASH,"/");  break;
        case '%': read_char(); tok=make_token(TOK_MOD,"%");    break;
        case ',': read_char(); tok=make_token(TOK_COMMA,",");  break;
        case '(': read_char(); tok=make_token(TOK_LPAREN,"("); break;
        case ')': read_char(); tok=make_token(TOK_RPAREN,")"); break;
        case '[': read_char(); tok=make_token(TOK_LBRACK,"["); break;
        case ']': read_char(); tok=make_token(TOK_RBRACK,"]"); break;
        case '{': read_char(); tok=make_token(TOK_LBRACE,"{"); break;
        case '}': read_char(); tok=make_token(TOK_RBRACE,"}"); break;
        case ';': read_char(); tok=make_token(TOK_SEMICOLON,";"); break;
        default:
            fprintf(stderr,"Error léxico %d:%d: '%c'\n",line,col,ch);
            tok=make_token(TOK_ERROR,""); read_char(); break;
    }
    return tok;
}

void lexer_destroy(void) { if (source) fclose(source); source = NULL; }
