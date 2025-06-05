#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static ASTNode* primary(ParserState* parser);
static ASTNode* expr(ParserState* parser, int min_precedence);
static ASTNode* data_type(ParserState* parser, int allow_incomplete);
static ASTNode* var_decl(ParserState* parser, int is_extern);
static ASTNode* def_decl(ParserState* parser);
static ASTNode* struct_decl(ParserState* parser);
static ASTNode* enum_decl(ParserState* parser);
static ASTNode* func_decl(ParserState* parser, int is_extern);
static ASTNode* return_stmt(ParserState* parser);
static ASTNode* if_stmt(ParserState* parser);
static ASTNode* while_stmt(ParserState* parser);
static ASTNode* scope(ParserState* parser);
static ASTNode* stmt(ParserState* parser);
static ASTNode* stmt_list(ParserState* parser, int in_scope);

static ASTNode* error(ParserState* parser, SourcePos pos, const char* fmt,
                      ...) {
    ErrorNode* err = arena_alloc(parser->arena, sizeof(ErrorNode));
    err->type = NODE_ERR;
    err->pos = pos;

    err->val = malloc(sizeof(Error));
    err->val->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->val->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return (ASTNode*)err;
}

static inline PrimitiveType get_intlit_type(unsigned int val) {
    if (val & (1 << 31)) {
        return TYPE_U32;
    }
    return TYPE_I32;
}

static ASTNode* primary(ParserState* parser) {
    Token tk = next_token(parser);

    ASTNode* node = NULL;

    switch (tk.type) {
        case TK_INT:
        case TK_TRUE:
        case TK_FALSE:
        case TK_NULL: {
            IntLitNode* intlit = arena_alloc(parser->arena, sizeof(IntLitNode));
            intlit->type = NODE_INTLIT;
            intlit->pos = parser->post_token_pos;
            if (tk.type == TK_INT) {
                intlit->data_type = get_intlit_type(tk.val);
                intlit->val = tk.val;
            } else if (tk.type == TK_NULL) {
                intlit->data_type = TYPE_VOID;  // This is actually void pointer
                intlit->val = 0;
            } else {
                intlit->data_type = TYPE_BOOL;
                intlit->val = tk.type == TK_TRUE;
            }
            node = (ASTNode*)intlit;
        } break;

        case TK_SIZEOF: {
            IntLitNode* intlit = arena_alloc(parser->arena, sizeof(IntLitNode));
            intlit->type = NODE_INTLIT;
            intlit->pos = parser->post_token_pos;
            tk = next_token(parser);
            if (tk.type != TK_LPAREN) {
                return error(parser, parser->pre_token_pos, "expected '('");
            }

            ASTNode* type_node = data_type(parser, 0);
            if (type_node->type == NODE_ERR) {
                return type_node;
            }

            tk = next_token(parser);
            if (tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos, "expected ')'");
            }

            intlit->data_type = TYPE_U32;
            assert(type_node->type == NODE_TYPE);
            intlit->val = ((TypeNode*)type_node)->data_type->size;

            node = (ASTNode*)intlit;
        } break;

        case TK_CAST: {
            CastNode* cast = arena_alloc(parser->arena, sizeof(CastNode));
            cast->type = NODE_CAST;
            cast->pos = parser->post_token_pos;
            tk = next_token(parser);
            if (tk.type != TK_LPAREN) {
                return error(parser, parser->pre_token_pos, "expected '('");
            }

            TypeNode* type_node = (TypeNode*)data_type(parser, 0);
            if (type_node->type == NODE_ERR) {
                return (ASTNode*)type_node;
            }
            assert(type_node->type == NODE_TYPE);

            cast->data_type = type_node->data_type;

            tk = next_token(parser);
            if (tk.type != TK_COMMA) {
                return error(parser, parser->pre_token_pos, "expected ','");
            }

            ASTNode* expr_node = expr(parser, 1);
            if (expr_node->type == NODE_ERR) {
                return expr_node;
            }
            cast->expr = expr_node;

            tk = next_token(parser);
            if (tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos, "expected ')'");
            }

            node = (ASTNode*)cast;
        } break;

        case TK_STR: {
            StrLitNode* strlit = arena_alloc(parser->arena, sizeof(StrLitNode));
            strlit->type = NODE_STRLIT;
            strlit->pos = parser->post_token_pos;
            strlit->val = tk.str;
            node = (ASTNode*)strlit;
        } break;

        case TK_IDENT: {
            SymbolTableEntry* ste = symbol_table_find(parser->sym, tk.str, 0);
            if (!ste) {
                return error(parser, parser->post_token_pos,
                             "'%.*s' undeclared", tk.str.len, tk.str.ptr);
            }

            switch (ste->type) {
                case SYM_VAR:
                case SYM_FUNC: {
                    VarNode* var_node =
                        arena_alloc(parser->arena, sizeof(VarNode));
                    var_node->type = NODE_VAR;
                    var_node->pos = parser->post_token_pos;
                    var_node->ste = ste;
                    node = (ASTNode*)var_node;
                } break;

                case SYM_DEF: {
                    DefSymbolTableEntry* def = (DefSymbolTableEntry*)ste;
                    if (def->val.is_str) {
                        StrLitNode* strlit =
                            arena_alloc(parser->arena, sizeof(StrLitNode));
                        strlit->type = NODE_STRLIT;
                        strlit->pos = parser->post_token_pos;
                        strlit->val = def->val.str;
                        node = (ASTNode*)strlit;
                    } else {
                        IntLitNode* intlit =
                            arena_alloc(parser->arena, sizeof(IntLitNode));
                        intlit->type = NODE_INTLIT;
                        intlit->pos = parser->post_token_pos;
                        intlit->data_type = def->val.data_type;
                        intlit->val = def->val.val;
                        node = (ASTNode*)intlit;
                    }
                } break;

                case SYM_TYPE:
                    return error(parser, parser->post_token_pos,
                                 "expected an expression", tk.str.len,
                                 tk.str.ptr);

                default:
                    UNREACHABLE();
            }
        } break;

        case TK_LPAREN: {
            node = expr(parser, 0);
            if (node->type == NODE_ERR) {
                return node;
            }
            tk = next_token(parser);
            if (tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos, "expected ')'");
            }
        } break;

        case TK_ADD:
        case TK_SUB:
        case TK_NOT:
        case TK_LNOT:
        case TK_MUL:
        case TK_AND: {
            ASTNode* right = primary(parser);
            if (right->type == NODE_ERR) {
                return right;
            }

            if (right->type == NODE_INTLIT) {
                IntLitNode* intlit = (IntLitNode*)right;
                if (intlit->data_type == TYPE_BOOL && tk.type == TK_LNOT) {
                    intlit->val = !intlit->val;
                    node = (ASTNode*)intlit;
                } else if (tk.type == TK_ADD || tk.type == TK_SUB ||
                           tk.type == TK_NOT) {
                    int val = intlit->val;
                    switch (tk.type) {
                        case TK_ADD:
                            intlit->data_type = TYPE_I32;
                            break;
                        case TK_SUB:
                            intlit->data_type = TYPE_I32;
                            intlit->val = (-val);
                            break;
                        case TK_NOT:
                            intlit->data_type = TYPE_U32;
                            intlit->val = (~val);
                            break;
                        default:
                            UNREACHABLE();
                    }
                    node = (ASTNode*)intlit;
                }
            }

            if (node == NULL) {
                UnaryOpNode* unary_node =
                    arena_alloc(parser->arena, sizeof(UnaryOpNode));
                unary_node->type = NODE_UNARYOP;
                unary_node->pos = parser->post_token_pos;
                unary_node->op = tk.type;
                unary_node->node = right;
                node = (ASTNode*)unary_node;
            }
        } break;

        case TK_ERR:
            return error(parser, parser->post_token_pos, "%.*s", tk.str.len,
                         tk.str.ptr);

        default:
            return error(parser, parser->post_token_pos, "unexpected token");
    }

    int is_trailing_operator = 1;
    while (is_trailing_operator) {
        tk = peek_token(parser);
        switch (tk.type) {
            case TK_DOT: {
                next_token(parser);
                FieldNode* field =
                    arena_alloc(parser->arena, sizeof(FieldNode));
                field->type = NODE_FIELD;
                field->pos = parser->post_token_pos;
                field->node = node;
                tk = next_token(parser);
                if (tk.type != TK_IDENT) {
                    return error(parser, parser->pre_token_pos,
                                 "expected an identifier");
                }
                field->ident = tk.str;
                node = (ASTNode*)field;
            } break;

            case TK_LBRACKET: {
                next_token(parser);
                IndexOfNode* indexof =
                    arena_alloc(parser->arena, sizeof(IndexOfNode));
                indexof->type = NODE_INDEXOF;
                indexof->pos = parser->post_token_pos;
                indexof->left = node;
                indexof->right = expr(parser, 0);
                if (indexof->right->type == NODE_ERR) {
                    return indexof->right;
                }
                node = (ASTNode*)indexof;

                tk = next_token(parser);
                if (tk.type != TK_RBRACKET) {
                    return error(parser, parser->pre_token_pos, "expected ']'");
                }
            } break;

            case TK_LPAREN: {
                next_token(parser);
                CallNode* call_node =
                    arena_alloc(parser->arena, sizeof(CallNode));
                call_node->type = NODE_CALL;
                call_node->pos = parser->post_token_pos;
                call_node->node = node;
                call_node->args = NULL;

                tk = peek_token(parser);
                if (tk.type == TK_RPAREN) {
                    next_token(parser);
                } else {
                    while (tk.type != TK_RPAREN) {
                        ASTNode* arg_node = expr(parser, 1);
                        if (arg_node->type == NODE_ERR) {
                            return arg_node;
                        }

                        ASTNodeList* arg =
                            arena_alloc(parser->arena, sizeof(ASTNodeList));
                        arg->node = arg_node;
                        arg->next = call_node->args;
                        call_node->args = arg;

                        tk = next_token(parser);
                        if (tk.type != TK_COMMA && tk.type != TK_RPAREN) {
                            return error(parser, parser->pre_token_pos,
                                         "expected ',' or ')'");
                        }
                    }
                }
                node = (ASTNode*)call_node;
            } break;

            default:
                is_trailing_operator = 0;
                break;
        }
    }

    return node;
}

