#ifndef LEXER_H
#define LEXER_H

#include "compiler.h"

void lexer_init(const char *filename);
Token next_token(void);
void lexer_destroy(void);
void lexer_error(const char *msg, int line, int col);
const char *get_source_line(int lineno);

#endif
