#include <stdio.h>

#include "compiler.h"
#include "chunk.h"
#include "scanner.h"

void compile(const char* source, Chunk* chunk) {
    init_scanner(source);
    advace();
    expression();
    consume(TOKEN_EOF, "Expect end of expressoin.");
}
