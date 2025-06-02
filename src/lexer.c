#include "lexer.h"

#include "parser.h"
#include "preprocessor.h"

static inline int is_digit(char c) { return c >= '0' && c <= '9'; }

static inline int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static inline int hex_digit_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }

    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }

    UNREACHABLE();
}

static inline int is_oct_digit(char c) { return c >= '0' && c <= '7'; }

static inline int is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

typedef struct StrToken {
    const char* s;
    TkType token_type;
} StrToken;

static StrToken str_tk[] = {
    {"var", TK_DECL},      {"const", TK_CONST}, {"if", TK_IF},
    {"else", TK_ELSE},     {"while", TK_WHILE}, {"fn", TK_FUNC},
    {"return", TK_RET},    {"break", TK_BREAK}, {"continue", TK_CONTINUE},
    {"extern", TK_EXTERN}, {"enum", TK_ENUM},   {"struct", TK_STRUCT},
    {"sizeof", TK_SIZEOF}, {"void", TK_VOID},   {"u8", TK_U8},
    {"u16", TK_U16},       {"u32", TK_U32},     {"i8", TK_I8},
    {"i16", TK_I16},       {"i32", TK_I32},     {"bool", TK_BOOL},
    {"true", TK_TRUE},     {"false", TK_FALSE}, {"null", TK_NULL},
    {"as", TK_CAST},
};

