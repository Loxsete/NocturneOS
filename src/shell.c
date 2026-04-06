#include "shell.h"
#include "kapi.h"
#include "vfs.h"
#include "ata.h"
#include "kprint.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "kstring.h"

#define INPUT_BUF_SIZE 256

static char input_buf[INPUT_BUF_SIZE];
static int  input_len = 0;
static char cwd[VFS_MAX_PATH] = "/";

extern kernel_api_t *kapi_get(void);


static void args_join(char **argv, int from, int argc, char *out, int max)
{
    int pos = 0;
    for (int i = from; i < argc && pos < max - 1; i++) {
        if (i > from && pos < max - 1) out[pos++] = ' ';
        for (int j = 0; argv[i][j] && pos < max - 1; j++)
            out[pos++] = argv[i][j];
    }
    out[pos] = '\0';
}


static int parse_args(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p == ' ') { *p = '\0'; p++; }
    }
    return argc;
}


static void resolve_path(const char *input, char *out, int max)
{
    if (input[0] == '/') { kstrcpy(out, input, max); return; }
    int cwdlen = kstrlen(cwd);
    kstrcpy(out, cwd, max);
    if (cwdlen > 0 && out[cwdlen-1] != '/') {
        out[cwdlen] = '/'; out[cwdlen+1] = '\0'; cwdlen++;
    }
    for (int i = 0; input[i] && cwdlen + i < max - 1; i++)
        out[cwdlen + i] = input[i];
    out[cwdlen + kstrlen(input)] = '\0';
}

static void cmd_disks(void)
{
    const char *bus_names[] = {"Primary", "Secondary"};
    const char *drv_names[] = {"Master", "Slave"};
    int found = 0;

    kputs("\n");
    for (int bus = 0; bus < 2; bus++) {
        for (int drv = 0; drv < 2; drv++) {
            ata_disk_t *d = &ata_disks[bus][drv];
            if (!d->present) continue;
            found = 1;

            uint32_t mb = (d->sectors / 2) / 1024;

            kputs("  ");
            kputs_col(bus_names[bus], COLOR_PROMPT);
            kputs(" ");
            kputs(drv_names[drv]);
            kputs(": ");
            kputs_col(d->model[0] ? d->model : "(no model)", COLOR_DIR);
            kputs(" — ");
            kputdec((uint64_t)mb);
            kputs(" MB\n");
        }
    }
    if (!found) kputs("  no disks found\n");
}

static void cmd_ls(int argc, char *argv[])
{
    char path[VFS_MAX_PATH];
    if (argc > 1) resolve_path(argv[1], path, VFS_MAX_PATH);
    else kstrcpy(path, cwd, VFS_MAX_PATH);

    vfs_node_t *dir = vfs_resolve(path);
    if (!dir) { kputs("\n  ls: not found: "); kputs(path); return; }
    if (!(dir->flags & VFS_FLAG_DIR)) { kputs("\n  ls: not a directory"); return; }

    kputs("\n");
    uint32_t i = 0;
    vfs_node_t *entry;
    while ((entry = vnode_readdir(dir, i++)) != NULL) {
        if (entry->flags & VFS_FLAG_DIR) {
            kputs_col("  ", COLOR_FG);
            kputs_col(entry->name, COLOR_DIR);
            kputs_col("/\n", COLOR_DIR);
        } else {
            kputs("  "); kputs(entry->name); kputs("\n");
        }
    }
}

