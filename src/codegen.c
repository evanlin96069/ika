#include "codegen.h"

#include <stdlib.h>

#ifndef NDEBUG

#define GEN(out, ...)                                   \
    do {                                                \
        fprintf(out, __VA_ARGS__);                      \
        fprintf(out, " # %s:%d\n", __func__, __LINE__); \
    } while (0)

#else

#define GEN(out, ...)              \
    do {                           \
        fprintf(out, __VA_ARGS__); \
        fprintf(out, "\n");        \
    } while (0)

#endif

#define genf(...)                         \
    do {                                  \
        if (state->out != NULL) {         \
            GEN(state->out, __VA_ARGS__); \
        }                                 \
    } while (0)

#ifdef _WIN32
#define OS_SYM_PREFIX "_"
#else
#define OS_SYM_PREFIX ""
#endif

#define NO_MEMCPY
#define INLINE_COPY_LIMIT 16

static inline int add_label(CodegenState* state) {
    return state->label_count++;
}

static int add_data(CodegenState* state, Str str) {
    for (int i = 0; i < state->data_count; i++) {
        if (str_eql(str, state->data[i])) {
            return i;
        }
    }
    state->data[state->data_count] = str;
    return state->data_count++;
}

static void emit_node(CodegenState* state, ASTNode* node);

static inline void emit_stmts(CodegenState* state, StatementListNode* stmts) {
    ASTNodeList* iter = stmts->stmts;
    while (iter) {
        emit_node(state, iter->node);
        iter = iter->next;
    }
}

static inline void emit_intlit(CodegenState* state, IntLitNode* lit) {
    genf("    movl $%d, %%eax", lit->val);
}

static inline void emit_strlit(CodegenState* state, StrLitNode* lit) {
    genf("    movl $.LC%d, %%eax", add_data(state, lit->val));
}

// load the value into register if size is 4 bytes, 2 bytes, or 1 byte,
// else do nothing.
static void emit_load_address(CodegenState* state, const Type* type) {
    switch (type->size) {
        case 4:
            genf("    movl (%%eax), %%eax");
            break;
        case 3:
            genf("    movl %%eax, %%ecx");
            genf("    movzwl (%%ecx), %%eax");
            genf("    movb 2(%%ecx), %%ah");
            break;
        case 2:
            if (type->primitive_type == TYPE_I16) {
                genf("    movswl (%%eax), %%eax");
            } else {
                genf("    movzwl (%%eax), %%eax");
            }
            break;
        case 1:
            if (type->primitive_type == TYPE_I8) {
                genf("    movsbl (%%eax), %%eax");
            } else {
                genf("    movzbl (%%eax), %%eax");
            }
            break;
        default:
            break;
    }
}

