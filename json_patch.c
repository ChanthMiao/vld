/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2020 Chanth Miao                                       |
   +----------------------------------------------------------------------+
   | This source file is subject to the 2-Clause BSD license which is     |
   | available through the LICENSE file, or online at                     |
   | http://opensource.org/licenses/bsd-license.php                       |
   +----------------------------------------------------------------------+
   | Authors:  Chanth Miao <chanthmiao@foxmail.com>                       |
   +----------------------------------------------------------------------+
*/

/**
 * The major of json patch is implemented in this file.
*/

#include "php.h"
#include "zend_alloc.h"
#include "branchinfo.h"
#include "srm_oparray.h"
#include "ext/standard/url.h"
#include "set.h"
#include "php_vld.h"

ZEND_EXTERN_MODULE_GLOBALS(vld)

/* extern data declaration, referring to "srm_oparray.c". */
extern op_usage opcodes[199];
extern int vld_find_jumps(zend_op_array *opa, unsigned int position, size_t *jump_count, int *jumps);
extern unsigned int vld_get_special_flags(const zend_op *op, unsigned int base_address);
#if PHP_VERSION_ID >= 70400
extern const char *get_assign_operation(uint32_t extended_value);
#endif

#define STR_ARRAY_LEN(arr) (sizeof(arr) / sizeof(char *))
#define NUM_KNOWN_OPCODES (sizeof(opcodes) / sizeof(opcodes[0]))

#ifdef VLD_DEBUG
#define cJSON_Delet_Wrap(O) \
    cJSON_Delete(O);        \
    O = NULL;               \
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
#else
#define cJSON_Delet_Wrap(O) \
    cJSON_Delete(O);        \
    O = NULL;
#endif

const char *op_cols[] = {"line", "#", "*", "E", "I", "O", "op_code", "op", "fetch", "ext", "return_type", "return", "op1_type", "op1", "op2_type", "op2", "ext_op_type", "ext_op"};
const int verbosity_flags[] = {1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 3, 1, 3, 1, 3, 1, 3, 1};

/* cJSON helper functions. */
cJSON *cJSON_AddArrayToObjectCS(cJSON *obj, const char *key)
{
    cJSON *array = cJSON_CreateArray();
    if (cJSON_AddItemToObjectCS(obj, key, array))
    {
        return array;
    }
    cJSON_Delete(array);
    return NULL;
}