static inline int get_precedence(TkType type) {
    switch (type) {
        case TK_COMMA:
            return 0;

        case TK_ASSIGN:
        case TK_AADD:
        case TK_ASUB:
        case TK_AMUL:
        case TK_ADIV:
        case TK_AMOD:
        case TK_ASHL:
        case TK_ASHR:
        case TK_AAND:
        case TK_AXOR:
        case TK_AOR:
            return 1;

        case TK_LOR:
            return 2;

        case TK_LAND:
            return 3;

        case TK_OR:
            return 4;

        case TK_XOR:
            return 5;

        case TK_AND:
            return 6;

        case TK_EQ:
        case TK_NE:
            return 7;

        case TK_LT:
        case TK_LE:
        case TK_GT:
        case TK_GE:
            return 8;

        case TK_SHL:
        case TK_SHR:
            return 9;

        case TK_ADD:
        case TK_SUB:
            return 10;

        case TK_MUL:
        case TK_DIV:
        case TK_MOD:
            return 11;

        default:
            return -1;
    }
}

static inline int is_left_associative(TkType type) {
    switch (type) {
        case TK_ASSIGN:
        case TK_AADD:
        case TK_ASUB:
        case TK_AMUL:
        case TK_ADIV:
        case TK_AMOD:
        case TK_ASHL:
        case TK_ASHR:
        case TK_AAND:
        case TK_AXOR:
        case TK_AOR:
            return 0;

        case TK_COMMA:
        case TK_MUL:
        case TK_DIV:
        case TK_MOD:
        case TK_ADD:
        case TK_SUB:
        case TK_SHL:
        case TK_SHR:
        case TK_LT:
        case TK_LE:
        case TK_GT:
        case TK_GE:
        case TK_EQ:
        case TK_NE:
        case TK_AND:
        case TK_XOR:
        case TK_OR:
        case TK_LAND:
        case TK_LOR:
            return 1;

        default:
            UNREACHABLE();
    }
}