static void emit_binop(CodegenState* state, BinaryOpNode* binop) {
    emit_node(state, binop->left);

    if (binop->op == TK_COMMA) {
        emit_node(state, binop->right);
        return;
    }

    const TypedASTNode* l_node = as_typed_ast(binop->left);
    const Type* l_type = &l_node->type_info.type;
    const TypedASTNode* r_node = as_typed_ast(binop->right);
    const Type* r_type = &r_node->type_info.type;

    if (l_node->type_info.is_address) {
        emit_load_address(state, l_type);
    }

    genf("    pushl %%eax");

    emit_node(state, binop->right);
    if (r_node->type_info.is_address) {
        emit_load_address(state, r_type);
    }

    genf("    movl %%eax, %%ecx");
    genf("    popl %%eax");

    if (is_bool(l_type)) {
        // Boolean operations
        if (binop->op == TK_EQ || binop->op == TK_NE) {
            if (binop->op == TK_EQ) {
                genf("    cmpl %%ecx, %%eax");
                genf("    sete %%al");
                genf("    movzbl %%al, %%eax");
            } else {  // TK_NE
                genf("    cmpl %%ecx, %%eax");
                genf("    setne %%al");
                genf("    movzbl %%al, %%eax");
            }
        } else {
            int label = add_label(state);
            if (binop->op == TK_LOR) {
                genf("    testl %%eax, %%eax");
                genf("    jnz .L%d", label);
            } else if (binop->op == TK_LAND) {
                genf("    testl %%eax, %%eax");
                genf("    jz .L%d", label);
            } else {
                UNREACHABLE();
            }

            emit_node(state, binop->right);
            if (r_node->type_info.is_address) {
                emit_load_address(state, r_type);
            }

            genf(".L%d:", label);
        }
    } else {
        switch (binop->op) {
            case TK_ADD:
            case TK_SUB: {
                int l_ptr = is_array_ptr(l_type);
                int r_ptr = is_array_ptr(r_type);

                if (l_ptr || r_ptr) {  // Pointers
                    const Type* p_type = l_ptr ? l_type : r_type;
                    int size;
                    if (is_void(p_type->inner_type)) {
                        size = 1;
                    } else if (p_type->inner_type->incomplete) {
                        UNREACHABLE();
                    } else {
                        size = p_type->inner_type->size;
                    }

                    if (size != 1) {
                        if (l_ptr) {
                            genf("    imull $%d, %%ecx", size);
                        } else {
                            genf("    imull $%d, %%eax", size);
                        }
                    }

                    if (binop->op == TK_ADD) {
                        genf("    addl %%ecx, %%eax");
                    } else {  // TK_SUB
                        genf("    subl %%ecx, %%eax");
                    }
                } else if (is_int(l_type) && is_int(r_type)) {  // Integers
                    if (binop->op == TK_ADD) {
                        genf("    addl %%ecx, %%eax");
                    } else {  // TK_SUB
                        genf("    subl %%ecx, %%eax");
                    }
                } else {
                    UNREACHABLE();
                }
            } break;

            case TK_EQ:
            case TK_NE:
            case TK_LT:
            case TK_LE:
            case TK_GT:
            case TK_GE: {
                genf("    cmpl %%ecx, %%eax");
                switch (binop->op) {
                    case TK_EQ:
                        genf("    sete %%al");
                        break;
                    case TK_NE:
                        genf("    setne %%al");
                        break;
                    case TK_LT:
                        genf("    setl %%al");
                        break;
                    case TK_LE:
                        genf("    setle %%al");
                        break;
                    case TK_GT:
                        genf("    setg %%al");
                        break;
                    case TK_GE:
                        genf("    setge %%al");
                        break;
                    default:
                        break;
                }
                genf("    movzbl %%al, %%eax");
            } break;

            default: {
                PrimitiveType result_type = implicit_type_convert(
                    l_type->primitive_type, r_type->primitive_type);
                int result_signed = is_signed(result_type);

                switch (binop->op) {
                    case TK_MUL:
                        genf("    imull %%ecx, %%eax");
                        break;

                    case TK_DIV:
                        if (result_signed) {
                            genf("    cdq");
                            genf("    idivl %%ecx");
                        } else {
                            genf("    xor %%edx, %%edx");
                            genf("    divl %%ecx");
                        }
                        break;

                    case TK_MOD:
                        if (result_signed) {
                            genf("    cdq");
                            genf("    idivl %%ecx");
                            genf("    movl %%edx, %%eax");
                        } else {
                            genf("    xor %%edx, %%edx");
                            genf("    divl %%ecx");
                            genf("    movl %%edx, %%eax");
                        }
                        break;

                    case TK_SHL:
                        genf("    movl %%ecx, %%edx");
                        genf("    shll %%cl, %%eax");
                        break;

                    case TK_SHR:
                        if (result_signed) {
                            genf("    movl %%ecx, %%edx");
                            genf("    sarl %%cl, %%eax");
                        } else {
                            genf("    movl %%ecx, %%edx");
                            genf("    shrl %%cl, %%eax");
                        }
                        break;

                    case TK_AND:
                        genf("    andl %%ecx, %%eax");
                        break;

                    case TK_XOR:
                        genf("    xorl %%ecx, %%eax");
                        break;

                    case TK_OR:
                        genf("    orl %%ecx, %%eax");
                        break;

                    default:
                        // fprintf(stderr, "Token type: %d\n", binop->op);
                        UNREACHABLE();
                }
            }
        }
    }
}

