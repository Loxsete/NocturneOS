#set document(title: "Nocturne OS Documentation")


#set page(
  margin: (x: 4em, y: 4em),
)

#set heading(numbering: "1.1")
#show heading.where(level: 1): set align(center)
#show heading.where(level: 1): set block(below: 32pt)
#show heading.where(level: 2): set block(below: 16pt)
#show heading.where(level: 2): set block(below: 16pt)

#show raw.where(block: false): it => (
  " "
    + box(
      fill: rgb("#0001"),
      stroke: 0.5pt + rgb("#0001"),
      outset: (y: 3pt, x: 4pt),
      radius: 2pt,
      it,
    )
    + " "
)


#align(center, title())


#outline()



#pagebreak()
= Memory Management (PMM) <memory-management-pmm>

== Info

*Path:*
- `include/mm/pmm.h`
- `src/mm/pmm.c`

*Info:*\
Bitmap-based physical page frame allocator. The bitmap is statically allocated (128 KiB, enough for 1M pages).
Limine's memmap is used to figure out which regions are actually usable.


== function `void pmm_init(void)`

*Signature:*
```c
void pmm_init(void);
```

*Info:*\
Starts with all pages marked as used, then walks the Limine memory map and frees usable regions.
Also accumulates total RAM across usable + bootloader + kernel sections for reporting.
Prints free/total to the framebuffer on boot.


== function `uint64_t pmm_alloc(void)`

*Signature:*
```c
uint64_t pmm_alloc(void);
```

*Info:*\
Linear scan through the bitmap for the first free page. Marks it used and returns the physical address.
Returns `0` on failure — callers should check this.


== function `uint64_t pmm_alloc_n(uint64_t)`

*Signature:*
```c
uint64_t pmm_alloc_n(uint64_t n);
```

*Info:*\
Same idea as `pmm_alloc` but finds `n` _contiguous_ free pages. Returns the base physical address of the region,
or `0` if nothing fits.


== function `void pmm_free(uint64_t)`

*Signature:*
```c
void pmm_free(uint64_t phys);
```

*Info:*\
Marks the page at `phys` as free. Silent no-op if the address is out of range.


== function `uint64_t pmm_free_pages(void)`

*Signature:*
```c
uint64_t pmm_free_pages(void);
```

*Info:*\
Returns the current count of free pages.


== function `uint64_t pmm_total_pages(void)`

*Signature:*
```c
uint64_t pmm_total_pages(void);
```

*Info:*\
Total pages tracked by the bitmap — not the same as free pages.


== function `uint64_t pmm_total_bytes(void)`

*Signature:*
```c
uint64_t pmm_total_bytes(void);
```

*Info:*\
Returns the total installed RAM in bytes. Used by `/proc/mem`.



#pagebreak()
= Virtual Memory Manager (VMM) <virtual-memory-manager-vmm>

*Path:*
- `include/mm/vmm.h`
- `src/mm/vmm.c`

*Info:*\
4-level page table manager for x86-64. Physical↔virtual translation goes through Limine's HHDM offset.
Intermediate tables are allocated on demand via the PMM.


== function `pml4_t vmm_init(void)`

*Signature:*
```c
pml4_t vmm_init(void);
```

*Info:*\
Builds the initial kernel page table from scratch. Maps the first 4 GiB through HHDM and the kernel image at
its virtual base. Switches to the new PML4 before returning.


== function `void vmm_map(pml4_t, uint64_t, uint64_t, uint64_t)`

*Signature:*
```c
void vmm_map(pml4_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);
```

*Info:*\
Maps one 4 KiB page `virt` → `phys`. Walks (and creates if needed) PDPT → PD → PT, then writes the final PTE.
Common flag combinations:
- `KERNEL_FLAGS` — present + write
- `USER_FLAGS` — present + write + user


== function `void vmm_unmap(pml4_t, uint64_t)`

*Signature:*
```c
void vmm_unmap(pml4_t pml4, uint64_t virt);
```

*Info:*\
Zeroes the PTE for `virt` and fires `invlpg` to flush the TLB entry.


== function `pml4_t vmm_new_space(void)`

