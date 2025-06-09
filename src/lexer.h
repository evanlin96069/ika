#ifndef LEXER_H
#define LEXER_H

#include "arena.h"
#include "str.h"

typedef enum TkType {
    TK_ERR = -1,
    TK_EOF = 0,
    TK_IDENT,
    TK_INT,
    TK_STR,
    TK_DECL,
    TK_CONST,
    TK_ENUM,
    TK_STRUCT,
    TK_FUNC,
    TK_EXTERN,
    TK_RET,
    TK_PRINT,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_BREAK,
    TK_CONTINUE,

    TK_SIZEOF,
    TK_CAST,

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
    TK_LBRACKET,
    TK_RBRACKET,
    TK_LBRACE,
    TK_RBRACE,
    TK_SEMICOLON,
    TK_COMMA,
    TK_COLON,
    TK_DOT,
    TK_ARGS,  // ...

    TK_BOOL,
    TK_TRUE,
    TK_FALSE,

    TK_NULL,  // null pointer

    TK_VOID,
    TK_U8,
    TK_U16,
    TK_U32,
    TK_I8,
    TK_I16,
    TK_I32,
} TkType;

typedef struct Token {
    TkType type;
    union {
        unsigned int val;
        Str str;
    };
} Token;

struct ParserState;

Token next_token_from_line(Arena* arena, const char* p, int* size);

Token next_token(struct ParserState* parser);

Token peek_token(struct ParserState* parser);

#endif
