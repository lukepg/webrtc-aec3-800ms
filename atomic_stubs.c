// ARM LSE atomic stubs for Android 8.0+ compatibility
// These provide fallback implementations using standard ARM atomics
// instead of ARM v8.1 LSE (Large System Extensions) atomics

#include <stdint.h>

// 32-bit atomic operations
uint32_t __aarch64_ldadd4_relax(uint32_t value, uint32_t *ptr) {
    uint32_t result;
    __asm__ __volatile__(
        "1: ldxr %w0, [%2]\n"
        "   add %w1, %w0, %w3\n"
        "   stxr %w1, %w1, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint32_t __aarch64_ldadd4_acq_rel(uint32_t value, uint32_t *ptr) {
    uint32_t result;
    __asm__ __volatile__(
        "1: ldaxr %w0, [%2]\n"
        "   add %w1, %w0, %w3\n"
        "   stlxr %w1, %w1, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint32_t __aarch64_swp4_rel(uint32_t value, uint32_t *ptr) {
    uint32_t result;
    __asm__ __volatile__(
        "1: ldxr %w0, [%2]\n"
        "   stlxr %w1, %w3, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

// 64-bit atomic operations
uint64_t __aarch64_ldadd8_relax(uint64_t value, uint64_t *ptr) {
    uint64_t result;
    __asm__ __volatile__(
        "1: ldxr %0, [%2]\n"
        "   add %1, %0, %3\n"
        "   stxr %w1, %1, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint64_t __aarch64_ldadd8_rel(uint64_t value, uint64_t *ptr) {
    uint64_t result;
    __asm__ __volatile__(
        "1: ldxr %0, [%2]\n"
        "   add %1, %0, %3\n"
        "   stlxr %w1, %1, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint64_t __aarch64_swp8_relax(uint64_t value, uint64_t *ptr) {
    uint64_t result;
    __asm__ __volatile__(
        "1: ldxr %0, [%2]\n"
        "   stxr %w1, %3, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(value)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}
