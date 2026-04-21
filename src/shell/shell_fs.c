// Filesystem shell commands: sys.fs.ls, sys.fs.cat.

#include "shell_internal.h"
#include <fs/vfs.h>
#include <syscall.h>

void cmd_fs_ls(void) {
    char *args = cmd_buffer + 9;
    while (*args == ' ') args++;
    if (use_syscalls)
        sys_iso_ls(*args ? args : "/");
    else
        vfs_ls(*args ? args : "/");
}

void cmd_fs_cat(void) {
    char *args = cmd_buffer + 10;
    while (*args == ' ') args++;
    if (*args == '\0') {
        sh_print("Usage: sys.fs.cat <path>\n");
        return;
    }
    uint8 *buf = (uint8 *)sh_malloc(65536);
    if (!buf) {
        sh_print("Out of memory\n");
        return;
    }
    int rd;
    if (use_syscalls)
        rd = sys_iso_read(args, buf, 65536);
    else
        rd = vfs_read_file(args, buf, 65536);
    if (rd > 0) {
        for (int i = 0; i < rd; i++)
            sh_putc((char)buf[i]);
        sh_putc('\n');
    } else {
        sh_print("File not found\n");
    }
    sh_free(buf);
}
