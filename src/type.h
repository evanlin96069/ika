#ifndef TYPE_H
#define TYPE_H

#include <assert.h>

#define MAX_ALIGNMENT 4
#define PTR_SIZE 4

typedef enum PrimitiveType {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
} PrimitiveType;

typedef enum TypeMetadataType {
    METADATA_PRIMITIVE,
    METADATA_TYPE,
    METADATA_ARRAY,
    METADATA_POINTER,
    METADATA_FUNC,
} TypeMetadataType;

typedef struct Type Type;

typedef struct ArgList {
    struct ArgList* next;
    const Type* type;
} ArgList;

typedef struct FuncMetadata {
    const Type* return_type;  // NULL if return void
    ArgList* args;
    int has_va_args;
} FuncMetadata;

struct Type {
    int incomplete;

    // If not incomplete
    int size;
    int alignment;

    TypeMetadataType type;
    union {
        PrimitiveType primitive_type;           // METADATA_PRIMITIVE
        struct TypeSymbolTableEntry* type_ste;  // METADATA_TYPE
        int array_size;                         // METADATA_ARRAY
        int pointer_level;                      // METADATA_ARRAY
        FuncMetadata func_data;                 // METADATA_FUNC
    };

    const struct Type* inner_type;
};

static inline int is_ptr(const Type* type) {
    return type->type == METADATA_POINTER;
}

static inline int is_array_ptr(const Type* type) {
    return type->type == METADATA_ARRAY && type->array_size == 0;
}

static inline int is_ptr_like(const Type* type) {
    return is_ptr(type) || is_array_ptr(type);
}

static inline int is_void(const Type* type) {
    return type->type == METADATA_PRIMITIVE &&
           type->primitive_type == TYPE_VOID;
}

static inline int is_void_ptr(const Type* type) {
    return is_ptr(type) && type->pointer_level == 1 &&
           is_void(type->inner_type);
}

static inline int is_bool(const Type* type) {
    return type->type == METADATA_PRIMITIVE &&
           type->primitive_type == TYPE_BOOL;
}

static inline int is_int(const Type* type) {
    return type->type == METADATA_PRIMITIVE &&
           type->primitive_type != TYPE_VOID &&
           type->primitive_type != TYPE_BOOL;
}

static inline int is_signed(PrimitiveType type) {
    switch (type) {
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
            return 0;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
            return 1;
        default:
            assert(0);
    }
}

// TODO: Int primitive tpye should probably be a struct with is_signed and size.
static inline PrimitiveType implicit_type_convert(PrimitiveType a,
                                                  PrimitiveType b) {
    switch (a) {
        case TYPE_U8:
            switch (b) {
                case TYPE_U8:
                    return TYPE_U8;
                case TYPE_U16:
                    return TYPE_U16;
                case TYPE_U32:
                    return TYPE_U32;
                case TYPE_I8:
                    return TYPE_U8;
                case TYPE_I16:
                    return TYPE_U16;
                case TYPE_I32:
                    return TYPE_U32;
                default:
                    assert(0);
            }
        case TYPE_U16:
            switch (b) {
                case TYPE_U8:
                    return TYPE_U16;
                case TYPE_U16:
                    return TYPE_U16;
                case TYPE_U32:
                    return TYPE_U32;
                case TYPE_I8:
                    return TYPE_U16;
                case TYPE_I16:
                    return TYPE_U16;
                case TYPE_I32:
                    return TYPE_U32;
                default:
                    assert(0);
            }
        case TYPE_U32:
            return TYPE_U32;
        case TYPE_I8:
            return b;
        case TYPE_I16:
            switch (b) {
                case TYPE_U8:
                    return TYPE_U16;
                case TYPE_U16:
                    return TYPE_U16;
                case TYPE_U32:
                    return TYPE_U32;
                case TYPE_I8:
                    return TYPE_I16;
                case TYPE_I16:
                    return TYPE_I16;
                case TYPE_I32:
                    return TYPE_I32;
                default:
                    assert(0);
            }
        case TYPE_I32:
            switch (b) {
                case TYPE_U8:
                    return TYPE_U32;
                case TYPE_U16:
                    return TYPE_U32;
                case TYPE_U32:
                    return TYPE_U32;
                case TYPE_I8:
                    return TYPE_I32;
                case TYPE_I16:
                    return TYPE_I32;
                case TYPE_I32:
                    return TYPE_I32;
                default:
                    assert(0);
            }
        default:
            assert(0);
    }
}

const Type* get_primitive_type(PrimitiveType type);
const Type* get_string_type(void);
const Type* get_void_ptr_type(void);

int is_equal_type(const Type* a, const Type* b);

#endif