static void emit_unaryop(CodegenState* state, UnaryOpNode* unaryop) {
    emit_node(state, unaryop->node);

    const TypedASTNode* node = as_typed_ast(unaryop->node);
    int is_address = node->type_info.is_address;
    const Type* type = &node->type_info.type;

    switch (unaryop->op) {
        case TK_ADD:
            if (is_address) {
                emit_load_address(state, type);
            }
            break;

        case TK_SUB:
            if (is_address) {
                emit_load_address(state, type);
            }
            genf("    negl %%eax");
            break;

        case TK_NOT:
            if (is_address) {
                emit_load_address(state, type);
            }
            genf("    notl %%eax");
            break;

        case TK_LNOT:
            if (is_address) {
                emit_load_address(state, type);
            }

            genf("    testl %%eax, %%eax");
            genf("    sete %%al");
            genf("    movzbl %%al, %%eax");
            break;

        case TK_MUL:
            if (is_address) {
                emit_load_address(state, type);
            }
            break;

        case TK_AND:
            break;

        default:
            UNREACHABLE();
    }
}

static int get_func_args_size(const FuncMetadata* func_data) {
    ArgList* curr = func_data->args;
    int args_size = 0;
    while (curr) {
        // For thiscall, we actually push the ECX back on stack and pretend it's
        // stdcall so we need to also count thisptr here
        /*
        if (func_data->callconv == CALLCONV_THISCALL && curr->next == NULL) {
            // skip thisptr
            break;
        }
        */
        int size = curr->type->size;
        int padding = (MAX_ALIGNMENT - (size % MAX_ALIGNMENT)) % MAX_ALIGNMENT;
        args_size += size + padding;

        curr = curr->next;
    }

    if (func_data->return_type->size > REGISTER_SIZE) {
        // We use System V ABI for returning struct (a pointer to the space as
        // the hidden first arguemnt) This will be wrong for MSVC ABI like
        // stdcall or thiscall
        args_size += PTR_SIZE;
    }

    return args_size;
}

static void emit_var(CodegenState* state, VarNode* var) {
    switch (var->ste->type) {
        case SYM_VAR: {
            // Variable
            VarSymbolTableEntry* var_ste = (VarSymbolTableEntry*)var->ste;
            if (var_ste->is_extern || var_ste->is_global) {
                genf("    movl $" OS_SYM_PREFIX "%.*s, %%eax",
                     var_ste->ident.len, var_ste->ident.ptr);
            } else if (var_ste->is_arg) {
                // Argument
                genf("    leal %d(%%ebp), %%eax",
                     var_ste->offset + var_ste->sym->arg_offset);
            } else {
                // Local variable
                genf("    leal -%d(%%ebp), %%eax", var_ste->offset);
            }
        } break;
        case SYM_FUNC: {
            // Function pointer
            FuncSymbolTableEntry* func_ste = (FuncSymbolTableEntry*)var->ste;
            const FuncMetadata* func_data = &func_ste->func_data;
            if (func_data->callconv == CALLCONV_STDCALL) {
                int args_size = get_func_args_size(func_data);
                genf("    movl $" OS_SYM_PREFIX "%.*s@%d, %%eax",
                     func_ste->ident.len, func_ste->ident.ptr, args_size);
            } else {
                genf("    movl $" OS_SYM_PREFIX "%.*s, %%eax",
                     func_ste->ident.len, func_ste->ident.ptr);
            }
        } break;
        default:
            UNREACHABLE();
    }
}

// src/dest cannot be %edx, %esi, %edi, %esp
static void emit_memcpy(CodegenState* state, const char* dest, const char* src,
                        int size) {
    assert(size > REGISTER_SIZE);

#ifdef NO_MEMCPY
    if (size <= INLINE_COPY_LIMIT) {
        for (int offset = 0; offset < size; offset += 4) {
            if (size - offset >= 4) {
                genf("    movl %d(%s), %%edx", offset, src);
                genf("    movl %%edx, %d(%s)", offset, dest);
            } else if (size - offset >= 2) {
                genf("    movw %d(%s), %%dx", offset, src);
                genf("    movw %%dx, %d(%s)", offset, dest);
                offset += 2;
                if (size - offset == 1) {
                    genf("    movb %d(%s), %%dl", offset, src);
                    genf("    movb %%dl, %d(%s)", offset, dest);
                }
                break;
            } else {
                genf("    movb %d(%s), %%dl", offset, src);
                genf("    movb %%dl, %d(%s)", offset, dest);
            }
        }
    } else {
        genf("    push %%esi");
        genf("    push %%edi");
        genf("    push %%ecx");

        genf("    movl %s, %%esi", src);
        genf("    movl %s, %%edi", dest);
        genf("    movl $%d, %%ecx", size);
        genf("    cld");
        genf("    rep movsb");

        genf("    pop %%ecx");
        genf("    pop %%edi");
        genf("    pop %%esi");
    }
#else
    genf("    movl $%d, %%edx", size);
    genf("    pushl %%edx");     // n
    genf("    push %s", src);    // src
    genf("    pushl %s", dest);  // dest
    genf("    call " OS_SYM_PREFIX "memcpy");
    genf("    addl $12, %%esp");
#endif
}

