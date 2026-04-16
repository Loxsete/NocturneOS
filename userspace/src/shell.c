#include <shell.h>
#include <ulib/io.h>
#include <ulib/string.h>
#include <ulib/syscall.h>

#define MAX_PATH 256
#define MAX_INPUT 256

static char cwd[MAX_PATH] = "/";
static char hostname[256];

static void resolve_path(const char *input, char *result)
{
    if (input[0] == '/') {
        u_strcpy(result, input, MAX_PATH);
        return;
    }
    u_strcpy(result, cwd, MAX_PATH);
    int len = u_strlen(result);
    if (len > 1 && result[len-1] != '/') result[len++] = '/', result[len] = 0;
    for (int i = 0; input[i] && len < MAX_PATH-1; i++)
        result[len++] = input[i];
    result[len] = 0;
}

static void print_ascii(void)
{
    static char buf[4096];
    int64_t n = sys_read("/tmp/ascii.txt", buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        u_puts(buf);
    }
}

static void read_hostname(const char *path, char *result)
{
    static char buf[256];
    int64_t n = sys_read("/etc/hostname", buf, sizeof(buf) - 1);
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = 0;
    else buf[n] = 0;
    u_strcpy(result, buf, 256);
}

static void cmd_ls(const char *arg)
{
    char path[MAX_PATH];
    if (arg) resolve_path(arg, path);
    else     u_strcpy(path, cwd, MAX_PATH);

    char name[256];
    uint32_t i = 0;
    int64_t r;
    u_puts("\n");
    while ((r = sys_readdir(path, i++, name)) != (int64_t)-1) {
        u_puts("  ");
        u_puts(name);
        if (r == 1) u_puts("/");
        u_puts("\n");
    }
}

static void cmd_cd(const char *arg)
{
    if (!arg) { u_strcpy(cwd, "/", MAX_PATH); return; }
    char path[MAX_PATH];
    resolve_path(arg, path);
    u_strcpy(cwd, path, MAX_PATH);
}

static void cmd_cat(const char *arg)
{
    if (!arg) { u_puts("\nusage: cat <file>\n"); return; }
    char path[MAX_PATH];
    resolve_path(arg, path);
    static char buf[512];
    int64_t n = sys_read(path, buf, sizeof(buf)-1);
    if (n < 0) { u_puts("\nnot found\n"); return; }
    buf[n] = 0;
    u_puts("\n");
    u_puts(buf);
    u_puts("\n");
}

static void cmd_mkdir(const char *arg)
{
    if (!arg) { u_puts("\nusage: mkdir <path>\n"); return; }
    char path[MAX_PATH];
    resolve_path(arg, path);
    sys_mkdir(path) == 0 ? u_puts("\nok\n") : u_puts("\nfailed\n");
}

static void cmd_touch(const char *arg)
{
    if (!arg) { u_puts("\nusage: touch <file>\n"); return; }
    char path[MAX_PATH];
    resolve_path(arg, path);
    sys_mkfile(path) == 0 ? u_puts("\nok\n") : u_puts("\nfailed\n");
}

static void cmd_rm(const char *arg)
{
    if (!arg) { u_puts("\nusage: rm <file>\n"); return; }
    char path[MAX_PATH];
    resolve_path(arg, path);
    sys_rm(path) == 0 ? u_puts("\nok\n") : u_puts("\nfailed\n");
}

static void cmd_write(const char *file, const char *text)
{
    if (!file || !text) { u_puts("\nusage: write <file> <text>\n"); return; }
    char path[MAX_PATH];
    resolve_path(file, path);
    sys_write(path, text, u_strlen(text)) >= 0 ? u_puts("\nok\n") : u_puts("\nfailed\n");
}

static void cmd_pwd(void)
{
    u_puts("\n"); u_puts(cwd); u_puts("\n");
}

static void cmd_help(void)
{
    u_puts("\ncommands:\n");
    u_puts("  ls [path]\n");
    u_puts("  cd [path]\n");
    u_puts("  pwd\n");
    u_puts("  cat <file>\n");
    u_puts("  mkdir <path>\n");
    u_puts("  touch <file>\n");
    u_puts("  rm <file>\n");
    u_puts("  write <file> <text>\n");
    u_puts("  echo <text>\n");
    u_puts("  clear\n");
    u_puts("  exit\n");
}

static int parse(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;

        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) { *p = 0; p++; }
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ') p++;
            if (*p) { *p = 0; p++; }
        }
    }

    return argc;
}

void user_shell(void)
{
    static char input[MAX_INPUT];

    u_puts("\nNocturneOS userspace shell!!!\n");

    print_ascii();

    while (1) {
        read_hostname("/etc/hostname", hostname);

        u_puts("\n[");
        u_puts(hostname);
        u_puts(":");
        u_puts(cwd);
        u_puts("] >> ");

        u_gets(input, sizeof(input));
        if (!input[0]) continue;

        char *argv[8];
        int argc = parse(input, argv, 8);
        if (argc == 0) continue;

        if      (!u_strcmp(argv[0], "exit"))  { u_puts("\n"); sys_exit(); }
        else if (!u_strcmp(argv[0], "clear")) sys_clear();
        else if (!u_strcmp(argv[0], "help"))  cmd_help();
        else if (!u_strcmp(argv[0], "pwd"))   cmd_pwd();
        else if (!u_strcmp(argv[0], "ls"))    cmd_ls(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "cd"))    cmd_cd(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "cat"))   cmd_cat(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "mkdir")) cmd_mkdir(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "touch")) cmd_touch(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "rm"))    cmd_rm(argc > 1 ? argv[1] : 0);
        else if (!u_strcmp(argv[0], "echo")) {
            u_puts("\n");
            for (int i = 1; i < argc; i++) {
                u_puts(argv[i]);
                if (i != argc - 1) u_puts(" ");
            }
            u_puts("\n");
        }
        else if (!u_strcmp(argv[0], "write")) {
            if (argc < 3) {
                cmd_write(0, 0);
            } else {
                char buf[512];
                int pos = 0;

                for (int i = 2; i < argc; i++) {
                    int j = 0;
                    while (argv[i][j] && pos < (int)sizeof(buf) - 1) {
                        buf[pos++] = argv[i][j++];
                    }
                    if (i != argc - 1 && pos < (int)sizeof(buf) - 1) {
                        buf[pos++] = ' ';
                    }
                }

                buf[pos] = 0;

                cmd_write(argv[1], buf);
            }
        }
        else {
            u_puts("\nunknown: ");
            u_puts(argv[0]);
            u_puts("\n");
        }
    }
}
