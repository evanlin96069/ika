#include "parser.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Lexer

static inline int is_digit(char c) { return c >= '0' && c <= '9'; }

static inline int is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int is_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static Token next_token_internal(ParserState* parser, int peek) {
    int start = 0;
    int offset = 0;
    Token tk;

    char c = (parser->src + parser->pos)[offset];
    while (is_space(c) || c == '/') {
        if (c == '/') {
            if ((parser->src + parser->pos)[offset + 1] == '/') {
                while (c != '\n' && c != '\0') {
                    c = (parser->src + parser->pos)[++offset];
                }
            } else {
                break;
            }
        }
        c = (parser->src + parser->pos)[++offset];
    }

    start = offset;

    switch (c) {
        case '*':
            tk.type = TK_MUL;
            offset++;
            break;

        case '/':
            tk.type = TK_DIV;
            offset++;
            break;

        case '%':
            tk.type = TK_MOD;
            offset++;
            break;

        case '+':
            tk.type = TK_ADD;
            offset++;
            break;

        case '-':
            tk.type = TK_SUB;
            offset++;
            break;

        case '<':
            if ((parser->src + parser->pos)[offset + 1] == '=') {
                offset++;
                tk.type = TK_LE;
            } else {
                tk.type = TK_LT;
            }
            offset++;
            break;

        case '>':
            if ((parser->src + parser->pos)[offset + 1] == '=') {
                offset++;
                tk.type = TK_GE;
            } else {
                tk.type = TK_GT;
            }
            offset++;
            break;

        case '&':
            if ((parser->src + parser->pos)[offset + 1] == '&') {
                offset++;
                tk.type = TK_LAND;
            } else {
                tk.type = TK_AND;
            }
            offset++;
            break;

        case '|':
            if ((parser->src + parser->pos)[offset + 1] == '|') {
                offset++;
                tk.type = TK_LOR;
            } else {
                tk.type = TK_OR;
            }
            offset++;
            break;

        case '^':
            tk.type = TK_XOR;
            offset++;
            break;

        case '~':
            tk.type = TK_LNOT;
            offset++;
            break;

        case '!':
            if ((parser->src + parser->pos)[offset + 1] == '=') {
                offset++;
                tk.type = TK_NE;
            } else {
                tk.type = TK_NOT;
            }
            offset++;
            break;

        case '=':
            if ((parser->src + parser->pos)[offset + 1] == '=') {
                offset++;
                tk.type = TK_EQ;
            } else {
                tk.type = TK_ASSIGN;
            }
            offset++;
            break;

        case '(':
            tk.type = TK_LPAREN;
            offset++;
            break;

        case ')':
            tk.type = TK_RPAREN;
            offset++;
            break;

        case '{':
            tk.type = TK_LBRACE;
            offset++;
            break;

        case '}':
            tk.type = TK_RBRACE;
            offset++;
            break;

        case ';':
            tk.type = TK_SEMICOLON;
            offset++;
            break;

        case '\0':
            tk.type = TK_NUL;
            break;

        case '"': {
            c = (parser->src + parser->pos)[++offset];
            Str s = {
                .ptr = parser->src + parser->pos + offset,
                .len = 0,
            };

            while (c != '"') {
                s.len++;
                c = (parser->src + parser->pos)[++offset];
            }
            offset++;

            tk.type = TK_STR;
            tk.str = s;
        } break;

        default:
            if (is_digit(c)) {
                tk.type = TK_INT;
                tk.val = 0;
                while (is_digit(c)) {
                    tk.val *= 10;
                    tk.val += (parser->src + parser->pos)[offset] - '0';
                    c = (parser->src + parser->pos)[++offset];
                }
            } else if (is_letter(c) || c == '_') {
                Str ident = {
                    .ptr = parser->src + parser->pos + offset,
                    .len = 1,
                };

                c = (parser->src + parser->pos)[++offset];
                while (is_letter(c) || is_digit(c) || c == '_') {
                    ident.len++;
                    c = (parser->src + parser->pos)[++offset];
                }

                if (str_eql(str("var"), ident)) {
                    tk.type = TK_DECL;
                } else if (str_eql(str("print"), ident)) {
                    tk.type = TK_PRINT;
                } else if (str_eql(str("if"), ident)) {
                    tk.type = TK_IF;
                } else if (str_eql(str("else"), ident)) {
                    tk.type = TK_ELSE;
                } else if (str_eql(str("while"), ident)) {
                    tk.type = TK_WHILE;
                } else {
                    tk.type = TK_IDENT;
                    tk.str = ident;
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

static ASTNode* primary(ParserState* parser);
static ASTNode* expr(ParserState* parser, int min_precedence);
static ASTNode* var_decl(ParserState* parser);
static ASTNode* print_stmt(ParserState* parser);
static ASTNode* if_stmt(ParserState* parser);
static ASTNode* while_stmt(ParserState* parser);
static ASTNode* scope(ParserState* parser);
static ASTNode* stmt(ParserState* parser);
static ASTNode* stmt_list(ParserState* parser, int in_scope);

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
            SymbolTableEntry* ste = symbol_table_find(parser->sym, tk.str, 0);
            if (ste) {
                VarNode* node = arena_alloc(parser->arena, sizeof(VarNode));
                node->type = NODE_VAR;
                node->ste = ste;
                return (ASTNode*)node;
            } else {
                return error(parser, parser->post_token_pos,
                             "'%.*s' undeclared", tk.str.len, tk.str.ptr);
            }
        }

        case TK_LPAREN: {
            ASTNode* node = expr(parser, 0);
            if (node->type == NODE_ERR) {
                return node;
            }
            tk = next_token(parser);
            if (tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos, "expected ')'");
            }
            return node;
        }

        case TK_ADD:
        case TK_SUB:
        case TK_NOT:
        case TK_LNOT: {
            UnaryOpNode* node = arena_alloc(parser->arena, sizeof(UnaryOpNode));
            node->type = NODE_UNARYOP;
            node->op = tk.type;
            node->node = primary(parser);
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

static inline int get_precedence(TokenType type) {
    switch (type) {
        case TK_ASSIGN:
            return 1;

        case TK_LOR:
            return 2;

        case TK_LAND:
            return 3;

        case TK_OR:
            return 4;

        case TK_XOR:
            return 5;

        case TK_AND:
            return 6;

        case TK_EQ:
        case TK_NE:
            return 7;

        case TK_LT:
        case TK_LE:
        case TK_GT:
        case TK_GE:
            return 5;

        case TK_ADD:
        case TK_SUB:
            return 6;

        case TK_MUL:
        case TK_DIV:
        case TK_MOD:
            return 7;

        default:
            return -1;
    }
}

static inline int is_left_associative(TokenType type) {
    switch (type) {
        case TK_ASSIGN:
            return 0;

        case TK_MUL:
        case TK_DIV:
        case TK_MOD:
        case TK_ADD:
        case TK_SUB:
        case TK_LT:
        case TK_LE:
        case TK_GT:
        case TK_GE:
        case TK_EQ:
        case TK_NE:
        case TK_AND:
        case TK_XOR:
        case TK_OR:
        case TK_LAND:
        case TK_LOR:
            return 1;

        default:
            assert(0);
    }
}

static ASTNode* expr(ParserState* parser, int min_precedence) {
    ASTNode* node = primary(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    int precedence = get_precedence(peek_token(parser).type);
    while (precedence != -1 && precedence >= min_precedence) {
        Token tk = next_token(parser);

        int next_precedence = precedence;
        if (is_left_associative(tk.type)) {
            next_precedence++;
        }

        if (next_precedence < min_precedence) {
            return node;
        }

        if (tk.type == TK_ASSIGN) {
            if (node->type != NODE_VAR) {
                return error(parser, parser->post_token_pos,
                             "lvalue required as left operand of assignment");
            }
            AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
            assign->type = NODE_ASSIGN;
            assign->left = (VarNode*)node;
            assign->right = expr(parser, next_precedence);
            if (assign->right->type == NODE_ERR) {
                return assign->right;
            }
            node = (ASTNode*)assign;
        } else {
            BinaryOpNode* binop =
                arena_alloc(parser->arena, sizeof(BinaryOpNode));
            binop->type = NODE_BINARYOP;
            binop->op = tk.type;
            binop->left = node;
            binop->right = expr(parser, next_precedence);
            if (binop->right->type == NODE_ERR) {
                return binop->right;
            }
            node = (ASTNode*)binop;
        }
        precedence = get_precedence(peek_token(parser).type);
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

    Str ident = tk.str;
    if (symbol_table_find(parser->sym, ident, 1) != NULL) {
        return error(parser, parser->post_token_pos, "redefinition of '%.*s'",
                     tk.str.len, tk.str.ptr);
    }

    VarNode* var = arena_alloc(parser->arena, sizeof(VarNode));
    var->type = NODE_VAR;
    ASTNode* node = (ASTNode*)var;

    tk = peek_token(parser);
    if (tk.type == TK_ASSIGN) {
        next_token(parser);
        AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
        assign->type = NODE_ASSIGN;
        assign->left = var;
        assign->right = expr(parser, 0);
        if (assign->right->type == NODE_ERR) {
            return assign->right;
        }
        node = (ASTNode*)assign;
    } else if (tk.type != TK_SEMICOLON) {
        next_token(parser);
        return error(parser, parser->pre_token_pos,
                     "Expected '=' or ';' after declaration");
    }

    SymbolTableEntry* ste = symbol_table_append(parser->sym, ident);
    var->ste = ste;

    return node;
}

static ASTNode* print_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_PRINT);

    PrintNode* print_node = arena_alloc(parser->arena, sizeof(PrintNode));
    print_node->type = NODE_PRINT;
    print_node->expr = expr(parser, 0);
    if (print_node->expr->type == NODE_ERR) {
        return print_node->expr;
    }

    return (ASTNode*)print_node;
}

static ASTNode* if_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_IF);

    IfStatementNode* if_node =
        arena_alloc(parser->arena, sizeof(IfStatementNode));
    if_node->type = NODE_IF;

    tk = next_token(parser);
    if (tk.type != TK_LPAREN) {
        return error(parser, parser->pre_token_pos, "Expected '('");
    }

    if_node->expr = expr(parser, 0);
    if (if_node->expr->type == NODE_ERR) {
        return if_node->expr;
    }

    tk = next_token(parser);
    if (tk.type != TK_RPAREN) {
        return error(parser, parser->pre_token_pos, "Expected ')'");
    }

    if_node->then_block = stmt(parser);
    if (if_node->then_block && if_node->then_block->type == NODE_ERR) {
        return if_node->then_block;
    }

    tk = peek_token(parser);
    if (tk.type == TK_ELSE) {
        next_token(parser);

        if_node->else_block = stmt(parser);
        if (if_node->else_block && if_node->else_block->type == NODE_ERR) {
            return if_node->else_block;
        }
    }

    return (ASTNode*)if_node;
}

static ASTNode* while_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_WHILE);

    WhileNode* while_node = arena_alloc(parser->arena, sizeof(WhileNode));
    while_node->type = NODE_WHILE;

    tk = next_token(parser);
    if (tk.type != TK_LPAREN) {
        return error(parser, parser->pre_token_pos, "Expected '('");
    }

    while_node->expr = expr(parser, 0);
    if (while_node->expr->type == NODE_ERR) {
        return while_node->expr;
    }

    tk = next_token(parser);
    if (tk.type != TK_RPAREN) {
        return error(parser, parser->pre_token_pos, "Expected ')'");
    }

    while_node->block = stmt(parser);
    if (while_node->block && while_node->block->type == NODE_ERR) {
        return while_node->block;
    }

    return (ASTNode*)while_node;
}

