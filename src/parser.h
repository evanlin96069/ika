#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "arena.h"

typedef struct ParserState {
    Arena* arena;
    LexerState lexer;
} ParserState;

typedef enum ASTNodeType {
    NODE_ERR = -1,
    NODE_LIT,
    NODE_BINOP,
    NODE_UNARYOP,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
} ASTNode;

typedef struct LitNode {
    ASTNodeType type;
    int val;
} LitNode;

typedef struct BinOpNode {
    ASTNodeType type;
    char op;
    ASTNode* left;
    ASTNode* right;
} BinOpNode;

typedef struct UnaryOpNode {
    ASTNodeType type;
    char op;
    ASTNode* node;
} UnaryOpNode;

typedef struct ErrorNode {
    ASTNodeType type;
    size_t pos;
    Token token;
    const char* msg;
} ErrorNode;

void parser_init(ParserState* parser, Arena* arena);
ASTNode* parser_parse(ParserState* parser, const char* src);

#endif