*Signature:*
```c
pml4_t vmm_new_space(void);
```

*Info:*\
Allocates a fresh PML4 and copies the upper half (indices 256–511) from the current page table.
This gives a new address space that still shares the kernel mappings.


== function `void vmm_switch(pml4_t)`

*Signature:*
```c
void vmm_switch(pml4_t pml4);
```

*Info:*\
Writes `pml4` into `cr3`. The pointer should be a virtual (HHDM) address — it gets converted to physical internally.


== function `uint64_t vmm_hhdm_offset(void)`

*Signature:*
```c
uint64_t vmm_hhdm_offset(void);
```

*Info:*\
Returns Limine's HHDM offset. Everything that needs `phys_to_virt` goes through this.



#pagebreak()
= Heap Allocator (kmalloc) <heap-allocator-kmalloc>

*Path:*
- `include/lib/kmalloc.h`
- `src/lib/kmalloc.c`

*Info:*\
A basic free-list allocator. Each block has an inline header storing its size and free flag.
Adjacent free blocks get coalesced on every `kfree` call.


== function `void kmalloc_init(void *, uint64_t)`

*Signature:*
```c
void kmalloc_init(void *start, uint64_t size);
```

*Info:*\
Sets up the heap at `start`. Called once from `vfs_setup()` after the VMM is up,
using PMM-allocated pages converted to virtual addresses.


== function `void *kmalloc(uint64_t)`

*Signature:*
```c
void *kmalloc(uint64_t size);
```

*Info:*\
Allocates `size` bytes, aligned to 8. Splits the chosen block if there's enough leftover space.
Returns `NULL` if no free block fits.


== function `void *kzalloc(uint64_t)`

*Signature:*
```c
void *kzalloc(uint64_t size);
```

*Info:*\
`kmalloc` + zero-fill. Use this for structs that need to start clean.


== function `void kfree(void *)`

*Signature:*
```c
void kfree(void *ptr);
```

*Info:*\
Frees the block and runs a single coalescing pass to merge neighboring free blocks.


== function `uint64_t kmalloc_heap_size(void)`

*Signature:*
```c
uint64_t kmalloc_heap_size(void);
```

*Info:*\
Returns the total heap size as passed to `kmalloc_init`.


== function `uint64_t kmalloc_free_size(void)`

*Signature:*
```c
uint64_t kmalloc_free_size(void);
```

*Info:*\
Returns the sum of all currently free block sizes — useful for `/proc/mem`.



#pagebreak()
= Virtual Filesystem (VFS) <virtual-filesystem-vfs>

*Path:*
- `include/fs/vfs.h`
- `src/fs/vfs.c`

*Info:*
A minimal VFS layer. Every node is a `vfs_node_t` with an ops pointer; actual work is delegated to
the filesystem that owns the node. Mount points are tracked in a flat table.


== Core types

```c
typedef struct vfs_node {
    char         name[VFS_MAX_NAME];
    uint32_t     flags;       // VFS_FLAG_FILE | VFS_FLAG_DIR | VFS_FLAG_MOUNTPT
    uint64_t     size;
    void        *fs_data;     // per-filesystem private data
    vfs_ops_t   *ops;
    struct vfs_node *parent;
    struct vfs_node *mount;   // if set, path resolution jumps here
} vfs_node_t;
```
```c
typedef struct {
    int64_t     (*read)   (vfs_node_t*, void*, uint64_t off, uint64_t size);
    int64_t     (*write)  (vfs_node_t*, const void*, uint64_t off, uint64_t size);
    vfs_node_t* (*finddir)(vfs_node_t*, const char *name);
    vfs_node_t* (*readdir)(vfs_node_t*, uint32_t index);
    vfs_node_t* (*mkdir)  (vfs_node_t*, const char *name);
    vfs_node_t* (*mkfile) (vfs_node_t*, const char *name);
    int         (*unlink) (vfs_node_t*, const char *name);
} vfs_ops_t;
```


== function `void vfs_init(void)`

*Signature:*
```c
void vfs_init(void);
```

*Info:*\
Zeroes the mount table. Call this before anything else.


