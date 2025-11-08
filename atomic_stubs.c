// ARM LSE atomic stubs for Android 8.0+ compatibility
// These provide fallback implementations using standard ARM atomics
// instead of ARM v8.1 LSE (Large System Extensions) atomics

#include <stdint.h>

// 32-bit atomic operations
uint32_t __aarch64_ldadd4_relax(uint32_t value, uint32_t *ptr) {
    uint32_t result, tmp, status;
    __asm__ __volatile__(
        "1: ldxr %w0, [%3]\n"
        "   add %w1, %w0, %w4\n"
        "   stxr %w2, %w1, [%3]\n"
        "   cbnz %w2, 1b\n"
        : "=&r"(result), "=&r"(tmp), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint32_t __aarch64_ldadd4_acq_rel(uint32_t value, uint32_t *ptr) {
    uint32_t result, tmp, status;
    __asm__ __volatile__(
        "1: ldaxr %w0, [%3]\n"
        "   add %w1, %w0, %w4\n"
        "   stlxr %w2, %w1, [%3]\n"
        "   cbnz %w2, 1b\n"
        : "=&r"(result), "=&r"(tmp), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint32_t __aarch64_swp4_rel(uint32_t value, uint32_t *ptr) {
    uint32_t result, status;
    __asm__ __volatile__(
        "1: ldxr %w0, [%2]\n"
        "   stlxr %w1, %w3, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

// 64-bit atomic operations
uint64_t __aarch64_ldadd8_relax(uint64_t value, uint64_t *ptr) {
    uint64_t result, tmp;
    uint32_t status;
    __asm__ __volatile__(
        "1: ldxr %0, [%3]\n"
        "   add %1, %0, %4\n"
        "   stxr %w2, %1, [%3]\n"
        "   cbnz %w2, 1b\n"
        : "=&r"(result), "=&r"(tmp), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint64_t __aarch64_ldadd8_rel(uint64_t value, uint64_t *ptr) {
    uint64_t result, tmp;
    uint32_t status;
    __asm__ __volatile__(
        "1: ldxr %0, [%3]\n"
        "   add %1, %0, %4\n"
        "   stlxr %w2, %1, [%3]\n"
        "   cbnz %w2, 1b\n"
        : "=&r"(result), "=&r"(tmp), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

uint64_t __aarch64_swp8_relax(uint64_t value, uint64_t *ptr) {
    uint64_t result;
    uint32_t status;
    __asm__ __volatile__(
        "1: ldxr %0, [%2]\n"
        "   stxr %w1, %3, [%2]\n"
        "   cbnz %w1, 1b\n"
        : "=&r"(result), "=&r"(status)
        : "r"(ptr), "r"(value)
        : "memory", "cc"
    );
    return result;
}

// Compare-and-swap operations
uint64_t __aarch64_cas8_acq_rel(uint64_t expected, uint64_t desired, uint64_t *ptr) {
    uint64_t result;
    uint32_t status;
    __asm__ __volatile__(
        "1: ldaxr %0, [%3]\n"
        "   cmp %0, %4\n"
        "   b.ne 2f\n"
        "   stlxr %w1, %5, [%3]\n"
        "   cbnz %w1, 1b\n"
        "2:\n"
        : "=&r"(result), "=&r"(status)
        : "r"(expected), "r"(ptr), "r"(expected), "r"(desired)
        : "memory", "cc"
    );
    return result;
}

// Compiler intrinsic: 128-bit left shift
// __int128 __ashlti3(__int128 a, int b)
typedef struct {
    uint64_t low;
    uint64_t high;
} uint128_t;

uint128_t __ashlti3(uint128_t a, int shift) {
    uint128_t result;

    if (shift == 0) {
        return a;
    }

    if (shift >= 128) {
        result.low = 0;
        result.high = 0;
        return result;
    }

    if (shift >= 64) {
        // Shift more than 64 bits
        result.low = 0;
        result.high = a.low << (shift - 64);
    } else {
        // Shift less than 64 bits
        result.low = a.low << shift;
        result.high = (a.high << shift) | (a.low >> (64 - shift));
    }

    return result;
}