static void emit_assign(CodegenState* state, AssignNode* assign) {
    emit_node(state, assign->left);

    const TypedASTNode* l_node = as_typed_ast(assign->left);
    const Type* l_type = &l_node->type_info.type;

    genf("    pushl %%eax");

    emit_node(state, assign->right);
    const TypedASTNode* r_node = as_typed_ast(assign->right);
    if (r_node->type_info.is_address) {
        emit_load_address(state, &r_node->type_info.type);
    }

    genf("    popl %%ecx");

    // ecx = left addr, eax = right
    switch (l_type->size) {
        case 4:
            genf("    movl %%eax, (%%ecx)");
            break;
        case 3:
            genf("    movw %%ax, (%%ecx)");
            genf("    movb %%ah, 2(%%ecx)");
            break;
        case 2:
            genf("    movw %%ax, (%%ecx)");
            break;
        case 1:
            genf("    movb %%al, (%%ecx)");
            break;
        default: {
            emit_memcpy(state, "%ecx", "%eax", l_type->size);
        } break;
    }

    genf("    movl %%ecx, %%eax");
}

static void emit_if(CodegenState* state, IfStatementNode* if_node) {
    /*
     *      <cond>
     *      JZ else_label
     *      <then_block>
     *      JMP end_label
     *  else_label:
     *      <else_block>
     *  end_label:
     */
    int end_label = add_label(state);
    int else_label = add_label(state);

    emit_node(state, if_node->expr);
    const TypedASTNode* expr_node = as_typed_ast(if_node->expr);
    if (expr_node->type_info.is_address) {
        emit_load_address(state, &expr_node->type_info.type);
    }

    genf("    testl %%eax, %%eax");
    genf("    jz .L%d", else_label);

    emit_node(state, if_node->then_block);

    genf("    jmp .L%d", end_label);
    genf(".L%d:", else_label);

    if (if_node->else_block) {
        emit_node(state, if_node->else_block);
    }

    genf(".L%d:", end_label);
}

static void emit_while(CodegenState* state, WhileNode* while_node) {
    /*
     *  loop_label:
     *      <cond>
     *      JZ end_label
     *      <block>
     *   inc_label:
     *      <inc>
     *      JMP loop_label
     *  end_lable:
     */

    int loop_label = add_label(state);
    int inc_label = add_label(state);
    int end_label = add_label(state);

    genf(".L%d:", loop_label);

    emit_node(state, while_node->expr);
    const TypedASTNode* expr_node = as_typed_ast(while_node->expr);
    if (expr_node->type_info.is_address) {
        emit_load_address(state, &expr_node->type_info.type);
    }

    genf("    testl %%eax, %%eax");
    genf("    jz .L%d", end_label);

    int prev_in_loop = state->in_loop;
    int prev_break_label = state->break_label;
    int prev_continue_label = state->continue_label;

    state->in_loop = 1;
    state->break_label = end_label;
    state->continue_label = inc_label;

    emit_node(state, while_node->block);

    state->in_loop = prev_in_loop;
    state->break_label = prev_break_label;
    state->continue_label = prev_continue_label;

    genf(".L%d:", inc_label);
    if (while_node->inc) {
        emit_node(state, while_node->inc);
    }

    genf("    jmp .L%d", loop_label);
    genf(".L%d:", end_label);
}

