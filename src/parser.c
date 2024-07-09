#include "parser.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static ASTNode* primary(ParserState* parser);
static ASTNode* expr(ParserState* parser, int min_precedence);
static ASTNode* var_decl(ParserState* parser, int is_extern);
static ASTNode* def_decl(ParserState* parser);
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

    err->val = malloc(sizeof(Error));
    err->val->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->val->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return (ASTNode*)err;
}

static ASTNode* primary(ParserState* parser) {
    Token tk = next_token(parser);

    ASTNode* node;

    switch (tk.type) {
        case TK_INT: {
            IntLitNode* intlit = arena_alloc(parser->arena, sizeof(IntLitNode));
            intlit->type = NODE_INTLIT;
            intlit->val = tk.val;
            node = (ASTNode*)intlit;
        } break;

        case TK_STR: {
            StrLitNode* strlit = arena_alloc(parser->arena, sizeof(StrLitNode));
            strlit->type = NODE_STRLIT;
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
                    var_node->ste = ste;
                    node = (ASTNode*)var_node;
                } break;

                case SYM_DEF: {
                    IntLitNode* intlit =
                        arena_alloc(parser->arena, sizeof(intlit));
                    intlit->type = NODE_INTLIT;
                    intlit->val = ((DefSymbolTableEntry*)ste)->val;
                    node = (ASTNode*)intlit;
                } break;

                default:
                    assert(0);
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
        case TK_AND:
        case TK_DOLLAR: {
            ASTNode* right = primary(parser);
            if (right->type == NODE_ERR) {
                return right;
            }

            if (right->type == NODE_INTLIT &&
                (tk.type != TK_MUL && tk.type != TK_AND &&
                 tk.type != TK_DOLLAR)) {
                IntLitNode* intlit = (IntLitNode*)right;
                int val = intlit->val;
                switch (tk.type) {
                    case TK_ADD:
                        break;
                    case TK_SUB:
                        intlit->val = (-val);
                        break;
                    case TK_NOT:
                        intlit->val = (~val);
                        break;
                    case TK_LNOT:
                        intlit->val = (!val);
                        break;
                    default:
                        assert(0);
                }
                node = (ASTNode*)intlit;
            } else {
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

    tk = peek_token(parser);
    switch (tk.type) {
        case TK_DOT: {
            next_token(parser);
            BinaryOpNode* binop =
                arena_alloc(parser->arena, sizeof(BinaryOpNode));
            binop->type = NODE_BINARYOP;
            binop->pos = parser->post_token_pos;
            binop->op = tk.type;
            binop->left = node;
            binop->right = primary(parser);
            if (binop->right->type == NODE_ERR) {
                return binop->right;
            }
            node = (ASTNode*)binop;
        } break;

        case TK_LPAREN: {
            next_token(parser);
            CallNode* call_node = arena_alloc(parser->arena, sizeof(CallNode));
            call_node->type = NODE_CALL;
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
            break;
    }

    return node;
}

static inline int get_precedence(TokenType type) {
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

static inline int is_left_associative(TokenType type) {
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
            assert(0);
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
            case TK_AOR: {
                AssignNode* assign =
                    arena_alloc(parser->arena, sizeof(AssignNode));
                assign->type = NODE_ASSIGN;
                assign->pos = parser->post_token_pos;
                assign->op = tk.type;
                assign->left = node;
                assign->right = expr(parser, next_precedence);
                if (assign->right->type == NODE_ERR) {
                    return assign->right;
                }
                node = (ASTNode*)assign;
            } break;

            default: {
                SourcePos pos = parser->post_token_pos;
                ASTNode* left = node;
                ASTNode* right = expr(parser, next_precedence);
                if (right->type == NODE_ERR) {
                    return right;
                }

                if (tk.type != TK_COMMA && left->type == NODE_INTLIT &&
                    right->type == NODE_INTLIT) {
                    int a = ((IntLitNode*)left)->val;
                    int b = ((IntLitNode*)right)->val;

                    IntLitNode* intlit = (IntLitNode*)left;

                    switch (tk.type) {
                        case TK_LOR:
                            intlit->val = (a || b);
                            break;

                        case TK_LAND:
                            intlit->val = (a && b);
                            break;

                        case TK_ADD:
                            intlit->val = (a + b);
                            break;

                        case TK_SUB:
                            intlit->val = (a - b);
                            break;

                        case TK_MUL:
                            intlit->val = (a * b);
                            break;

                        case TK_DIV:
                            if (b == 0) {
                                return error(parser, pos, "division by zero");
                            }
                            intlit->val = (a / b);
                            break;

                        case TK_MOD:
                            if (b == 0) {
                                return error(parser, pos, "modulo by zero");
                            }
                            intlit->val = (a % b);
                            break;

                        case TK_SHL:
                            intlit->val = (a << b);
                            break;

                        case TK_SHR:
                            intlit->val = (a >> b);
                            break;

                        case TK_AND:
                            intlit->val = (a & b);
                            break;

                        case TK_XOR:
                            intlit->val = (a ^ b);
                            break;

                        case TK_OR:
                            intlit->val = (a | b);
                            break;

                        case TK_EQ:
                            intlit->val = (a == b);
                            break;

                        case TK_NE:
                            intlit->val = (a != b);
                            break;

                        case TK_LT:
                            intlit->val = (a < b);
                            break;

                        case TK_LE:
                            intlit->val = (a <= b);
                            break;

                        case TK_GT:
                            intlit->val = (a > b);
                            break;

                        case TK_GE:
                            intlit->val = (a >= b);
                            break;

                        default:
                            assert(0);
                    }
                    node = (ASTNode*)intlit;
                } else {
                    BinaryOpNode* binop =
                        arena_alloc(parser->arena, sizeof(BinaryOpNode));
                    binop->type = NODE_BINARYOP;
                    binop->pos = parser->post_token_pos;
                    binop->op = tk.type;
                    binop->left = left;
                    binop->right = right;
                    node = (ASTNode*)binop;
                }
            }
        }

        precedence = get_precedence(peek_token(parser).type);
    }

    return node;
}

static ASTNode* var_decl(ParserState* parser, int is_extern) {
    Token tk = next_token(parser);
    assert(tk.type == TK_DECL);

    tk = next_token(parser);

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    if (symbol_table_find(parser->sym, ident, 1) != NULL) {
        return error(parser, parser->post_token_pos, "redefinition of '%.*s'",
                     tk.str.len, tk.str.ptr);
    }

    VarSymbolTableEntry* ste =
        symbol_table_append_var(parser->sym, ident, 0, is_extern);

    tk = peek_token(parser);
    if (tk.type == TK_ASSIGN) {
        if (is_extern) {
            next_token(parser);
            return error(parser, parser->post_token_pos,
                         "initializing extern variable is not allowed");
        }

        VarNode* var = arena_alloc(parser->arena, sizeof(VarNode));
        var->type = NODE_VAR;
        var->ste = (SymbolTableEntry*)ste;

        next_token(parser);

        AssignNode* assign = arena_alloc(parser->arena, sizeof(AssignNode));
        assign->op = TK_ASSIGN;
        assign->type = NODE_ASSIGN;
        assign->left = (ASTNode*)var;
        assign->right = expr(parser, 0);
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
    assert(tk.type == TK_DEF);

    tk = next_token(parser);

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    if (symbol_table_find(parser->sym, ident, 1) != NULL) {
        return error(parser, parser->post_token_pos, "redefinition of '%.*s'",
                     tk.str.len, tk.str.ptr);
    }

    tk = next_token(parser);
    if (tk.type != TK_ASSIGN) {
        return error(parser, parser->pre_token_pos,
                     "expected '=' after define");
    }

    SourcePos pos = parser->pre_token_pos;

    ASTNode* val = expr(parser, 0);
    if (val->type == NODE_ERR) {
        return val;
    }

    if (val->type != NODE_INTLIT) {
        return error(parser, pos,
                     "defined element is not a compile-time constant integer");
    }

    symbol_table_append_def(parser->sym, ident, ((IntLitNode*)val)->val);

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
    int increment = 1;

    tk = next_token(parser);
    while (tk.type != TK_RBRACE) {
        if (tk.type != TK_IDENT) {
            return error(parser, parser->post_token_pos,
                         "expected an identifier");
        }

        Str ident = tk.str;
        if (symbol_table_find(parser->sym, ident, 1) != NULL) {
            return error(parser, parser->post_token_pos,
                         "redefinition of identifier '%.*s'", tk.str.len,
                         tk.str.ptr);
        }

        Token peek = peek_token(parser);
        if (peek.type == TK_ASSIGN) {
            next_token(parser);
            SourcePos pos = parser->pre_token_pos;
            ASTNode* val = expr(parser, 0);
            if (val->type == NODE_ERR) {
                return val;
            }

            if (val->type != NODE_INTLIT) {
                return error(parser, pos,
                             "expected a compile-time constant integer");
            }

            enum_val = ((IntLitNode*)val)->val;
        }

        peek = peek_token(parser);
        if (peek.type == TK_COLON) {
            next_token(parser);
            SourcePos pos = parser->pre_token_pos;
            ASTNode* val = primary(parser);
            if (val->type == NODE_ERR) {
                return val;
            }

            if (val->type != NODE_INTLIT) {
                return error(parser, pos,
                             "expected a compile-time constant integer");
            }

            increment = ((IntLitNode*)val)->val;
        } else {
            increment = 1;
        }

        symbol_table_append_def(parser->sym, ident, enum_val);
        enum_val += increment;

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

    if (tk.type != TK_IDENT) {
        return error(parser, parser->post_token_pos, "expected an identifier");
    }

    Str ident = tk.str;
    SymbolTableEntry* ste = symbol_table_find(parser->global_sym, ident, 1);

    FuncSymbolTableEntry* func;
    if (ste == NULL) {
        func = symbol_table_append_func(parser->sym, ident, is_extern);
    } else if (ste->type == SYM_FUNC &&
               ((FuncSymbolTableEntry*)ste)->node == NULL) {
        // forward declaration
        func = (FuncSymbolTableEntry*)ste;
    } else {
        return error(parser, parser->post_token_pos, "redefinition of '%.*s'",
                     tk.str.len, tk.str.ptr);
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

    tk = peek_token(parser);
    if (tk.type == TK_RPAREN) {
        next_token(parser);
    } else {
        while (tk.type != TK_RPAREN) {
            tk = next_token(parser);
            if (tk.type != TK_IDENT) {
                return error(parser, parser->post_token_pos,
                             "expected an identifier");
            }

            ident = tk.str;
            if (symbol_table_find(parser->sym, ident, 1) != NULL) {
                return error(parser, parser->post_token_pos,
                             "redefinition of '%.*s'", tk.str.len, tk.str.ptr);
            }
            symbol_table_append_var(parser->sym, ident, 1, 0);

            tk = next_token(parser);
            if (tk.type != TK_COMMA && tk.type != TK_RPAREN) {
                return error(parser, parser->pre_token_pos,
                             "expected ',' or ')'");
            }
        }
    }

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

            case TK_DEF:
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
    arena_reset(parser->arena);
    parser->src = src;
    parser->line = 0;
    parser->pos = 0;

    symbol_table_append_var(parser->sym, str("argc"), 0, 0);
    symbol_table_append_var(parser->sym, str("argv"), 0, 0);

    return stmt_list(parser, 0);
}
