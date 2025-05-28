#ifndef PARSER_H
#define PARSER_H

#include "arena.h"
#include "error.h"
#include "lexer.h"
#include "preprocessor.h"
#include "symbol_table.h"
#include "type.h"

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
    NODE_TYPE,
    NODE_INDEXOF,
    NODE_FIELD,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    SourcePos pos;
} ASTNode;

typedef struct IntLitNode {
    ASTNodeType type;
    SourcePos pos;
    int val;
    PrimitiveType data_type;
} IntLitNode;

typedef struct StrLitNode {
    ASTNodeType type;
    SourcePos pos;
    Str val;
} StrLitNode;

typedef struct BinaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TkType op;
    ASTNode* left;
    ASTNode* right;
} BinaryOpNode;

typedef struct UnaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TkType op;
    ASTNode* node;
} UnaryOpNode;

typedef struct VarNode {
    ASTNodeType type;
    SourcePos pos;
    SymbolTableEntry* ste;
} VarNode;

typedef struct AssignNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* left;
    ASTNode* right;
} AssignNode;

typedef struct IfStatementNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* expr;
    ASTNode* then_block;
    ASTNode* else_block;
} IfStatementNode;

typedef struct WhileNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* expr;
    ASTNode* inc;
    ASTNode* block;
} WhileNode;

typedef struct GotoNode {
    ASTNodeType type;
    SourcePos pos;
    TkType op;
} GotoNode;

typedef struct ErrorNode {
    ASTNodeType type;
    SourcePos pos;
    Error* val;
} ErrorNode;

typedef struct ASTNodeList ASTNodeList;
struct ASTNodeList {
    ASTNode* node;
    ASTNodeList* next;
};

typedef struct StatementListNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNodeList* stmts;
    ASTNodeList* _tail;
} StatementListNode;

typedef struct CallNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* node;
    ASTNodeList* args;
} CallNode;

typedef struct PrintNode {
    ASTNodeType type;
    SourcePos pos;
    Str fmt;
    ASTNodeList* args;
} PrintNode;

typedef struct ReturnNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* expr;
} ReturnNode;

typedef struct IndexOfNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* left;
    ASTNode* right;
} IndexOfNode;

typedef struct FieldNode {
    ASTNodeType type;
    SourcePos pos;
    ASTNode* node;
    Str ident;
} FieldNode;

typedef struct TypeNode {
    ASTNodeType type;
    SourcePos pos;
    const Type* data_type;
} TypeNode;

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
