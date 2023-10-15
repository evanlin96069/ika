#include "parser.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Lexer

static inline int is_digit(char c) { return c >= '0' && c <= '9'; }

static inline int is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static Token next_token_internal(ParserState* parser, int peek) {
    int start = 0;
    int offset = 0;
    Token tk;

    char c = (parser->src + parser->pos)[offset];
    while (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        offset++;
        c = (parser->src + parser->pos)[offset];
    }

    start = offset;

    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '=':
        case '(':
        case ')':
        case ';':
            tk.type = c;
            offset++;
            break;

        case '\0':
            tk.type = TK_NUL;
            break;

        default:
            if (is_digit(c)) {
                tk.type = TK_INT;
                tk.val = 0;
                while (is_digit(c)) {
                    tk.val *= 10;
                    tk.val += (parser->src + parser->pos)[offset] - '0';
                    offset++;
                    c = (parser->src + parser->pos)[offset];
                }
            } else if (is_letter(c) || c == '_') {
                int i = 0;
                char ident[IDENT_MAX_LENGTH + 1] = {0};

                ident[i++] = c;
                offset++;
                c = (parser->src + parser->pos)[offset];

                while ((is_letter(c) || is_digit(c) || c == '_') &&
                       i < IDENT_MAX_LENGTH) {
                    ident[i++] = c;
                    offset++;
                    c = (parser->src + parser->pos)[offset];
                }

                if (strcmp(ident, "var") == 0) {
                    tk.type = TK_DECL;
                } else if (strcmp(ident, "print") == 0) {
                    tk.type = TK_PRINT;
                } else {
                    tk.type = TK_IDENT;
                    tk.ident = arena_alloc(parser->arena, sizeof(ident));
                    memcpy(tk.ident, ident, sizeof(ident));
                }
            } else {
                tk.type = TK_ERR;
            }
    }

    if (!peek) {
        parser->pre_token_pos = parser->pos;
        parser->post_token_pos = parser->pos + start;
        parser->pos += offset;
    }

    return tk;
}

static inline Token next_token(ParserState* parser) {
    parser->token = next_token_internal(parser, 0);
    return parser->token;
}
static inline Token peek_token(ParserState* parser) {
    return next_token_internal(parser, 1);
}

// Parser

static ASTNode* expr(ParserState* parser);
static ASTNode* primary(ParserState* parser);
static ASTNode* term(ParserState* parser);
static ASTNode* stmt(ParserState* parser);
static ASTNode* var_decl(ParserState* parser);
static ASTNode* print(ParserState* parser);
static ASTNode* stmt_list(ParserState* parser);

static ASTNode* error(ParserState* parser, int pos, const char* fmt, ...) {
    ErrorNode* err = arena_alloc(parser->arena, sizeof(ErrorNode));
    err->type = NODE_ERR;
    err->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->msg, sizeof(err->msg), fmt, ap);
    va_end(ap);

    return (ASTNode*)err;
}

static ASTNode* primary(ParserState* parser) {
    Token tk = next_token(parser);
    switch (tk.type) {
        case TK_INT: {
            IntLitNode* node = arena_alloc(parser->arena, sizeof(IntLitNode));
            node->type = NODE_INTLIT;
            node->val = tk.val;
            return (ASTNode*)node;
        }

        case TK_IDENT: {
            SymbolTableEntry* ste = symbol_table_find(parser->sym, tk.ident);
            if (ste) {
                VarNode* node = arena_alloc(parser->arena, sizeof(VarNode));
                node->type = NODE_VAR;
                node->ste = ste;
                return (ASTNode*)node;
            } else {
                return error(parser, parser->post_token_pos,  "'%s' undeclared", tk.ident);
            }
        }

        case TK_LBRACK: {
            ASTNode* node = expr(parser);
            if (node->type == NODE_ERR) {
                return node;
            }
            tk = next_token(parser);
            if (tk.type != TK_RBRACK) {
                return error(parser, parser->pre_token_pos, "expected ')'");
            }
            return node;
        }

        case TK_ADD:
        case TK_SUB: {
            UnaryOpNode* node = arena_alloc(parser->arena, sizeof(UnaryOpNode));
            node->type = NODE_UNARYOP;
            node->op = tk.type;
            node->node = expr(parser);
            if (node->node->type == NODE_ERR) {
                return node->node;
            }
            return (ASTNode*)node;
        }

        case TK_ERR:
            return error(parser, parser->post_token_pos, "unknown token");

        default:
            return error(parser, parser->post_token_pos, "unexpected token");
    }
}

