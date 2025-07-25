#ifndef AST_H
#define AST_H

#include "error.h"
#include "lexer.h"
#include "source.h"
#include "symbol_table.h"
#include "type.h"

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
    NODE_CAST,
    NODE_ASM,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;
} ASTNode;

typedef struct TypedASTNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;
} TypedASTNode;

typedef struct IntLitNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    int val;
    PrimitiveType data_type;
} IntLitNode;

typedef struct StrLitNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    Str val;
} StrLitNode;

typedef struct BinaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    TkType op;
    ASTNode* left;
    ASTNode* right;
} BinaryOpNode;

typedef struct UnaryOpNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    TkType op;
    ASTNode* node;
} UnaryOpNode;

typedef struct VarNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    SymbolTableEntry* ste;
} VarNode;

typedef struct AssignNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    ASTNode* left;
    ASTNode* right;
    int from_decl;
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
    TypeInfo type_info;

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
    TypeInfo type_info;

    ASTNode* left;
    ASTNode* right;
} IndexOfNode;

typedef struct FieldNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    ASTNode* node;
    Str ident;
} FieldNode;

typedef struct TypeNode {
    ASTNodeType type;
    SourcePos pos;

    const Type* data_type;
} TypeNode;

typedef struct CastNode {
    ASTNodeType type;
    SourcePos pos;
    TypeInfo type_info;

    const Type* data_type;
    ASTNode* expr;
} CastNode;

typedef struct AsmNode {
    ASTNodeType type;
    SourcePos pos;

    Str asm_str;
} AsmNode;

static inline TypedASTNode* as_typed_ast(ASTNode* node) {
    switch (node->type) {
        case NODE_INTLIT:
        case NODE_STRLIT:
        case NODE_BINARYOP:
        case NODE_UNARYOP:
        case NODE_VAR:
        case NODE_CALL:
        case NODE_ASSIGN:
        case NODE_INDEXOF:
        case NODE_FIELD:
        case NODE_CAST:
            return (TypedASTNode*)node;
        default:
            UNREACHABLE();
    }
}

#endif
