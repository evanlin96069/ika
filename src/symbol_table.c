#include "symbol_table.h"

static inline int djb2_hash(Str s) {
    int hash = 5381;
    for (int i = 0; i < s.len; i++) {
        hash = ((hash << 5) + hash) + s.ptr[i];
    }

    return hash;
}

void symbol_table_init(SymbolTable* sym, int offset, int* stack_size,
                       int is_global, UtlArenaAllocator* arena) {
    sym->offset = offset;
    sym->is_global = is_global;
    sym->parent = NULL;
    sym->arena = arena;
    sym->ste = NULL;

    if (!stack_size) {
        stack_size = utlarena_alloc(arena, sizeof(int));
        *stack_size = 0;
    }
    sym->stack_size = stack_size;

    sym->arg_size = 0;                // fill in during parsing
    sym->arg_offset = 8;              // saved ebp + return address
    sym->max_struct_return_size = 0;  // fill in during type check
}

static inline void symbol_table_append(SymbolTable* sym,
                                       SymbolTableEntry* ste) {
    ste->sym = sym;
    ste->next = sym->ste;
    sym->ste = ste;
}

VarSymbolTableEntry* symbol_table_append_var(SymbolTable* sym, Str ident,
                                             int is_arg, int is_extern,
                                             const Type* data_type,
                                             SourcePos pos) {
    VarSymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(VarSymbolTableEntry));
    ste->type = SYM_VAR;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->pos = pos;

    ste->is_arg = is_arg;
    ste->is_extern = is_extern;
    ste->is_global = sym->is_global;

    ste->data_type = data_type;
    ste->init_val = NULL;

    int size = data_type->size;
    int alignment = data_type->alignment;

    if (is_extern == 0) {
        if (is_arg) {
            // Arguments
            if (size < 4) {
                size = 4;
            }
            alignment = MAX_ALIGNMENT;
            int alignment_off = sym->arg_size % alignment;
            if (alignment_off != 0) {
                sym->arg_size += alignment - alignment_off;
            }
            ste->offset = sym->arg_size;
            sym->arg_size += size;
        } else {
            // Variables
            int alignment_off = sym->offset % alignment;
            if (alignment_off != 0) {
                sym->offset += alignment - alignment_off;
            }

            if (ste->is_global) {
                ste->offset = sym->offset;
                sym->offset += size;
            } else {
                ste->offset = sym->offset + size;  // this is negative offset
                sym->offset += size;
            }
        }

        if (ste->is_global == 0 && *sym->stack_size < sym->offset) {
            *sym->stack_size = sym->offset;
            int alignment_off = sym->offset % MAX_ALIGNMENT;
            if (alignment_off != 0) {
                *sym->stack_size += MAX_ALIGNMENT - alignment_off;
            }
        }
    }

    symbol_table_append(sym, (SymbolTableEntry*)ste);

    return ste;
}

FieldSymbolTableEntry* symbol_table_append_field(SymbolTable* sym, Str ident,
                                                 const Type* data_type,
                                                 SourcePos pos) {
    FieldSymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(FieldSymbolTableEntry));
    ste->type = SYM_FIELD;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->pos = pos;

    ste->data_type = data_type;

    int size = data_type->size;
    int alignment = data_type->alignment;

    int alignment_off = sym->offset % alignment;
    if (alignment_off != 0) {
        sym->offset += alignment - alignment_off;
    }

    ste->offset = sym->offset;
    sym->offset += size;

    if (*sym->stack_size < sym->offset) {
        *sym->stack_size = sym->offset;
        int alignment_off = sym->offset % MAX_ALIGNMENT;
        if (alignment_off != 0) {
            *sym->stack_size += MAX_ALIGNMENT - alignment_off;
        }
    }

    symbol_table_append(sym, (SymbolTableEntry*)ste);

    return ste;
}

DefSymbolTableEntry* symbol_table_append_def(SymbolTable* sym, Str ident,
                                             DefSymbolValue val,
                                             SourcePos pos) {
    DefSymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(DefSymbolTableEntry));
    ste->type = SYM_DEF;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->pos = pos;

    ste->val = val;

    symbol_table_append(sym, (SymbolTableEntry*)ste);

    return ste;
}

FuncSymbolTableEntry* symbol_table_append_func(SymbolTable* sym, Str ident,
                                               int is_extern, SourcePos pos) {
    FuncSymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(FuncSymbolTableEntry));
    ste->type = SYM_FUNC;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->pos = pos;

    ste->is_extern = is_extern;
    // ste->func_data = func_data; // fill in during parsing

    ste->node = NULL;      // fill in during parsing
    ste->func_sym = NULL;  // fill in during parsing

    symbol_table_append(sym, (SymbolTableEntry*)ste);

    return ste;
}

TypeSymbolTableEntry* symbol_table_append_type(SymbolTable* sym, Str ident,
                                               SourcePos pos) {
    TypeSymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(TypeSymbolTableEntry));
    ste->type = SYM_TYPE;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);
    ste->pos = pos;

    ste->incomplete = 1;
    ste->size = 0;
    ste->alignment = 0;
    ste->name_space = NULL;

    symbol_table_append(sym, (SymbolTableEntry*)ste);

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

SymbolTableEntry* symbol_table_append_sym(SymbolTable* sym, Str ident) {
    SymbolTableEntry* ste =
        utlarena_alloc(sym->arena, sizeof(SymbolTableEntry));
    ste->type = SYM_NONE;

    ste->ident = ident;
    ste->hash = djb2_hash(ident);

    symbol_table_append(sym, ste);
    return ste;
}

int symbol_table_remove(SymbolTable* sym, Str ident) {
    if (!sym)
        return 0;

    SymbolTableEntry* prev = NULL;
    SymbolTableEntry* curr = sym->ste;

    int hash = djb2_hash(ident);

    while (curr) {
        if (curr->hash == hash && str_eql(curr->ident, ident)) {
            if (prev) {
                prev->next = curr->next;
            } else {
                sym->ste = curr->next;
            }
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }

    return 0;
}