== function `int vfs_mount(const char *, vfs_node_t *)`

*Signature:*
```c
int vfs_mount(const char *path, vfs_node_t *fs_root);
```

*Info:*\
Mounts `fs_root` at `path`. Mounting at `"/"` sets the global root. For everything else,
`vfs_resolve` finds the target node and sets its `mount` pointer.
Returns `0` on success, `-1` if the table is full (`VFS_MAX_MOUNTS`).


== function `vfs_node_t *vfs_resolve(const char *)`

*Signature:*
```c
vfs_node_t *vfs_resolve(const char *path);
```

*Info:*\
Walks the tree component by component, following mount points when it hits them.
Returns `NULL` if any component doesn't exist.


== function `int64_t vfs_read(const char *, void *, uint64_t , uint64_t sze)`

*Signature:*
```c
int64_t vfs_read(const char *path, void *buf, uint64_t offset, uint64_t size);
```

*Info:*\
Shorthand: resolve path, then read. Returns bytes read or `-1`.


== function `int64_t vfs_write(const char *, const void *, uint64_t , uint64_t)`

*Signature:*
```c
int64_t vfs_write(const char *path, const void *buf, uint64_t offset, uint64_t size);
```

*Info:*\
Shorthand: resolve path, then write. Returns bytes written or `-1`.


== function `vfs_node_t *vfs_mkdir(const char *)`

*Signature:*
```c
vfs_node_t *vfs_mkdir(const char *path);
```

*Info:*\
Splits the path into parent + name, resolves the parent, calls `ops->mkdir`.


== function `vfs_node_t *vfs_mkfile(const char *)`

*Signature:*
```c
vfs_node_t *vfs_mkfile(const char *path);
```

*Info:*\
Same as `vfs_mkdir` but for files.


== function `int vfs_unlink(const char *)`

*Signature:*
```c
int vfs_unlink(const char *path);
```

*Info:*\
Resolves the parent directory and calls `ops->unlink` with the file name. Returns `0` or `-1`.


== function `int64_t vnode_read(vfs_node_t *, void *, uint64_t , uint64_t )`

*Signature:*
```c
int64_t vnode_read(vfs_node_t *node, void *buf, uint64_t offset, uint64_t size);
```

*Info:*\
Direct read on an already-resolved node. Returns `-1` if `ops->read` is NULL.

== function `int64_t vnode_write(vfs_node_t *, const void *, uint64_t, uint64_t)`

*Signature:*
```c
int64_t vnode_write(vfs_node_t *node, const void *buf, uint64_t offset, uint64_t size);
```

*Info:*\
Direct write on an already-resolved node. Returns `-1` if `ops->write` is NULL.


== function `vfs_node_t *vnode_finddir(vfs_node_t *node, const char *name)`

*Signature:*
```c
vfs_node_t *vnode_finddir(vfs_node_t *node, const char *name);
```

*Info:*\
Looks up a child by name in a directory node.


== function `vfs_node_t *vnode_readdir(vfs_node_t *, uint32_t )`

*Signature:*
```c
vfs_node_t *vnode_readdir(vfs_node_t *node, uint32_t index);
```

*Info:*\
Returns the `index`-th child of a directory. Used by `SYS_READDIR` to enumerate entries.



#pagebreak()
= TmpFS <tmpfs>

*Path:*
- `include/fs/tmpfs.h`
- `src/fs/tmpfs.c`

*Info:*\
In-memory filesystem. Files store their content in `kmalloc`'d buffers that grow on write.
Directories keep a flat array of child pointers (max `TMPFS_MAX_CHILDREN = 64` per dir).


== function `vfs_node_t *tmpfs_create(void)`

*Signature:*
```c
vfs_node_t *tmpfs_create(void);
```

*Info:*\
Creates a new root directory node for a tmpfs instance. Pass the result to `vfs_mount("/", ...)`.

All directory ops are supported: `finddir`, `readdir`, `mkdir`, `mkfile`, `unlink`.
File write automatically reallocates the buffer if the new size exceeds the current one.



#pagebreak()
= ProcFS <procfs>