static inline void emit_goto(CodegenState* state, GotoNode* node) {
    switch (node->op) {
        case TK_BREAK:
            genf("    jmp .L%d", state->break_label);
            break;
        case TK_CONTINUE:
            genf("    jmp .L%d", state->continue_label);
            break;
        default:
            UNREACHABLE();
    }
}

static void emit_call(CodegenState* state, CallNode* call) {
    /*
     *  local 3         [ebp]-16 <-ESP
     *  local 2         [ebp]-8
     *  local 1         [ebp]-4
     *  saved EBP       <-EBP
     *  return addr
     *  arg 1           [ebp]+8
     *  arg 2           [ebp]+12
     *  arg 3           [ebp]+16
     */

    const TypedASTNode* func_node = as_typed_ast(call->node);
    const Type* func_type = &func_node->type_info.type;
    assert(func_type->type == METADATA_FUNC);

    ASTNodeList* curr = call->args;
    int args_size = 0;
    while (curr) {
        emit_node(state, curr->node);

        const TypedASTNode* node = as_typed_ast(curr->node);
        if (node->type_info.is_address) {
            emit_load_address(state, &node->type_info.type);
        }

        int size = node->type_info.type.size;
        // padding
        size += (MAX_ALIGNMENT - (size % MAX_ALIGNMENT)) % MAX_ALIGNMENT;

        if (size <= REGISTER_SIZE) {
            genf("    pushl %%eax");
        } else {
            genf("    subl $%d, %%esp", size);
            genf("    movl %%esp, %%ecx");
            emit_memcpy(state, "%ecx", "%eax", size);
        }

        args_size += size;
        curr = curr->next;
    }

    const Type* return_type = func_type->func_data.return_type;
    if (return_type->size > REGISTER_SIZE) {
        genf("    leal -%d(%%ebp), %%eax", state->temp_struct_stack_offset);
        genf("    pushl %%eax");
        args_size += PTR_SIZE;
    }

    emit_node(state, call->node);

    if (func_node->type_info.is_address) {
        emit_load_address(state, func_type);
    }

    if (func_type->func_data.callconv == CALLCONV_THISCALL) {
        genf("    popl %%ecx");
    }

    genf("    call *%%eax");

    if (func_type->func_data.callconv == CALLCONV_CDECL && args_size > 0) {
        genf("    addl $%d, %%esp", args_size);
    }

    // Return value
    if (is_void(return_type)) {
        return;
    }

    if (return_type->type != METADATA_PRIMITIVE) {
        return;
    }

    switch (return_type->size) {
        case 4:
            break;
        case 2:
            if (return_type->primitive_type == TYPE_I16) {
                genf("    movswl %%ax, %%eax");
            } else {
                genf("    movzwl %%ax, %%eax");
            }
            break;
        case 1:
            if (return_type->primitive_type == TYPE_I8) {
                genf("    movsbl %%al, %%eax");
            } else {
                genf("    movzbl %%al, %%eax");
            }
            break;
        default:
            UNREACHABLE();
    }
}

static void emit_print(CodegenState* state, PrintNode* print_node) {
    int arg_count = 0;
    ASTNodeList* curr = print_node->args;
    while (curr) {
        emit_node(state, curr->node);

        TypedASTNode* node = as_typed_ast(curr->node);
        if (node->type_info.is_address) {
            emit_load_address(state, &node->type_info.type);
        }

        genf("    pushl %%eax");
        arg_count++;
        curr = curr->next;
    }

    genf("    pushl $.LC%d", add_data(state, print_node->fmt));

    arg_count++;

    genf("    call " OS_SYM_PREFIX "printf");
    genf("    addl $%d, %%esp", arg_count * REGISTER_SIZE);
}

static void emit_ret(CodegenState* state, ReturnNode* ret) {
    if (ret->expr) {
        emit_node(state, ret->expr);
        const TypedASTNode* expr_node = as_typed_ast(ret->expr);
        if (expr_node->type_info.is_address) {
            emit_load_address(state, &expr_node->type_info.type);
        }

        const Type* return_type = &(as_typed_ast(ret->expr)->type_info.type);
        if (return_type->size > REGISTER_SIZE) {
            genf("    movl 8(%%ebp), %%ecx");
            emit_memcpy(state, "%ecx", "%eax", return_type->size);
            genf("    movl 8(%%ebp), %%eax");
        }
    }

    genf("    jmp .L%d", state->return_label);
}