static ASTNode* scope(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_LBRACE);

    SymbolTable* sym = arena_alloc(parser->sym->arena, sizeof(SymbolTable));
    symbol_table_init(sym, parser->sym->arena);
    sym->parent = parser->sym;
    parser->sym = sym;

    ASTNode* node = stmt_list(parser, 1);
    if (node->type == NODE_ERR)
        return node;

    tk = next_token(parser);
    if (tk.type != TK_RBRACE) {
        return error(parser, parser->pre_token_pos, "Expected '}'");
    }

    parser->sym = sym->parent;

    return node;
}

static ASTNode* stmt(ParserState* parser) {
    ASTNode* node;
    Token tk = peek_token(parser);
    switch (tk.type) {
        case TK_SEMICOLON:
            tk = next_token(parser);
            return NULL;

        case TK_PRINT:
            node = print_stmt(parser);
            if (node->type == NODE_ERR)
                return node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos, "Expected ';'");
            }
            break;

        case TK_IF:
            node = if_stmt(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_WHILE:
            node = while_stmt(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_LBRACE:
            node = scope(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_STR: {
            next_token(parser);
            StrLitNode* str_node =
                arena_alloc(parser->arena, sizeof(StrLitNode));
            str_node->type = NODE_STRLIT;
            str_node->str = tk.str;
            node = (ASTNode*)str_node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos, "Expected ';'");
            }
        } break;

        default:
            node = expr(parser, 0);
            if (node->type == NODE_ERR)
                return node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos, "Expected ';'");
            }
    }
    return node;
}

static ASTNode* stmt_list(ParserState* parser, int in_scope) {
    StatementListNode* stmts =
        arena_alloc(parser->arena, sizeof(StatementListNode));
    stmts->type = NODE_STMTS;
    stmts->stmts = NULL;

    for (Token tk = peek_token(parser);
         tk.type != TK_NUL && (!in_scope || tk.type != TK_RBRACE);
         tk = peek_token(parser)) {
        ASTNode* node;
        if (tk.type == TK_DECL) {
            node = var_decl(parser);
            if (node->type == NODE_ERR)
                return node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos, "Expected ';'");
            }
        } else {
            node = stmt(parser);
        }

        if (node == NULL)
            continue;

        if (node->type == NODE_ERR)
            return node;

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

    return stmt_list(parser, 0);
}
