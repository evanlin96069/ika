#include "sema.h"

#include <stdarg.h>
#include <stdlib.h>

static Error* error(SourcePos pos, const char* fmt, ...) {
    Error* result = malloc(sizeof(Error));
    result->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(result->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return result;
}

static Error* type_check_node(SemaState* state, ASTNode* node);

static Error* type_check_stmts(SemaState* state, StatementListNode* stmts) {
    ASTNodeList* iter = stmts->stmts;
    while (iter) {
        Error* err = type_check_node(state, iter->node);
        if (err != NULL) {
            return err;
        }

        iter = iter->next;
    }

    return NULL;
}

static Error* type_check_binop(SemaState* state, BinaryOpNode* binop) {
    Error* err = type_check_node(state, binop->left);
    if (err != NULL) {
        return err;
    }

    if (binop->op == TK_COMMA) {
        err = type_check_node(state, binop->right);
        if (err != NULL) {
            return err;
        }

        binop->type_info = as_typed_ast(binop->right)->type_info;
        return NULL;
    }

    const Type* l_type = &(as_typed_ast(binop->left)->type_info.type);
    if (!(is_bool(l_type) || is_int(l_type) || is_ptr_like(l_type))) {
        return error(binop->pos, "invalid left operand to do binary operation");
    }

    if (is_bool(l_type)) {
        // Boolean operations
        if (binop->op == TK_EQ || binop->op == TK_NE) {
            err = type_check_node(state, binop->right);
            if (err != NULL) {
                return err;
            }

            const Type* r_type = &(as_typed_ast(binop->right)->type_info.type);
            if (!is_bool(r_type)) {
                return error(binop->pos,
                             "invalid right operand to do boolean operation");
            }
        } else {
            if (binop->op != TK_LOR && binop->op != TK_LAND) {
                return error(binop->pos, "invalid boolean operator");
            }

            err = type_check_node(state, binop->right);
            if (err != NULL) {
                return err;
            }

            const Type* r_type = &(as_typed_ast(binop->right)->type_info.type);
            if (!is_bool(r_type)) {
                return error(binop->pos,
                             "invalid right operand to do boolean operation");
            }
        }
        binop->type_info.is_lvalue = 0;
        binop->type_info.type = *get_primitive_type(TYPE_BOOL);
    } else {
        err = type_check_node(state, binop->right);
        if (err != NULL) {
            return err;
        }

        const Type* r_type = &(as_typed_ast(binop->right)->type_info.type);
        if (!is_int(r_type) && !is_ptr_like(r_type)) {
            return error(binop->pos,
                         "invalid right operand to do binary operation");
        }

        // At this point, both operands are either pointer or integer.
        switch (binop->op) {
            case TK_ADD:
            case TK_SUB: {
                int l_ptr = is_array_ptr(l_type);
                int r_ptr = is_array_ptr(r_type);

                if (l_ptr || r_ptr) {  // Pointers
                    if (l_ptr && r_ptr) {
                        return error(binop->pos,
                                     "invalid operands to do binary operation");
                    }

                    const Type* p_type = l_ptr ? l_type : r_type;
                    if (!is_void(p_type->inner_type) &&
                        p_type->inner_type->incomplete) {
                        return error(binop->pos, "use of incomplete tyoe");
                    }

                    binop->type_info.type = *p_type;
                } else if (is_int(l_type) && is_int(r_type)) {  // Integers
                    binop->type_info.type =
                        *get_primitive_type(implicit_type_convert(
                            l_type->primitive_type, r_type->primitive_type));
                } else {
                    return error(binop->pos,
                                 "invalid operands to do binary operation");
                }
            } break;

            case TK_EQ:
            case TK_NE:
            case TK_LT:
            case TK_LE:
            case TK_GT:
            case TK_GE: {
                int is_valid_types = 0;
                if (binop->op == TK_EQ || binop->op == TK_NE) {
                    if (is_int(l_type) && is_int(r_type)) {
                        is_valid_types = 1;
                    } else if (is_void_ptr(l_type) && is_ptr_like(r_type)) {
                        is_valid_types = 1;
                    } else if (is_void_ptr(r_type) && is_ptr_like(l_type)) {
                        is_valid_types = 1;
                    } else if (is_equal_type(l_type, r_type)) {
                        is_valid_types = 1;
                    }
                } else {
                    if (is_int(l_type) && is_int(r_type)) {
                        is_valid_types = 1;
                    } else if (is_array_ptr(l_type) &&
                               is_equal_type(l_type, r_type)) {
                        is_valid_types = 1;
                    }
                }

                if (!is_valid_types) {
                    return error(binop->pos,
                                 "invalid operands for comparison operation");
                }
                binop->type_info.type = *get_primitive_type(TYPE_BOOL);
            } break;

            default: {
                if (!is_int(l_type) || !is_int(r_type)) {
                    return error(binop->pos,
                                 "invalid operands to do binary operation");
                }

                PrimitiveType result_type = implicit_type_convert(
                    l_type->primitive_type, r_type->primitive_type);
                binop->type_info.type = *get_primitive_type(result_type);
            }
        }
        binop->type_info.is_lvalue = 0;
    }

    return NULL;
}

static Error* type_check_unaryop(SemaState* state, UnaryOpNode* unaryop) {
    Error* err = type_check_node(state, unaryop->node);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* node = as_typed_ast(unaryop->node);
    int is_lvalue = node->type_info.is_lvalue;
    const Type* type = &node->type_info.type;

    unaryop->type_info.is_lvalue = 0;
    unaryop->type_info.type = *type;

    switch (unaryop->op) {
        case TK_ADD:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }
            break;

        case TK_SUB:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }
            break;

        case TK_NOT:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }
            break;

        case TK_LNOT:
            if (!is_bool(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }
            break;

        case TK_MUL:
            if (!is_ptr_like(type)) {
                return error(unaryop->pos,
                             "indirection requires pointer operand");
            }

            if (is_ptr(type)) {
                unaryop->type_info.type.pointer_level--;
                if (unaryop->type_info.type.pointer_level == 0) {
                    unaryop->type_info.type =
                        *unaryop->type_info.type.inner_type;
                }
            } else {
                unaryop->type_info.type = *unaryop->type_info.type.inner_type;
            }

            unaryop->type_info.is_lvalue = 1;
            break;

        case TK_AND:
            if (is_lvalue == 0) {
                return error(unaryop->pos,
                             "lvalue required as unary '&' operand");
            }

            if (is_ptr(type)) {
                unaryop->type_info.type.pointer_level++;
            } else {
                Type* inner = arena_alloc(state->arena, sizeof(Type));
                *inner = unaryop->type_info.type;
                unaryop->type_info.type.type = METADATA_POINTER;
                unaryop->type_info.type.size = PTR_SIZE;
                unaryop->type_info.type.alignment = PTR_SIZE;
                unaryop->type_info.type.pointer_level = 1;
                unaryop->type_info.type.inner_type = inner;
            }
            break;

        default:
            UNREACHABLE();
    }

    return NULL;
}

