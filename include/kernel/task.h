#pragma once

#include <stdint.h>

#define TASK_STACK_SIZE 8192

typedef enum {
    TASK_READY,
    TASK_RUNNING
} task_state_t;

typedef struct task {
    uint64_t rsp;

    uint8_t *stack;

    task_state_t state;

    struct task *next;
} task_t;

extern task_t *current_task;

void task_init(void);

task_t *task_create(void (*entry)(void));

void task_yield(void);

void task_switch(task_t *next);