#include "cfg.h"
#include "symtab.h"
#include "type.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* This is the global handle for the current function (or declaration list)
 * being translated. It is cleared every time cfg_create is called.
 */
struct cfg current_cfg = {0};

static void cleanup(void)
{
    int i;
    struct block *block;

    if (current_cfg.params.capacity)
        free(current_cfg.params.symbol);

    if (current_cfg.locals.capacity)
        free(current_cfg.locals.symbol);

    if (current_cfg.capacity) {
        for (i = 0; i < current_cfg.size; ++i) {
            block = current_cfg.nodes[i];
            if (block->n) free(block->code);
            free(block);
        }
        free(current_cfg.nodes);
    }

    memset(&current_cfg, 0, sizeof(current_cfg));
}

void cfg_init_current(void)
{
    static int done;

    if (!done) {
        done = 1;
        atexit(cleanup);
    } else
        cleanup();

    current_cfg.head = cfg_block_init();
    current_cfg.body = cfg_block_init();
}

struct block *cfg_block_init(void)
{
    struct block *block;

    block = calloc(1, sizeof(*block));
    block->label = sym_create_label();
    if (current_cfg.size == current_cfg.capacity) {
        current_cfg.capacity += 16;
        current_cfg.nodes =
            realloc(current_cfg.nodes,
                current_cfg.capacity * sizeof(*current_cfg.nodes));
    }

    current_cfg.nodes[current_cfg.size++] = block;
    return block;
}

void cfg_ir_append(struct block *block, struct op op)
{
    /* Current block can be NULL when parsing an expression that should not be
     * evaluated, for example argument to sizeof. */
    if (block) {
        block->n += 1;
        block->code = realloc(block->code, sizeof(*block->code) * block->n);
        block->code[block->n - 1] = op;
    }
}

void cfg_register_local(struct symbol *symbol)
{
    if (current_cfg.locals.capacity <= current_cfg.locals.length) {
        current_cfg.locals.capacity += 64;
        current_cfg.locals.symbol =
            realloc(current_cfg.locals.symbol,
                current_cfg.locals.capacity * sizeof(struct symbol *));
    }

    current_cfg.locals.symbol[current_cfg.locals.length++] = symbol;
}

void cfg_register_param(struct symbol *symbol)
{
    if (current_cfg.params.capacity <= current_cfg.params.length) {
        current_cfg.params.capacity += 8;
        current_cfg.params.symbol =
            realloc(current_cfg.params.symbol,
                current_cfg.params.capacity * sizeof(struct symbol *));
    }

    current_cfg.params.symbol[current_cfg.params.length++] = symbol;
}

struct var var_direct(const struct symbol *sym)
{
    struct var var = {0};

    var.type = &sym->type;
    var.symbol = sym;

    switch (sym->symtype) {
    case SYM_ENUM_VALUE:
        var.kind = IMMEDIATE;
        var.imm.i = sym->enum_value;
        break;
    case SYM_STRING_VALUE:
        var.kind = IMMEDIATE;
        break;
    default:
        assert(sym->symtype != SYM_LABEL);
        var.kind = DIRECT;
        var.lvalue = sym->name[0] != '.';
        break;
    }

    return var;
}

struct var var_int(int value)
{
    struct var var = {0};

    var.kind = IMMEDIATE;
    var.type = &basic_type__int;
    var.imm.i = value;
    return var;
}

struct var var_zero(int size)
{
    struct var var = {0};

    var.kind = IMMEDIATE;
    var.type = BASIC_TYPE_SIGNED(size);
    var.imm.i = 0;
    return var;
}

struct var var_void(void)
{
    struct var var = {0};

    var.kind = IMMEDIATE;
    var.type = &basic_type__void;
    return var;
}

struct var create_var(const struct typetree *type)
{
    struct symbol *temp = sym_create_tmp(type);
    struct var res = var_direct(temp);

    cfg_register_local(temp);
    res.lvalue = 1;
    return res;
}