static void emit_field(CodegenState* state, FieldNode* field) {
    emit_node(state, field->node);

    const Type* l_type = &(as_typed_ast(field->node)->type_info.type);
    if (l_type->type == METADATA_POINTER && l_type->pointer_level == 1) {
        // member access through pointer
        emit_load_address(state, l_type);
        l_type = l_type->inner_type;
    }

    const TypeSymbolTableEntry* type_ste = l_type->type_ste;
    FieldSymbolTableEntry* ste = (FieldSymbolTableEntry*)symbol_table_find(
        type_ste->name_space, field->ident, 1);
    assert(ste != NULL && ste->type == SYM_FIELD);

    genf("    leal %d(%%eax), %%eax", ste->offset);
}

static void emit_indexof(CodegenState* state, IndexOfNode* idxof) {
    emit_node(state, idxof->left);

    const TypedASTNode* l_node = as_typed_ast(idxof->left);
    const Type* l_type = &l_node->type_info.type;
    if (l_node->type_info.is_address && l_type->array_size == 0) {
        emit_load_address(state, &l_node->type_info.type);
    }

    genf("    pushl %%eax");

    emit_node(state, idxof->right);

    const TypedASTNode* r_node = as_typed_ast(idxof->right);
    if (r_node->type_info.is_address) {
        emit_load_address(state, &r_node->type_info.type);
    }

    genf("    popl %%ecx");
    // ecx = array, eax = index
    genf("    imull $%d, %%eax", l_type->inner_type->size);
    genf("    addl %%ecx, %%eax");
}

static void emit_cast(CodegenState* state, CastNode* cast) {
    UNUSED(state);
    emit_node(state, cast->expr);

    const TypedASTNode* expr = as_typed_ast(cast->expr);
    if (expr->type_info.is_address) {
        emit_load_address(state, &expr->type_info.type);
    }
}

static void emit_node(CodegenState* state, ASTNode* node) {
    switch (node->type) {
        case NODE_STMTS:
            emit_stmts(state, (StatementListNode*)node);
            break;

        case NODE_INTLIT:
            emit_intlit(state, (IntLitNode*)node);
            break;

        case NODE_STRLIT:
            emit_strlit(state, (StrLitNode*)node);
            break;

        case NODE_BINARYOP:
            emit_binop(state, (BinaryOpNode*)node);
            break;

        case NODE_UNARYOP:
            emit_unaryop(state, (UnaryOpNode*)node);
            break;

        case NODE_VAR:
            emit_var(state, (VarNode*)node);
            break;

        case NODE_ASSIGN:
            emit_assign(state, (AssignNode*)node);
            break;

        case NODE_IF:
            emit_if(state, (IfStatementNode*)node);
            break;

        case NODE_WHILE:
            emit_while(state, (WhileNode*)node);
            break;

        case NODE_GOTO:
            emit_goto(state, (GotoNode*)node);
            break;

        case NODE_CALL:
            emit_call(state, (CallNode*)node);
            break;

        case NODE_PRINT:
            emit_print(state, (PrintNode*)node);
            break;

        case NODE_RET:
            emit_ret(state, (ReturnNode*)node);
            break;

        case NODE_FIELD:
            emit_field(state, (FieldNode*)node);
            break;

        case NODE_INDEXOF:
            emit_indexof(state, (IndexOfNode*)node);
            break;

        case NODE_CAST:
            emit_cast(state, (CastNode*)node);
            break;

        default:
            UNREACHABLE();
    }
}

static inline void emit_func_start(CodegenState* state, int stack_size) {
    genf("    pushl %%ebp");
    genf("    movl %%esp, %%ebp");
    if (stack_size > 0) {
        genf("    subl $%d, %%esp", stack_size);
    }
}

static inline void emit_func_exit(CodegenState* state, int args_size) {
    genf(".L%d:", state->return_label);
    genf("    leave");

    if (args_size > 0) {
        genf("    ret $%d", args_size);
    } else {
        genf("    ret");
    }
}

static inline void setup_func_state(CodegenState* state,
                                    const Type* return_type,
                                    const SymbolTable* sym) {
    state->return_label = add_label(state);
    state->return_type = return_type;
    state->temp_struct_stack_offset = *sym->stack_size;
}

