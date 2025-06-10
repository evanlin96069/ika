#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include "source.h"
#include "symbol_table.h"

typedef struct ParserState {
    UtlArenaAllocator* arena;
    UtlAllocator* temp_allocator;
    SymbolTable* sym;
    SymbolTable* global_sym;

    const SourceState* src;
    SourceLine* line;
    size_t pos;  // current pos

    Token token;
    SourcePos prev_token_end;
    SourcePos token_start;
    SourcePos token_end;
} ParserState;

void parser_init(ParserState* parser, SymbolTable* sym,
                 UtlArenaAllocator* arena, UtlAllocator* temp_allocator);
ASTNode* parser_parse(ParserState* parser, const SourceState* src);

#endif