static void cmd_cd(int argc, char *argv[])
{
    if (argc < 2) { kstrcpy(cwd, "/", VFS_MAX_PATH); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *dir = vfs_resolve(path);
    if (!dir) { kputs("\n  cd: not found: "); kputs(path); return; }
    if (!(dir->flags & VFS_FLAG_DIR)) { kputs("\n  cd: not a directory"); return; }
    kstrcpy(cwd, path, VFS_MAX_PATH);
    int len = kstrlen(cwd);
    if (len > 1 && cwd[len-1] == '/') cwd[len-1] = '\0';
}

static void cmd_pwd(void)
{
    kputs("\n  "); kputs(cwd);
}

static void cmd_cat(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: cat <path>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *node = vfs_resolve(path);
    if (!node) { kputs("\n  cat: not found: "); kputs(path); return; }
    if (node->flags & VFS_FLAG_DIR) { kputs("\n  cat: is a directory"); return; }

    char buf[1024];
    uint64_t offset = 0;
    int64_t  n;
    kputs("\n");
    while ((n = vnode_read(node, buf, offset, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        kputs(buf);
        offset += (uint64_t)n;
    }
}

static void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: mkdir <path>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (!vfs_mkdir(path)) { kputs("\n  mkdir: failed: "); kputs(path); return; }
    kputs("\n  created: "); kputs(path);
}

static void cmd_touch(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: touch <path>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (!vfs_mkfile(path)) { kputs("\n  touch: failed: "); kputs(path); return; }
    kputs("\n  created: "); kputs(path);
}

static void cmd_rm(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: rm <path>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    if (vfs_unlink(path) < 0) { kputs("\n  rm: failed: "); kputs(path); return; }
    kputs("\n  removed: "); kputs(path);
}

static void cmd_write(int argc, char *argv[])
{
    if (argc < 3) { kputs("\n  usage: write <path> <text>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    char text[INPUT_BUF_SIZE];
    args_join(argv, 2, argc, text, INPUT_BUF_SIZE);

    vfs_node_t *node = vfs_resolve(path);
    if (!node) node = vfs_mkfile(path);
    if (!node) { kputs("\n  write: cannot create: "); kputs(path); return; }
    if (vnode_write(node, text, 0, (uint64_t)kstrlen(text)) < 0)
        { kputs("\n  write: failed"); return; }
    kputs("\n  written: "); kputs(path);
}

static void cmd_hexdump(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: hexdump <path>"); return; }
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
    vfs_node_t *node = vfs_resolve(path);
    if (!node) { kputs("\n  hexdump: not found: "); kputs(path); return; }
    if (node->flags & VFS_FLAG_DIR) { kputs("\n  hexdump: is a directory"); return; }

    static const char hex[] = "0123456789ABCDEF";
    uint8_t buf[16];
    uint64_t offset = 0;
    int64_t  n;
    kputs("\n");
    while ((n = vnode_read(node, buf, offset, 16)) > 0) {
        char addr[9];
        uint64_t a = offset;
        for (int i = 7; i >= 0; i--) { addr[i] = hex[a & 0xF]; a >>= 4; }
        addr[8] = '\0';
        kputs(addr); kputs("  ");
        for (int64_t i = 0; i < 16; i++) {
            if (i < n) {
                char h[3] = { hex[(buf[i]>>4)&0xF], hex[buf[i]&0xF], '\0' };
                kputs(h);
            } else kputs("  ");
            kputs(" ");
            if (i == 7) kputs(" ");
        }
        kputs(" |");
        for (int64_t i = 0; i < n; i++) {
            char c[2] = { (buf[i] >= 0x20 && buf[i] < 0x7F) ? (char)buf[i] : '.', '\0' };
            kputs(c);
        }
        kputs("|\n");
        offset += (uint64_t)n;
    }
}

static void tree_recurse(vfs_node_t *dir, int depth)
{
    if (depth > 8) return;
    uint32_t i = 0;
    vfs_node_t *entry;
    while ((entry = vnode_readdir(dir, i++)) != NULL) {
        for (int d = 0; d < depth; d++) kputs("  ");
        if (entry->flags & VFS_FLAG_DIR) {
            kputs_col(entry->name, COLOR_DIR);
            kputs_col("/\n", COLOR_DIR);
            tree_recurse(entry, depth + 1);
        } else { kputs(entry->name); kputs("\n"); }
    }
}

static void cmd_tree(int argc, char *argv[])
{
    char path[VFS_MAX_PATH];
    if (argc > 1) resolve_path(argv[1], path, VFS_MAX_PATH);
    else kstrcpy(path, cwd, VFS_MAX_PATH);
    vfs_node_t *dir = vfs_resolve(path);
    if (!dir) { kputs("\n  tree: not found: "); kputs(path); return; }
    if (!(dir->flags & VFS_FLAG_DIR)) { kputs("\n  tree: not a directory"); return; }
    kputs("\n");
    kputs_col(path, COLOR_DIR); kputs_col("/\n", COLOR_DIR);
    tree_recurse(dir, 1);
}

static void cmd_exec(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n  usage: exec <path>"); return; }
 
    char path[VFS_MAX_PATH];
    resolve_path(argv[1], path, VFS_MAX_PATH);
 
    vfs_node_t *node = vfs_resolve(path);
    if (!node) { kputs("\n  exec: not found: "); kputs(path); return; }
    if (node->flags & VFS_FLAG_DIR) { kputs("\n  exec: is a directory"); return; }
 
    extern void *kmalloc(uint64_t);
    extern kernel_api_t *kapi_get(void);
 
    uint64_t bufsize = node->size > 0 ? node->size : 65536;
    void *buf = kmalloc(bufsize);
    if (!buf) { kputs("\n  exec: out of memory"); return; }
 
    int64_t n = vnode_read(node, buf, 0, bufsize);
    if (n <= 0) { kputs("\n  exec: read failed or empty"); return; }
 
    kputs("\n  exec: running "); kputs(path); kputs("\n");
 
    kapp_entry_t entry = (kapp_entry_t)buf;
    entry(kapi_get());
 
    kputs("\n  exec: done");
}


static void cmd_echo(int argc, char *argv[])
{
    if (argc < 2) { kputs("\n"); return; }
    char text[INPUT_BUF_SIZE];
    args_join(argv, 1, argc, text, INPUT_BUF_SIZE);
    kputs("\n  "); kputs(text);
}

static void cmd_help(void)
{
    kputs("\n");
    kputs_col("  Navigation:\n", COLOR_PROMPT);
    kputs("    cd [path]            change directory\n");
    kputs("    pwd                  print working directory\n");
    kputs_col("  Filesystem:\n", COLOR_PROMPT);
    kputs("    ls [path]            list directory\n");
    kputs("    cat <path>           print file\n");
    kputs("    tree [path]          directory tree\n");
    kputs("    mkdir <path>         create directory\n");
    kputs("    touch <path>         create empty file\n");
    kputs("    rm <path>            remove file\n");
    kputs("    write <path> <text>  write text to file\n");
    kputs("    hexdump <path>       hex dump file\n");
    kputs_col("  Execution:\n", COLOR_PROMPT);
    kputs("    exec <path>          run flat binary from VFS (now dont work, maybe late <3)\n");
    kputs_col("  Other:\n", COLOR_PROMPT);
    kputs("    echo <text>          print text\n");
    kputs("    clear                clear screen\n");
    kputs("    help                 this message\n");
}

void shell_exec(const char *input)
{
    static char buf[INPUT_BUF_SIZE];
    int len = kstrlen(input);
    if (len == 0) return;
    if (len >= INPUT_BUF_SIZE) len = INPUT_BUF_SIZE - 1;
    for (int i = 0; i < len; i++) buf[i] = input[i];
    buf[len] = '\0';

    char *argv[16];
    int   argc = parse_args(buf, argv, 16);
    if (argc == 0) return;

    if      (kstrcmp(argv[0], "help")    == 0) cmd_help();
    else if (kstrcmp(argv[0], "clear")   == 0) { fb_fill(COLOR_BG); fb_cursor_x = 0; fb_cursor_y = 0; }
    else if (kstrcmp(argv[0], "disks")   == 0) cmd_disks();
    else if (kstrcmp(argv[0], "cd")      == 0) cmd_cd(argc, argv);
    else if (kstrcmp(argv[0], "pwd")     == 0) cmd_pwd();
    else if (kstrcmp(argv[0], "ls")      == 0) cmd_ls(argc, argv);
    else if (kstrcmp(argv[0], "cat")     == 0) cmd_cat(argc, argv);
    else if (kstrcmp(argv[0], "mkdir")   == 0) cmd_mkdir(argc, argv);
    else if (kstrcmp(argv[0], "touch")   == 0) cmd_touch(argc, argv);
    else if (kstrcmp(argv[0], "rm")      == 0) cmd_rm(argc, argv);
    else if (kstrcmp(argv[0], "write")   == 0) cmd_write(argc, argv);
    else if (kstrcmp(argv[0], "hexdump") == 0) cmd_hexdump(argc, argv);
    else if (kstrcmp(argv[0], "tree")    == 0) cmd_tree(argc, argv);
    else if (kstrcmp(argv[0], "exec")    == 0) cmd_exec(argc, argv);
    else if (kstrcmp(argv[0], "echo")    == 0) cmd_echo(argc, argv);
    else { kputs("\n  unknown: "); kputs(argv[0]); kputs("\n  type 'help'"); }
}

void shell_prompt(void)
{
    kputs_col("\nlxtos", COLOR_PROMPT);
    kputs_col(":", COLOR_FG);
    kputs_col(cwd, COLOR_DIR);
    kputs_col("> ", COLOR_FG);
}

void draw_banner(void)
{
    fb_cursor_x = 0;
    fb_cursor_y = 0;
    kputs_col("lxtos kernel\n", COLOR_PROMPT);
    kputs("type 'help' for commands\n");
}

void shell_run(void)
{
    shell_prompt();
    while (1) {
        char c = kb_getchar();
        if (!c) continue;
        if (c == '\n') {
            input_buf[input_len] = '\0';
            shell_exec(input_buf);
            input_len = 0;
            shell_prompt();
        } else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                fb_putchar_cursor('\b', COLOR_INPUT, COLOR_BG);
            }
        } else {
            if (input_len < INPUT_BUF_SIZE - 1) {
                input_buf[input_len++] = c;
                fb_putchar_cursor(c, COLOR_INPUT, COLOR_BG);
            }
        }
    }
}
