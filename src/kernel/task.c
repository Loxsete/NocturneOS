#include <kernel/task.h>
#include <lib/kmalloc.h>
#include <lib/kstring.h>

extern void arch_task_switch(uint64_t *old_rsp, uint64_t new_rsp);

task_t *current_task = NULL;

static task_t *task_list = NULL;

static void task_add(task_t *task)
{
    if (!task_list) {
        task_list = task;
        task->next = task;
        return;
    }

    task_t *t = task_list;

    while (t->next != task_list)
        t = t->next;

    t->next = task;
    task->next = task_list;
}

void task_init(void)
{
    current_task = NULL;
}

static void task_bootstrap(void)
{
    void (*entry)(void);

    __asm__ volatile(
        "pop %0"
        : "=r"(entry)
    );

    entry();

    while (1)
        task_yield();
}

task_t *task_create(void (*entry)(void))
{
    task_t *task = kmalloc(sizeof(task_t));

    kmemset(task, 0, sizeof(task_t));

    task->stack = kmalloc(TASK_STACK_SIZE);

    uint64_t *stack =
        (uint64_t *)(task->stack + TASK_STACK_SIZE);


    stack = (uint64_t *)((uint64_t)stack & ~0xFULL);


    *--stack = (uint64_t)entry;

    task->rsp = (uint64_t)stack;

    task->state = TASK_READY;

    task_add(task);

    return task;
}


void task_switch(task_t *next)
{
    task_t *prev = current_task;

    current_task = next;

    next->state = TASK_RUNNING;

    if (!prev) {
        uint64_t dummy = 0;
        arch_task_switch(&dummy, next->rsp);
    } else {
        arch_task_switch(&prev->rsp, next->rsp);
    }
}


void task_yield(void)
{
    if (!current_task)
        return;

    task_t *next = current_task->next;

    while (next->state != TASK_READY &&
           next != current_task)
    {
        next = next->next;
    }

    if (next == current_task)
        return;

    current_task->state = TASK_READY;

    task_switch(next);
}
