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
    TK_STR,
    TK_DECL,
    TK_FUNC,
    TK_RET,
    TK_PRINT,
    TK_IF,
    TK_ELSE,
    TK_WHILE,

    TK_MUL,
    TK_DIV,
    TK_MOD,
    TK_ADD,
    TK_SUB,
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

    TK_NOT,
    TK_LNOT,

    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_SEMICOLON,
    TK_COMMA,
} TokenType;

typedef struct Token {
    TokenType type;
    union {
        int val;
        Str str;
    };
} Token;

// AST

typedef enum ASTNodeType {
    NODE_ERR = -1,
    NODE_STMTS,
    NODE_INTLIT,
    NODE_STRLIT,
    NODE_BINARYOP,
    NODE_UNARYOP,
    NODE_VAR,
    NODE_CALL,
    NODE_RET,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_IF,
    NODE_WHILE,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
} ASTNode;

typedef struct IntLitNode {
    ASTNodeType type;
    int val;
} IntLitNode;

typedef struct StrLitNode {
    ASTNodeType type;
    Str str;
} StrLitNode;

typedef struct BinaryOpNode {
    ASTNodeType type;
    char op;
    ASTNode* left;
    ASTNode* right;
} BinaryOpNode;

typedef struct UnaryOpNode {
    ASTNodeType type;
    char op;
    ASTNode* node;
} UnaryOpNode;

typedef struct VarNode {
    ASTNodeType type;
    VarSymbolTableEntry* ste;
} VarNode;

typedef struct AssignNode {
    ASTNodeType type;
    VarNode* left;
    ASTNode* right;
} AssignNode;

typedef struct PrintNode {
    ASTNodeType type;
    ASTNode* expr;
} PrintNode;

typedef struct IfStatementNode {
    ASTNodeType type;
    ASTNode* expr;
    ASTNode* then_block;
    ASTNode* else_block;
} IfStatementNode;

typedef struct WhileNode {
    ASTNodeType type;
    ASTNode* expr;
    ASTNode* block;
} WhileNode;

typedef struct ErrorNode {
    ASTNodeType type;
    size_t pos;
    char msg[ERROR_MAX_LENGTH];
} ErrorNode;

typedef struct ASTNodeList ASTNodeList;
struct ASTNodeList {
    ASTNode* node;
    ASTNodeList* next;
};

typedef struct StatementListNode {
    ASTNodeType type;
    ASTNodeList* stmts;
    ASTNodeList* _tail;
} StatementListNode;

typedef struct CallNode {
    ASTNodeType type;
    FuncSymbolTableEntry* ste;
    ASTNodeList* args;
} CallNode;

typedef struct ReturnNode {
    ASTNodeType type;
    ASTNode* expr;
} ReturnNode;

typedef struct ParserState {
    Arena* arena;
    SymbolTable* sym;
    SymbolTable* global_sym;

    const char* src;
    size_t pos;

    Token token;
    size_t pre_token_pos;
    size_t post_token_pos;
} ParserState;

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena);
ASTNode* parser_parse(ParserState* parser, const char* src);

#endif
