#ifndef CODEGEN_H
#define CODEGEN_H

#include "compiler.h"

void codegen_init(const char *filename);
void codegen_program(ASTNode *program);
void codegen_finish(void);

#endif