*Path:*
- `include/fs/procfs.h`
- `src/fs/procfs.c`

*Info:*\
Read-only virtual filesystem for exposing kernel state.
Each file is backed by a callback that generates its content on read.


== function `vfs_node_t *procfs_create(void)`

*Signature:*
```c
vfs_node_t *procfs_create(void);
```

*Info:*\
Creates the procfs root. Call once, then pass to `vfs_mount("/proc", ...)`.


== function `vfs_node_t *procfs_register(const char *, procfs_read_fn)`

*Signature:*
```c
vfs_node_t *procfs_register(const char *name, procfs_read_fn fn);
```

*Info:*\
Registers a file under `/proc`. The callback signature:

```c
typedef int64_t (*procfs_read_fn)(void *buf, uint64_t max);
```

Write up to `max` bytes into `buf`, return how many you wrote. Limited to `PROCFS_MAX_ENTRIES` (32) files total.


== Built-in procfs files

#table(
  columns: 3,
  "Path", "Function", "Content",
  "/proc/mem", "proc_mem_read", "Total RAM, heap size, heap free",
  "/proc/cpuinfo", "proc_cpu_read", "Vendor string + brand string from CPUID",
)



#pagebreak()
= Ext2 <ext2>

*Path:*
- `include/fs/ext2.h`
- `src/fs/ext2.c`

*Info:*\
Read/write ext2 over ATA PIO. Only direct block pointers (`i_block[0..11`) are supported — no indirect blocks.


== function `vfs_node_t *ext2_mount(int, int)`

*Signature:*
```c
vfs_node_t *ext2_mount(int bus, int drive);
```

*Info:*\
Reads the superblock, checks for magic `0xEF53`, builds an `ext2_fs_t` context,
and returns a VFS node for inode 2 (root). Returns `NULL` if the disk isn't ext2.


== Internal functions

These aren't exposed publicly but are worth knowing if you're working on the driver:

#table(
  columns: 2,
  "Function", "Description",
  `ext2_read_inode`, "Reads an inode from the inode table on disk",
  `ext2_write_inode`, "Writes a modified inode back",
  `ext2_alloc_inode`, "Finds a free inode via bitmap, marks it used",
  `ext2_alloc_block`, "Finds a free block via bitmap, marks it used",
  `ext2_read_inode_data`, "Reads file data through direct block pointers",
  `ext2_dir_lookup`, "Scans a directory block for an entry by name",
  `ext2_dir_add_entry`, "Inserts a new entry into a directory block",
  `ext2_create_file`, "Allocates inode + adds directory entry",
  `ext2_write_file`, "Writes data into inode's first block (overwrites)",
)



#pagebreak()
= InitRAMFS <initramfs>

*Path:*
- `include/fs/initramfs.h`
- `src/fs/initramfs.c`

*Info:*\
CPIO unpacker (newc format). Runs at boot to populate the VFS from an archive embedded directly in the kernel ELF.


== function `int initramfs_unpack(void *, uint64_t, vfs_node_t *)`

*Signature:*
```c
int initramfs_unpack(void *data, uint64_t size, vfs_node_t *root);
```

*Info:*\
Walks the CPIO archive and creates VFS nodes under `root`. Handles both `070701` and `070702` magic.
Creates parent directories as needed, skips `.` and empty entries, stops at `TRAILER!!!`. Always returns `0`.

The archive gets linked into the kernel via `ld -r -b binary` and is
referenced through `_binary_build_initramfs_cpio_start` / `_binary_build_initramfs_cpio_end`.



#pagebreak()
= Kernel Print <kernel-print>

*Path:*
- `include/console/kprint.h`
- `src/console/kprint.c`

*Info:*\
Thin wrappers around the framebuffer renderer. These are for kernel-side output only — userspace goes through syscalls.


== function `void kputs(const char *)`

*Signature:*
```c
void kputs(const char *s);
```

*Info:*\
Prints a string using `COLOR_FG` / `COLOR_BG`.


== function `void kputs_col(const char *, uint32_t)`

*Signature:*
```c
void kputs_col(const char *s, uint32_t fg);
```

