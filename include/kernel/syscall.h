#pragma once
#include <stdint.h>


// System call dispatcher
uint64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3);
