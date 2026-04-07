#include <console/shell.h>
#include <fs/vfs.h>
#include <drivers/ata.h>
#include <lib/kstring.h>
#include <lib/kitoa.h>

#define INPUT_BUF_SIZE 256

static char input_buf[INPUT_BUF_SIZE];
static int  input_len = 0;
static char cwd[VFS_MAX_PATH] = "/";

static vfs_node_t *console;

static void out(const char *s)
{
    if (console) vnode_write(console, s, 0, kstrlen(s));
}

static void outc(char c)
{
    if (console) vnode_write(console, &c, 0, 1);
}

static int parse_args(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p  = line;
    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; }
    }
    return argc;
}

static void resolve_path(const char *input, char *result, int max)
{
    if (input[0] == '/') {
        kstrcpy(result, input, max);
        return;
    }
    int len = kstrlen(cwd);
    kstrcpy(result, cwd, max);
    if (len > 1 && result[len - 1] != '/')
        result[len++] = '/', result[len] = 0;
    for (int i = 0; input[i] && len < max - 1; i++)
        result[len++] = input[i];
    result[len] = 0;
}



static void cmd_help(void)
{
    out("\n");
    out("  ls [path]          - list directory\n");
    out("  cd [path]          - change directory\n");
    out("  pwd                - print working directory\n");
    out("  cat <file>         - print file contents\n");
    out("  mkdir <path>       - create directory\n");
    out("  touch <path>       - create empty file\n");
    out("  rm <path>          - remove file\n");
    out("  write <file> <txt> - write text to file\n");
    out("  tree [path]        - recursive directory tree\n");
    out("  disks              - list detected ATA disks\n");
    out("  help               - this message\n");
}

static void cmd_clear(void)
{
    out("\x1b[2J");
}

static void cmd_ls(int argc, char *argv[])
{
    char path[VFS_MAX_PATH];
    if (argc > 1) resolve_path(argv[1], path, VFS_MAX_PATH);
    else          kstrcpy(path, cwd, VFS_MAX_PATH);

    vfs_node_t *dir = vfs_resolve(path);
    if (!dir || !(dir->flags & VFS_FLAG_DIR)) { out("\nnot found\n"); return; }

    out("\n");
    uint32_t i = 0;
    vfs_node_t *e;
    while ((e = vnode_readdir(dir, i++))) {
        out("  ");
        out(e->name);
        if (e->flags & VFS_FLAG_DIR) out("/");
        out("\n");
    }
}

