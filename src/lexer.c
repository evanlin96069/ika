#include "lexer.h"

#include "parser.h"
#include "utl/allocator/utlstackfallback.h"
#include "utl/utlvector.h"

#define DEFAULT_STR_SIZE 256

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

static const StrToken str_tk[] = {
    {"var", TK_DECL},      {"const", TK_CONST},   {"if", TK_IF},
    {"else", TK_ELSE},     {"while", TK_WHILE},   {"fn", TK_FUNC},
    {"return", TK_RET},    {"break", TK_BREAK},   {"continue", TK_CONTINUE},
    {"extern", TK_EXTERN}, {"pub", TK_PUB},       {"enum", TK_ENUM},
    {"struct", TK_STRUCT}, {"packed", TK_PACKED}, {"sizeof", TK_SIZEOF},
    {"void", TK_VOID},     {"u8", TK_U8},         {"u16", TK_U16},
    {"u32", TK_U32},       {"i8", TK_I8},         {"i16", TK_I16},
    {"i32", TK_I32},       {"bool", TK_BOOL},     {"true", TK_TRUE},
    {"false", TK_FALSE},   {"null", TK_NULL},     {"as", TK_CAST},
    {"asm", TK_ASM},
};

// Parse escape sequence in string literal
// - p: null-terminated string start with '\'
// - size: return the size of the escape sequence
// - Returns TK_INT or TK_ERR
static inline Token handle_string_escape(const char* p, int* size) {
    assert(*p == '\\');
    *size = 1;

    p++;
    (*size)++;

    Token tk;
    tk.type = TK_INT;

    char c = *p;
    switch (c) {
        case '\'':
        case '"':
        case '\\':
            break;
        case '0':
            tk.val = '\0';
            break;
        case 'n':
            tk.val = '\n';
            break;
        case 'r':
            tk.val = '\r';
            break;
        case 't':
            tk.val = '\t';
            break;
        case 'x': {
            p++;
            c = *p;
            if (!is_hex_digit(c)) {
                tk.type = TK_ERR;
                tk.str = str("expected two hex digits after \\x");
                break;
            }
            (*size)++;

            tk.val = hex_digit_to_int(c);

            p++;
            c = *p;
            if (!is_hex_digit(c)) {
                tk.type = TK_ERR;
                tk.str = str("expected two hex digits after \\x");
                break;
            }
            (*size)++;

            tk.val <<= 4;
            tk.val += hex_digit_to_int(c);
        } break;

        default:
            tk.type = TK_ERR;
            tk.str = str("invalid escape character");
    }

    return tk;
}

Token next_token_from_line(UtlArenaAllocator* arena,
                           UtlAllocator* temp_allocator, const char* p,
                           const StrToken* keywords, int keyword_count,
                           int* start_offset, int* end_offset) {
    Token tk;
    int pos = 0;

    *start_offset = 0;
    *end_offset = 0;

    while (*p == ' ' || *p == '\t') {
        p++;
        pos++;
    }

    *start_offset = pos;

    if (*p == '\0' || (*p == '/' && *(p + 1) == '/')) {
        tk.type = TK_EOF;
        return tk;
    }

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
            tk.type = TK_STR;

            char buffer[DEFAULT_STR_SIZE];
            UtlStackFallbackAllocator sf =
                utlstackfallback_init(buffer, sizeof(buffer), temp_allocator);
            UtlAllocator* allocator = utlstackfallback_allocator(&sf);

            UtlVector(char) s = utlvector_init(allocator);

            p++;
            pos++;
            while (*p != '"' && *p != '\0') {
                char c = *p;
                if (*p == '\\') {
                    int size;
                    Token result = handle_string_escape(p, &size);
                    p += size;
                    pos += size;

                    if (result.type == TK_ERR) {
                        tk = result;
                        break;
                    }

                    c = result.val;
                } else {
                    p++;
                    pos++;
                }

                utlvector_push(&s, c);
            }

            if (tk.type != TK_ERR) {
                char* new_buffer = utlarena_alloc(arena, s.size);
                memcpy(new_buffer, s.data, s.size);

                Str result;
                result.len = s.size;
                result.ptr = new_buffer;

                tk.str = result;

                if (*p != '"') {
                    tk.type = TK_ERR;
                    tk.str = str("missing terminating \" character");
                    break;
                }
                pos++;
            }

            utlvector_deinit(&s);
        } break;

        case '\'':
            tk.type = TK_INT;
            tk.val = 0;

            p++;
            pos++;
            while (*p != '\'' && *p != '\0') {
                unsigned char c = *p;
                if (*p == '\\') {
                    int size;
                    Token result = handle_string_escape(p, &size);
                    p += size;
                    pos += size;

                    if (result.type == TK_ERR) {
                        tk = result;
                        break;
                    }

                    c = result.val;
                } else {
                    p++;
                    pos++;
                }

                tk.val <<= 8;
                tk.val += c;
            }

            if (tk.type != TK_ERR) {
                if (*p != '\'') {
                    tk.type = TK_ERR;
                    tk.str = str("missing terminating ' character");
                    break;
                }
                pos++;
            }
            break;

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

                for (int i = 0; i < keyword_count; i++) {
                    if (str_eql(str(keywords[i].s), ident)) {
                        tk.type = keywords[i].token_type;
                        break;
                    }
                }

            } else {
                tk.type = TK_ERR;
                tk.str = str("unknown token");
            }
        }
    }

    *end_offset = pos;
    return tk;
}

static Token next_token_internal(ParserState* parser, int peek) {
    Token tk;

    SourceLine* line = parser->line;
    assert(line);

    size_t pos = parser->pos;

    SourcePos prev_token_end = {
        .index = pos,
        .line = *line,
    };

    // skip to next token start
    char* p = &line->content[pos];
    while (*p == ' ' || *p == '\t' || *p == '\0' || *p == '/') {
        if (*p == '\0' || (*p == '/' && *(p + 1) == '/')) {
            // next line
            if (line->next == NULL) {
                if (!peek) {
                    SourcePos curr_pos = {
                        .index = pos,
                        .line = *line,
                    };
                    parser->prev_token_end = prev_token_end;
                    parser->token_start = curr_pos;
                    parser->token_end = curr_pos;
                }
                tk.type = TK_EOF;
                return tk;
            }
            line = line->next;
            pos = 0;
            p = line->content;
        } else if (*p == '/') {
            break;
        } else {
            pos++;
            p++;
        }
    }

    SourcePos token_start = {
        .index = pos,
        .line = *line,
    };

    int start_offset, end_offset;
    tk = next_token_from_line(parser->arena, parser->temp_allocator, p, str_tk,
                              ARRAY_SIZE(str_tk), &start_offset, &end_offset);
    pos += end_offset;

    SourcePos token_end = {
        .index = pos,
        .line = *line,
    };

    if (!peek) {
        parser->prev_token_end = prev_token_end;
        parser->token_start = token_start;
        parser->token_end = token_end;
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
