#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE4_1__)
#include <smmintrin.h>
#endif
#endif

namespace yuvdiff {
namespace simd {

inline uint64_t sq_diff_sum_8bit(const uint8_t* a, const uint8_t* b, size_t n) {
    uint64_t total_sq_diff = 0;
    size_t i = 0;

#if defined(__AVX2__)
    __m256i acc64 = _mm256_setzero_si256();
    const __m256i zero = _mm256_setzero_si256();

    for (; i + 32 <= n; i += 32) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

        // Unpack low 16 bytes to 16-bit
        __m256i va_lo = _mm256_unpacklo_epi8(va, zero);
        __m256i vb_lo = _mm256_unpacklo_epi8(vb, zero);
        __m256i diff_lo = _mm256_sub_epi16(va_lo, vb_lo);
        __m256i sq_lo = _mm256_madd_epi16(diff_lo, diff_lo); // 8x 32-bit sums

        // Unpack high 16 bytes to 16-bit
        __m256i va_hi = _mm256_unpackhi_epi8(va, zero);
        __m256i vb_hi = _mm256_unpackhi_epi8(vb, zero);
        __m256i diff_hi = _mm256_sub_epi16(va_hi, vb_hi);
        __m256i sq_hi = _mm256_madd_epi16(diff_hi, diff_hi); // 8x 32-bit sums

        // Convert 32-bit sums to 64-bit and accumulate
        __m256i sq_lo_64_0 = _mm256_unpacklo_epi32(sq_lo, zero);
        __m256i sq_lo_64_1 = _mm256_unpackhi_epi32(sq_lo, zero);
        __m256i sq_hi_64_0 = _mm256_unpacklo_epi32(sq_hi, zero);
        __m256i sq_hi_64_1 = _mm256_unpackhi_epi32(sq_hi, zero);

        acc64 = _mm256_add_epi64(acc64, sq_lo_64_0);
        acc64 = _mm256_add_epi64(acc64, sq_lo_64_1);
        acc64 = _mm256_add_epi64(acc64, sq_hi_64_0);
        acc64 = _mm256_add_epi64(acc64, sq_hi_64_1);
    }

    alignas(32) uint64_t lanes[4];
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), acc64);
    total_sq_diff += lanes[0] + lanes[1] + lanes[2] + lanes[3];
#endif

    for (; i < n; ++i) {
        int32_t diff = static_cast<int32_t>(a[i]) - static_cast<int32_t>(b[i]);
        total_sq_diff += static_cast<uint64_t>(diff * diff);
    }

    return total_sq_diff;
}

inline uint64_t sq_diff_sum_16bit(const uint16_t* a, const uint16_t* b, size_t n) {
    uint64_t total_sq_diff = 0;
    size_t i = 0;

#if defined(__AVX2__)
    __m256i acc64 = _mm256_setzero_si256();
    const __m256i zero = _mm256_setzero_si256();

    for (; i + 16 <= n; i += 16) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

        __m256i diff = _mm256_sub_epi16(va, vb);
        __m256i sq = _mm256_madd_epi16(diff, diff); // 8x 32-bit sums of pairs

        __m256i sq_64_0 = _mm256_unpacklo_epi32(sq, zero);
        __m256i sq_64_1 = _mm256_unpackhi_epi32(sq, zero);

        acc64 = _mm256_add_epi64(acc64, sq_64_0);
        acc64 = _mm256_add_epi64(acc64, sq_64_1);
    }

    alignas(32) uint64_t lanes[4];
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), acc64);
    total_sq_diff += lanes[0] + lanes[1] + lanes[2] + lanes[3];
#endif

    for (; i < n; ++i) {
        int32_t diff = static_cast<int32_t>(a[i]) - static_cast<int32_t>(b[i]);
        total_sq_diff += static_cast<uint64_t>(diff * diff);
    }

    return total_sq_diff;
}

} // namespace simd
} // namespace yuvdiff
