#include "symbol_table.h"

#include "parser.h"

static inline int djb2_hash(Str s) {
    int hash = 5381;
    for (int i = 0; i < s.len; i++) {
        hash = ((hash << 5) + hash) + s.ptr[i];
    }

    return hash;
}

void symbol_table_init(SymbolTable* sym, int offset, int* stack_size,
                       int is_global, Arena* arena) {
    sym->offset = offset;
    sym->is_global = is_global;
    sym->parent = NULL;
    sym->arena = arena;
    sym->ste = NULL;
    if (!stack_size) {
        stack_size = arena_alloc(arena, sizeof(int));
        *stack_size = 0;
    }
    sym->stack_size = stack_size;
    sym->arg_size = 0;  // fill in during parsing
}

VarSymbolTableEntry* symbol_table_append_var(SymbolTable* sym, Str ident,
                                             int is_arg) {
    VarSymbolTableEntry* ste =
        arena_alloc(sym->arena, sizeof(VarSymbolTableEntry));
    ste->type = SYM_VAR;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->is_global = sym->is_global;

    if (is_arg) {
        ste->offset = sym->arg_size + 8;
        sym->arg_size += 4;
    } else {
        if (ste->is_global) {
            ste->offset = sym->offset;
        } else {
            ste->offset = -sym->offset - 4;
        }
        sym->offset += 4;
    }

    if (ste->is_global == 0 && *sym->stack_size < sym->offset) {
        *sym->stack_size = sym->offset;
    }

    ste->next = NULL;
    if (!sym->ste) {
        sym->ste = sym->_tail = (SymbolTableEntry*)ste;
    } else {
        sym->_tail->next = (SymbolTableEntry*)ste;
        sym->_tail = sym->_tail->next;
    }

    return ste;
}

FuncSymbolTableEntry* symbol_table_append_func(SymbolTable* sym, Str ident) {
    FuncSymbolTableEntry* ste =
        arena_alloc(sym->arena, sizeof(FuncSymbolTableEntry));
    ste->type = SYM_FUNC;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);

    ste->node = NULL;  // fill in during parsing
    ste->sym = NULL;   // fill in during parsing

    ste->next = NULL;
    if (!sym->ste) {
        sym->ste = sym->_tail = (SymbolTableEntry*)ste;
    } else {
        sym->_tail->next = (SymbolTableEntry*)ste;
        sym->_tail = sym->_tail->next;
    }

    return ste;
}

SymbolTableEntry* symbol_table_find(SymbolTable* sym, Str ident,
                                    int in_current_scope) {
    if (!sym)
        return NULL;

    SymbolTableEntry* ste = sym->ste;

    int hash = djb2_hash(ident);

    while (ste) {
        if (ste->hash == hash && str_eql(ste->ident, ident)) {
            return ste;
        }
        ste = ste->next;
    }

    if (!in_current_scope)
        return symbol_table_find(sym->parent, ident, 0);
    return NULL;
}
