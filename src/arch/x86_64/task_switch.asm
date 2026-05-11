global arch_task_switch

section .text

arch_task_switch:

    ;rdi = &old_rsp
    ;rsi = new_rsp

    ;save current rsp
    mov [rdi], rsp

    ;load next rsp
    mov rsp, rsi

    ret