static ASTNode* expr(ParserState* parser, int min_precedence) {
    ASTNode* node = primary(parser);
    if (node->type == NODE_ERR) {
        return node;
    }

    int precedence = get_precedence(peek_token(parser).type);
    while (precedence != -1 && precedence >= min_precedence) {
        Token tk = next_token(parser);

        int next_precedence = precedence;
        if (is_left_associative(tk.type)) {
            next_precedence++;
        }

        if (next_precedence < min_precedence) {
            return node;
        }

        switch (tk.type) {
            case TK_ASSIGN: {
                AssignNode* assign =
                    arena_alloc(parser->arena, sizeof(AssignNode));
                assign->type = NODE_ASSIGN;
                assign->pos = parser->post_token_pos;
                assign->left = node;
                assign->right = expr(parser, next_precedence);
                assign->from_decl = 0;
                if (assign->right->type == NODE_ERR) {
                    return assign->right;
                }
                node = (ASTNode*)assign;
            } break;

            case TK_AADD:
            case TK_ASUB:
            case TK_AMUL:
            case TK_ADIV:
            case TK_AMOD:
            case TK_ASHL:
            case TK_ASHR:
            case TK_AAND:
            case TK_AXOR:
            case TK_AOR: {
                AssignNode* assign =
                    arena_alloc(parser->arena, sizeof(AssignNode));
                assign->type = NODE_ASSIGN;
                assign->pos = parser->post_token_pos;
                assign->left = node;
                assign->from_decl = 0;

                BinaryOpNode* binop =
                    arena_alloc(parser->arena, sizeof(BinaryOpNode));
                binop->type = NODE_BINARYOP;
                binop->pos = parser->post_token_pos;
                switch (tk.type) {
                    case TK_AADD:
                        binop->op = TK_ADD;
                        break;
                    case TK_ASUB:
                        binop->op = TK_SUB;
                        break;
                    case TK_AMUL:
                        binop->op = TK_MUL;
                        break;
                    case TK_ADIV:
                        binop->op = TK_DIV;
                        break;
                    case TK_AMOD:
                        binop->op = TK_MOD;
                        break;
                    case TK_ASHL:
                        binop->op = TK_SHL;
                        break;
                    case TK_ASHR:
                        binop->op = TK_SHR;
                        break;
                    case TK_AAND:
                        binop->op = TK_AND;
                        break;
                    case TK_AXOR:
                        binop->op = TK_XOR;
                        break;
                    case TK_AOR:
                        binop->op = TK_OR;
                        break;
                    default:
                        UNREACHABLE();
                }
                binop->left = assign->left;
                binop->right = expr(parser, next_precedence);
                if (binop->right->type == NODE_ERR) {
                    return binop->right;
                }

                assign->right = (ASTNode*)binop;
                node = (ASTNode*)assign;
            } break;

            default: {
                SourcePos pos = parser->post_token_pos;
                ASTNode* out = NULL;
                ASTNode* left = node;
                ASTNode* right = expr(parser, next_precedence);
                if (right->type == NODE_ERR) {
                    return right;
                }

                if (tk.type != TK_COMMA && left->type == NODE_INTLIT &&
                    right->type == NODE_INTLIT) {
                    IntLitNode* l_lit = (IntLitNode*)left;
                    IntLitNode* r_lit = (IntLitNode*)right;

                    unsigned int a = l_lit->val;
                    unsigned int b = r_lit->val;

                    if (l_lit->data_type == TYPE_BOOL &&
                        r_lit->data_type == TYPE_BOOL) {
                        out = (ASTNode*)l_lit;
                        switch (tk.type) {
                            case TK_LOR:
                                l_lit->val = (a || b);
                                break;

                            case TK_LAND:
                                l_lit->val = (a && b);
                                break;

                            default:
                                out = NULL;
                                break;
                        }
                    } else if ((l_lit->data_type == TYPE_I32 ||
                                l_lit->data_type == TYPE_U32) &&
                               (r_lit->data_type == TYPE_I32 ||
                                r_lit->data_type == TYPE_U32)) {
                        out = (ASTNode*)l_lit;
                        int has_unsigned = (l_lit->data_type == TYPE_U32 ||
                                            r_lit->data_type == TYPE_U32);
                        switch (tk.type) {
                            case TK_ADD:
                                l_lit->val = (a + b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_SUB:
                                l_lit->val = (a - b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_MUL:
                                l_lit->val = (a * b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_DIV:
                                if (b == 0) {
                                    return error(parser, pos,
                                                 "division by zero");
                                }

                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                    l_lit->val = (a / b);
                                } else {
                                    int sa = a;
                                    int sb = b;
                                    l_lit->data_type = (sa / sb);
                                }
                                break;

                            case TK_MOD:
                                if (b == 0) {
                                    return error(parser, pos, "modulo by zero");
                                }

                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                    l_lit->val = (a % b);
                                } else {
                                    int sa = a;
                                    int sb = b;
                                    l_lit->data_type = (sa % sb);
                                }
                                break;

                            case TK_SHL:
                                l_lit->val = (a << b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_SHR:
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                    l_lit->val = (a >> b);
                                } else {
                                    int sa = a;
                                    int sb = b;
                                    l_lit->data_type = (sa >> sb);
                                }
                                break;

                            case TK_AND:
                                l_lit->val = (a & b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_XOR:
                                l_lit->val = (a ^ b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_OR:
                                l_lit->val = (a | b);
                                if (has_unsigned) {
                                    l_lit->data_type = TYPE_U32;
                                }
                                break;

                            case TK_EQ:
                                l_lit->val = (a == b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            case TK_NE:
                                l_lit->val = (a != b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            case TK_LT:
                                l_lit->val = (a < b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            case TK_LE:
                                l_lit->val = (a <= b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            case TK_GT:
                                l_lit->val = (a > b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            case TK_GE:
                                l_lit->val = (a >= b);
                                l_lit->data_type = TYPE_BOOL;
                                break;

                            default:
                                out = NULL;
                        }
                    }
                }

                if (out == NULL) {
                    BinaryOpNode* binop =
                        arena_alloc(parser->arena, sizeof(BinaryOpNode));
                    binop->type = NODE_BINARYOP;
                    binop->pos = pos;
                    binop->op = tk.type;
                    binop->left = left;
                    binop->right = right;
                    out = (ASTNode*)binop;
                }
                node = out;
            }
        }

        precedence = get_precedence(peek_token(parser).type);
    }

    return node;
}

static inline int is_primitive_type(Token tk) {
    switch (tk.type) {
        case TK_VOID:
        case TK_BOOL:
        case TK_U8:
        case TK_U16:
        case TK_U32:
        case TK_I8:
        case TK_I16:
        case TK_I32:
            return 1;
        default:
            return 0;
    }
}

static inline PrimitiveType primitive_type_token_to_type(Token tk) {
    switch (tk.type) {
        case TK_VOID:
            return TYPE_VOID;
        case TK_BOOL:
            return TYPE_BOOL;
        case TK_U8:
            return TYPE_U8;
        case TK_U16:
            return TYPE_U16;
        case TK_U32:
            return TYPE_U32;
        case TK_I8:
            return TYPE_I8;
        case TK_I16:
            return TYPE_I16;
        case TK_I32:
            return TYPE_I32;
        default:
            UNREACHABLE();
    }
}

typedef struct StrCallConv {
    const char* s;
    CallConvType call_type;
} StrCallConv;

static StrCallConv str_callconv[] = {
    {"cdecl", CALLCONV_CDECL},
    {"stdcall", CALLCONV_STDCALL},
    {"thiscall", CALLCONV_THISCALL},
};

static ASTNode* data_type(ParserState* parser, int allow_incomplete) {
    Token tk = next_token(parser);

    TypeNode* type_node = arena_alloc(parser->arena, sizeof(TypeNode));
    type_node->type = NODE_TYPE;
    type_node->pos = parser->post_token_pos;

    if (is_primitive_type(tk)) {
        // Primitive type
        PrimitiveType primitive = primitive_type_token_to_type(tk);
        if (primitive == TYPE_VOID) {
            if (!allow_incomplete) {
                return error(parser, parser->post_token_pos,
                             "incomplete type is not allowed");
            }
        }
        type_node->data_type = get_primitive_type(primitive);
        return (ASTNode*)type_node;
    }

    Type* type = arena_alloc(parser->arena, sizeof(Type));
    switch (tk.type) {
        case TK_MUL: {
            type->incomplete = 0;
            type->size = PTR_SIZE;
            type->alignment = PTR_SIZE;

            // Pointer
            type->type = METADATA_POINTER;
            type->pointer_level = 1;
            tk = peek_token(parser);
            while (tk.type == TK_MUL) {
                type->pointer_level++;
                next_token(parser);
                tk = peek_token(parser);
            }

            ASTNode* inner = data_type(parser, 1);
            if (inner->type == NODE_ERR) {
                return inner;
            }
            type->inner_type = ((TypeNode*)inner)->data_type;

        } break;

        case TK_LBRACKET: {
            // Array
            type->type = METADATA_ARRAY;

            if (peek_token(parser).type == TK_RBRACKET) {
                // Unknown size array (a pointer)
                tk = next_token(parser);

                type->array_size = 0;

                type->incomplete = 0;
                type->size = PTR_SIZE;
                type->alignment = PTR_SIZE;

                ASTNode* inner = data_type(parser, 1);
                if (inner->type == NODE_ERR) {
                    return inner;
                }
                assert(inner->type == NODE_TYPE);
                type->inner_type = ((TypeNode*)inner)->data_type;
            } else {
                // Regular array
                ASTNode* size_node = expr(parser, 0);

                if (size_node->type == NODE_ERR) {
                    return size_node;
                }

                IntLitNode* size_lit = (IntLitNode*)size_node;
                if (size_node->type != NODE_INTLIT ||
                    (size_lit->data_type != TYPE_I32 &&
                     size_lit->data_type != TYPE_U32)) {
                    return error(parser, size_node->pos,
                                 "size of the array type is not a compile-time "
                                 "constant integer");
                }

                int size = size_lit->val;
                if (size <= 0) {
                    return error(
                        parser, size_lit->pos,
                        "size of the array type is not a positive integer");
                }
                type->array_size = size;

                tk = next_token(parser);
                if (tk.type != TK_RBRACKET) {
                    return error(parser, parser->post_token_pos,
                                 "expected ']'");
                }

                ASTNode* inner = data_type(parser, 0);
                if (inner->type == NODE_ERR) {
                    return inner;
                }
                assert(inner->type == NODE_TYPE);
                type->inner_type = ((TypeNode*)inner)->data_type;

                type->incomplete = 0;
                type->size = type->inner_type->size * size;
                type->alignment = type->inner_type->alignment;
            }
        } break;

        case TK_IDENT: {
            // Custom type
            type->type = METADATA_TYPE;

            Str ident = tk.str;
            SymbolTableEntry* ste =
                symbol_table_find(parser->global_sym, ident, 0);
            if (ste == NULL) {
                return error(parser, parser->post_token_pos,
                             "'%.*s' undeclared", tk.str.len, tk.str.ptr);
            }
            if (ste->type != SYM_TYPE) {
                return error(parser, parser->post_token_pos,
                             "'%.*s' is not a type", tk.str.len, tk.str.ptr);
            }

            TypeSymbolTableEntry* type_ste = (TypeSymbolTableEntry*)ste;
            type->type_ste = type_ste;

            if (!type_ste->incomplete) {
                type->incomplete = 0;
                type->size = type_ste->size;
                type->alignment = type_ste->alignment;
            } else if (allow_incomplete) {
                type->incomplete = 1;
            } else {
                return error(parser, parser->post_token_pos,
                             "incomplete type is not allowed");
            }
        } break;

        case TK_FUNC: {
            // Function pointer
            type->type = METADATA_FUNC;

            type->incomplete = 0;
            type->size = PTR_SIZE;
            type->alignment = PTR_SIZE;

            tk = next_token(parser);
            CallConvType call_type = CALLCONV_CDECL;
            SourcePos callconv_pos = parser->post_token_pos;
            if (tk.type == TK_STR) {
                int found = 0;
                for (unsigned int i = 0;
                     i < sizeof(str_callconv) / sizeof(str_callconv[0]); i++) {
                    if (str_eql(str(str_callconv[i].s), tk.str)) {
                        call_type = str_callconv[i].call_type;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    return error(parser, callconv_pos,
                                 "unknown calling convention");
                }
                tk = next_token(parser);
            }

            if (tk.type != TK_LPAREN) {
                return error(parser, parser->pre_token_pos, "expected '('");
            }

            FuncMetadata func_data;
            func_data.args = NULL;
            func_data.callconv = call_type;
            func_data.has_va_args = 0;

            int has_thisptr = 0;
            int first_arg = 1;

            tk = peek_token(parser);
            if (tk.type == TK_RPAREN) {
                next_token(parser);
            } else {
                while (tk.type != TK_RPAREN) {
                    tk = next_token(parser);
                    if (tk.type == TK_ARGS) {
                        if (call_type != CALLCONV_CDECL) {
                            return error(parser, parser->pre_token_pos,
                                         "vararg is not allowed in this "
                                         "calling convention");
                        }
                        func_data.has_va_args = 1;
                        tk = next_token(parser);
                        if (tk.type != TK_RPAREN) {
                            return error(parser, parser->pre_token_pos,
                                         "expected ')'");
                        }
                        break;
                    }
                    if (tk.type != TK_IDENT) {
                        return error(parser, parser->post_token_pos,
                                     "expected an identifier");
                    }

                    tk = next_token(parser);
                    if (tk.type != TK_COLON) {
                        return error(parser, parser->pre_token_pos,
                                     "expected ':'");
                    }

                    ASTNode* arg_type = data_type(parser, 0);
                    if (arg_type->type == NODE_ERR) {
                        return arg_type;
                    }
                    assert(arg_type->type == NODE_TYPE);

                    if (first_arg) {
                        first_arg = 0;
                        has_thisptr = is_ptr(((TypeNode*)arg_type)->data_type);
                    }

                    ArgList* arg = arena_alloc(parser->arena, sizeof(ArgList));
                    arg->next = func_data.args;
                    arg->type = ((TypeNode*)arg_type)->data_type;
                    func_data.args = arg;

                    tk = next_token(parser);
                    if (tk.type != TK_COMMA && tk.type != TK_RPAREN) {
                        return error(parser, parser->pre_token_pos,
                                     "expected ',' or ')'");
                    }
                }
            }

            if (call_type == CALLCONV_THISCALL && !has_thisptr) {
                return error(parser, callconv_pos, "thiscall requires thisptr");
            }

            tk = peek_token(parser);
            if (tk.type == TK_VOID) {
                next_token(parser);
                func_data.return_type = get_primitive_type(TYPE_VOID);
            } else {
                ASTNode* return_type = data_type(parser, 0);
                if (return_type->type == NODE_ERR) {
                    return return_type;
                }
                assert(return_type->type == NODE_TYPE);
                func_data.return_type = ((TypeNode*)return_type)->data_type;
            }

            type->func_data = func_data;
        } break;

        default:
            return error(parser, parser->post_token_pos, "expected a type");
    }

    type_node->data_type = type;

    return (ASTNode*)type_node;
}

static ASTNode* var_decl(ParserState* parser, int is_extern) {
    Token tk = next_token(parser);
    assert(tk.type == TK_DECL);

    tk = next_token(parser);
    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    SourcePos ident_pos = parser->post_token_pos;
    if (symbol_table_find(parser->sym, ident, 1) != NULL) {
        return error(parser, ident_pos, "redefinition of '%.*s'", tk.str.len,
                     tk.str.ptr);
    }

    tk = next_token(parser);
    if (tk.type != TK_COLON) {
        return error(parser, parser->post_token_pos, "expected ':'");
    }

    ASTNode* var_type = data_type(parser, 0);
    if (var_type->type == NODE_ERR) {
        return var_type;
    }
    assert(var_type->type == NODE_TYPE);

    VarSymbolTableEntry* ste =
        symbol_table_append_var(parser->sym, ident, 0, is_extern,
                                ((TypeNode*)var_type)->data_type, ident_pos);

    tk = peek_token(parser);
    if (tk.type == TK_ASSIGN) {
        if (is_extern) {
            next_token(parser);
            return error(parser, parser->post_token_pos,
                         "initializing extern variable is not allowed");
        }

        VarNode* var = arena_alloc(parser->arena, sizeof(VarNode));
        var->type = NODE_VAR;
        var->pos = parser->post_token_pos;
        var->ste = (SymbolTableEntry*)ste;

        next_token(parser);

        AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
        assign->type = NODE_ASSIGN;
        assign->pos = parser->post_token_pos;
        assign->left = (ASTNode*)var;
        assign->right = expr(parser, 0);
        assign->from_decl = 1;
        if (assign->right->type == NODE_ERR) {
            return assign->right;
        }
        return (ASTNode*)assign;
    } else if (tk.type != TK_SEMICOLON) {
        next_token(parser);
        return error(parser, parser->pre_token_pos,
                     "expected '=' or ';' after declaration");
    }

    return NULL;
}

static ASTNode* def_decl(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_CONST);

    tk = next_token(parser);

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    SourcePos ident_pos = parser->post_token_pos;
    if (symbol_table_find(parser->sym, ident, 1) != NULL) {
        return error(parser, ident_pos, "redefinition of '%.*s'", tk.str.len,
                     tk.str.ptr);
    }

    tk = next_token(parser);
    if (tk.type != TK_ASSIGN) {
        return error(parser, parser->pre_token_pos,
                     "expected '=' after define");
    }

    SourcePos pos = parser->pre_token_pos;

    ASTNode* val_node = expr(parser, 0);
    if (val_node->type == NODE_ERR) {
        return val_node;
    }

    DefSymbolValue def_val = {0};
    if (val_node->type == NODE_INTLIT) {
        IntLitNode* lit = (IntLitNode*)val_node;
        def_val.is_str = 0;
        def_val.val = lit->val;
        def_val.data_type = lit->data_type;
    } else if (val_node->type == NODE_STRLIT) {
        StrLitNode* lit = (StrLitNode*)val_node;
        def_val.is_str = 1;
        def_val.str = lit->val;
    } else {
        return error(parser, pos,
                     "defined element is not a compile-time constant integer "
                     "or string literal");
    }

    symbol_table_append_def(parser->sym, ident, def_val, ident_pos);

    return NULL;
}

static ASTNode* struct_decl(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_STRUCT);

    tk = next_token(parser);

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    SourcePos ident_pos = parser->post_token_pos;
    SymbolTableEntry* ste = symbol_table_find(parser->sym, ident, 1);
    TypeSymbolTableEntry* type_ste;
    if (ste == NULL) {
        type_ste = symbol_table_append_type(parser->sym, ident, ident_pos);
    } else if (ste->type == SYM_TYPE &&
               ((TypeSymbolTableEntry*)ste)->incomplete) {
        type_ste = (TypeSymbolTableEntry*)ste;
    } else {
        return error(parser, ident_pos, "redefinition of '%.*s'", tk.str.len,
                     tk.str.ptr);
    }

    tk = peek_token(parser);
    if (tk.type == TK_SEMICOLON) {
        // Forward declaration
        return NULL;
    }

    if (tk.type != TK_LBRACE) {
        return error(parser, parser->post_token_pos, "expected '{'");
    }

    tk = next_token(parser);

    int alignment = 0;

    SymbolTable* name_space =
        arena_alloc(parser->sym->arena, sizeof(SymbolTable));
    symbol_table_init(name_space, 0, NULL, 0, parser->sym->arena);

    tk = next_token(parser);
    while (tk.type != TK_RBRACE) {
        if (tk.type != TK_IDENT) {
            return error(parser, parser->post_token_pos,
                         "expected an identifier or '}'");
        }

        Str ident = tk.str;
        SourcePos ident_pos = parser->post_token_pos;
        if (symbol_table_find(name_space, ident, 1)) {
            return error(parser, ident_pos, "redefinition of '%.*s'",
                         tk.str.len, tk.str.ptr);
        }

        tk = next_token(parser);
        if (tk.type != TK_COLON) {
            return error(parser, parser->post_token_pos, "expected ':'");
        }

        ASTNode* type_node = data_type(parser, 0);
        if (type_node->type == NODE_ERR) {
            return type_node;
        }
        assert(type_node->type == NODE_TYPE);

        const Type* type = ((TypeNode*)type_node)->data_type;
        symbol_table_append_field(name_space, ident, type, ident_pos);
        if (type->alignment > alignment) {
            alignment = type->alignment;
        }

        tk = next_token(parser);
        if (tk.type == TK_COMMA) {
            tk = next_token(parser);
        } else if (tk.type != TK_RBRACE) {
            return error(parser, parser->post_token_pos, "expected ',' or '}'");
        }
    }

    int struct_size = *name_space->stack_size;
    if (struct_size == 0) {
        struct_size = 1;
        alignment = 1;
    }

    type_ste->size = struct_size;
    type_ste->name_space = name_space;
    type_ste->alignment = alignment;
    type_ste->incomplete = 0;

    return NULL;
}

static ASTNode* enum_decl(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_ENUM);

    tk = next_token(parser);
    if (tk.type != TK_LBRACE) {
        return error(parser, parser->post_token_pos, "expected '{'");
    }

    int enum_val = 0;

    tk = next_token(parser);
    while (tk.type != TK_RBRACE) {
        if (tk.type != TK_IDENT) {
            return error(parser, parser->post_token_pos,
                         "expected an identifier");
        }

        Str ident = tk.str;
        SourcePos ident_pos = parser->post_token_pos;
        if (symbol_table_find(parser->sym, ident, 1) != NULL) {
            return error(parser, ident_pos, "redefinition of identifier '%.*s'",
                         tk.str.len, tk.str.ptr);
        }

        Token peek = peek_token(parser);
        if (peek.type == TK_ASSIGN) {
            next_token(parser);
            SourcePos pos = parser->pre_token_pos;
            ASTNode* lit_node = expr(parser, 1);
            if (lit_node->type == NODE_ERR) {
                return lit_node;
            }

            IntLitNode* lit = (IntLitNode*)lit_node;
            if (lit->type != NODE_INTLIT ||
                (lit->data_type != TYPE_I32 && lit->data_type != TYPE_U32)) {
                return error(parser, pos,
                             "expected a compile-time constant integer");
            }

            enum_val = lit->val;
        }

        DefSymbolValue def_val = {
            .is_str = 0,
            .val = enum_val,
            .data_type = get_intlit_type(enum_val),
        };
        symbol_table_append_def(parser->sym, ident, def_val, ident_pos);
        enum_val++;

        tk = next_token(parser);
        if (tk.type == TK_COMMA) {
            tk = next_token(parser);
            if (tk.type == TK_RBRACE) {
                break;
            }
        } else if (tk.type != TK_RBRACE) {
            return error(parser, parser->post_token_pos,
                         "expected ',' or '}' after identifier");
        }
    }

    return NULL;
}

static ASTNode* func_decl(ParserState* parser, int is_extern) {
    Token tk = next_token(parser);
    assert(tk.type == TK_FUNC);

    tk = next_token(parser);
    CallConvType call_type = CALLCONV_CDECL;
    SourcePos callconv_pos = parser->post_token_pos;
    if (tk.type == TK_STR) {
        int found = 0;
        for (unsigned int i = 0;
             i < sizeof(str_callconv) / sizeof(str_callconv[0]); i++) {
            if (str_eql(str(str_callconv[i].s), tk.str)) {
                call_type = str_callconv[i].call_type;
                found = 1;
                break;
            }
        }
        if (!found) {
            return error(parser, callconv_pos, "unknown calling convention");
        }
        tk = next_token(parser);
    }

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    SourcePos ident_pos = parser->post_token_pos;
    SymbolTableEntry* ste = symbol_table_find(parser->global_sym, ident, 1);

    FuncSymbolTableEntry* func;
    if (ste == NULL) {
        func =
            symbol_table_append_func(parser->sym, ident, is_extern, ident_pos);
    } else if (ste->type == SYM_FUNC &&
               ((FuncSymbolTableEntry*)ste)->node == NULL) {
        // forward declaration
        func = (FuncSymbolTableEntry*)ste;
    } else {
        return error(parser, ident_pos, "redefinition of '%.*s'", tk.str.len,
                     tk.str.ptr);
    }

    tk = next_token(parser);
    if (tk.type != TK_LPAREN) {
        return error(parser, parser->pre_token_pos, "expected '('");
    }

    SymbolTable* sym =
        arena_alloc(parser->global_sym->arena, sizeof(SymbolTable));
    symbol_table_init(sym, 0, NULL, 0, parser->global_sym->arena);

    sym->parent = parser->global_sym;
    parser->sym = sym;

    FuncMetadata func_data;
    func_data.args = NULL;
    func_data.callconv = call_type;
    func_data.has_va_args = 0;

    int has_thisptr = 0;
    int first_arg = 1;

    tk = peek_token(parser);
    if (tk.type == TK_RPAREN) {
        next_token(parser);
    } else {
        while (tk.type != TK_RPAREN) {
            tk = next_token(parser);
            if (tk.type == TK_ARGS) {
                if (call_type != CALLCONV_CDECL) {
                    return error(
                        parser, parser->pre_token_pos,
                        "vararg is not allowed in this calling convention");
                }
                func_data.has_va_args = 1;
                tk = next_token(parser);
                if (tk.type != TK_RPAREN) {
                    return error(parser, parser->pre_token_pos, "expected ')'");
                }
                break;
            }
            if (tk.type != TK_IDENT) {
                return error(parser, parser->post_token_pos,
                             "expected an identifier");
            }

            ident = tk.str;
            ident_pos = parser->post_token_pos;
            if (symbol_table_find(parser->sym, ident, 1) != NULL) {
                return error(parser, ident_pos, "redefinition of '%.*s'",
                             tk.str.len, tk.str.ptr);
            }

            tk = next_token(parser);
            if (tk.type != TK_COLON) {
                return error(parser, parser->pre_token_pos, "expected ':'");
            }

            ASTNode* arg_type = data_type(parser, 0);
            if (arg_type->type == NODE_ERR) {
                return arg_type;
            }
            assert(arg_type->type == NODE_TYPE);

            if (first_arg) {
                first_arg = 0;
                has_thisptr = is_ptr(((TypeNode*)arg_type)->data_type);
            }

            symbol_table_append_var(parser->sym, ident, 1, 0,
                                    ((TypeNode*)arg_type)->data_type,
                                    ident_pos);

            ArgList* arg = arena_alloc(parser->arena, sizeof(ArgList));
            arg->next = func_data.args;
            arg->type = ((TypeNode*)arg_type)->data_type;
            func_data.args = arg;

            tk = next_token(parser);
            if (tk.type != TK_COMMA && tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos,
                             "expected ',' or ')'");
            }
        }
    }

    if (call_type == CALLCONV_THISCALL && !has_thisptr) {
        return error(parser, callconv_pos, "thiscall requires thisptr");
    }

    tk = peek_token(parser);
    if (tk.type == TK_VOID) {
        next_token(parser);
        func_data.return_type = get_primitive_type(TYPE_VOID);
    } else {
        ASTNode* return_type = data_type(parser, 0);
        if (return_type->type == NODE_ERR) {
            return return_type;
        }
        assert(return_type->type == NODE_TYPE);
        func_data.return_type = ((TypeNode*)return_type)->data_type;
    }

    func->func_data = func_data;

    tk = peek_token(parser);
    if (tk.type == TK_LBRACE) {
        if (func->is_extern) {
            next_token(parser);
            return error(parser, parser->post_token_pos,
                         "implementing extern function is not allowed");
        }

        ASTNode* node = scope(parser);
        if (node->type == NODE_ERR) {
            return node;
        }
        func->node = node;
    } else if (tk.type != TK_SEMICOLON) {
        next_token(parser);
        return error(parser, parser->pre_token_pos, "expected '{' or ';'");
    }

    parser->sym = parser->global_sym;

    func->sym = sym;

    return NULL;
}

static ASTNode* return_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_RET);

    tk = peek_token(parser);
    ReturnNode* ret_node = arena_alloc(parser->arena, sizeof(ReturnNode));
    ret_node->type = NODE_RET;
    ret_node->pos = parser->post_token_pos;
    if (tk.type == TK_SEMICOLON) {
        ret_node->expr = NULL;
    } else {
        ret_node->expr = expr(parser, 0);
        if (ret_node->expr->type == NODE_ERR) {
            return ret_node->expr;
        }
    }

    return (ASTNode*)ret_node;
}

static ASTNode* if_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_IF);

    IfStatementNode* if_node =
        arena_alloc(parser->arena, sizeof(IfStatementNode));
    if_node->type = NODE_IF;

    tk = next_token(parser);
    if (tk.type != TK_LPAREN) {
        return error(parser, parser->pre_token_pos, "expected '('");
    }

    if_node->expr = expr(parser, 0);
    if (if_node->expr->type == NODE_ERR) {
        return if_node->expr;
    }

    tk = next_token(parser);
    if (tk.type != TK_RPAREN) {
        return error(parser, parser->pre_token_pos, "expected ')'");
    }

    if_node->then_block = stmt(parser);
    if (if_node->then_block && if_node->then_block->type == NODE_ERR) {
        return if_node->then_block;
    }

    tk = peek_token(parser);
    if (tk.type == TK_ELSE) {
        next_token(parser);

        if_node->else_block = stmt(parser);
        if (if_node->else_block && if_node->else_block->type == NODE_ERR) {
            return if_node->else_block;
        }
    } else {
        if_node->else_block = NULL;
    }

    return (ASTNode*)if_node;
}

static ASTNode* while_stmt(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_WHILE);

    WhileNode* while_node = arena_alloc(parser->arena, sizeof(WhileNode));
    while_node->type = NODE_WHILE;
    while_node->pos = parser->post_token_pos;

    tk = next_token(parser);
    if (tk.type != TK_LPAREN) {
        return error(parser, parser->pre_token_pos, "expected '('");
    }

    while_node->expr = expr(parser, 0);
    if (while_node->expr->type == NODE_ERR) {
        return while_node->expr;
    }

    tk = next_token(parser);
    if (tk.type != TK_RPAREN) {
        return error(parser, parser->pre_token_pos, "expected ')'");
    }

    tk = peek_token(parser);
    if (tk.type == TK_COLON) {
        next_token(parser);
        while_node->inc = expr(parser, 0);
        if (while_node->inc->type == NODE_ERR) {
            return while_node->inc;
        }
    } else {
        while_node->inc = NULL;
    }

    while_node->block = stmt(parser);
    if (while_node->block && while_node->block->type == NODE_ERR) {
        return while_node->block;
    }

    return (ASTNode*)while_node;
}

