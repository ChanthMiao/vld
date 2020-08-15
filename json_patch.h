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

#ifndef JSON_PATCH_H
#define JSON_PATCH_H

#include "cJSON.h"

typedef struct _json_wrap
{
    cJSON *date; /* Reserved field for future development. */
    unsigned int inner_len;
    unsigned int outer_len;
    char *class;
} json_wrap;

json_wrap *json_patch_init(void);
void json_patch_free(void);
void cJSON_vld_dump_oparray(zend_op_array *opa);

#endif /*JSON_PATCH_H*/