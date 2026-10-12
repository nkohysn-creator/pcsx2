#pragma once
// PCSX2 2.8.2 Fase 8 - SSE2-only replacements for ROUNDPS and DPPS.
// Limitations: floating-point exception/NaN payload behavior may differ from SSE4.1.
#include <xmmintrin.h>
#include <emmintrin.h>

namespace pcsx2_sse2_phase8 {

// On inputs with magnitude >= 2^23, every finite binary32 value is already
// an integer. Preserve those lanes and all infinities/NaNs without conversion.
// Uses CVTPS2DQ with MXCSR rounding to match ROUNDPS result bits for finite data.
static inline __m128 round_finite(__m128 safe, unsigned rounding) {
    const __m128i ints = _mm_cvttps_epi32(safe); // Always truncate, independent of MXCSR.
    const __m128 tr = _mm_cvtepi32_ps(ints);     // Exact integer->float for |x| < 2^23.
    const __m128i one = _mm_set1_epi32(1);
    if (rounding == 3) return tr;
    if (rounding == 1) { // Down, floor
        const __m128i below = _mm_castps_si128(_mm_cmplt_ps(safe, tr));
        return _mm_cvtepi32_ps(_mm_sub_epi32(ints, _mm_and_si128(below, one)));
    }
    if (rounding == 2) { // Up, ceil
        const __m128i above = _mm_castps_si128(_mm_cmpgt_ps(safe, tr));
        return _mm_cvtepi32_ps(_mm_add_epi32(ints, _mm_and_si128(above, one)));
    }
    // Round to nearest, ties to even. Avoid changing MXCSR, which also avoids
    // unsafe compiler reordering and expensive global rounding mode changes.
    const __m128 absdiff = _mm_and_ps(_mm_sub_ps(safe, tr),
        _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));
    const __m128 half = _mm_set1_ps(0.5f);
    const __m128i greater = _mm_castps_si128(_mm_cmpgt_ps(absdiff, half));
    const __m128i equal = _mm_castps_si128(_mm_cmpeq_ps(absdiff, half));
    const __m128i odd = _mm_and_si128(ints, one);
    const __m128i change = _mm_and_si128(one, _mm_or_si128(greater, _mm_and_si128(equal, _mm_cmpeq_epi32(odd, one))));
    const __m128i sign = _mm_srai_epi32(_mm_castps_si128(safe), 31);
    const __m128i step = _mm_sub_epi32(_mm_xor_si128(change, sign), sign); // signed +/- 1
    return _mm_cvtepi32_ps(_mm_add_epi32(ints, step));
}

template <int mode>
static inline __m128 round_ps(__m128 a) {
    static_assert((mode & ~15) == 0, "ROUNDPS mode out of range");
    const __m128i bits = _mm_castps_si128(a);
    const __m128i magnitude = _mm_and_si128(bits, _mm_set1_epi32(0x7fffffff));
    const __m128i small = _mm_cmplt_epi32(magnitude, _mm_set1_epi32(0x4b000000));
    const __m128 safe = _mm_and_ps(a, _mm_castsi128_ps(small));
    const unsigned rounding = (mode & 4) ? ((_mm_getcsr() >> 13) & 3) : (mode & 3);
    const __m128 rounded = round_finite(safe, rounding);
    const __m128 signbit = _mm_and_ps(a, _mm_castsi128_ps(_mm_set1_epi32(0x80000000u)));
    const __m128 signed_result = _mm_or_ps(rounded, signbit);
    const __m128 small_mask = _mm_castsi128_ps(small);
    return _mm_or_ps(_mm_and_ps(small_mask, signed_result), _mm_andnot_ps(small_mask, a));
}

// Masked multiply/sum in the order of two horizontal pair sums, then total.
// For ordinary finite inputs follows the DPPS lane masks. Exceptional results
// such as NaNs, +/-Inf and status flags need separate validation at build time.
template <int imm>
static inline __m128 dp_ps(__m128 a, __m128 b) {
    static_assert((imm & ~255) == 0, "DPPS immediate outside 8-bit range");
    const __m128 input_mask = _mm_castsi128_ps(_mm_setr_epi32(
        (imm & 0x10) ? -1 : 0, (imm & 0x20) ? -1 : 0,
        (imm & 0x40) ? -1 : 0, (imm & 0x80) ? -1 : 0));
    const __m128 products = _mm_mul_ps(_mm_and_ps(a, input_mask), _mm_and_ps(b, input_mask));
    const __m128 swapped = _mm_shuffle_ps(products, products, _MM_SHUFFLE(2,3,0,1));
    const __m128 pairs = _mm_add_ps(products, swapped);
    const __m128 swapped_pairs = _mm_shuffle_ps(pairs, pairs, _MM_SHUFFLE(1,0,3,2));
    const __m128 total = _mm_add_ps(pairs, swapped_pairs);
    const __m128 out_mask = _mm_castsi128_ps(_mm_setr_epi32(
        (imm & 1) ? -1 : 0, (imm & 2) ? -1 : 0,
        (imm & 4) ? -1 : 0, (imm & 8) ? -1 : 0));
    return _mm_and_ps(total, out_mask);
}
} // namespace pcsx2_sse2_phase8
