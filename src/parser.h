#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "preprocessor.h"
#include "symbol_table.h"

typedef struct ParserState {
    Arena* arena;
    SymbolTable* sym;
    SymbolTable* global_sym;

    SourceState* src;
    size_t line;
    size_t pos;  // current pos

    Token token;
    SourcePos pre_token_pos;   // pos at the end of previous token
    SourcePos post_token_pos;  // pos at the start of this token
} ParserState;

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena);
ASTNode* parser_parse(ParserState* parser, SourceState* src);

#endif
