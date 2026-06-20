// qemu_clock.cpp
//
// Provides a real clock_gettime() for the riscv64-unknown-elf-g++ +
// qemu-riscv64 combination, by issuing the actual Linux clock_gettime
// syscall via `ecall` instead of relying on newlib's version (which
// isn't implemented on this bare-metal target -- that's why it was
// undeclared/unusable before).
//
// qemu-riscv64 is QEMU's USER-MODE emulator: every `ecall` this binary
// executes gets intercepted by QEMU and forwarded to the REAL host
// kernel, using the standard Linux RV64 syscall ABI. So this function
// returns genuine host wall-clock time -- the same CLOCK_MONOTONIC
// source your host build already uses -- which is what makes host-vs-
// RISC-V timing numbers directly comparable. rdcycle is a real CPU
// register, but QEMU explicitly doesn't model it cycle-accurately, so
// it's only good for relative comparisons, not real elapsed time. This
// is the fix for that.
//
// This file ONLY overrides clock_gettime(). It does not touch file I/O --
// load_image()/save_image() still won't work under QEMU. If you want
// real file I/O too, that needs the _open/_read/_write/_close/_lseek
// overrides from the full syscalls file, added as a separate concern.
//
// Build note: just drop this in as src/qemu_clock.cpp. Both SRCS and
// EMBED_SRCS in your Makefile already wildcard over every *.cpp in src/
// (excluding only main.cpp/riscv_main.cpp/the generator), so it gets
// picked up and linked automatically -- no Makefile edit needed.

#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

// RISC-V 64-bit Linux syscall number for clock_gettime (generic syscall
// ABI, shared with arm64 and others -- verified against the standard
// table: openat=56, close=57, lseek=62, read=63, write=64, fstat=80,
// clock_gettime=113).
#define SYS_clock_gettime 113

extern "C" {

// Issues a raw Linux syscall via ecall. QEMU traps this instruction and
// forwards it to the host kernel using the real RV64 calling convention:
// syscall number in a7, arguments in a0-a2 (clock_gettime only needs 2).
static long issue_qemu_syscall(long num, long a0, long a1, long a2)
{
    register long r_num asm("a7") = num;
    register long r_a0  asm("a0") = a0;
    register long r_a1  asm("a1") = a1;
    register long r_a2  asm("a2") = a2;
    asm volatile(
        "ecall"
        : "+r"(r_a0)
        : "r"(r_num), "r"(r_a1), "r"(r_a2)
        : "memory"
    );
    return r_a0;
}

int clock_gettime(int clk_id, struct timespec* tp)
{
    if (!tp) return -1;
    return (int)issue_qemu_syscall(SYS_clock_gettime, (long)clk_id, (long)tp, 0);
}

} // extern "C"
