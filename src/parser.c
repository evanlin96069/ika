#include "parser.h"

#include <stdlib.h>

static ASTNode* expr(ParserState* parser);
static ASTNode* primary(ParserState* parser);
static ASTNode* term(ParserState* parser);
static ASTNode* error(ParserState* parser, const char* msg);

void parser_init(ParserState* parser, Arena* arena) {
    parser->arena = arena;
}

static ASTNode* error(ParserState* parser, const char* msg) {
    ErrorNode* err = arena_alloc(parser->arena, sizeof(ErrorNode));
    err->type = NODE_ERR;
    err->pos = parser->lexer.pos;
    err->token = parser->lexer.token;
    err->msg = msg;
    return (ASTNode*)err;
}

static ASTNode* primary(ParserState* parser) {
    lexer_next(&parser->lexer);
    switch (parser->lexer.token.type) {
        case TK_INT: {
            LitNode* node = arena_alloc(parser->arena, sizeof(LitNode));
            node->type = NODE_LIT;
            node->val = parser->lexer.token.val;
            return (ASTNode*)node;
        }
        case TK_LBRACK: {
            ASTNode* node = expr(parser);
            if (node->type == NODE_ERR) {
                return node;
            }
            lexer_next(&parser->lexer);
            if (parser->lexer.token.type != TK_RBRACK) {
                return error(parser, "Expected a close bracket");
            }
            return node;
        }

        case TK_ADD:
        case TK_SUB: {
            UnaryOpNode* node = arena_alloc(parser->arena, sizeof(UnaryOpNode));
            node->type = NODE_UNARYOP;
            node->op = parser->lexer.token.type;
            node->node = expr(parser);
            if (node->node->type == NODE_ERR) {
                return node->node;
            }
            return (ASTNode*)node;
        }

        case TK_ERR:
            return error(parser, "Unknown token");

        default:
            return error(parser, "Unexpected token");
    }
}

static ASTNode* term(ParserState* parser) {
    ASTNode* node = primary(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = lexer_peek(&parser->lexer);
    while (tk.type == TK_MUL || tk.type == TK_DIV) {
        lexer_next(&parser->lexer);
        BinOpNode* binop = arena_alloc(parser->arena, sizeof(BinOpNode));
        binop->type = NODE_BINOP;
        binop->op = parser->lexer.token.type;
        binop->left = node;
        binop->right = expr(parser);
        if (binop->right->type == NODE_ERR) {
            return binop->right;
        }
        node = (ASTNode*)binop;

        tk = lexer_peek(&parser->lexer);
    }

    return node;
}

static ASTNode* expr(ParserState* parser) {
    ASTNode* node = term(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = lexer_peek(&parser->lexer);
    while (tk.type == TK_ADD || tk.type == TK_SUB) {
        lexer_next(&parser->lexer);
        BinOpNode* binop = arena_alloc(parser->arena, sizeof(BinOpNode));
        binop->type = NODE_BINOP;
        binop->op = parser->lexer.token.type;;
        binop->left = node;
        binop->right = expr(parser);
        if (binop->right->type == NODE_ERR) {
            return binop->right;
        }
        node = (ASTNode*)binop;

        tk = lexer_peek(&parser->lexer);
    }

    return node;
}
ASTNode* parser_parse(ParserState* parser, const char* src) {
    lexer_init(&parser->lexer, src);
    arena_reset(parser->arena);

    ASTNode* node = expr(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    Token tk = lexer_peek(&parser->lexer);
    if (tk.type != TK_NUL) {
        return error(parser, "Unexpected trailing token");
    }

    return node;
}