static ASTNode* term(ParserState* parser) {
    ASTNode* node = primary(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = peek_token(parser);
    while (tk.type == TK_MUL || tk.type == TK_DIV) {
        next_token(parser);
        BinaryOpNode* binop = arena_alloc(parser->arena, sizeof(BinaryOpNode));
        binop->type = NODE_BINARYOP;
        binop->op = tk.type;
        binop->left = node;
        binop->right = expr(parser);
        if (binop->right->type == NODE_ERR) {
            return binop->right;
        }
        node = (ASTNode*)binop;

        tk = peek_token(parser);
    }

    return node;
}

static ASTNode* expr(ParserState* parser) {
    ASTNode* node = term(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = peek_token(parser);
    while (tk.type == TK_ADD || tk.type == TK_SUB) {
        next_token(parser);
        BinaryOpNode* binop = arena_alloc(parser->arena, sizeof(BinaryOpNode));
        binop->type = NODE_BINARYOP;
        binop->op = tk.type;
        ;
        binop->left = node;
        binop->right = expr(parser);
        if (binop->right->type == NODE_ERR) {
            return binop->right;
        }
        node = (ASTNode*)binop;

        tk = peek_token(parser);
    }

    return node;
}

static ASTNode* stmt(ParserState* parser) {
    ASTNode* node = expr(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = peek_token(parser);
    if (tk.type == TK_ASSIGN) {
        next_token(parser);
        if (node->type != NODE_VAR) {
            return error(parser, parser->post_token_pos, "lvalue required as left operand of assignment");
        }

        AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
        assign->type = NODE_ASSIGN;
        assign->left = (VarNode*)node;
        assign->right = stmt(parser);
        if (assign->right->type == NODE_ERR) {
            return assign->right;
        }
        node = (ASTNode*)assign;
    }

    return node;
}

static ASTNode* var_decl(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_DECL);

    tk = next_token(parser);

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    if (symbol_table_find(parser->sym, tk.ident) != NULL) {
        return error(parser, parser->post_token_pos, "redefinition of '%s'", tk.ident);
    }

    char* ident = tk.ident;
    VarNode* var = arena_alloc(parser->arena, sizeof(VarNode));
    var->type = NODE_VAR;
    ASTNode* node = (ASTNode*)var;

    tk = peek_token(parser);
    if (tk.type == TK_ASSIGN) {
        next_token(parser);
        AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
        assign->type = NODE_ASSIGN;
        assign->left = var;
        assign->right = stmt(parser);
        if (assign->right->type == NODE_ERR) {
            return assign->right;
        }
        node = (ASTNode*)assign;
    } else if (tk.type != TK_SEMICOLON) {
        next_token(parser);
        return error(parser, parser->pre_token_pos, "Expected '=' or ';' after declaration");
    }

    SymbolTableEntry* ste = symbol_table_append(parser->sym, ident);
    var->ste = ste;

    return node;
}

static ASTNode* print(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_PRINT);

    PrintNode* print_node = arena_alloc(parser->arena, sizeof(PrintNode));
    print_node->type = NODE_PRINT;
    print_node->stmt = stmt(parser);

    return (ASTNode*)print_node;
}

static ASTNode* stmt_list(ParserState* parser) {
    StatementListNode* stmts = arena_alloc(parser->arena, sizeof(StatementListNode));
    stmts->type = NODE_STMTS;
    stmts->stmts = NULL;

    Token tk = peek_token(parser);
    while(tk.type != TK_NUL) {
        ASTNode* node;
        switch (tk.type) {
            case TK_SEMICOLON:
                tk = next_token(parser);
                continue;

            case TK_DECL:
                node = var_decl(parser);
                if (node->type == NODE_ERR)
                    return node;
                break;

            case TK_PRINT:
                node = print(parser);
                if (node->type == NODE_ERR)
                    return node;
                break;

            default:
                node = stmt(parser);
                if (node->type == NODE_ERR)
                    return node;
        }

        tk = next_token(parser);
        if (tk.type != TK_SEMICOLON) {
            return error(parser, parser->pre_token_pos, "Expected ';'");
        }

        tk = peek_token(parser);

        ASTNodeList* list = arena_alloc(parser->arena, sizeof(ASTNodeList));
        list->node = node;
        list->next = NULL;
        if (!stmts->stmts) {
            stmts->stmts = list;
            stmts->_tail = list;
        } else {
            stmts->_tail->next = list;
            stmts->_tail = list;
        }
    }
    return (ASTNode*)stmts;
}

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena) {
    parser->arena = arena;
    parser->sym = sym;
}

ASTNode* parser_parse(ParserState* parser, const char* src) {
    arena_reset(parser->arena);
    parser->src = src;
    parser->pos = 0;

    return stmt_list(parser);
}
