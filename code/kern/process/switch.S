// LAB4 功能三：保存顺序必须与 proc.h 中 struct context 的字段顺序一致。
#include <riscv.h>

    .text
    .globl switch_to
    .type switch_to, @function
switch_to:
    # a0 指向旧 context，a1 指向新 context；RV64 每个字段占 8 字节。
    # 保存 ra、sp 和 ABI 规定的 12 个被调用者保存寄存器。
    STORE ra,  0 * REGBYTES(a0)
    STORE sp,  1 * REGBYTES(a0)
    STORE s0,  2 * REGBYTES(a0)
    STORE s1,  3 * REGBYTES(a0)
    STORE s2,  4 * REGBYTES(a0)
    STORE s3,  5 * REGBYTES(a0)
    STORE s4,  6 * REGBYTES(a0)
    STORE s5,  7 * REGBYTES(a0)
    STORE s6,  8 * REGBYTES(a0)
    STORE s7,  9 * REGBYTES(a0)
    STORE s8, 10 * REGBYTES(a0)
    STORE s9, 11 * REGBYTES(a0)
    STORE s10, 12 * REGBYTES(a0)
    STORE s11, 13 * REGBYTES(a0)

    # 从新 context 恢复相同的 14 个寄存器，切换到新线程的栈。
    LOAD ra,  0 * REGBYTES(a1)
    LOAD sp,  1 * REGBYTES(a1)
    LOAD s0,  2 * REGBYTES(a1)
    LOAD s1,  3 * REGBYTES(a1)
    LOAD s2,  4 * REGBYTES(a1)
    LOAD s3,  5 * REGBYTES(a1)
    LOAD s4,  6 * REGBYTES(a1)
    LOAD s5,  7 * REGBYTES(a1)
    LOAD s6,  8 * REGBYTES(a1)
    LOAD s7,  9 * REGBYTES(a1)
    LOAD s8, 10 * REGBYTES(a1)
    LOAD s9, 11 * REGBYTES(a1)
    LOAD s10, 12 * REGBYTES(a1)
    LOAD s11, 13 * REGBYTES(a1)
    # ret 跳转到新 ra；线程首次运行时该地址是 forkret。
    ret
    .size switch_to, .-switch_to
