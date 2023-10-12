#include "lexer.h"

#include <assert.h>
#include <stddef.h>

void lexer_init(LexerState* lexer, const char* src) {
    lexer->src = src;
    lexer->pos = 0;
}

static Token _next(LexerState* lexer, int peek) {
    int i = 0;
    Token tk;

    char c = (lexer->src + lexer->pos)[i];
    while (c == ' ' || c == '\n' || c == '\t') {
        i++;
        c = (lexer->src + lexer->pos)[i];
    }

    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
            tk.type = c;
            i++;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            tk.type = TK_INT;
            tk.val = 0;
            while (c >= '0' && c <= '9') {
                tk.val *= 10;
                tk.val += (lexer->src + lexer->pos)[i] - '0';
                i++;
                c = (lexer->src + lexer->pos)[i];
            }
            break;
        case '\0':
            tk.type = TK_NUL;
            break;
        default:
            tk.type = TK_ERR;
    }

    if (!peek) {
        lexer->pos += i;
    }

    return tk;
}

void lexer_next(LexerState* lexer) { lexer->token = _next(lexer,0); }
Token lexer_peek(LexerState* lexer) { return _next(lexer,1); }