*Info:*\
Same as `kputs` but with a custom foreground color. The boot banner uses this.


== function `void kputhex(uint64_t val)`

*Signature:*
```c
void kputhex(uint64_t val);
```

*Info:*\
Prints `val` as `0x` + 16 uppercase hex digits.


== function `void kputdec(uint64_t val)`

*Signature:*
```c
void kputdec(uint64_t val);
```

*Info:*\
Prints `val` in decimal.



#pagebreak()
= String Library <string-library>

*Path:*
- `include/lib/kstring.h`
- `src/lib/kstring.c`
- `include/lib/kitoa.h`
- `src/lib/kitoa.c`

*Info:*\
Standard string/memory ops, reimplemented freestanding. No libc dependency.

#table(
  columns: 3,
  "Function", "Signature", "Description",
  "kstrlen", "int kstrlen(const char *s)", "String length",
  "kstrcmp", "int kstrcmp(const char *a, const char *b)", "Full string compare",
  "kstrncmp", "int kstrncmp(const char *a, const char *b, int n)", "Length-limited compare",
  "kstrcpy", "void kstrcpy(char *dst, const char *src, int max)", "Bounded copy, always null-terminates",
  "kmemcpy", "void kmemcpy(void *dst, const void *src, uint64_t n)", "Byte copy",
  "kmemset", "void kmemset(void *dst, uint8_t val, uint64_t n)", "Byte fill",
  "memset", "void *memset(void *dst, int v, uint64_t n)", "C-compatible alias, returns dst",
)

*`kitoa` / `kitoa_hex` :*
#table(
  columns: 3,
  "Function", "Signature", "Description",
  "kitoa", "void kitoa(uint32_t n, char *buf)", "uint32_t → decimal string",
  "kitoa_hex",
  "void kitoa_hex(uint64_t val, char *buf)",
  "uint64_t → 16-char uppercase hex, no 0x prefix; buf must be ≥ 17 bytes",
)



#pagebreak()
= Kernel API Table (kapi) <kernel-api-table-kapi>

*Path:*
- `include/kernel/kapi.h`
- `src/kernel/kapi.c`

*Info:*\
A structure of function pointers that is passed to ELF functions in userspace by the ELF loader.
Instead of going through system calls for everything, userspace can call kernel procedures directly through this table.
The structure is versioned (the `version` field) so userspace can check for compatibility. Not yet used


== function `kernel_api_t *kapi_get(void)`

*Signature:*
```c
kernel_api_t *kapi_get(void);
```

*Info:*\
Returns a pointer to the single static `kernel_api_t` instance.


== `kernel_api_t` fields

#table(
  columns: 3,
  "Field", "Type", "Description",
  "puts", "void (*)(const char*)", "Print string",
  "putchar", "void (*)(char)", "Print one char",
  "puthex", "void (*)(uint64_t)", "Print hex",
  "putdec", "void (*)(uint64_t)", "Print decimal",
  "getchar", "char (*)(void)", "Blocking keyboard read",
  "malloc", "void *(*)(uint64_t)", "Heap alloc",
  "free", "void (*)(void*)", "Heap free",
  "strlen", "int (*)(const char*)", "String length",
  "strcmp", "int (*)(const char*, const char*)", "String compare",
  "strcpy", "void (*)(char*, const char*, int)", "Bounded string copy",
  "memcpy", "void (*)(void*, const void*, uint64_t)", "Memory copy",
  "memset", "void (*)(void*, uint8_t, uint64_t)", "Memory fill",
  "vfs_read", "int64_t (*)(const char*, void*, uint64_t, uint64_t)", "Read file by path",
  "vfs_write", "int64_t (*)(const char*, const void*, uint64_t, uint64_t)", "Write file by path",
  "vfs_open", "void *(*)(const char*)", "Resolve path, get node pointer",
  "vfs_node_read", "int64_t (*)(void*, void*, uint64_t, uint64_t)", "Read from an open node",
  "version", "uint32_t", "API version, currently 1",
)



#pagebreak()
= Syscall Dispatch <syscall-dispatch>

