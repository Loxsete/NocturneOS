#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/string.h>
#include <ulib/syscall.h>
#include <ulib/args.h>

#define RESET "\033[0m"
#define BOLD  "\033[1m"
#define ARCH  "x86_64"

typedef struct {
    const char *name;
    const char *title;
    const char *label;
    const char *value;
    const char *line;
    const char *ascii;
} theme_t;

static theme_t themes[] = {
    {"default", "\033[32m", "\033[33m", "\033[32m", "\033[35m", "\033[36m"},
    {"gruvbox", "\033[38;5;142m", "\033[38;5;214m", "\033[38;5;223m", "\033[38;5;172m", "\033[38;5;167m"},
    {"nord",    "\033[38;5;81m",  "\033[38;5;110m", "\033[38;5;189m", "\033[38;5;67m",  "\033[38;5;117m"},
    {"dracula", "\033[38;5;212m", "\033[38;5;141m", "\033[38;5;255m", "\033[38;5;97m",  "\033[38;5;207m"},
    {"catppuccin", "\033[38;5;218m", "\033[38;5;111m", "\033[38;5;225m", "\033[38;5;183m", "\033[38;5;175m"}
};

static theme_t *theme = &themes[2];

static char hostname[64];
static char os_name[64];
static char os_version[64];
static char os_codename[64];
static char pretty_name[128];

static char mem_total[64];
static char mem_free[64];
static char heap_total[64];
static char heap_free[64];

static char cpu_vendor[64];
static char cpu_model[128];

static char art[2048];

static void read_file(const char *path, char *buf, int max)
{
    int fd = sys_open(path);
    if (fd < 0) {
        buf[0] = 0;
        return;
    }
    int64_t n = sys_fread(fd, buf, max - 1);
    if (n < 0) n = 0;
    buf[n] = 0;
    if (n > 0 && buf[n-1] == '\n')
        buf[n-1] = 0;
    sys_close(fd);
}

static void get_os_field(const char *key, char *out, int max)
{
    char buf[512];
    read_file("/etc/os-release", buf, sizeof(buf));
    char *p = buf;
    while (*p) {
        char *eq = strchr(p, '=');
        if (!eq) break;
        *eq = 0;
        char *k = p;
        char *v = eq + 1;
        char *nl = strchr(v, '\n');
        if (nl) *nl = 0;
        if (!strcmp(k, key)) {
            if (v[0] == '"') {
                v++;
                char *end = strchr(v, '"');
                if (end) *end = 0;
            }
            strncpy(out, v, max-1);
            out[max-1] = 0;
            return;
        }
        p = nl ? nl+1 : NULL;
    }
    out[0] = 0;
}

static void set_theme(const char *name)
{
    for (size_t i = 0; i < sizeof(themes)/sizeof(theme_t); i++) {
        if (!strcmp(themes[i].name, name)) {
            theme = &themes[i];
            return;
        }
    }
}

static void print_help(void)
{
    printf("fetch - system info tool\n\n");
    printf("Usage:\n");
    printf("  fetch [options]\n\n");
    printf("Options:\n");
    printf("  --theme <name>   set theme\n");
    printf("  --list-themes    list themes\n");
    printf("  --help           show help\n\n");
    printf("Themes:\n");
    for (size_t i = 0; i < sizeof(themes)/sizeof(theme_t); i++)
        printf("  %s\n", themes[i].name);
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--theme")) {
            if (i+1 < argc) {
                set_theme(argv[i+1]);
                i++;
            }
        } else if (!strcmp(argv[i], "--list-themes")) {
            for (size_t j = 0; j < sizeof(themes)/sizeof(theme_t); j++)
                printf("%s\n", themes[j].name);
            exit(0);
        } else if (!strcmp(argv[i], "--help")) {
            print_help();
            exit(0);
        }
    }

    get_os_field("NAME", os_name, sizeof(os_name));
    get_os_field("VERSION", os_version, sizeof(os_version));
    get_os_field("CODENAME", os_codename, sizeof(os_codename));
    get_os_field("PRETTY_NAME", pretty_name, sizeof(pretty_name));

    read_file("/etc/hostname", hostname, sizeof(hostname));

    read_file("/proc/mem_total", mem_total, sizeof(mem_total));
    read_file("/proc/mem_free", mem_free, sizeof(mem_free));
    read_file("/proc/heap_total", heap_total, sizeof(heap_total));
    read_file("/proc/heap_free", heap_free, sizeof(heap_free));

    read_file("/proc/cpu_vendor", cpu_vendor, sizeof(cpu_vendor));
    read_file("/proc/cpu_model", cpu_model, sizeof(cpu_model));

    read_file("/tmp/silex_kernel.txt", art, sizeof(art));

    printf("\n");
    printf("%s%s%s\n", theme->ascii, art, RESET);

    printf(BOLD "%s%s" RESET "@" BOLD "%s%s %s" RESET "\n",
           theme->title, hostname,
           theme->value, os_name, os_version);

    printf("%s----------------%s\n", theme->line, RESET);

    // OS: Kairo x86_64
    printf("%sOS:%s        %s %s\n", theme->label, RESET, os_name, ARCH);
    printf("%sKernel:%s    Silex %s\n", theme->label, RESET, os_version);
    printf("%sCodename:%s  %s\n", theme->label, RESET, os_codename);
    printf("%sHost:%s      %s\n", theme->label, RESET, hostname);

    printf("%sMemory:%s    %s total / %s%s free%s\n",
           theme->label, RESET,
           mem_total, theme->value, mem_free, RESET);

    printf("%sHeap:%s      %s total / %s%s free%s\n",
           theme->label, RESET,
           heap_total, theme->value, heap_free, RESET);

    printf("%sCPU:%s       %s\n", theme->label, RESET, cpu_model);
    printf("%sVendor:%s    %s\n", theme->label, RESET, cpu_vendor);

    printf("\n");
    return 0;
}
