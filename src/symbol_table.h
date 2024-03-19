#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "arena.h"
#include "str.h"

struct ASTNode;

typedef enum SymbolType {
    SYM_VAR,
    SYM_FUNC,
} SymbolType;

typedef struct SymbolTable SymbolTable;
typedef struct SymbolTableEntry SymbolTableEntry;

struct SymbolTableEntry {
    Str ident;
    int hash;
    SymbolTableEntry* next;
    SymbolType type;
};

typedef struct VarSymbolTableEntry {
    Str ident;
    int hash;
    SymbolTableEntry* next;
    SymbolType type;

    int is_global;
    int offset;
} VarSymbolTableEntry;

typedef struct FuncSymbolTableEntry {
    Str ident;
    int hash;
    SymbolTableEntry* next;
    SymbolType type;

    int label;
    struct ASTNode* node;
    SymbolTable* sym;
} FuncSymbolTableEntry;

struct SymbolTable {
    SymbolTable* parent;
    Arena* arena;
    SymbolTableEntry* ste;
    SymbolTableEntry* _tail;
    int* stack_size;
    int is_global;
    int offset;
    int arg_size;
};

void symbol_table_init(SymbolTable* sym, int offset, int* stack_size,
                       int is_global, Arena* arena);
VarSymbolTableEntry* symbol_table_append_var(SymbolTable* sym, Str ident,
                                             int is_arg);
FuncSymbolTableEntry* symbol_table_append_func(SymbolTable* sym, Str ident);
SymbolTableEntry* symbol_table_find(SymbolTable* sym, Str ident,
                                    int in_current_scope);

#endif
