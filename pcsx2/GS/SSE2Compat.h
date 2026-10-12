#pragma once
// PCSX2 SSE2 port experiment: selected SSE4.1 integer operations
// Pure SSE2 replacements. Requires MSVC x64 or GCC/Clang x86-64.
// This does NOT make PCSX2 2.8.2 SSE2 compatible by itself.
#include <emmintrin.h>

namespace pcsx2_sse2_port {

// SSE4.1 _mm_blendv_epi8(a,b,mask) uses the high bit of each mask byte.
static inline __m128i blendv_epi8(__m128i a, __m128i b, __m128i mask) {
    const __m128i select_b = _mm_cmpgt_epi8(_mm_setzero_si128(), mask);
    return _mm_or_si128(_mm_andnot_si128(select_b,a), _mm_and_si128(select_b,b));
}

template<int imm8>
static inline __m128i blend_epi16(__m128i a, __m128i b) {
    const __m128i select_b = _mm_setr_epi16(
        (imm8&0x01)?-1:0,(imm8&0x02)?-1:0,(imm8&0x04)?-1:0,(imm8&0x08)?-1:0,
        (imm8&0x10)?-1:0,(imm8&0x20)?-1:0,(imm8&0x40)?-1:0,(imm8&0x80)?-1:0);
    return _mm_or_si128(_mm_andnot_si128(select_b,a), _mm_and_si128(select_b,b));
}

static inline __m128i min_epi32(__m128i a, __m128i b) {
    const __m128i select_b = _mm_cmpgt_epi32(a,b);
    return _mm_or_si128(_mm_andnot_si128(select_b,a), _mm_and_si128(select_b,b));
}
static inline __m128i max_epi32(__m128i a, __m128i b) {
    const __m128i select_a = _mm_cmpgt_epi32(a,b);
    return _mm_or_si128(_mm_and_si128(select_a,a), _mm_andnot_si128(select_a,b));
}
static inline __m128i min_epu32(__m128i a, __m128i b) {
    const __m128i sign = _mm_set1_epi32(static_cast<int>(0x80000000u));
    const __m128i select_b = _mm_cmpgt_epi32(_mm_xor_si128(a,sign),_mm_xor_si128(b,sign));
    return _mm_or_si128(_mm_andnot_si128(select_b,a), _mm_and_si128(select_b,b));
}
static inline __m128i max_epu32(__m128i a, __m128i b) {
    const __m128i sign = _mm_set1_epi32(static_cast<int>(0x80000000u));
    const __m128i select_a = _mm_cmpgt_epi32(_mm_xor_si128(a,sign),_mm_xor_si128(b,sign));
    return _mm_or_si128(_mm_and_si128(select_a,a), _mm_andnot_si128(select_a,b));
}

// Multiply low 32-bit parts, keeping 32-bit product modulo 2^32.
static inline __m128i mullo_epi32(__m128i a, __m128i b) {
    const __m128i even = _mm_mul_epu32(a,b);
    const __m128i odd = _mm_mul_epu32(_mm_srli_epi64(a,32), _mm_srli_epi64(b,32));
    return _mm_unpacklo_epi32(
        _mm_shuffle_epi32(even,_MM_SHUFFLE(2,0,2,0)),
        _mm_shuffle_epi32(odd,_MM_SHUFFLE(2,0,2,0)));
}

template<int lane>
static inline int extract_epi32(__m128i a) {
    static_assert(lane>=0 && lane<4, "lane must be 0..3");
    return _mm_cvtsi128_si32(_mm_srli_si128(a,lane*4));
}
static inline int testz_si128(__m128i a, __m128i b) {
    const __m128i v = _mm_and_si128(a,b);
    const __m128i is_zero = _mm_cmpeq_epi8(v,_mm_setzero_si128());
    return _mm_movemask_epi8(is_zero) == 0xFFFF;
}

} // namespace pcsx2_sse2_port
