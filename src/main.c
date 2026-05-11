#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "compiler.h"
#include "semantic.h"
#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void run_gcc(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -no-pie build/output.s -o build/prog 2>&1");
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Error: gcc falló al compilar el assembly\n");
        exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_fuente>\n", argv[0]);
        return 1;
    }

    const char *orig_file = argv[1];

    char *src_file = preprocess(orig_file);
    if (!src_file) return 1;

    lexer_init(src_file);

    ASTNode *ast = parse_program();

    semantic_check(ast);

    if (argc >= 3 && strcmp(argv[2], "--dump-ast") == 0) {
        ast_print(ast, 0);
        ast_free(ast);
        lexer_destroy();
        unlink(src_file);
        free(src_file);
        return 0;
    }

    codegen_init("build/output.s");
    codegen_program(ast);
    codegen_finish();

    ast_free(ast);
    lexer_destroy();

    printf("Assembly generado en build/output.s\n");
    printf("Compilando con gcc...\n");
    run_gcc();
    printf("Ejecutable: build/prog\n");

    unlink(src_file);
    free(src_file);

    return 0;
}
