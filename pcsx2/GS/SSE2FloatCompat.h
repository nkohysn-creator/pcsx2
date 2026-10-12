#pragma once
// PCSX2 2.8.2 experimental SSE2 replacement helpers for float-vector *bit* operations.
// No SSSE3, SSE4.1, AVX required. Does NOT implement ROUNDPS / DPPS.
#include <xmmintrin.h>
#include <emmintrin.h>

namespace pcsx2_sse2_port {

// SSE4.1 BLENDPS uses immediate bits to select lanes from b.
template<int mask>
static inline __m128 blend_ps(__m128 a, __m128 b) {
    static_assert(mask >= 0 && mask <= 15, "BLENDPS mask must be 4 bits");
    const __m128 select_b = _mm_castsi128_ps(_mm_setr_epi32(
        (mask & 1) ? -1 : 0, (mask & 2) ? -1 : 0,
        (mask & 4) ? -1 : 0, (mask & 8) ? -1 : 0));
    return _mm_or_ps(_mm_andnot_ps(select_b, a), _mm_and_ps(select_b, b));
}

// SSE4.1 BLENDVPS selects from b when the sign bit of the mask lane is set.
static inline __m128 blendv_ps(__m128 a, __m128 b, __m128 mask) {
    const int bits = _mm_movemask_ps(mask);
    const __m128 select_b = _mm_castsi128_ps(_mm_setr_epi32(
        (bits & 1) ? -1 : 0, (bits & 2) ? -1 : 0,
        (bits & 4) ? -1 : 0, (bits & 8) ? -1 : 0));
    return _mm_or_ps(_mm_andnot_ps(select_b, a), _mm_and_ps(select_b, b));
}

// SSE4.1 INSERTPS with zero mask = 0 (the form used by GSVector4::insert32).
template<int src, int dst>
static inline __m128 insert_ps(__m128 a, __m128 b) {
    static_assert(src >= 0 && src < 4 && dst >= 0 && dst < 4, "invalid INSERTPS lane");
    const __m128 selected = _mm_shuffle_ps(b, b, _MM_SHUFFLE(src, src, src, src));
    return blend_ps<(1 << dst)>(a, selected);
}

// SSE4.1 EXTRACTPS returns the raw 32-bit pattern, not a numeric float conversion.
template<int lane>
static inline int extract_ps(__m128 a) {
    static_assert(lane >= 0 && lane < 4, "invalid EXTRACTPS lane");
    const __m128i bits = _mm_castps_si128(a);
    return _mm_cvtsi128_si32(_mm_srli_si128(bits, lane * 4));
}

} // namespace pcsx2_sse2_port
