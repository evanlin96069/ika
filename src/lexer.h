#ifndef LEXER_H
#define LEXER_H

#include "str.h"

typedef enum TokenType {
    TK_ERR = -1,
    TK_NUL = 0,
    TK_IDENT,
    TK_INT,
    TK_STR,
    TK_DECL,
    TK_CONST,
    TK_ENUM,
    TK_FUNC,
    TK_EXTERN,
    TK_RET,
    TK_PRINT,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_BREAK,
    TK_CONTINUE,

    TK_MUL,
    TK_DIV,
    TK_MOD,
    TK_ADD,
    TK_SUB,
    TK_SHL,
    TK_SHR,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_EQ,
    TK_NE,
    TK_AND,
    TK_XOR,
    TK_OR,
    TK_LAND,
    TK_LOR,
    TK_ASSIGN,
    TK_AADD,
    TK_ASUB,
    TK_AMUL,
    TK_ADIV,
    TK_AMOD,
    TK_ASHL,
    TK_ASHR,
    TK_AAND,
    TK_AXOR,
    TK_AOR,

    TK_NOT,
    TK_LNOT,

    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_SEMICOLON,
    TK_COMMA,
    TK_COLON,
    TK_DOT,
    TK_DOLLAR,
} TokenType;

typedef struct Token {
    TokenType type;
    union {
        int val;
        Str str;
    };
} Token;

struct ParserState;

Token next_token_internal(struct ParserState* parser, int peek);

Token next_token(struct ParserState* parser);

Token peek_token(struct ParserState* parser);

#endif
