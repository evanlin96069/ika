#include "symbol_table.h"

static inline int djb2_hash(Str s) {
    int hash = 5381;
    for (size_t i = 0; i < s.len; i++) {
        hash = ((hash << 5) + hash) + s.ptr[i];
    }

    return hash;
}

void symbol_table_init(SymbolTable* table, Arena* arena) {
    table->parent = NULL;
    table->arena = arena;
    table->ste = NULL;
}

SymbolTableEntry* symbol_table_append(SymbolTable* table, Str ident) {
    SymbolTableEntry* ste = arena_alloc(table->arena, sizeof(SymbolTableEntry));
    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->next = table->ste;
    ste->val = 0;
    table->ste = ste;
    return ste;
}

SymbolTableEntry* symbol_table_find(SymbolTable* table, Str ident,
                                    int in_current_scope) {
    if (!table)
        return NULL;

    SymbolTableEntry* ste = table->ste;

    int hash = djb2_hash(ident);

    while (ste) {
        if (ste->hash == hash && ste->ident.len == ident.len &&
            str_eql(ste->ident, ident)) {
            return ste;
        }
        ste = ste->next;
    }

    if (!in_current_scope)
        return symbol_table_find(table->parent, ident, 0);
    return NULL;
}