static void cmd_cd(int argc, char *argv[])
{
    if (argc < 2) { kstrcpy(cwd, "/", VFS_MAX_PATH); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *dir = vfs_resolve(path);
    if (!dir || !(dir->flags & VFS_FLAG_DIR)) { out("\nbad dir\n"); return; }
    kstrcpy(cwd, path, VFS_MAX_PATH);
}

static void cmd_pwd(void)
{
    out("\n"); out(cwd); out("\n");
}

static void cmd_cat(int argc, char *argv[])
{
    if (argc < 2) { out("\nusage: cat <file>\n"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *node = vfs_resolve(path);
    if (!node || (node->flags & VFS_FLAG_DIR)) { out("\nnot found\n"); return; }

    char buf[256];
    int64_t n;
    uint64_t off = 0;
    out("\n");
    while ((n = vnode_read(node, buf, off, sizeof(buf) - 1)) > 0) {
        buf[n] = 0;
        out(buf);
        off += n;
    }
    out("\n");
}

static void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) { out("\nusage: mkdir <path>\n"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (!vfs_mkdir(path)) { out("\nfailed\n"); return; }
    out("\nok\n");
}

static void cmd_touch(int argc, char *argv[])
{
    if (argc < 2) { out("\nusage: touch <file>\n"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (!vfs_mkfile(path)) { out("\nfailed\n"); return; }
    out("\nok\n");
}

static void cmd_rm(int argc, char *argv[])
{
    if (argc < 2) { out("\nusage: rm <file>\n"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (vfs_unlink(path) < 0) { out("\nfailed\n"); return; }
    out("\nok\n");
}

static void cmd_write(int argc, char *argv[])
{
    if (argc < 3) { out("\nusage: write <file> <text>\n"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *node = vfs_resolve(path);
    if (!node) {
        node = vfs_mkfile(path);
        if (!node) { out("\nfailed to create\n"); return; }
    }
    vnode_write(node, argv[2], 0, kstrlen(argv[2]));
    out("\nok\n");
}

static void tree_recursive(vfs_node_t *dir, int depth)
{
    uint32_t i = 0;
    vfs_node_t *e;
    while ((e = vnode_readdir(dir, i++))) {
        for (int d = 0; d < depth; d++) out("  ");
        out(e->name);
        if (e->flags & VFS_FLAG_DIR) {
            out("/\n");
            tree_recursive(e, depth + 1);
        } else {
            out("\n");
        }
    }
}

static void cmd_tree(int argc, char *argv[])
{
    char path[VFS_MAX_PATH];
    if (argc > 1) resolve_path(argv[1], path, VFS_MAX_PATH);
    else          kstrcpy(path, cwd, VFS_MAX_PATH);

    vfs_node_t *dir = vfs_resolve(path);
    if (!dir || !(dir->flags & VFS_FLAG_DIR)) { out("\nnot found\n"); return; }

    out("\n");
    out(path); out("/\n");
    tree_recursive(dir, 1);
}

static void cmd_disks(void)
{
    char buf[16];
    out("\n");
    int found = 0;
    for (int bus = 0; bus < 2; bus++) {
        for (int drv = 0; drv < 2; drv++) {
            ata_disk_t *d = &ata_disks[bus][drv];
            if (!d->present) continue;
            found = 1;
            out("  ");
            out(bus == 0 ? "primary" : "secondary");
            out(drv == 0 ? " master  " : " slave   ");
            out(d->model);
            out("  ");
            kitoa(d->sectors / 2048, buf);
            out(buf);
            out(" MB\n");
        }
    }
    if (!found) out("  no disks found\n");
}

void shell_exec(const char *input)
{
    static char buf[INPUT_BUF_SIZE];
    int len = kstrlen(input);
    if (len >= INPUT_BUF_SIZE) len = INPUT_BUF_SIZE - 1;
    for (int i = 0; i < len; i++) buf[i] = input[i];
    buf[len] = 0;

    char *argv[16];
    int   argc = parse_args(buf, argv, 16);
    if (argc == 0) return;

    if      (!kstrcmp(argv[0], "help"))  cmd_help();
    else if (!kstrcmp(argv[0], "ls"))    cmd_ls(argc, argv);
    else if (!kstrcmp(argv[0], "cd"))    cmd_cd(argc, argv);
    else if (!kstrcmp(argv[0], "pwd"))   cmd_pwd();
    else if (!kstrcmp(argv[0], "cat"))   cmd_cat(argc, argv);
    else if (!kstrcmp(argv[0], "mkdir")) cmd_mkdir(argc, argv);
    else if (!kstrcmp(argv[0], "touch")) cmd_touch(argc, argv);
    else if (!kstrcmp(argv[0], "rm"))    cmd_rm(argc, argv);
    else if (!kstrcmp(argv[0], "write")) cmd_write(argc, argv);
    else if (!kstrcmp(argv[0], "tree"))  cmd_tree(argc, argv);
    else if (!kstrcmp(argv[0], "disks")) cmd_disks();
    else if (!kstrcmp(argv[0], "clear")) cmd_clear();
    else { out("\nunknown: "); out(argv[0]); out("\n"); }
}

void shell_prompt(void)
{
    out("\n");
    out("[");
    out(cwd);
    out("] ");
    out(">> ");
}

void shell_run(void)
{
    console = vfs_resolve("/dev/console");
    if (!console) return;

    shell_prompt();
    while (1) {
        char c;
        if (vnode_read(console, &c, 0, 1) <= 0) continue;
        if (c == '\n') {
            input_buf[input_len] = 0;
            shell_exec(input_buf);
            input_len = 0;
            shell_prompt();
        } else {
            if (input_len < INPUT_BUF_SIZE - 1) {
                input_buf[input_len++] = c;
                outc(c);
            }
        }
    }
}