static Error* type_check_var(SemaState* state, VarNode* var) {
    UNUSED(state);

    switch (var->ste->type) {
        case SYM_VAR: {
            // Variable
            VarSymbolTableEntry* var_ste = (VarSymbolTableEntry*)var->ste;
            var->type_info.is_lvalue = 1;
            var->type_info.type = *var_ste->data_type;
        } break;
        case SYM_FUNC: {
            // Function pointer
            FuncSymbolTableEntry* func_ste = (FuncSymbolTableEntry*)var->ste;
            var->type_info.is_lvalue = 0;
            var->type_info.type.type = METADATA_FUNC;
            var->type_info.type.func_data = func_ste->func_data;
        } break;
        default:
            UNREACHABLE();
    }
    return NULL;
}

static inline int is_allowed_type_convert(const Type* left, const Type* right) {
    if (is_equal_type(left, right)) {
        return 1;
    }

    // integer conversion
    if (is_int(left) && is_int(right)) {
        return 1;
    }

    // void pointer conversion
    if (is_ptr_like(right) && is_void_ptr(left)) {
        return 1;
    }
    if (is_ptr_like(left) && is_void_ptr(right)) {
        return 1;
    }

    // convert pointer to array to array pointer
    if (is_array_ptr(left) && is_ptr(right) && right->pointer_level == 1) {
        const Type* l_inner = left->inner_type;
        const Type* r_inner = right->inner_type;

        if (r_inner->type == METADATA_ARRAY && r_inner->array_size != 0) {
            return is_equal_type(l_inner, r_inner->inner_type);
        }
    }

    return 0;
}