Token next_token_internal(ParserState* parser, int peek) {
    Token tk = {0};

    if (!parser->src) {
        tk.type = TK_NUL;
        return tk;
    }

    const SourceLine* lines = parser->src->lines;

    size_t line = parser->line;
    size_t pos = parser->pos;

    SourcePos pre_token_pos = {
        .index = pos,
        .line = lines[line],
    };

    // skip to next token start
    char* p = &lines[line].content[pos];
    while (*p == ' ' || *p == '\t' || *p == '\0' || *p == '/') {
        if (*p == '\0' || (*p == '/' && *(p + 1) == '/')) {
            // next line
            if (line + 1 >= parser->src->line_count) {
                tk.type = TK_NUL;
                return tk;
            }
            line++;
            pos = 0;
            p = lines[line].content;
        } else if (*p == '/') {
            break;
        } else {
            pos++;
            p++;
        }
    }

    SourcePos post_token_pos = {
        .index = pos,
        .line = lines[line],
    };

    switch (*p) {
        case '*':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_AMUL;
            } else {
                tk.type = TK_MUL;
            }
            pos++;
            break;

        case '/':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_ADIV;
            } else {
                tk.type = TK_DIV;
            }
            pos++;
            break;

        case '%':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_AMOD;
            } else {
                tk.type = TK_MOD;
            }
            pos++;
            break;

        case '+':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_AADD;
            } else {
                tk.type = TK_ADD;
            }
            pos++;
            break;

        case '-':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_ASUB;
            } else {
                tk.type = TK_SUB;
            }
            pos++;
            break;

        case '<': {
            char next = *(p + 1);
            if (next == '=') {
                pos++;
                tk.type = TK_LE;
            } else if (next == '<') {
                pos++;
                next = *(p + 2);
                if (next == '=') {
                    pos++;
                    tk.type = TK_ASHL;
                } else {
                    tk.type = TK_SHL;
                }
            } else {
                tk.type = TK_LT;
            }
            pos++;
        } break;

        case '>': {
            char next = *(p + 1);
            if (next == '=') {
                pos++;
                tk.type = TK_GE;
            } else if (next == '>') {
                pos++;
                next = *(p + 2);
                if (next == '=') {
                    pos++;
                    tk.type = TK_ASHR;
                } else {
                    tk.type = TK_SHR;
                }
            } else {
                tk.type = TK_GT;
            }
            pos++;
        } break;

        case '&': {
            char next = *(p + 1);
            if (next == '&') {
                pos++;
                tk.type = TK_LAND;
            } else if (next == '=') {
                pos++;
                tk.type = TK_AAND;
            } else {
                tk.type = TK_AND;
            }
            pos++;
        } break;

        case '|': {
            char next = *(p + 1);
            if (next == '|') {
                pos++;
                tk.type = TK_LOR;
            } else if (next == '=') {
                pos++;
                tk.type = TK_AOR;
            } else {
                tk.type = TK_OR;
            }
            pos++;
        } break;

        case '^':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_AXOR;
            } else {
                tk.type = TK_XOR;
            }
            pos++;
            break;

        case '~':
            tk.type = TK_NOT;
            pos++;
            break;

        case '!':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_NE;
            } else {
                tk.type = TK_LNOT;
            }
            pos++;
            break;

        case '=':
            if (*(p + 1) == '=') {
                pos++;
                tk.type = TK_EQ;
            } else {
                tk.type = TK_ASSIGN;
            }
            pos++;
            break;

        case '(':
            tk.type = TK_LPAREN;
            pos++;
            break;

        case ')':
            tk.type = TK_RPAREN;
            pos++;
            break;

        case '[':
            tk.type = TK_LBRACKET;
            pos++;
            break;

        case ']':
            tk.type = TK_RBRACKET;
            pos++;
            break;

        case '{':
            tk.type = TK_LBRACE;
            pos++;
            break;

        case '}':
            tk.type = TK_RBRACE;
            pos++;
            break;

        case ';':
            tk.type = TK_SEMICOLON;
            pos++;
            break;

        case ',':
            tk.type = TK_COMMA;
            pos++;
            break;

        case ':':
            tk.type = TK_COLON;
            pos++;
            break;

        case '.':
            if (*(p + 1) == '.' && *(p + 2) == '.') {
                pos += 3;
                tk.type = TK_ARGS;
            } else {
                tk.type = TK_DOT;
                pos++;
            }
            break;

        case '"': {
            p++;
            pos++;
            Str s = {
                .ptr = p,
                .len = 0,
            };

            // TODO: Parse string properly
            while (*p != '"' && *p != '\0') {
                if (*p == '\\') {
                    s.len++;
                    p++;
                    pos++;
                }
                s.len++;
                p++;
                pos++;
            }

            pos++;

            if (*p != '"') {
                tk.type = TK_ERR;
                tk.str = str("missing terminating \" character");
            } else {
                tk.type = TK_STR;
                tk.str = s;
            }
        } break;

        case '\'': {
            tk.type = TK_INT;
            tk.val = 0;

            p++;
            pos++;
            while (*p != '\'') {
                char c = *p;
                if (*p == '\\') {
                    p++;
                    pos++;
                    c = *p;
                    switch (c) {
                        case '\'':
                        case '"':
                        case '?':
                        case '\\':
                            break;
                        case '0':
                            c = '\0';
                            break;
                        case 'a':
                            c = '\a';
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'f':
                            c = '\f';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case 'v':
                            c = '\v';
                            break;
                    }
                }
                tk.val <<= 8;
                tk.val += c;
                p++;
                pos++;
            }
            pos++;
        } break;

        default: {
            if (is_digit(*p)) {
                if (*p == '0') {
                    char next = *(p + 1);
                    if (next == 'x' || next == 'X') {  // hex
                        p += 2;
                        pos += 2;

                        tk.val = 0;

                        if (!is_hex_digit(*p)) {
                            tk.type = TK_ERR;
                        } else {
                            while (is_hex_digit(*p)) {
                                tk.val = tk.val * 16 + hex_digit_to_int(*p);
                                p++;
                                pos++;
                            }

                            if (is_letter(*p)) {
                                tk.type = TK_ERR;
                            } else {
                                tk.type = TK_INT;
                            }
                        }

                        if (tk.type == TK_ERR) {
                            tk.str =
                                str("invalid digit in hexadecimal constant");
                        }
                    } else if (next == 'b' || next == 'B') {  // bin
                        p += 2;
                        pos += 2;

                        tk.val = 0;
                        if (*p != '0' && *p != '1') {
                            tk.type = TK_ERR;
                        } else {
                            while (*p == '0' || *p == '1') {
                                tk.val = tk.val * 2 + (*p - '0');
                                p++;
                                pos++;
                            }

                            if (is_digit(*p) || is_letter(*p)) {
                                tk.type = TK_ERR;
                            } else {
                                tk.type = TK_INT;
                            }
                        }

                        if (tk.type == TK_ERR) {
                            tk.str = str("invalid digit in binary constant");
                        }
                    } else {  // oct
                        p++;
                        pos++;

                        tk.val = 0;

                        while (is_oct_digit(*p)) {
                            tk.val = tk.val * 8 + (*p - '0');
                            p++;
                            pos++;
                        }

                        if (is_digit(*p) || is_letter(*p)) {
                            tk.type = TK_ERR;
                            tk.str = str("invalid digit in octal constant");
                        } else {
                            tk.type = TK_INT;
                        }
                    }
                } else {  // dec
                    tk.type = TK_INT;
                    tk.val = 0;
                    while (is_digit(*p)) {
                        tk.val *= 10;
                        tk.val += *p - '0';
                        p++;
                        pos++;
                    }

                    if (is_letter(*p)) {
                        tk.type = TK_ERR;
                        tk.str = str("invalid digit in decimal constant");
                    } else {
                        tk.type = TK_INT;
                    }
                }
            } else if (is_letter(*p) || *p == '_') {
                Str ident = {
                    .ptr = p,
                    .len = 1,
                };

                p++;
                pos++;
                while (is_letter(*p) || is_digit(*p) || *p == '_') {
                    ident.len++;
                    p++;
                    pos++;
                }

                tk.type = TK_IDENT;
                tk.str = ident;

                for (unsigned int i = 0; i < sizeof(str_tk) / sizeof(str_tk[0]);
                     i++) {
                    if (str_eql(str(str_tk[i].s), ident)) {
                        tk.type = str_tk[i].token_type;
                        break;
                    }
                }

            } else {
                tk.type = TK_ERR;
                tk.str = str("unknown token");
            }
        }
    }

    if (!peek) {
        parser->pre_token_pos = pre_token_pos;
        parser->post_token_pos = post_token_pos;
        parser->line = line;
        parser->pos = pos;
    }

    return tk;
}

Token next_token(ParserState* parser) {
    parser->token = next_token_internal(parser, 0);
    return parser->token;
}

Token peek_token(ParserState* parser) { return next_token_internal(parser, 1); }
