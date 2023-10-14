#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "symbol_table.h"

#define ERROR_MAX_LENGTH 255

// Lexer

typedef enum TokenType {
    TK_ERR = -1,
    TK_NUL = 0,
    TK_IDENT,
    TK_INT,
    TK_DECL,
    TK_ADD = '+',
    TK_SUB = '-',
    TK_MUL = '*',
    TK_DIV = '/',
    TK_ASSIGN = '=',
    TK_LBRACK = '(',
    TK_RBRACK = ')',
    TK_SEMICOLON = ';',
} TokenType;

typedef struct Token {
    TokenType type;
    union {
        int val;
        char* ident;
    };
} Token;

// AST

typedef enum ASTNodeType {
    NODE_ERR = -1,
    NODE_LIT,
    NODE_BINOP,
    NODE_UNARYOP,
    NODE_VAR,
    NODE_ASSIGN,
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

typedef struct VarNode {
    ASTNodeType type;
    SymbolTableEntry* ste;
} VarNode;

typedef struct AssignNode {
    ASTNodeType type;
    VarNode* left;
    ASTNode* right;
} AssignNode;

typedef struct ErrorNode {
    ASTNodeType type;
    char msg[ERROR_MAX_LENGTH];
} ErrorNode;

typedef struct ParserState {
    Arena* arena;
    SymbolTable* sym;

    const char* src;
    size_t pos;

    Token token;
    size_t token_pos;
} ParserState;

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena);
ASTNode* parser_parse(ParserState* parser, const char* src);

#endif