static Error* type_check_assign(SemaState* state, AssignNode* assign) {
    Error* err = type_check_node(state, assign->left);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* l_node = as_typed_ast(assign->left);
    if (l_node->type_info.is_lvalue == 0) {
        return error(assign->pos,
                     "lvalue required as left operand of assignment");
    }

    err = type_check_node(state, assign->right);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* r_node = as_typed_ast(assign->right);
    const Type* l_type = &l_node->type_info.type;
    const Type* r_type = &r_node->type_info.type;

    if (!is_allowed_type_convert(l_type, r_type)) {
        return error(assign->pos, "type is not assignable");
    }

    assign->type_info.is_lvalue = 1;
    assign->type_info.type = *l_type;
    return NULL;
}

static Error* type_check_if(SemaState* state, IfStatementNode* if_node) {
    Error* err = type_check_node(state, if_node->expr);
    if (err != NULL) {
        return err;
    }

    if (!is_bool(&(as_typed_ast(if_node->expr)->type_info.type))) {
        return error(if_node->expr->pos, "expected type 'bool'");
    }

    err = type_check_node(state, if_node->then_block);
    if (err != NULL) {
        return err;
    }

    if (if_node->else_block) {
        err = type_check_node(state, if_node->else_block);
        if (err != NULL) {
            return err;
        }
    }

    return NULL;
}

static Error* type_check_while(SemaState* state, WhileNode* while_node) {
    Error* err = type_check_node(state, while_node->expr);
    if (err != NULL) {
        return err;
    }

    if (!is_bool(&(as_typed_ast(while_node->expr)->type_info.type))) {
        return error(while_node->expr->pos, "expected type 'bool'");
    }

    int prev_in_loop = state->in_loop;

    state->in_loop = 1;

    err = type_check_node(state, while_node->block);
    if (err != NULL) {
        return err;
    }

    state->in_loop = prev_in_loop;

    if (while_node->inc) {
        err = type_check_node(state, while_node->inc);
        if (err != NULL) {
            return err;
        }
    }

    return NULL;
}

static Error* type_check_goto(SemaState* state, GotoNode* node) {
    switch (node->op) {
        case TK_BREAK:
            if (state->in_loop == 0) {
                return error(node->pos, "break statement not within a loop");
            }
            break;
        case TK_CONTINUE:
            if (state->in_loop == 0) {
                return error(node->pos, "continue statement not within a loop");
            }
            break;
        default:
            UNREACHABLE();
    }

    return NULL;
}

static Error* type_check_call(SemaState* state, CallNode* call) {
    Error* err = type_check_node(state, call->node);
    if (err != NULL) {
        return err;
    }

    const Type* func_type = &(as_typed_ast(call->node)->type_info.type);
    if (func_type->type != METADATA_FUNC) {
        return error(call->pos,
                     "called object is not a function or function pointer");
    }

    ASTNodeList* curr = call->args;
    ArgList* arg_type = func_type->func_data.args;
    int has_va_args = func_type->func_data.has_va_args;
    while (curr) {
        err = type_check_node(state, curr->node);
        if (err != NULL) {
            return err;
        }

        if (arg_type != NULL) {
            // TODO: Add va_args type check
            if (!has_va_args &&
                !is_allowed_type_convert(
                    arg_type->type,
                    &(as_typed_ast(curr->node)->type_info.type))) {
                return error(curr->node->pos,
                             "passing argument with invalid type");
            }
            arg_type = arg_type->next;
        } else if (!has_va_args) {
            return error(call->pos, "too many arguments");
        }
        curr = curr->next;
    }

    if (arg_type != NULL) {
        return error(call->pos, "too few arguments");
    }

    call->type_info.is_lvalue = 0;
    call->type_info.type = *func_type->func_data.return_type;
    return NULL;
}

static Error* type_check_print(SemaState* state, PrintNode* print_node) {
    ASTNodeList* curr = print_node->args;
    while (curr) {
        Error* err = type_check_node(state, curr->node);
        if (err != NULL) {
            return err;
        }

        if (as_typed_ast(curr->node)->type_info.type.size > 4) {
            return error(curr->node->pos, "passing argument with invalid type");
        }

        curr = curr->next;
    }

    return NULL;
}

static Error* type_check_ret(SemaState* state, ReturnNode* ret) {
    const Type* ret_type = get_primitive_type(TYPE_VOID);

    if (ret->expr) {
        Error* err = type_check_node(state, ret->expr);
        if (err != NULL) {
            return err;
        }

        ret_type = &(as_typed_ast(ret->expr)->type_info.type);
    }

    if (!is_allowed_type_convert(state->ret_type, ret_type)) {
        return error(ret->pos, "invalid return type");
    }

    return NULL;
}