static ASTNode* scope(ParserState* parser) {
    Token tk = next_token(parser);
    assert(tk.type == TK_LBRACE);

    SymbolTable* sym = arena_alloc(parser->sym->arena, sizeof(SymbolTable));
    symbol_table_init(sym, parser->sym->offset, parser->sym->stack_size, 0,
                      parser->sym->arena);
    sym->parent = parser->sym;
    parser->sym = sym;

    ASTNode* node = stmt_list(parser, 1);
    if (node->type == NODE_ERR)
        return node;

    tk = next_token(parser);
    if (tk.type != TK_RBRACE) {
        return error(parser, parser->pre_token_pos, "expected '}'");
    }

    parser->sym = sym->parent;

    return node;
}

static ASTNode* stmt(ParserState* parser) {
    ASTNode* node;
    Token tk = peek_token(parser);
    switch (tk.type) {
        case TK_SEMICOLON:
            tk = next_token(parser);
            return NULL;

        case TK_RET:
            node = return_stmt(parser);
            if (node->type == NODE_ERR)
                return node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos,
                             "expected ';' after return statement");
            }
            break;

        case TK_IF:
            node = if_stmt(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_WHILE:
            node = while_stmt(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_LBRACE:
            node = scope(parser);
            if (node->type == NODE_ERR)
                return node;
            break;

        case TK_STR: {
            next_token(parser);
            PrintNode* print_node =
                arena_alloc(parser->arena, sizeof(PrintNode));
            node = (ASTNode*)print_node;

            print_node->type = NODE_PRINT;
            print_node->pos = parser->post_token_pos;
            print_node->fmt = tk.str;
            print_node->args = NULL;

            tk = next_token(parser);
            while (tk.type == TK_COMMA) {
                ASTNode* arg_node = expr(parser, 1);
                if (arg_node->type == NODE_ERR)
                    return arg_node;

                ASTNodeList* arg =
                    arena_alloc(parser->arena, sizeof(ASTNodeList));
                arg->node = arg_node;
                arg->next = print_node->args;
                print_node->args = arg;

                tk = next_token(parser);
            }

            if (tk.type != TK_COMMA && tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos,
                             "expected ',' or ';' after string literal");
            }
        } break;

        case TK_BREAK:
        case TK_CONTINUE: {
            next_token(parser);

            GotoNode* goto_node = arena_alloc(parser->arena, sizeof(GotoNode));
            node = (ASTNode*)goto_node;
            goto_node->type = NODE_GOTO;
            goto_node->pos = parser->post_token_pos;
            goto_node->op = tk.type;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                if (tk.type == TK_BREAK) {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' after break statement");
                } else {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' after continue statement");
                }
            }
        } break;

        default:
            node = expr(parser, 0);
            if (node->type == NODE_ERR)
                return node;

            tk = next_token(parser);
            if (tk.type != TK_SEMICOLON) {
                return error(parser, parser->pre_token_pos,
                             "expected ';' after expression");
            }
    }
    return node;
}

