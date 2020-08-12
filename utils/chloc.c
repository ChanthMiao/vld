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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void help_msg(void)
{
    fputs("Usage:\n", stderr);
    fputs("chloc <old prefix> <new prefix> <appended ext name> <path>\n", stderr);
}

int main(int argc, char const *argv[])
{
    if (argc != 5)
    {
        help_msg();
        return 1;
    }
    // 设定参数别名
    const char *old_prefix = argv[1];
    const char *new_prefix = argv[2];
    const char *appended_ext = argv[3];
    const char *path = argv[4];
    // 检验用户传入的'old prefix'是否存在
    if (strstr(path, old_prefix) != path)
    {
        fputs("Old prefix not found\n", stderr);
        help_msg();
        return 1;
    }
    // 计算各参数长度
    size_t old_prefix_len = strlen(old_prefix);
    size_t new_prefix_len = strlen(new_prefix);
    size_t path_len = strlen(path);
    size_t appended_ext_len = strlen(appended_ext);
    size_t bare_path_len = path_len - old_prefix_len;
    // 估算新路径字符串占用的字节数, 并分配相应内存
    size_t maxlen = bare_path_len + new_prefix_len + appended_ext_len + 1;
    char *new_path = (char *)calloc(maxlen, sizeof(char));
    // calloc 失败
    if (!new_path)
    {
        fputs(strerror(errno), stderr);
        return 1;
    }
    // 构造'new path'
    char *cur = new_path;
    memcpy(cur, new_prefix, new_prefix_len);
    cur += new_prefix_len;
    memcpy(cur, path + old_prefix_len, bare_path_len);
    cur += bare_path_len;
    memcpy(cur, appended_ext, appended_ext_len);
    fputs(new_path, stdout);
    free(new_path);
    return 0;
}
