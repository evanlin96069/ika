#include "codegen.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "symbol_table.h"
#include "type.h"

#ifdef _DEBUG

#define GEN(out, ...)                                   \
    do {                                                \
        fprintf(out, __VA_ARGS__);                      \
        fprintf(out, " # %s:%d\n", __FILE__, __LINE__); \
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

static EmitResult error(SourcePos pos, const char* fmt, ...) {
    EmitResult result;
    result.type = RESULT_ERROR;
    result.error = malloc(sizeof(Error));
    result.error->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(result.error->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return result;
}

static EmitResult emit_node(CodegenState* state, ASTNode* node);

static EmitResult emit_stmts(CodegenState* state, StatementListNode* stmts) {
    EmitResult result;

    ASTNodeList* iter = stmts->stmts;
    while (iter) {
        result = emit_node(state, iter->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        iter = iter->next;
    }

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_intlit(CodegenState* state, IntLitNode* lit) {
    EmitResult result = {.type = RESULT_OK, .info = {0}};
    genf("    movl $%d, %%eax", lit->val);
    result.info.is_lvalue = 0;
    if (lit->data_type == TYPE_VOID) {
        result.info.type = *get_void_ptr_type();
    } else {
        result.info.type = *get_primitive_type(lit->data_type);
    }
    return result;
}

static EmitResult emit_strlit(CodegenState* state, StrLitNode* lit) {
    EmitResult result = {.type = RESULT_OK, .info = {0}};
    genf("    movl $DAT_%d, %%eax", add_data(state, lit->val));
    result.info.is_lvalue = 0;
    result.info.type = *get_string_type();
    return result;
}

// load the value into register if size is 4 bytes, 2 bytes, or 1 byte,
// else do nothing.
static void emit_rvalify(CodegenState* state, const Type* type) {
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

static EmitResult emit_binop(CodegenState* state, BinaryOpNode* binop) {
    EmitResult result;

    result = emit_node(state, binop->left);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (binop->op == TK_COMMA) {
        return emit_node(state, binop->right);
    }

    const Type l_type = result.info.type;
    if (!(is_bool(&l_type) || is_int(&l_type) || is_ptr_like(&l_type))) {
        return error(binop->pos, "invalid left operand to do binary operation");
    }

    if (result.info.is_lvalue) {
        emit_rvalify(state, &l_type);
    }

    // Boolean operations
    if (is_bool(&l_type)) {
        if (binop->op == TK_EQ || binop->op == TK_NE) {
            genf("    pushl %%eax");

            result = emit_node(state, binop->right);
            if (result.type == RESULT_ERROR) {
                return result;
            }

            const Type r_type = result.info.type;
            if (!is_bool(&r_type)) {
                return error(binop->pos,
                             "invalid right operand to do boolean operation");
            }

            if (result.info.is_lvalue) {
                emit_rvalify(state, &r_type);
            }

            genf("    movl %%eax, %%ecx");
            genf("    popl %%eax");

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
                genf("    jnz LAB_%d", label);
            } else if (binop->op == TK_LAND) {
                genf("    testl %%eax, %%eax");
                genf("    jz LAB_%d", label);
            } else {
                return error(binop->pos, "invalid boolean operator");
            }
            result = emit_node(state, binop->right);
            if (result.type == RESULT_ERROR) {
                return result;
            }

            const Type r_type = result.info.type;
            if (!is_bool(&r_type)) {
                return error(binop->pos,
                             "invalid right operand to do boolean operation");
            }

            if (result.info.is_lvalue) {
                emit_rvalify(state, &r_type);
            }

            genf("LAB_%d:", label);
        }
        result.type = RESULT_OK;
        result.info.is_lvalue = 0;
        result.info.type = *get_primitive_type(TYPE_BOOL);
    } else {
        genf("    pushl %%eax");

        result = emit_node(state, binop->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        const Type r_type = result.info.type;
        if (!is_int(&r_type) && !is_ptr_like(&r_type)) {
            return error(binop->pos,
                         "invalid right operand to do binary operation");
        }

        if (result.info.is_lvalue) {
            emit_rvalify(state, &r_type);
        }

        genf("    movl %%eax, %%ecx");
        genf("    popl %%eax");

        // At this point, both operands are either pointer or integer.
        switch (binop->op) {
            case TK_ADD:
            case TK_SUB: {
                int l_ptr = is_array_ptr(&l_type);
                int r_ptr = is_array_ptr(&r_type);

                if (l_ptr || r_ptr) {  // Pointers
                    if (l_ptr && r_ptr) {
                        return error(binop->pos,
                                     "invalid operands to do binary operation");
                    }

                    const Type* p_type = l_ptr ? &l_type : &r_type;
                    int size;
                    if (is_void(p_type->inner_type)) {
                        size = 1;
                    } else if (p_type->inner_type->incomplete) {
                        return error(binop->pos, "use of incomplete tyoe");
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
                    result.info.type = *p_type;
                } else if (is_int(&l_type) && is_int(&r_type)) {  // Integers
                    if (binop->op == TK_ADD) {
                        genf("    addl %%ecx, %%eax");
                    } else {  // TK_SUB
                        genf("    subl %%ecx, %%eax");
                    }
                    result.info.type =
                        *get_primitive_type(implicit_type_convert(
                            l_type.primitive_type, r_type.primitive_type));
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
                    if (is_int(&l_type) && is_int(&r_type)) {
                        is_valid_types = 1;
                    } else if (is_void_ptr(&l_type) && is_ptr_like(&r_type)) {
                        is_valid_types = 1;
                    } else if (is_void_ptr(&r_type) && is_ptr_like(&l_type)) {
                        is_valid_types = 1;
                    } else if (is_equal_type(&l_type, &r_type)) {
                        is_valid_types = 1;
                    }
                } else {
                    if (is_int(&l_type) && is_int(&r_type)) {
                        is_valid_types = 1;
                    } else if (is_array_ptr(&l_type) &&
                               is_equal_type(&l_type, &r_type)) {
                        is_valid_types = 1;
                    }
                }

                if (!is_valid_types) {
                    return error(binop->pos,
                                 "invalid operands for comparison operation");
                }

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

                result.info.type = *get_primitive_type(TYPE_BOOL);
            } break;

            default: {
                if (!is_int(&l_type) || !is_int(&r_type)) {
                    return error(binop->pos,
                                 "invalid operands to do binary operation");
                }

                PrimitiveType result_type = implicit_type_convert(
                    l_type.primitive_type, r_type.primitive_type);
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
                        assert(0);
                }
                result.info.type = *get_primitive_type(result_type);
            }
        }
        result.type = RESULT_OK;
        result.info.is_lvalue = 0;
    }

    return result;
}

static EmitResult emit_unaryop(CodegenState* state, UnaryOpNode* unaryop) {
    EmitResult result;

    result = emit_node(state, unaryop->node);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    int is_lvalue = result.info.is_lvalue;
    const Type* type = &result.info.type;

    result.info.is_lvalue = 0;

    switch (unaryop->op) {
        case TK_ADD:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }

            if (is_lvalue) {
                emit_rvalify(state, type);
            }
            break;

        case TK_SUB:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }

            if (is_lvalue) {
                emit_rvalify(state, type);
            }
            genf("    negl %%eax");
            break;

        case TK_NOT:
            if (!is_int(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }

            if (is_lvalue) {
                emit_rvalify(state, type);
            }
            genf("    notl %%eax");
            break;

        case TK_LNOT:
            if (!is_bool(type)) {
                return error(unaryop->pos,
                             "invalid type to do unary operation");
            }

            if (is_lvalue) {
                emit_rvalify(state, type);
            }

            genf("    testl %%eax, %%eax");
            genf("    sete %%al");
            genf("    movzbl %%al, %%eax");
            break;

        case TK_MUL:
            if (!is_ptr_like(type)) {
                return error(unaryop->pos,
                             "indirection requires pointer operand");
            }

            if (is_lvalue) {
                emit_rvalify(state, type);
            }

            if (is_ptr(type)) {
                result.info.type.pointer_level--;
                if (result.info.type.pointer_level == 0) {
                    result.info.type = *result.info.type.inner_type;
                }
            } else {
                result.info.type = *result.info.type.inner_type;
            }

            result.info.is_lvalue = 1;
            break;

        case TK_AND:
            if (is_lvalue == 0) {
                return error(unaryop->pos,
                             "lvalue required as unary '&' operand");
            }

            if (is_ptr(type)) {
                result.info.type.pointer_level++;
            } else {
                Type* inner = arena_alloc(state->arena, sizeof(Type));
                memcpy(inner, &result.info.type, sizeof(Type));
                result.info.type.type = METADATA_POINTER;
                result.info.type.size = PTR_SIZE;
                result.info.type.alignment = PTR_SIZE;
                result.info.type.pointer_level = 1;
                result.info.type.inner_type = inner;
            }
            break;

        default:
            assert(0);
    }

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_var(CodegenState* state, VarNode* var) {
    assert(var->ste->type != SYM_DEF);

    EmitResult result;
    if (var->ste->type == SYM_VAR) {
        // Variable
        VarSymbolTableEntry* var_ste = (VarSymbolTableEntry*)var->ste;
        if (var_ste->is_extern) {
            // Extern variable
            genf("    movl $%.*s, %%eax", var_ste->ident.len,
                 var_ste->ident.ptr);
        } else if (var_ste->is_global) {
            // Global variable
            genf("    movl $VAR_%.*s, %%eax", var_ste->ident.len,
                 var_ste->ident.ptr);
        } else {
            // Local variable
            genf("    leal %d(%%ebp), %%eax", var_ste->offset);
        }
        result.info.is_lvalue = 1;
        result.info.type = *var_ste->data_type;
    } else if (var->ste->type == SYM_FUNC) {
        // Function pointer
        FuncSymbolTableEntry* func_ste = (FuncSymbolTableEntry*)var->ste;
        if (func_ste->is_extern || func_ste->node == NULL) {
            // Extern function
            genf("    movl $%.*s, %%eax", func_ste->ident.len,
                 func_ste->ident.ptr);
        } else {
            // ika function
            genf("    movl $FUNC_%.*s, %%eax", func_ste->ident.len,
                 func_ste->ident.ptr);
        }
        result.info.is_lvalue = 0;
        result.info.type.type = METADATA_FUNC;
        result.info.type.func_data = func_ste->func_data;
    }

    result.type = RESULT_OK;
    return result;
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

static EmitResult emit_assign(CodegenState* state, AssignNode* assign) {
    EmitResult result;

    result = emit_node(state, assign->left);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (result.info.is_lvalue == 0) {
        return error(assign->pos,
                     "lvalue required as left operand of assignment");
    }

    const Type l_type = result.info.type;

    genf("    pushl %%eax");

    result = emit_node(state, assign->right);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    const Type r_type = result.info.type;

    if (result.info.is_lvalue) {
        emit_rvalify(state, &result.info.type);
    }

    if (!is_allowed_type_convert(&l_type, &r_type)) {
        return error(assign->pos, "type is not assignable");
    }

    genf("    popl %%ecx");

    // left address on ecx, right value in eax
    switch (l_type.size) {
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
        default:
            genf("    movl $%d, %%edx", l_type.size);
            genf("    pushl %%edx");  // n
            genf("    push %%eax");   // src
            genf("    pushl %%ecx");  // dest
            genf("    call memcpy");
            genf("    addl $12, %%esp");
            break;
    }

    genf("    movl %%ecx, %%eax");

    result.type = RESULT_OK;
    result.info.is_lvalue = 1;
    result.info.type = l_type;
    return result;
}

static EmitResult emit_if(CodegenState* state, IfStatementNode* if_node) {
    /*
     *      <cond>
     *      JZ else_label
     *      <then_block>
     *      JMP end_label
     *  else_label:
     *      <else_block>
     *  end_label:
     */

    EmitResult result;

    int end_label = add_label(state);
    int else_label = add_label(state);

    result = emit_node(state, if_node->expr);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (!is_bool(&result.info.type)) {
        return error(if_node->expr->pos, "expected type 'bool'");
    }

    if (result.info.is_lvalue) {
        emit_rvalify(state, &result.info.type);
    }

    genf("    testl %%eax, %%eax");
    genf("    jz LAB_%d", else_label);

    result = emit_node(state, if_node->then_block);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    genf("    jmp LAB_%d", end_label);

    genf("LAB_%d:", else_label);

    if (if_node->else_block) {
        result = emit_node(state, if_node->else_block);
        if (result.type == RESULT_ERROR) {
            return result;
        }
    }

    genf("LAB_%d:", end_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_while(CodegenState* state, WhileNode* while_node) {
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

    EmitResult result;

    int loop_label = add_label(state);
    int inc_label = add_label(state);
    int end_label = add_label(state);

    genf("LAB_%d:", loop_label);

    result = emit_node(state, while_node->expr);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (!is_bool(&result.info.type)) {
        return error(while_node->expr->pos, "expected type 'bool'");
    }

    if (result.info.is_lvalue) {
        emit_rvalify(state, &result.info.type);
    }

    genf("    testl %%eax, %%eax");
    genf("    jz LAB_%d", end_label);

    int prev_in_loop = state->in_loop;
    int prev_break_label = state->break_label;
    int prev_continue_label = state->continue_label;

    state->in_loop = 1;
    state->break_label = end_label;
    state->continue_label = inc_label;

    result = emit_node(state, while_node->block);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    state->in_loop = prev_in_loop;
    state->break_label = prev_break_label;
    state->continue_label = prev_continue_label;

    genf("LAB_%d:", inc_label);
    if (while_node->inc) {
        result = emit_node(state, while_node->inc);
        if (result.type == RESULT_ERROR) {
            return result;
        }
    }

    genf("    jmp LAB_%d", loop_label);

    genf("LAB_%d:", end_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_goto(CodegenState* state, GotoNode* node) {
    EmitResult result;

    switch (node->op) {
        case TK_BREAK:
            if (state->in_loop == 0) {
                return error(node->pos, "break statement not within a loop");
            }
            genf("    jmp LAB_%d", state->break_label);
            break;
        case TK_CONTINUE:
            if (state->in_loop == 0) {
                return error(node->pos, "continue statement not within a loop");
            }
            genf("    jmp LAB_%d", state->continue_label);
            break;
        default:
            assert(0);
    }

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_call(CodegenState* state, CallNode* call) {
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

    EmitResult result;

    // Don't emit things, we just want to get the function type.
    FILE* old_out = state->out;
    state->out = NULL;
    EmitResult func_result = emit_node(state, call->node);
    state->out = old_out;

    if (func_result.type == RESULT_ERROR) {
        return func_result;
    }

    const Type* func_type = &func_result.info.type;
    if (func_type->type != METADATA_FUNC) {
        return error(call->pos,
                     "called object is not a function or function pointer");
    }

    ASTNodeList* curr = call->args;
    ArgList* arg_type = func_type->func_data.args;
    int has_va_args = func_type->func_data.has_va_args;
    int args_size = 0;
    while (curr) {
        result = emit_node(state, curr->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (arg_type != NULL) {
            // TODO: Add va_args type check
            if (!has_va_args &&
                !is_allowed_type_convert(arg_type->type, &result.info.type)) {
                return error(curr->node->pos,
                             "passing argument with invalid type");
            }
            arg_type = arg_type->next;
        } else if (!has_va_args) {
            return error(call->pos, "too many arguments");
        }

        if (result.info.is_lvalue) {
            emit_rvalify(state, &result.info.type);
        }

        int size = result.info.type.size;
        int padding = (MAX_ALIGNMENT - (size % MAX_ALIGNMENT)) % MAX_ALIGNMENT;

        if (size <= 4) {
            genf("    pushl %%eax");
        } else {
            genf("    subl $%d, %%esp", size + padding);
            genf("    movl %%esp, %%edx");
            genf("    movl $%d, %%ecx", size);
            genf("    pushl %%ecx");  // n
            genf("    pushl %%eax");  // src
            genf("    pushl %%edx");  // dest
            genf("    call memcpy");
            genf("    addl $12, %%esp");
        }

        args_size += size + padding;
        curr = curr->next;
    }

    if (arg_type != NULL) {
        return error(call->pos, "too few arguments");
    }

    emit_node(state, call->node);
    if (func_result.info.is_lvalue) {
        emit_rvalify(state, func_type);
    }
    genf("    call *%%eax");

    if (args_size > 0) {
        genf("    addl $%d, %%esp", args_size);
    }

    // TODO: Return struct
    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    result.info.type = *func_type->func_data.return_type;
    return result;
}

static EmitResult emit_print(CodegenState* state, PrintNode* print_node) {
    EmitResult result;

    int arg_count = 0;
    ASTNodeList* curr = print_node->args;
    while (curr) {
        result = emit_node(state, curr->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.type.size > 4) {
            return error(curr->node->pos, "passing argument with invalid type");
        }

        if (result.info.is_lvalue) {
            emit_rvalify(state, &result.info.type);
        }

        genf("    pushl %%eax");
        arg_count++;
        curr = curr->next;
    }

    genf("    pushl $DAT_%d", add_data(state, print_node->fmt));
    arg_count++;

    genf("    call printf");
    genf("    addl $%d, %%esp", arg_count * 4);

    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    result.info.type = *get_primitive_type(TYPE_VOID);
    return result;
}

static EmitResult emit_ret(CodegenState* state, ReturnNode* ret) {
    EmitResult result = {0};

    if (ret->expr) {
        result = emit_node(state, ret->expr);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(state, &result.info.type);
        }
    } else {
        result.info.type = *get_primitive_type(TYPE_VOID);
    }

    if (!is_allowed_type_convert(state->ret_type, &result.info.type)) {
        return error(ret->pos, "invalid return type");
    }

    if (result.info.type.size > 4) {
        return error(ret->pos, "returning large type is not implemented yet");
    }

    genf("    jmp LAB_%d", state->ret_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_field(CodegenState* state, FieldNode* field) {
    EmitResult result;

    result = emit_node(state, field->node);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    const Type* l_type = &result.info.type;
    if (l_type->type == METADATA_POINTER && l_type->pointer_level == 1) {
        // member access through pointer
        emit_rvalify(state, l_type);
        l_type = l_type->inner_type;
    }

    if (l_type->type != METADATA_TYPE) {
        return error(field->pos,
                     "request for member in something not a struct");
    }

    const TypeSymbolTableEntry* type_ste = l_type->type_ste;
    FieldSymbolTableEntry* ste = (FieldSymbolTableEntry*)symbol_table_find(
        type_ste->name_space, field->ident, 1);
    if (ste == NULL || ste->type != SYM_FIELD) {
        return error(field->pos, "type has no member '%.*s'", field->ident.len,
                     field->ident.ptr);
    }

    genf("    leal %d(%%eax), %%eax", ste->offset);

    result.type = RESULT_OK;
    result.info.is_lvalue = 1;
    result.info.type = *ste->data_type;
    return result;
}

static EmitResult emit_indexof(CodegenState* state, IndexOfNode* idxof) {
    EmitResult result;

    result = emit_node(state, idxof->left);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    Type l_type = result.info.type;
    if (l_type.type != METADATA_ARRAY) {
        return error(idxof->pos,
                     "subscripted value is neither array nor array pointer");
    }

    if (result.info.is_lvalue && l_type.array_size == 0) {
        emit_rvalify(state, &result.info.type);
    }

    genf("    pushl %%eax");

    result = emit_node(state, idxof->right);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    Type r_type = result.info.type;
    if (!is_int(&r_type)) {
        return error(idxof->pos, "array subscript is not an integer");
    }

    if (result.info.is_lvalue) {
        emit_rvalify(state, &result.info.type);
    }

    genf("    popl %%ecx");
    // ecx = array, eax = index
    genf("    imull $%d, %%eax", l_type.inner_type->size);
    genf("    addl %%ecx, %%eax");

    result.type = RESULT_OK;
    result.info.is_lvalue = 1;
    result.info.type = *l_type.inner_type;
    return result;
}

static EmitResult emit_node(CodegenState* state, ASTNode* node) {
    EmitResult result;

    switch (node->type) {
        case NODE_STMTS:
            result = emit_stmts(state, (StatementListNode*)node);
            break;

        case NODE_INTLIT:
            result = emit_intlit(state, (IntLitNode*)node);
            break;

        case NODE_STRLIT:
            result = emit_strlit(state, (StrLitNode*)node);
            break;

        case NODE_BINARYOP:
            result = emit_binop(state, (BinaryOpNode*)node);
            break;

        case NODE_UNARYOP:
            result = emit_unaryop(state, (UnaryOpNode*)node);
            break;

        case NODE_VAR:
            result = emit_var(state, (VarNode*)node);
            break;

        case NODE_ASSIGN:
            result = emit_assign(state, (AssignNode*)node);
            break;

        case NODE_IF:
            result = emit_if(state, (IfStatementNode*)node);
            break;

        case NODE_WHILE:
            result = emit_while(state, (WhileNode*)node);
            break;

        case NODE_GOTO:
            result = emit_goto(state, (GotoNode*)node);
            break;

        case NODE_CALL:
            result = emit_call(state, (CallNode*)node);
            break;

        case NODE_PRINT:
            result = emit_print(state, (PrintNode*)node);
            break;

        case NODE_RET:
            result = emit_ret(state, (ReturnNode*)node);
            break;

        case NODE_FIELD:
            result = emit_field(state, (FieldNode*)node);
            break;

        case NODE_INDEXOF:
            result = emit_indexof(state, (IndexOfNode*)node);
            break;

        case NODE_TYPE:
            assert(0);
            break;

        default:
            assert(0);
    }

    return result;
}

static inline void emit_func_start(CodegenState* state, int stack_size) {
    genf("    pushl %%ebp");
    genf("    movl %%esp, %%ebp");
    // genf("    pushl %%edi");
    // genf("    pushl %%esi");
    // genf("    pushl %%ebx");
    if (stack_size) {
        genf("    subl $%d, %%esp", stack_size);
    }
}

static inline void emit_func_exit(CodegenState* state) {
    genf("LAB_%d:", state->ret_label);
    // genf("    popl %%ebx");
    // genf("    popl %%esi");
    // genf("    popl %%edi");
    genf("    leave");
    genf("    ret");
}

static EmitResult emit_func(CodegenState* state, FuncSymbolTableEntry* func) {
    EmitResult result;

    int prev_ret_label = state->ret_label;
    const Type* prev_ret_type = state->ret_type;
    state->ret_label = add_label(state);
    state->ret_type = func->func_data.return_type;

    assert(func->node);
    assert(func->is_extern == 0);

    genf("FUNC_%.*s:", func->ident.len, func->ident.ptr);
    emit_func_start(state, *func->sym->stack_size);

    result = emit_node(state, func->node);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    emit_func_exit(state);

    state->ret_label = prev_ret_label;
    state->ret_type = prev_ret_type;

    result.type = RESULT_OK;
    return result;
}

Error* codegen(CodegenState* state, ASTNode* node, SymbolTable* sym) {
    EmitResult result;

    SymbolTableEntry* curr = sym->ste;

    // Global variables
    SymbolTableEntry* ste = sym->ste;
    genf(".data");
    while (ste) {
        if (ste->type == SYM_VAR) {
            VarSymbolTableEntry* var = (VarSymbolTableEntry*)ste;
            if (var->is_extern == 0) {
                int size = var->data_type->size;
                int padding =
                    (MAX_ALIGNMENT - (size % MAX_ALIGNMENT)) % MAX_ALIGNMENT;
                genf("VAR_%.*s:", var->ident.len, var->ident.ptr);
                genf("    .zero %d", size + padding);
            }
        }
        ste = ste->next;
    }

    // Functions
    genf(".text");
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            if (func->node) {
                result = emit_func(state, func);
                if (result.type == RESULT_ERROR) {
                    return result.error;
                }
                fprintf(state->out, "\n");
            }
        }
        curr = curr->next;
    }

    // main
    state->ret_label = add_label(state);
    state->ret_type = get_primitive_type(TYPE_I32);

    genf(".global main");
    genf("main:");
    emit_func_start(state, *sym->stack_size);

    genf("    movl 8(%%ebp), %%eax");
    genf("    movl %%eax, VAR_argc");
    genf("    movl 12(%%ebp), %%eax");
    genf("    movl %%eax, VAR_argv");

    result = emit_node(state, node);
    if (result.type == RESULT_ERROR) {
        return result.error;
    }

    genf("    xorl %%eax, %%eax");
    emit_func_exit(state);
    genf(".type main, @function");
    fprintf(state->out, "\n");

    // Strings
    genf(".data");
    for (int i = 0; i < state->data_count; i++) {
        genf("DAT_%d:", i);
        genf("    .asciz \"%.*s\"", state->data[i].len, state->data[i].ptr);
    }

    return NULL;
}