static ASTNode* stmt_list(ParserState* parser, int in_scope) {
    StatementListNode* stmts =
        arena_alloc(parser->arena, sizeof(StatementListNode));
    stmts->type = NODE_STMTS;
    stmts->pos = parser->post_token_pos;
    stmts->stmts = NULL;

    for (Token tk = peek_token(parser);
         tk.type != TK_NUL && (!in_scope || tk.type != TK_RBRACE);
         tk = peek_token(parser)) {
        ASTNode* node = NULL;

        int is_extern = 0;

        if (tk.type == TK_EXTERN) {
            next_token(parser);
            tk = peek_token(parser);
            if (tk.type != TK_FUNC && tk.type != TK_DECL) {
                next_token(parser);
                return error(parser, parser->post_token_pos,
                             "expected function or variable declaration");
            }
            is_extern = 1;
        }

        switch (tk.type) {
            case TK_FUNC:
                if (in_scope) {
                    next_token(parser);
                    return error(parser, parser->post_token_pos,
                                 "function definition is not allowed here");
                }
                node = func_decl(parser, is_extern);
                if (node && node->type == NODE_ERR) {
                    return node;
                }
                break;

            case TK_STRUCT:
                if (in_scope) {
                    next_token(parser);
                    return error(parser, parser->post_token_pos,
                                 "struct definition is not allowed here");
                }
                node = struct_decl(parser);
                if (node && node->type == NODE_ERR) {
                    return node;
                }

                tk = next_token(parser);
                if (tk.type != TK_SEMICOLON) {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' at end of declaration");
                }
                break;

            case TK_DECL:
                node = var_decl(parser, is_extern);
                if (node && node->type == NODE_ERR) {
                    return node;
                }

                tk = next_token(parser);
                if (tk.type != TK_SEMICOLON) {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' at end of declaration");
                }
                break;

            case TK_CONST:
                node = def_decl(parser);
                if (node && node->type == NODE_ERR) {
                    return node;
                }

                tk = next_token(parser);
                if (tk.type != TK_SEMICOLON) {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' at end of declaration");
                }
                break;

            case TK_ENUM:
                node = enum_decl(parser);
                if (node && node->type == NODE_ERR) {
                    return node;
                }

                tk = next_token(parser);
                if (tk.type != TK_SEMICOLON) {
                    return error(parser, parser->pre_token_pos,
                                 "expected ';' at end of declaration");
                }
                break;

            default:
                node = stmt(parser);
        }

        if (node == NULL) {
            continue;
        }

        if (node->type == NODE_ERR) {
            return node;
        }

        ASTNodeList* list = arena_alloc(parser->arena, sizeof(ASTNodeList));
        list->node = node;
        list->next = NULL;
        if (!stmts->stmts) {
            stmts->stmts = list;
        } else {
            stmts->_tail->next = list;
        }
        stmts->_tail = list;
    }
    return (ASTNode*)stmts;
}

void parser_init(ParserState* parser, SymbolTable* sym, Arena* arena) {
    parser->arena = arena;
    parser->sym = parser->global_sym = sym;
}

ASTNode* parser_parse(ParserState* parser, SourceState* src) {
    parser->src = src;
    parser->line = 0;
    parser->pos = 0;

    return stmt_list(parser, 0);
}
