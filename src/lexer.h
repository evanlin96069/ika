#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum TokenType {
    TK_ERR = -1,
    TK_NUL = 0,
    TK_INT,
    TK_ADD = '+',
    TK_SUB = '-',
    TK_MUL = '*',
    TK_DIV = '/',
    TK_LBRACK = '(',
    TK_RBRACK = ')',
} TokenType;

typedef struct Token {
    TokenType type;
    int val;
} Token;

typedef struct LexerState {
    const char* src;
    size_t pos;
    Token token;
} LexerState;

void lexer_init(LexerState* lexer, const char* src);
void lexer_next(LexerState* lexer);
Token lexer_peek(LexerState* lexer);

#endif