static void emit_func(CodegenState* state, FuncSymbolTableEntry* func) {
    setup_func_state(state, func->func_data.return_type, func->func_sym);

    const FuncMetadata* func_data = &func->func_data;
    int args_size = get_func_args_size(func_data);
    if (func_data->callconv == CALLCONV_STDCALL) {
        genf(OS_SYM_PREFIX "%.*s@%d:", func->ident.len, func->ident.ptr,
             args_size);
    } else {
        genf(OS_SYM_PREFIX "%.*s:", func->ident.len, func->ident.ptr);
    }

    if (func->func_data.callconv == CALLCONV_THISCALL) {
        genf("    popl %%edx");   // return address
        genf("    pushl %%ecx");  // thisptr
        if (func_data->return_type->size > REGISTER_SIZE) {
            genf("    pushl %%eax");  // return value address
        }
        genf("    pushl %%edx");
    }

    emit_func_start(state, *func->func_sym->stack_size);

    emit_node(state, func->node);

    if (func->func_data.return_type->size > REGISTER_SIZE) {
        // Just in case function has no return but has return type
        genf("    movl 8(%%ebp), %%eax");
    }

    if (func_data->callconv == CALLCONV_CDECL) {
        emit_func_exit(state, 0);
    } else {
        emit_func_exit(state, args_size);
    }

    if (func_data->callconv == CALLCONV_STDCALL) {
        genf(".globl " OS_SYM_PREFIX "%.*s@%d", func->ident.len,
             func->ident.ptr, args_size);
    } else {
        genf(".globl " OS_SYM_PREFIX "%.*s", func->ident.len, func->ident.ptr);
    }
}

void codegen(CodegenState* state, ASTNode* node, SymbolTable* sym,
             Str entry_sym) {
    int has_user_defined_entry = (symbol_table_find(sym, entry_sym, 1) != NULL);

    // Global variables
    genf(".data");

    SymbolTableEntry* curr = sym->ste;
    while (curr) {
        if (curr->type == SYM_VAR) {
            VarSymbolTableEntry* var = (VarSymbolTableEntry*)curr;
            if (var->is_extern == 0) {
                genf(OS_SYM_PREFIX "%.*s:", var->ident.len, var->ident.ptr);
                if (var->init_val) {
                    switch (var->init_val->type) {
                        case NODE_INTLIT: {
                            IntLitNode* intlit = (IntLitNode*)var->init_val;
                            genf("    .long %d", intlit->val);
                        } break;
                        case NODE_STRLIT: {
                            StrLitNode* strlit = (StrLitNode*)var->init_val;
                            genf("    .long .LC%d",
                                 add_data(state, strlit->val));
                        } break;
                        default:
                            UNREACHABLE();
                    }
                } else {
                    int size = var->data_type->size;
                    int padding = (MAX_ALIGNMENT - (size % MAX_ALIGNMENT)) %
                                  MAX_ALIGNMENT;
                    genf("    .zero %d", size + padding);
                }

                genf(".globl " OS_SYM_PREFIX "%.*s", var->ident.len,
                     var->ident.ptr);
                fprintf(state->out, "\n");
            }
        }
        curr = curr->next;
    }

    // Functions
    genf(".text");

    curr = sym->ste;
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            if (func->node) {
                emit_func(state, func);
                fprintf(state->out, "\n");
            }
        }
        curr = curr->next;
    }

    // entry function
    if (!has_user_defined_entry) {
        setup_func_state(state, get_primitive_type(TYPE_I32), sym);
        ;

        genf(OS_SYM_PREFIX "%.*s:", entry_sym.len, entry_sym.ptr);
        emit_func_start(state, *sym->stack_size);
        emit_node(state, node);
        genf("    xorl %%eax, %%eax");
        emit_func_exit(state, 0);

        genf(".globl " OS_SYM_PREFIX "%.*s", entry_sym.len, entry_sym.ptr);
    }

    fprintf(state->out, "\n");

    // Strings
    genf(".data");
    for (int i = 0; i < state->data_count; i++) {
        genf(".LC%d:", i);
        genf("    .string  \"%.*s\"", state->data[i].len, state->data[i].ptr);
    }
}
