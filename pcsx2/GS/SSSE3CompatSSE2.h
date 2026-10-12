#pragma once
// PCSX2 2.8.2 Fase 3. Four selected SSSE3 operations expressed in SSE2.
// Exploratory compatibility code; not optimized; not a full PCSX2 port.
#include <emmintrin.h>

namespace pcsx2_sse2_port {

// PSHUFB, 16 byte lookup, low nibble selects src, high bit zeroes output.
// Deliberate SSE2 fallback; optimization of this costly implementation is future work.
static inline __m128i shuffle_epi8(__m128i a, __m128i control) {
    alignas(16) unsigned char source[16];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(source), a);
    const __m128i indices = _mm_and_si128(control, _mm_set1_epi8(0x0f));
    const __m128i enabled = _mm_cmpeq_epi8(
        _mm_and_si128(control, _mm_set1_epi8((char)0x80)), _mm_setzero_si128());
    __m128i output = _mm_setzero_si128();
    for (int i = 0; i < 16; ++i) {
        const __m128i matching = _mm_and_si128(enabled, _mm_cmpeq_epi8(indices, _mm_set1_epi8((char)i)));
        output = _mm_or_si128(output, _mm_and_si128(matching, _mm_set1_epi8((char)source[i])));
    }
    return output;
}

// PALIGNR: concatenation [b low 16B][a high 16B] shifted right by count bytes.
template<int count>
static inline __m128i alignr_epi8(__m128i a, __m128i b) {
    static_assert(count >= 0 && count <= 255, "PALIGNR count must be a byte");
    if constexpr (count == 0) return b;
    else if constexpr (count < 16) return _mm_or_si128(_mm_srli_si128(b, count), _mm_slli_si128(a, 16 - count));
    else if constexpr (count == 16) return a;
    else if constexpr (count < 32) return _mm_srli_si128(a, count - 16);
    else return _mm_setzero_si128();
}

// PABSD: absolute signed 32-bit ints, wrap INT_MIN to INT_MIN.
static inline __m128i abs_epi32(__m128i a) {
    const __m128i negative = _mm_cmpgt_epi32(_mm_setzero_si128(), a);
    return _mm_sub_epi32(_mm_xor_si128(a,negative), negative);
}

// PMULHRSW: rounded signed high multiply, truncate to 16 bits (not saturation).
static inline __m128i mulhrs_epi16(__m128i a, __m128i b) {
    const __m128i low = _mm_mullo_epi16(a,b);
    const __m128i high = _mm_mulhi_epi16(a,b);
    const __m128i low15 = _mm_and_si128(low, _mm_set1_epi16(0x7fff));
    const __m128i rounding = _mm_srli_epi16(
        _mm_cmpgt_epi16(low15, _mm_set1_epi16(0x3fff)), 15);
    return _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(high,1), _mm_srli_epi16(low,15)),rounding);
}

}