*Path:*
- `include/kernel/syscall.h`
- `src/kernel/syscall.c`

*Info:*\
Triggered by `int 0x80` from userspace (vector 128). The ISR saves registers and
calls `syscall_dispatch` with the syscall number from `rax` and arguments from `rdi`, `rsi`, `rdx`.


== function `uint64_t syscall_dispatch(uint64_t, uint64_t, uint64_t, uint64_t)`

*Signature:*
```c
uint64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
```

*Info:*\
Big switch on `num`. Return value goes back into the saved `rax` in the interrupt frame.


== Syscall table

#table(
  columns: 4,
  "Constant", "Name", "Arguments", "Return",
  "SYS_PUTCHAR", "Write char", "a1 = char", "0",
  "SYS_PUTS", "Write string", "a1 = const char *", "0",
  "SYS_GETCHAR", "Read char (blocking)", "—", "char",
  "SYS_EXIT", "Halt", "—", "—",
  "SYS_GETS", "Read line", "a1 = buf, a2 = max", "bytes read",
  "SYS_CLEAR", "Clear screen", "—", "0",
  "SYS_READDIR", "Get dir entry by index", "a1 = path, a2 = index, a3 = out buf", "1 dir / 0 file / -1 error",
  "SYS_MKDIR", "Create directory", "a1 = path", "0 / -1",
  "SYS_MKFILE", "Create file", "a1 = path", "0 / -1",
  "SYS_RM", "Remove entry", "a1 = path", "0 / -1",
  "SYS_READ", "Read file", "a1 = path, a2 = buf, a3 = size", "bytes read / -1",
  "SYS_WRITE", "Write file (creates if missing)", "a1 = path, a2 = buf, a3 = size", "bytes written / -1",
)



#pagebreak()
= Architecture: GDT <architecture-gdt>

*Path:*
- `src/arch/x86_64/gdt.c`


== function `void gdt_init(void)`

*Signature:*
```c
void gdt_init(void);
```

*Info:*\
Sets up a 7-entry GDT and loads it. Layout:

#table(
  columns: 3,
  "Index", "Selector", "Description",
  "0", "0x00", "Null",
  "1", "0x08", "Kernel code (ring 0, 64-bit)",
  "2", "0x10", "Kernel data",
  "3", "0x18", "User data (ring 3)",
  "4", "0x20", "User code (ring 3, 64-bit)",
  "5–6", "0x28", "TSS (takes two slots in 64-bit mode)",
)

After `lgdt`, segment registers are reloaded via a far return trick in `gdt_flush_segments`,
then `tss_load(0x28)` loads the TSS selector.



#pagebreak()
= Architecture: IDT <architecture-idt>

*Path:*
- `src/arch/x86_64/idt.c`


== function `void idt_init(void)`

*Signature:*
```c
void idt_init(void);
```

*Info:*\
Installs three IDT entries and calls `lidt`:

#table(
  columns: 4,
  "Vector", "Handler", "DPL", "Purpose",
  "0", "isr0", "0", "CPU exception — prints the vector number and halts",
  "33", "irq1", "0", "PS/2 keyboard (IRQ1, remapped to 0x21 by PIC)",
  "128", "isr128", "3", "Syscall gate — accessible from ring 3",
)


== function `void idt_set(int, void *, uint8_t)`

*Signature:*
```c
void idt_set(int vec, void *isr, uint8_t flags);
```

*Info:*\
Writes a 64-bit gate entry for vector `vec`. `flags` = type + DPL byte, e.g. `0x8E` for a kernel interrupt gate,
`0xEE` for a user-accessible one.



#pagebreak()
= Architecture: TSS <architecture-tss>

*Path:*
- `src/arch/x86_64/tss.c`


== function `void tss_init(void)`

*Signature:*
```c
void tss_init(void);
```

*Info:*\
Allocates a 16 KiB kernel stack via `pmm_alloc_n(4)`, converts it to a virtual address,
and writes the stack top into `kernel_tss.rsp0`. This is the stack the CPU switches to
on any ring-3 → ring-0 transition (syscalls, exceptions). Must be called after both the PMM and GDT are initialized.
