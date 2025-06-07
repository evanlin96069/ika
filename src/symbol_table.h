#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "arena.h"
#include "preprocessor.h"
#include "str.h"
#include "type.h"

struct ASTNode;

typedef enum SymbolType {
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
    int is_extern;
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

    int is_extern;
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
    Arena* arena;
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
                       int is_global, Arena* arena);

VarSymbolTableEntry* symbol_table_append_var(SymbolTable* sym, Str ident,
                                             int is_arg, int is_extern,
                                             const Type* data_type,
                                             SourcePos pos);
FieldSymbolTableEntry* symbol_table_append_field(SymbolTable* sym, Str ident,
                                                 const Type* data_type,
                                                 SourcePos pos);
DefSymbolTableEntry* symbol_table_append_def(SymbolTable* sym, Str ident,
                                             DefSymbolValue val, SourcePos pos);
FuncSymbolTableEntry* symbol_table_append_func(SymbolTable* sym, Str ident,
                                               int is_extern, SourcePos pos);
TypeSymbolTableEntry* symbol_table_append_type(SymbolTable* sym, Str ident,
                                               SourcePos pos);

SymbolTableEntry* symbol_table_find(SymbolTable* sym, Str ident,
                                    int in_current_scope);

#endif
