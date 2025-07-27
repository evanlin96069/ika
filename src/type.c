#include "type.h"

static const Type primitive_tpyes[] = {
    [TYPE_VOID] =
        {
            .incomplete = 1,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_VOID,
        },
    [TYPE_BOOL] =
        {
            .size = 1,
            .alignment = 1,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_BOOL,
        },
    [TYPE_U8] =
        {
            .size = 1,
            .alignment = 1,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_U8,
        },
    [TYPE_I8] =
        {
            .size = 1,
            .alignment = 1,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_I8,
        },
    [TYPE_U16] =
        {
            .size = 2,
            .alignment = 2,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_U16,
        },
    [TYPE_I16] =
        {
            .size = 2,
            .alignment = 2,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_I16,
        },
    [TYPE_U32] =
        {
            .size = 4,
            .alignment = 4,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_U32,
        },
    [TYPE_I32] =
        {
            .size = 4,
            .alignment = 4,
            .type = METADATA_PRIMITIVE,
            .primitive_type = TYPE_I32,
        },
};

const Type* get_primitive_type(PrimitiveType type) {
    return &primitive_tpyes[type];
}

static Type string_type = {
    .size = PTR_SIZE,
    .alignment = PTR_SIZE,
    .type = METADATA_ARRAY,
    .array_size = 0,
    .inner_type = &primitive_tpyes[TYPE_U8],
};

const Type* get_string_type(void) { return &string_type; }

static Type void_ptr_type = {
    .size = PTR_SIZE,
    .alignment = PTR_SIZE,
    .type = METADATA_POINTER,
    .pointer_level = 1,
    .inner_type = &primitive_tpyes[TYPE_VOID],
};

const Type* get_void_ptr_type(void) { return &void_ptr_type; }

int is_equal_type(const Type* a, const Type* b) {
    if (a->type != b->type) {
        return 0;
    }

    switch (a->type) {
        case METADATA_PRIMITIVE:
            return a->primitive_type == b->primitive_type;

        case METADATA_TYPE:
            return a->type_ste == b->type_ste;

        case METADATA_ARRAY:
            if (a->array_size != b->array_size) {
                return 0;
            }
            return is_equal_type(a->inner_type, b->inner_type);

        case METADATA_POINTER:
            if (a->pointer_level != b->pointer_level) {
                return 0;
            }
            return is_equal_type(a->inner_type, b->inner_type);

        case METADATA_FUNC: {
            if (a->func_data.has_va_args != b->func_data.has_va_args) {
                return 0;
            }

            if (!is_equal_type(a->func_data.return_type,
                               b->func_data.return_type)) {
                return 0;
            }

            ArgList* a_arg = a->func_data.args;
            ArgList* b_arg = b->func_data.args;

            while (a_arg != NULL && b_arg != NULL) {
                if (!is_equal_type(a_arg->type, b_arg->type)) {
                    return 0;
                }
                a_arg = a_arg->next;
                b_arg = b_arg->next;
            }

            return a_arg == NULL && b_arg == NULL;
        }

        default:
            UNREACHABLE();
    }
}