cJSON *cJSON_AddObjectToObjectCS(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_CreateObject();
    if (cJSON_AddItemToObjectCS(obj, key, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_AddNumberToObjectCS(cJSON *obj, const char *key, const double number)
{
    cJSON *item = cJSON_CreateNumber(number);
    if (cJSON_AddItemToObjectCS(obj, key, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_AddNumberToArray(cJSON *array, const double number)
{
    cJSON *item = cJSON_CreateNumber(number);
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_AddStringToObjectCS(cJSON *obj, const char *key, const char *string)
{
    cJSON *item = cJSON_CreateString(string);
    if (cJSON_AddItemToObjectCS(obj, key, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_AddStringRefToObjectCS(cJSON *obj, const char *key, const char *string)
{
    cJSON *item = cJSON_CreateStringReference(string);
    if (cJSON_AddItemToObjectCS(obj, key, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

cJSON *cJSON_AddNullObjectCS(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_CreateNull();
    if (cJSON_AddItemToObjectCS(obj, key, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

cJSON *cJSON_AddStringToArray(cJSON *array, const char *string)
{
    cJSON *item = cJSON_CreateString(string);
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

cJSON *cJSON_AddStringRefToArray(cJSON *array, const char *string)
{
    cJSON *item = cJSON_CreateStringReference(string);
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

cJSON *cJSON_AddNullToArray(cJSON *array)
{
    cJSON *item = cJSON_CreateNull();
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

/* json patch functions. */

static inline cJSON *cJSON_vld_dump_zval_null(ZVAL_VALUE_TYPE value, cJSON *array)
{
    cJSON *item = cJSON_CreateNull();
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

static inline cJSON *cJSON_vld_dump_zval_long(ZVAL_VALUE_TYPE value, cJSON *array)
{
    cJSON *item = cJSON_CreateNumber((double)value.lval);
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

static inline cJSON *cJSON_vld_dump_zval_double(ZVAL_VALUE_TYPE value, cJSON *array)
{
    cJSON *item = cJSON_CreateNumber(value.dval);
    if (cJSON_AddItemToArray(array, item))
    {
        return item;
    }
    cJSON_Delet_Wrap(item);
    return NULL;
}

static inline cJSON *cJSON_vld_dump_zval_string(ZVAL_VALUE_TYPE value, cJSON *array)
{
    ZVAL_VALUE_STRING_TYPE *new_str;
    cJSON *item;
    new_str = php_url_encode(ZVAL_STRING_VALUE(value), ZVAL_STRING_LEN(value) PHP_URLENCODE_NEW_LEN(new_len));
    item = cJSON_AddStringToArray(array, ZSTRING_VALUE(new_str));
    efree(new_str);
    return item;
}

static inline cJSON *cJSON_vld_dump_zval_array(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<array>");
}

static inline cJSON *cJSON_vld_dump_zval_object(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<object>");
}

static inline cJSON *cJSON_vld_dump_zval_bool(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<bool>");
}

static inline cJSON *cJSON_vld_dump_zval_true(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<true>");
}

static inline cJSON *cJSON_vld_dump_zval_false(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<false>");
}

static inline cJSON *cJSON_vld_dump_zval_resource(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<resource>");
}

static inline cJSON *cJSON_vld_dump_zval_constant(ZVAL_VALUE_TYPE value, cJSON *array)
{
    cJSON *item;
    char *buf;
    buf = (char *)malloc(ZVAL_STRING_LEN(value) + 11);
    sprintf(buf, "<const:'%s'>", ZVAL_STRING_VALUE(value));
    item = cJSON_AddStringToArray(array, buf);
    free(buf);
    return item;
}

static inline cJSON *cJSON_vld_dump_zval_constant_ast(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<const ast>");
}

static inline cJSON *cJSON_vld_dump_zval_undef(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<undef>");
}

static inline cJSON *cJSON_vld_dump_zval_reference(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<reference>");
}

static inline cJSON *cJSON_vld_dump_zval_indirect(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<indirect>");
}

static inline cJSON *cJSON_vld_dump_zval_ptr(ZVAL_VALUE_TYPE value, cJSON *array)
{
    return cJSON_AddStringRefToArray(array, "<ptr>");
}

cJSON *cJSON_vld_dump_zval(zval val, cJSON *array)
{
    switch (val.u1.v.type)
    {
    case IS_NULL:
        return cJSON_vld_dump_zval_null(val.value, array);
    case IS_LONG:
        return cJSON_vld_dump_zval_long(val.value, array);
    case IS_DOUBLE:
        return cJSON_vld_dump_zval_double(val.value, array);
    case IS_STRING:
        return cJSON_vld_dump_zval_string(val.value, array);
    case IS_ARRAY:
        return cJSON_vld_dump_zval_array(val.value, array);
    case IS_OBJECT:
        return cJSON_vld_dump_zval_object(val.value, array);
    case IS_RESOURCE:
        return cJSON_vld_dump_zval_resource(val.value, array);
#if PHP_VERSION_ID < 70300
    case IS_CONSTANT:
        return cJSON_vld_dump_zval_constant(val.value, array);
#endif
    case IS_CONSTANT_AST:
        return cJSON_vld_dump_zval_constant_ast(val.value, array);
    case IS_UNDEF:
        return cJSON_vld_dump_zval_undef(val.value, array);
    case IS_FALSE:
        return cJSON_vld_dump_zval_false(val.value, array);
    case IS_TRUE:
        return cJSON_vld_dump_zval_true(val.value, array);
    case IS_REFERENCE:
        return cJSON_vld_dump_zval_reference(val.value, array);
    case IS_INDIRECT:
        return cJSON_vld_dump_zval_indirect(val.value, array);
    case IS_PTR:
        return cJSON_vld_dump_zval_ptr(val.value, array);
    }
    return cJSON_AddStringRefToArray(array, "<unknown>");
}

int cJSON_vld_dump_znode(cJSON *type_array, cJSON *value_array, unsigned int node_type, VLD_ZNODE node, unsigned int base_address, zend_op_array *op_array, int opline)
{
    char *tbuf;
    char buf[128];
    int index = 0;
    cJSON *cols[2] = {NULL, NULL};

    switch (node_type)
    {
    case IS_UNUSED:
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_UNUSED"))
            {
                goto fail;
            }
        }
        if (!cJSON_AddNullToArray(value_array))
        {
            goto fail;
        }
        break;
    case IS_CONST: /* 1 */
        if (VLD_G(verbosity) >= 3)
        {
            memset(buf, 0, 128);
            sprintf(buf, "IS_CONST (%d)", VLD_ZNODE_ELEM(node, var) / sizeof(zval));
            if (!cJSON_AddStringToArray(type_array, buf))
            {
                goto fail;
            }
        }
#if PHP_VERSION_ID >= 70300
        if (!cJSON_vld_dump_zval(*RT_CONSTANT((op_array->opcodes) + opline, node), value_array))
        {
            goto fail;
        }
#else
        if (!cJSON_vld_dump_zval(*RT_CONSTANT_EX(op_array->literals, node), value_array))
        {
            goto fail;
        }
#endif
        break;

    case IS_TMP_VAR: /* 2 */
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_TMP_VAR"))
            {
                goto fail;
            }
        }
        memset(buf, 0, 128);
        sprintf(buf, "~%d", VAR_NUM(VLD_ZNODE_ELEM(node, var)));
        if (!cJSON_AddStringToArray(value_array, buf))
        {
            goto fail;
        }
        break;
    case IS_VAR: /* 4 */
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_VAR"))
            {
                goto fail;
            }
        }
        memset(buf, 0, 128);
        sprintf(buf, "$%d", VAR_NUM(VLD_ZNODE_ELEM(node, var)));
        if (!cJSON_AddStringToArray(value_array, buf))
        {
            goto fail;
        }
        break;
    case IS_CV: /* 16 */
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_CV"))
            {
                goto fail;
            }
        }
        sprintf(buf, "!%d", (VLD_ZNODE_ELEM(node, var) - sizeof(zend_execute_data)) / sizeof(zval));
        if (!cJSON_AddStringToArray(value_array, buf))
        {
            goto fail;
        }
        break;
    case VLD_IS_OPNUM:
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_OPNUM"))
            {
                goto fail;
            }
        }
        sprintf(buf, "->%d", VLD_ZNODE_JMP_LINE(node, opline, base_address));
        if (!cJSON_AddStringToArray(value_array, buf))
        {
            goto fail;
        }
        /* Cleanup flag is not necessary for the last one. */
        /* cols[1] = value_array; */
        break;
    case VLD_IS_OPLINE:
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_OPLINE"))
            {
                goto fail;
            }
        }
        sprintf(buf, "->%d", VLD_ZNODE_JMP_LINE(node, opline, base_address));
        if (!cJSON_AddStringToArray(value_array, buf))
        {
            goto fail;
        }
        break;
    case VLD_IS_CLASS:
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_CLASS"))
            {
                goto fail;
            }
        }
#if PHP_VERSION_ID >= 70300
        if (!cJSON_vld_dump_zval(*RT_CONSTANT((op_array->opcodes) + opline, node), value_array))
        {
            goto fail;
        }
#else
        if (!cJSON_vld_dump_zval(*RT_CONSTANT_EX(op_array->literals, node), value_array))
        {
            goto fail;
        }
#endif
        break;
#if PHP_VERSION_ID >= 70200
    case VLD_IS_JMP_ARRAY:
    {
        zval *array_value;
        HashTable *myht;
        zend_ulong num;
        zend_string *key;
        zval *val;
        cJSON *jump_list;

        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(type_array, "IS_JMP_ARRAY"))
            {
                goto fail;
            }
        }

#if PHP_VERSION_ID >= 70300
        array_value = RT_CONSTANT((op_array->opcodes) + opline, node);
#else
        array_value = RT_CONSTANT_EX(op_array->literals, node);
#endif
        myht = Z_ARRVAL_P(array_value);
        jump_list = cJSON_CreateArray();
        ZVAL_VALUE_STRING_TYPE *new_str;
        ZEND_HASH_FOREACH_KEY_VAL_IND(myht, num, key, val)
        {
            memset(buf, 0, 128);
            if (key == NULL)
            {
                sprintf(buf, "%d:->%d, ", num, opline + (val->value.lval / sizeof(zend_op)));
                if (!cJSON_AddStringToArray(jump_list, buf))
                {
                    cJSON_Delet_Wrap(jump_list);
                    goto fail;
                }
            }
            else
            {
                new_str = php_url_encode(ZSTRING_VALUE(key), key->len PHP_URLENCODE_NEW_LEN(new_len));
                tbuf = (char *)malloc(new_str->len + 6);
                sprintf(tbuf, "'%s':->%d, ", ZSTRING_VALUE(new_str), opline + (val->value.lval / sizeof(zend_op)));
                if (!cJSON_AddStringToArray(jump_list, tbuf))
                {
                    efree(new_str);
                    free(tbuf);
                    cJSON_Delet_Wrap(jump_list);
                    goto fail;
                }
                efree(new_str);
                free(tbuf);
            }
        }
        ZEND_HASH_FOREACH_END();
        if (!cJSON_AddItemToArray(value_array, jump_list))
        {
            goto fail;
        }
    }
    break;
#endif
    default:
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddNullToArray(type_array))
            {
                goto fail;
            }
        }
        if (!cJSON_AddNullToArray(value_array))
        {
            goto fail;
        }
    }
    return 1;
fail:
    return 0;
}

int cJSON_vld_dump_op(int nr, zend_op *op_ptr, unsigned int base_address, int notdead, int entry, int start, int end, zend_op_array *opa, cJSON *op_arrays)
{
    static unsigned int last_lineno = (unsigned int)-1;
    int print_sep = 0, len;
    const char *fetch_type = "";
    unsigned int flags, op1_type, op2_type, res_type;
    const zend_op op = op_ptr[nr];
    char buf[64];
    const char *const_table[] = {"*", "E", ">", ">"};
    int const_flags[] = {notdead ? 0 : 1, entry, start, end};
    static cJSON *pre_opa = NULL;
    static cJSON *cols[18];
    cJSON *col = NULL;
    cJSON *tmp = NULL;

    if (pre_opa == op_arrays)
    {
        goto handle;
    }
    else
    {
        memset(cols, 0, sizeof(cols));
        pre_opa = NULL;
    }

    if (op.lineno == 0)
    {
        return 1;
    }

    for (int i = 0; i < STR_ARRAY_LEN(op_cols); i++)
    {
        if (VLD_G(verbosity) >= verbosity_flags[i] && !(cols[i] = cJSON_GetObjectItem(op_arrays, op_cols[i])))
        {
            memset(cols, 0, sizeof(cols));
            pre_opa = NULL;
            return 0;
        }
    }

handle:
    memset(buf, '\0', 64);
    if (op.opcode >= NUM_KNOWN_OPCODES)
    {
        flags = ALL_USED;
    }
    else
    {
        flags = opcodes[op.opcode].flags;
    }

    op1_type = op.VLD_TYPE(op1);
    op2_type = op.VLD_TYPE(op2);
    res_type = op.VLD_TYPE(result);

    if (flags == SPECIAL)
    {
        flags = vld_get_special_flags(&op, base_address);
    }
    if (flags & OP1_OPLINE)
    {
        op1_type = VLD_IS_OPLINE;
    }
    if (flags & OP2_OPLINE)
    {
        op2_type = VLD_IS_OPLINE;
    }
    if (flags & OP1_OPNUM)
    {
        op1_type = VLD_IS_OPNUM;
    }
    if (flags & OP2_OPNUM)
    {
        op2_type = VLD_IS_OPNUM;
    }
    if (flags & OP1_CLASS)
    {
        op1_type = VLD_IS_CLASS;
    }
    if (flags & RES_CLASS)
    {
        res_type = VLD_IS_CLASS;
    }
    if (flags & OP2_JMP_ARRAY)
    {
        op2_type = VLD_IS_JMP_ARRAY;
    }

#if PHP_VERSION_ID >= 70000 && PHP_VERSION_ID < 70100
    switch (op.opcode)
    {
    case ZEND_FAST_RET:
        if (op.extended_value == ZEND_FAST_RET_TO_FINALLY)
        {
            fetch_type = "to_finally";
        }
        else if (op.extended_value == ZEND_FAST_RET_TO_CATCH)
        {
            fetch_type = "to_catch";
        }
        break;
    case ZEND_FAST_CALL:
        if (op.extended_value == ZEND_FAST_CALL_FROM_FINALLY)
        {
            fetch_type = "from_finally";
        }
        break;
    }
#endif

#if PHP_VERSION_ID >= 70400
    if (op.opcode == ZEND_ASSIGN_DIM_OP)
    {
        fetch_type = get_assign_operation(op.extended_value);
    }
#endif
#if PHP_VERSION_ID >= 70100
    if (op.opcode == ZEND_NEW /* && op1_type == IS_UNUSED*/)
    {
        int ftype = op.op1.num & ZEND_FETCH_CLASS_MASK;
#else
    if (op.opcode == ZEND_FETCH_CLASS)
    {
        int ftype = op.extended_value & ZEND_FETCH_CLASS_MASK;
#endif
        switch (ftype)
        {
        case ZEND_FETCH_CLASS_SELF:
            fetch_type = "self";
            break;
        case ZEND_FETCH_CLASS_PARENT:
            fetch_type = "parent";
            break;
        case ZEND_FETCH_CLASS_STATIC:
            fetch_type = "static";
            break;
        case ZEND_FETCH_CLASS_AUTO:
            fetch_type = "auto";
            break;
        }
    }

    if (flags & OP_FETCH)
    {
        switch (op.VLD_EXTENDED_VALUE(op2))
        {
        case ZEND_FETCH_GLOBAL:
            fetch_type = "global";
            break;
        case ZEND_FETCH_LOCAL:
            fetch_type = "local";
            break;
#if PHP_VERSION_ID < 70100
        case ZEND_FETCH_STATIC:
            fetch_type = "static";
            break;
        case ZEND_FETCH_STATIC_MEMBER:
            fetch_type = "static member";
            break;
#endif
#ifdef ZEND_FETCH_GLOBAL_LOCK
        case ZEND_FETCH_GLOBAL_LOCK:
            fetch_type = "global lock";
            break;
#endif
#ifdef ZEND_FETCH_AUTO_GLOBAL
        case ZEND_FETCH_AUTO_GLOBAL:
            fetch_type = "auto global";
            break;
#endif
        default:
            fetch_type = "unknown";
            break;
        }
    }

    if (!cJSON_AddStringRefToArray(cols[8], fetch_type))
    {
        goto fail;
    }

    if (op.lineno == last_lineno)
    {
        tmp = cJSON_AddNullToArray(cols[0]);
    }
    else
    {
        tmp = cJSON_AddNumberToArray(cols[0], (double)op.lineno);
        last_lineno = op.lineno;
    }
    if (!tmp)
    {
        goto fail;
    }
    if (!cJSON_AddNumberToArray(cols[1], (double)nr))
    {
        goto fail;
    }
    /* Process col '*'\'E'\'I'\'O' */
    for (int i = 0; i < 4; i++)
    {
        col = cols[i + 2];
        tmp = (const_flags[i]) ? cJSON_AddStringRefToArray(col, const_table[i]) : cJSON_AddNullToArray(col);
        if (!tmp)
        {
            goto fail;
        }
    }

    col = cols[7];
    tmp = (op.opcode >= NUM_KNOWN_OPCODES) ? cJSON_AddStringRefToArray(col, "UNKNOWN_OPCODE") : cJSON_AddStringRefToArray(col, opcodes[op.opcode].name);
    if (!tmp)
    {
        goto fail;
    }

    if (VLD_G(verbosity) >= 3)
    {
        if (!cJSON_AddNumberToArray(cols[6], (double)op.opcode))
        {
            goto fail;
        }
    }

    col = cols[9];
    if (flags & EXT_VAL)
    {
#if PHP_VERSION_ID >= 70300
        tmp = op.opcode == ZEND_CATCH ? cJSON_AddStringRefToArray(col, "last") : cJSON_AddNumberToArray(col, (double)op.extended_value);
#else
        tmp = cJSON_AddNumberToArray(col, (double)op.extended_value);
#endif
    }
    else
    {
        tmp = cJSON_AddNullToArray(col);
    }
    if (!tmp)
    {
        goto fail;
    }

#if PHP_VERSION_ID >= 70100
    if ((flags & RES_USED) && op.result_type != IS_UNUSED)
    {
#else
    if ((flags & RES_USED) && !(op.VLD_EXTENDED_VALUE(result) & EXT_TYPE_UNUSED))
    {
#endif
        if (!cJSON_vld_dump_znode(cols[10], cols[11], res_type, op.result, base_address, opa, nr))
        {
            goto fail;
        }
    }
    else
    {
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddNullToArray(cols[10]))
            {
                goto fail;
            }
        }
        if (!cJSON_AddNullToArray(cols[11]))
        {
            goto fail;
        }
    }
    if (flags & OP1_USED)
    {
        if (!cJSON_vld_dump_znode(cols[12], cols[13], op1_type, op.op1, base_address, opa, nr))
        {
            goto fail;
        }
    }
    else
    {
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddNullToArray(cols[12]))
            {
                goto fail;
            }
        }
        if (!cJSON_AddNullToArray(cols[13]))
        {
            goto fail;
        }
    }
    if (flags & OP2_USED)
    {
        if (flags & OP2_INCLUDE)
        {
            char *op2_name = NULL;
            if (VLD_G(verbosity) >= 3)
            {
                if (!cJSON_AddStringRefToArray(cols[14], "OP2_INCLUDE"))
                {
                    goto fail;
                }
            }
            switch (op.extended_value)
            {
            case ZEND_INCLUDE_ONCE:
                op2_name = "INCLUDE_ONCE";
                break;
            case ZEND_REQUIRE_ONCE:
                op2_name = "REQUIRE_ONCE";
                break;
            case ZEND_INCLUDE:
                op2_name = "INCLUDE";
                break;
            case ZEND_REQUIRE:
                op2_name = "REQUIRE";
                break;
            case ZEND_EVAL:
                op2_name = "EVAL";
                break;
            default:
                op2_name = "!!ERROR!!";
                break;
            }
            if (!cJSON_AddStringRefToArray(cols[15], op2_name))
            {
                goto fail;
            }
        }
        else
        {
            if (!cJSON_vld_dump_znode(cols[14], cols[15], op2_type, op.op2, base_address, opa, nr))
            {
                goto fail;
            }
        }
    }
    else
    {
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddNullToArray(cols[14]))
            {
                goto fail;
            }
        }
        if (!cJSON_AddNullToArray(cols[15]))
        {
            goto fail;
        }
    }
    if (flags & EXT_VAL_JMP_ABS)
    {
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(cols[16], "EXT_JMP_ABS"))
            {
                goto fail;
            }
        }
        sprintf(buf, "->%d", op.extended_value);
        if (!cJSON_AddStringToArray(cols[17], buf))
        {
            goto fail;
        }
    }
    if (flags & EXT_VAL_JMP_REL)
    {
        if (VLD_G(verbosity) >= 3)
        {
            if (!cJSON_AddStringRefToArray(cols[16], "EXT_JMP_REL"))
            {
                goto fail;
            }
        }
        sprintf(buf, "->%d", nr + ((int)op.extended_value / sizeof(zend_op)));
        if (!cJSON_AddStringToArray(cols[17], buf))
        {
            goto fail;
        }
    }
    /*  FIXME: Not sure if it would accessiable along with 'flag & EXT_VAL_JMP_*'. */
    if ((flags & NOP2_OPNUM) && !(flags & (EXT_VAL_JMP_ABS | EXT_VAL_JMP_REL)))
    {
        zend_op next_op = op_ptr[nr + 1];
        if (!cJSON_vld_dump_znode(cols[16], cols[17], VLD_IS_OPNUM, next_op.op2, base_address, opa, nr))
        {
            goto fail;
        }
    }
    VLD_G(json_data)->inner_len++;
    return 1;
fail:
    memset(cols, 0, sizeof(cols));
    pre_opa = NULL;
    return 0;
}

void vld_analyse_oparray_quiet(zend_op_array *opa, vld_set *set, vld_branch_info *branch_info);
void vld_analyse_branch_quiet(zend_op_array *opa, unsigned int position, vld_set *set, vld_branch_info *branch_info);

int cJSON_vld_branch_info_dump(zend_op_array *opa, vld_branch_info *branch_info, cJSON *fn)
{
    unsigned int i, j;
    const char *fname = opa->function_name ? ZSTRING_VALUE(opa->function_name) : "__main";
    cJSON *col;
    cJSON *sline;
    cJSON *eline;
    cJSON *sop;
    cJSON *eop;
    cJSON *outs;
    cJSON *tmp;

    if (VLD_G(path_dump_file))
    {
        fprintf(VLD_G(path_dump_file), "subgraph cluster_%p {\n\tlabel=\"%s\";\n\tgraph [rankdir=\"LR\"];\n\tnode [shape = record];\n", opa, fname);

        for (i = 0; i < branch_info->starts->size; i++)
        {
            if (vld_set_in(branch_info->starts, i))
            {
                fprintf(
                    VLD_G(path_dump_file),
                    "\t\"%s_%d\" [ label = \"{ op #%d-%d | line %d-%d }\" ];\n",
                    fname, i, i,
                    branch_info->branches[i].end_op,
                    branch_info->branches[i].start_lineno,
                    branch_info->branches[i].end_lineno);
                if (vld_set_in(branch_info->entry_points, i))
                {
                    fprintf(VLD_G(path_dump_file), "\t%s_ENTRY -> %s_%d\n", fname, fname, i);
                }
                for (j = 0; j < branch_info->branches[i].outs_count; j++)
                {
                    if (branch_info->branches[i].outs[j])
                    {
                        if (branch_info->branches[i].outs[j] == VLD_JMP_EXIT)
                        {
                            fprintf(VLD_G(path_dump_file), "\t%s_%d -> %s_EXIT;\n", fname, i, fname);
                        }
                        else
                        {
                            fprintf(VLD_G(path_dump_file), "\t%s_%d -> %s_%d;\n", fname, i, fname, branch_info->branches[i].outs[j]);
                        }
                    }
                }
            }
        }
        fprintf(VLD_G(path_dump_file), "}\n");
    }

    if (!fn)
    {
        return 1;
    }
    col = cJSON_GetObjectItem(fn, "branch");
    if (!col)
    {
        return 0;
    }
    sline = cJSON_GetObjectItem(col, "sline");
    eline = cJSON_GetObjectItem(col, "eline");
    sop = cJSON_GetObjectItem(col, "sop");
    eop = cJSON_GetObjectItem(col, "eop");
    outs = cJSON_GetObjectItem(col, "outs");
    if (!sline || !eline || !sop || !eop || !outs)
    {
        return 0;
    }

    for (i = 0; i < branch_info->starts->size; i++)
    {
        if (vld_set_in(branch_info->starts, i))
        {
            if (!cJSON_AddNumberToArray(sline, (double)branch_info->branches[i].start_lineno))
            {
                return 0;
            }
            if (!cJSON_AddNumberToArray(eline, (double)branch_info->branches[i].end_lineno))
            {
                return 0;
            }
            if (!cJSON_AddNumberToArray(sop, (double)i))
            {
                return 0;
            }
            if (!cJSON_AddNumberToArray(eop, (double)branch_info->branches[i].end_op))
            {
                return 0;
            }

            for (j = 0; j < branch_info->branches[i].outs_count; j++)
            {
                if (branch_info->branches[i].outs[j] && !cJSON_AddNumberToArray(outs, (double)branch_info->branches[i].outs[j]))
                {
                    return 0;
                }
            }
        }
    }
    col = cJSON_GetObjectItem(fn, "path");
    if (!col)
    {
        return 0;
    }
    for (i = 0; i < branch_info->paths_count; i++)
    {
        tmp = cJSON_CreateArray();
        for (j = 0; j < branch_info->paths[i]->elements_count; j++)
        {
            if (!cJSON_AddNumberToArray(tmp, (double)branch_info->paths[i]->elements[j]))
            {
                cJSON_Delet_Wrap(tmp);
                return 0;
            }
        }
        if (!cJSON_AddItemToArray(col, tmp))
        {
            cJSON_Delet_Wrap(tmp);
            return 0;
        }
    }
}

void cJSON_vld_dump_oparray(zend_op_array *opa)
{
    unsigned int i;
    int j;
    vld_set *set;
    vld_branch_info *branch_info;
    unsigned int base_address = (unsigned int)(zend_intptr_t) & (opa->opcodes[0]);
    const char *path_cols[] = {"sline", "eline", "sop", "eop", "outs"};
    cJSON *fn = cJSON_CreateObject();
    cJSON *tmp;

    set = vld_set_create(opa->last);
    branch_info = vld_branch_info_create(opa->last);

    if (VLD_G(dump_paths))
    {
        vld_analyse_oparray_quiet(opa, set, branch_info);
    }

    if (!((VLD_G(json_data)->class) ? cJSON_AddStringToObjectCS(fn, "class", VLD_G(json_data)->class) : cJSON_AddNullObjectCS(fn, "class")))
    {
        cJSON_Delet_Wrap(fn);
        goto dump;
    }
    if (!(ZSTRING_VALUE(opa->filename) ? cJSON_AddStringToObjectCS(fn, "filename", ZSTRING_VALUE(opa->filename)) : cJSON_AddNullObjectCS(fn, "filename")))
    {
        cJSON_Delet_Wrap(fn);
        goto dump;
    }
    if (!(ZSTRING_VALUE(opa->function_name) ? cJSON_AddStringToObjectCS(fn, "function name", ZSTRING_VALUE(opa->function_name)) : cJSON_AddNullObjectCS(fn, "function name")))
    {
        cJSON_Delet_Wrap(fn);
        goto dump;
    }
    if (!cJSON_AddNumberToObjectCS(fn, "number of ops", (double)opa->last))
    {
        cJSON_Delet_Wrap(fn);
        goto dump;
    }

    tmp = cJSON_AddArrayToObjectCS(fn, "compiled vars");
    for (j = 0; j < opa->last_var; j++)
    {
        if (!cJSON_AddStringToArray(tmp, OPARRAY_VAR_NAME(opa->vars[j])))
        {
            cJSON_Delet_Wrap(fn);
            goto dump;
        }
    }
    /* Pre allocate arrays. */
    tmp = cJSON_AddObjectToObjectCS(fn, "ops");
    for (j = 0; j < STR_ARRAY_LEN(op_cols); j++)
    {
        if (VLD_G(verbosity) >= verbosity_flags[j] && !cJSON_AddArrayToObjectCS(tmp, op_cols[j]))
        {
            cJSON_Delet_Wrap(fn);
            goto dump;
        }
    }
    VLD_G(json_data)->inner_len = 0;
    for (i = 0; i < opa->last; i++)
    {
        if (!cJSON_vld_dump_op(i, opa->opcodes, base_address, vld_set_in(set, i), vld_set_in(branch_info->entry_points, i), vld_set_in(branch_info->starts, i), vld_set_in(branch_info->ends, i), opa, tmp))
        {
            cJSON_Delet_Wrap(fn);
            goto dump;
        }
    }
dump:
    if (VLD_G(dump_paths))
    {
        if (fn)
        {
            if (!cJSON_AddArrayToObjectCS(fn, "path"))
            {
                cJSON_Delet_Wrap(fn);
                goto only_dump;
            }
            tmp = cJSON_AddObjectToObjectCS(fn, "branch");
            if (!tmp)
            {
                cJSON_Delet_Wrap(fn);
                goto only_dump;
            }
            for (j = 0; j < STR_ARRAY_LEN(path_cols); j++)
            {
                if (!cJSON_AddArrayToObjectCS(tmp, path_cols[j]))
                {
                    cJSON_Delet_Wrap(fn);
                    break;
                }
            }
        }
    only_dump:
        vld_branch_post_process(opa, branch_info);
        vld_branch_find_paths(branch_info);
        if (!cJSON_vld_branch_info_dump(opa, branch_info, fn))
        {
            cJSON_Delet_Wrap(fn);
        }
    }
    vld_set_free(set);
    vld_branch_info_free(branch_info);
    if (fn)
    {
        /* Print delimiter before every function block, except the first one. */
        if (VLD_G(json_data)->outer_len)
        {
            if (VLD_G(format))
            {
                fputs(",\n", stdout);
            }
            else
            {
                fputs(",", stdout);
            }
        }
        char *func;
        if (VLD_G(format))
        {
            func = cJSON_Print(fn);
        }
        else
        {
            func = cJSON_PrintUnformatted(fn);
        }
        fputs(func, stdout);
        VLD_G(json_data)->outer_len++;
        /* Free it every time. */
        cJSON_Delet_Wrap(fn);
    }
    return;
}

void vld_analyse_oparray_quiet(zend_op_array *opa, vld_set *set, vld_branch_info *branch_info)
{
    unsigned int position = 0;

    while (position < opa->last)
    {
        if (position == 0)
        {
            vld_analyse_branch_quiet(opa, position, set, branch_info);
            vld_set_add(branch_info->entry_points, position);
        }
        else if (opa->opcodes[position].opcode == ZEND_CATCH)
        {
            vld_analyse_branch_quiet(opa, position, set, branch_info);
            vld_set_add(branch_info->entry_points, position);
        }
        position++;
    }
    vld_set_add(branch_info->ends, opa->last - 1);
    branch_info->branches[opa->last - 1].start_lineno = opa->opcodes[opa->last - 1].lineno;
}

void vld_analyse_branch_quiet(zend_op_array *opa, unsigned int position, vld_set *set, vld_branch_info *branch_info)
{
    vld_set_add(branch_info->starts, position);
    branch_info->branches[position].start_lineno = opa->opcodes[position].lineno;
    /* First we see if the branch has been visited, if so we bail out. */
    if (vld_set_in(set, position))
    {
        return;
    }
    /* Loop over the opcodes until the end of the array, or until a jump point has been found */
    vld_set_add(set, position);
    while (position < opa->last)
    {
        size_t jump_count = 0;
        int jumps[VLD_BRANCH_MAX_OUTS];
        size_t i;
        /* See if we have a jump instruction */
        if (vld_find_jumps(opa, position, &jump_count, jumps))
        {
            for (i = 0; i < jump_count; i++)
            {
                if (jumps[i] == VLD_JMP_EXIT || jumps[i] >= 0)
                {
                    vld_branch_info_update(branch_info, position, opa->opcodes[position].lineno, i, jumps[i]);
                    if (jumps[i] != VLD_JMP_EXIT)
                    {
                        vld_analyse_branch_quiet(opa, jumps[i], set, branch_info);
                    }
                }
            }
            break;
        }
        /* See if we have a throw instruction */
        if (opa->opcodes[position].opcode == ZEND_THROW)
        {
            vld_set_add(branch_info->ends, position);
            branch_info->branches[position].start_lineno = opa->opcodes[position].lineno;
            break;
        }
        /* See if we have an exit instruction */
        if (opa->opcodes[position].opcode == ZEND_EXIT)
        {
            vld_set_add(branch_info->ends, position);
            branch_info->branches[position].start_lineno = opa->opcodes[position].lineno;
            break;
        }
        /* See if we have a return instruction */
        if (
            opa->opcodes[position].opcode == ZEND_RETURN || opa->opcodes[position].opcode == ZEND_RETURN_BY_REF)
        {
            vld_set_add(branch_info->ends, position);
            branch_info->branches[position].start_lineno = opa->opcodes[position].lineno;
            break;
        }
        position++;
        vld_set_add(set, position);
    }
}

json_wrap *json_patch_init(void)
{
    cJSON_InitHooks(NULL);
    json_wrap *res = (json_wrap *)calloc(1, sizeof(json_wrap));
    if (!res)
    {
        fputs("Fail to allocate memory for json patch.", stderr);
        exit(-1);
    }
    return res;
}

void json_patch_free(void)
{
    if (VLD_G(json_data))
    {
        free(VLD_G(json_data));
    }
}