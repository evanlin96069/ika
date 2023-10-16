#include "symbol_table.h"

#include <string.h>

static inline int djb2_hash(char* str) {
    int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

void symbol_table_init(SymbolTable* table, Arena* arena) {
    table->parent = NULL;
    table->arena = arena;
    table->ste = NULL;
}

SymbolTableEntry* symbol_table_append(SymbolTable* table, char* ident) {
    SymbolTableEntry* ste = arena_alloc(table->arena, sizeof(SymbolTableEntry));
    memcpy(ste->ident, ident, IDENT_MAX_LENGTH);
    ste->hash = djb2_hash(ident);
    ste->next = table->ste;
    ste->val = 0;
    table->ste = ste;
    return ste;
}

SymbolTableEntry* symbol_table_find(SymbolTable* table, char* ident,
                                    int in_current_scope) {
    if (!table)
        return NULL;

    SymbolTableEntry* ste = table->ste;

    int hash = djb2_hash(ident);

    while (ste) {
        if (ste->hash == hash && strcmp(ste->ident, ident) == 0) {
            return ste;
        }
        ste = ste->next;
    }

    if (!in_current_scope)
        return symbol_table_find(table->parent, ident, 0);
    return NULL;
}
