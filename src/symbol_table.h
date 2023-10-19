#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "arena.h"
#include "str.h"

typedef struct SymbolTableEntry SymbolTableEntry;
struct SymbolTableEntry {
    Str ident;
    int hash;
    int offset;
    SymbolTableEntry* next;
};

typedef struct SymbolTable SymbolTable;
struct SymbolTable {
    SymbolTable* parent;
    Arena* arena;
    SymbolTableEntry* ste;
    int* stack_size;
    int offset;
};

void symbol_table_init(SymbolTable* table, int offset, int* stack_size,
                       Arena* arena);
SymbolTableEntry* symbol_table_append(SymbolTable* table, Str ident);
SymbolTableEntry* symbol_table_find(SymbolTable* table, Str ident,
                                    int in_current_scope);

#endif
