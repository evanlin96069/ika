#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "error.h"
#include "lexer.h"
#include "preprocessor.h"
#include "symbol_table.h"

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
    NODE_PRINT,
    NODE_RET,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_GOTO,
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
    Str val;
} StrLitNode;

typedef struct BinaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TokenType op;
    ASTNode* left;
    ASTNode* right;
} BinaryOpNode;

typedef struct UnaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TokenType op;
    ASTNode* node;
} UnaryOpNode;

typedef struct VarNode {
    ASTNodeType type;
    SymbolTableEntry* ste;
} VarNode;

typedef struct AssignNode {
    ASTNodeType type;
    SourcePos pos;
    TokenType op;
    ASTNode* left;
    ASTNode* right;
} AssignNode;

typedef struct IfStatementNode {
    ASTNodeType type;
    ASTNode* expr;
    ASTNode* then_block;
    ASTNode* else_block;
} IfStatementNode;

typedef struct WhileNode {
    ASTNodeType type;
    ASTNode* expr;
    ASTNode* inc;
    ASTNode* block;
} WhileNode;

typedef struct GotoNode {
    ASTNodeType type;
    SourcePos pos;
    TokenType op;
} GotoNode;

typedef struct ErrorNode {
    ASTNodeType type;
    Error* val;
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
    ASTNode* node;
    ASTNodeList* args;
} CallNode;

typedef struct PrintNode {
    ASTNodeType type;
    Str fmt;
    ASTNodeList* args;
} PrintNode;

typedef struct ReturnNode {
    ASTNodeType type;
    ASTNode* expr;
} ReturnNode;

// Parser

typedef struct ParserState {
    Arena* arena;
    SymbolTable* sym;
    SymbolTable* global_sym;

    SourceState* src;
    size_t line;
    size_t pos;  // current pos

    Token token;
    SourcePos pre_token_pos;   // pos at the end of previous token
    SourcePos post_token_pos;  // pos at the start of this token
} ParserState;

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena);
ASTNode* parser_parse(ParserState* parser, SourceState* src);

#endif
