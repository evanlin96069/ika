#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "source.h"
#include "str.h"
#include "type.h"
#include "utl/allocator/utlarena.h"

struct ASTNode;

typedef enum SymbolType {
    SYM_NONE,
    SYM_VAR,
    SYM_DEF,
    SYM_FUNC,
    SYM_TYPE,
    SYM_FIELD,
} SymbolType;

typedef struct SymbolTable SymbolTable;
typedef struct SymbolTableEntry SymbolTableEntry;

typedef struct VarSymbolTableEntry VarSymbolTableEntry;
typedef struct FieldSymbolTableEntry FieldSymbolTableEntry;
typedef struct DefSymbolTableEntry DefSymbolTableEntry;
typedef struct FuncSymbolTableEntry FuncSymbolTableEntry;
typedef struct TypeSymbolTableEntry TypeSymbolTableEntry;

typedef enum SymbolAttr {
    SYM_ATTR_NONE,
    SYM_ATTR_EXPORT,
    SYM_ATTR_EXTERN,
} SymbolAttr;

struct SymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;  // refernece back to the symbol table
};

struct VarSymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;

    int is_arg;
    SymbolAttr attr;
    int is_global;
    int offset;
    const Type* data_type;
    struct ASTNode* init_val;  // only used for global variable
};

struct FieldSymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;

    int offset;
    const Type* data_type;
};

typedef struct DefSymbolValue {
    int is_str;
    union {
        Str str;
        unsigned int val;
    };
    PrimitiveType data_type;  // for val
} DefSymbolValue;

struct DefSymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;

    DefSymbolValue val;
};

struct FuncSymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;

    SymbolAttr attr;
    FuncMetadata func_data;

    struct ASTNode* node;
    SymbolTable* func_sym;
};

struct TypeSymbolTableEntry {
    Str ident;
    int hash;
    SourcePos pos;
    SymbolTableEntry* next;
    SymbolType type;
    SymbolTable* sym;

    int incomplete;
    int size;
    int alignment;
    SymbolTable* name_space;
};

struct SymbolTable {
    SymbolTable* parent;
    UtlArenaAllocator* arena;
    SymbolTableEntry* ste;
    int* stack_size;  // total stack size
    int is_global;    // is global symbol table
    int offset;       // offset for local variables
    int arg_size;     // total size of arguments
    int arg_offset;   // offset for the arguments (usually saved ebp + return
                      // address = 8)
    int max_struct_return_size;  // size of the max return type in this function
};

void symbol_table_init(SymbolTable* sym, int offset, int* stack_size,
                       int is_global, UtlArenaAllocator* arena);

VarSymbolTableEntry* symbol_table_append_var(SymbolTable* sym, Str ident,
                                             int is_arg, SymbolAttr attr,
                                             const Type* data_type,
                                             SourcePos pos);
FieldSymbolTableEntry* symbol_table_append_field(SymbolTable* sym, Str ident,
                                                 const Type* data_type,
                                                 int packed, SourcePos pos);
DefSymbolTableEntry* symbol_table_append_def(SymbolTable* sym, Str ident,
                                             DefSymbolValue val, SourcePos pos);
FuncSymbolTableEntry* symbol_table_append_func(SymbolTable* sym, Str ident,
                                               SymbolAttr attr, SourcePos pos);
TypeSymbolTableEntry* symbol_table_append_type(SymbolTable* sym, Str ident,
                                               SourcePos pos);

SymbolTableEntry* symbol_table_find(SymbolTable* sym, Str ident,
                                    int in_current_scope);

/*
 *These are for preprocessor symbols (#define, #undef)
 */

// Append an empty symbol
SymbolTableEntry* symbol_table_append_sym(SymbolTable* sym, Str ident);

// Remove a symbol
// return 1 if found and removed
int symbol_table_remove(SymbolTable* sym, Str ident);

#endif