static Error* type_check_field(SemaState* state, FieldNode* field) {
    Error* err = type_check_node(state, field->node);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* node = as_typed_ast(field->node);
    const Type* type = &node->type_info.type;
    if (type->type == METADATA_POINTER && type->pointer_level == 1) {
        // member access through pointer
        type = type->inner_type;
    }

    if (type->type != METADATA_TYPE) {
        return error(field->pos,
                     "request for member in something not a struct");
    }

    const TypeSymbolTableEntry* type_ste = type->type_ste;
    FieldSymbolTableEntry* ste = (FieldSymbolTableEntry*)symbol_table_find(
        type_ste->name_space, field->ident, 1);
    if (ste == NULL || ste->type != SYM_FIELD) {
        return error(field->pos, "type has no member '%.*s'", field->ident.len,
                     field->ident.ptr);
    }

    field->type_info.is_lvalue = 1;
    field->type_info.type = *ste->data_type;
    return NULL;
}

static Error* type_check_indexof(SemaState* state, IndexOfNode* idxof) {
    Error* err = type_check_node(state, idxof->left);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* l_node = as_typed_ast(idxof->left);
    const Type* l_type = &l_node->type_info.type;
    if (l_type->type != METADATA_ARRAY) {
        return error(idxof->pos,
                     "subscripted value is neither array nor array pointer");
    }

    err = type_check_node(state, idxof->right);
    if (err != NULL) {
        return err;
    }

    const TypedASTNode* r_node = as_typed_ast(idxof->right);
    const Type* r_type = &r_node->type_info.type;
    if (!is_int(r_type)) {
        return error(idxof->pos, "array subscript is not an integer");
    }

    idxof->type_info.is_lvalue = 1;
    idxof->type_info.type = *l_type->inner_type;
    return NULL;
}

static Error* type_check_node(SemaState* state, ASTNode* node) {
    switch (node->type) {
        case NODE_STMTS:
            return type_check_stmts(state, (StatementListNode*)node);

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            lit->type_info.is_lvalue = 0;
            if (lit->data_type == TYPE_VOID) {
                lit->type_info.type = *get_void_ptr_type();
            } else {
                lit->type_info.type = *get_primitive_type(lit->data_type);
            }
        }
            return NULL;

        case NODE_STRLIT: {
            StrLitNode* lit = (StrLitNode*)node;
            lit->type_info.is_lvalue = 0;
            lit->type_info.type = *get_string_type();
        }
            return NULL;

        case NODE_BINARYOP:
            return type_check_binop(state, (BinaryOpNode*)node);

        case NODE_UNARYOP:
            return type_check_unaryop(state, (UnaryOpNode*)node);

        case NODE_VAR:
            return type_check_var(state, (VarNode*)node);

        case NODE_ASSIGN:
            return type_check_assign(state, (AssignNode*)node);

        case NODE_IF:
            return type_check_if(state, (IfStatementNode*)node);

        case NODE_WHILE:
            return type_check_while(state, (WhileNode*)node);

        case NODE_GOTO:
            return type_check_goto(state, (GotoNode*)node);

        case NODE_CALL:
            return type_check_call(state, (CallNode*)node);

        case NODE_PRINT:
            return type_check_print(state, (PrintNode*)node);

        case NODE_RET:
            return type_check_ret(state, (ReturnNode*)node);

        case NODE_FIELD:
            return type_check_field(state, (FieldNode*)node);

        case NODE_INDEXOF:
            return type_check_indexof(state, (IndexOfNode*)node);

        default:
            UNREACHABLE();
    }
}

static Error* type_check_func(SemaState* state, FuncSymbolTableEntry* func) {
    const Type* prev_ret_type = state->ret_type;
    state->ret_type = func->func_data.return_type;

    assert(func->node);
    assert(func->is_extern == 0);

    Error* err = type_check_node(state, func->node);
    if (err != NULL) {
        return err;
    }

    state->ret_type = prev_ret_type;

    return NULL;
}

Error* sema(SemaState* state, ASTNode* node, SymbolTable* sym) {
    Error* err;

    // Functions
    SymbolTableEntry* curr = sym->ste;
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            if (func->node) {
                err = type_check_func(state, func);
                if (err != NULL) {
                    return err;
                }
            }
        }
        curr = curr->next;
    }

    state->ret_type = get_primitive_type(TYPE_I32);

    err = type_check_node(state, node);
    if (err != NULL) {
        return err;
    }

    return NULL;
}
