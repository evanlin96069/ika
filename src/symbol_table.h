#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "arena.h"

#define IDENT_MAX_LENGTH 255

typedef struct SymbolTableEntry SymbolTableEntry;
struct SymbolTableEntry {
    char ident[IDENT_MAX_LENGTH];
    int hash;
    int val;
    SymbolTableEntry* next;
};

typedef struct SymbolTable SymbolTable;
struct SymbolTable {
    SymbolTable* parent;
    Arena* arena;
    SymbolTableEntry* ste;
};

void symbol_table_init(SymbolTable* table, Arena* arena);
SymbolTableEntry* symbol_table_append(SymbolTable* table, char* ident);
SymbolTableEntry* symbol_table_find(SymbolTable* table, char* ident,
                                    int in_current_scope);

#endif
