.section __TEXT,__text

.globl _main
.p2align 2
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    adrp x0, _str0@PAGE
    add x0, x0, _str0@PAGEOFF
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    bl _printf
    mov w0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

.section __DATA,__data
.section __TEXT,__cstring
_str0:
    .asciz "Hello, World!\n"
