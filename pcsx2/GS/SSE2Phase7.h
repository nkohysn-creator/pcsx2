#pragma once
// Experimental PCSX2 v2.8.2 phase 7: remaining integer-vector SSE4.1 & SSSE3 helpers.
// All functions below use only SSE2 and accept the same lane semantics as the original intrinsics.
// This does NOT imply a complete PCSX2 port.
#include <emmintrin.h>
#include <stdint.h>

namespace pcsx2_sse2_phase7 {
static inline __m128i choose(__m128i a, __m128i b, __m128i mask) {
  return _mm_or_si128(_mm_andnot_si128(mask,a),_mm_and_si128(mask,b));
}
static inline __m128i min_epi8(__m128i a, __m128i b) {return choose(a,b,_mm_cmpgt_epi8(a,b));}
static inline __m128i max_epi8(__m128i a, __m128i b) {return choose(b,a,_mm_cmpgt_epi8(a,b));}
static inline __m128i min_epu16(__m128i a, __m128i b) {
  const __m128i sign=_mm_set1_epi16((short)0x8000u);
  return choose(a,b,_mm_cmpgt_epi16(_mm_xor_si128(a,sign),_mm_xor_si128(b,sign)));
}
static inline __m128i max_epu16(__m128i a, __m128i b) {
  const __m128i sign=_mm_set1_epi16((short)0x8000u);
  return choose(b,a,_mm_cmpgt_epi16(_mm_xor_si128(a,sign),_mm_xor_si128(b,sign)));
}
static inline __m128i min_epi32(__m128i a,__m128i b) {return choose(a,b,_mm_cmpgt_epi32(a,b));}
static inline __m128i max_epi32(__m128i a,__m128i b) {return choose(b,a,_mm_cmpgt_epi32(a,b));}
static inline __m128i packus_epi32(__m128i a, __m128i b) {
  const __m128i zero=_mm_setzero_si128(), maxv=_mm_set1_epi32(65535);
  const __m128i shift=_mm_set1_epi32(32768);
  a=_mm_sub_epi32(min_epi32(max_epi32(a,zero),maxv),shift);
  b=_mm_sub_epi32(min_epi32(max_epi32(b,zero),maxv),shift);
  return _mm_xor_si128(_mm_packs_epi32(a,b),_mm_set1_epi16((short)0x8000u));
}
static inline __m128i cvtepu8_epi16(__m128i a) {return _mm_unpacklo_epi8(a,_mm_setzero_si128());}
static inline __m128i cvtepu16_epi32(__m128i a) {return _mm_unpacklo_epi16(a,_mm_setzero_si128());}
static inline __m128i cvtepu32_epi64(__m128i a) {return _mm_unpacklo_epi32(a,_mm_setzero_si128());}
static inline __m128i cvtepi8_epi16(__m128i a) {return _mm_unpacklo_epi8(a,_mm_cmpgt_epi8(_mm_setzero_si128(),a));}
static inline __m128i cvtepi16_epi32(__m128i a) {return _mm_unpacklo_epi16(a,_mm_cmpgt_epi16(_mm_setzero_si128(),a));}
static inline __m128i cvtepi32_epi64(__m128i a) {return _mm_unpacklo_epi32(a,_mm_srai_epi32(a,31));}
static inline __m128i cvtepi8_epi32(__m128i a) {return cvtepi16_epi32(cvtepi8_epi16(a));}
static inline __m128i cvtepu8_epi32(__m128i a) {return cvtepu16_epi32(cvtepu8_epi16(a));}
static inline __m128i cvtepi8_epi64(__m128i a) {return cvtepi32_epi64(cvtepi8_epi32(a));}
static inline __m128i cvtepi16_epi64(__m128i a) {return cvtepi32_epi64(cvtepi16_epi32(a));}
static inline __m128i cvtepu16_epi64(__m128i a) {return cvtepu32_epi64(cvtepu16_epi32(a));}
static inline __m128i cmpeq_epi64(__m128i a,__m128i b) {
  __m128i eq=_mm_cmpeq_epi32(a,b);
  return _mm_and_si128(eq,_mm_shuffle_epi32(eq,_MM_SHUFFLE(2,3,0,1)));
}
static inline __m128i hadd_epi32(__m128i a,__m128i b) {
  __m128i aa=_mm_add_epi32(_mm_shuffle_epi32(a,_MM_SHUFFLE(3,1,2,0)),_mm_shuffle_epi32(a,_MM_SHUFFLE(2,0,3,1)));
  __m128i bb=_mm_add_epi32(_mm_shuffle_epi32(b,_MM_SHUFFLE(3,1,2,0)),_mm_shuffle_epi32(b,_MM_SHUFFLE(2,0,3,1)));
  return _mm_unpacklo_epi64(aa,bb);
}
static inline __m128i hsub_epi32(__m128i a,__m128i b) {
  __m128i aa=_mm_sub_epi32(_mm_shuffle_epi32(a,_MM_SHUFFLE(3,1,2,0)),_mm_shuffle_epi32(a,_MM_SHUFFLE(2,0,3,1)));
  __m128i bb=_mm_sub_epi32(_mm_shuffle_epi32(b,_MM_SHUFFLE(3,1,2,0)),_mm_shuffle_epi32(b,_MM_SHUFFLE(2,0,3,1)));
  return _mm_unpacklo_epi64(aa,bb);
}
static inline __m128i hadds_epi16(__m128i a,__m128i b) {
  return _mm_packs_epi32(_mm_madd_epi16(a,_mm_set1_epi16(1)),_mm_madd_epi16(b,_mm_set1_epi16(1)));
}
template<int lane> static inline __m128i insert_epi8(__m128i a,int v) {
  static_assert(lane>=0 && lane<16, "byte lane out of range");
  const __m128i mask=_mm_setr_epi8(
    lane==0?-1:0,lane==1?-1:0,lane==2?-1:0,lane==3?-1:0,
    lane==4?-1:0,lane==5?-1:0,lane==6?-1:0,lane==7?-1:0,
    lane==8?-1:0,lane==9?-1:0,lane==10?-1:0,lane==11?-1:0,
    lane==12?-1:0,lane==13?-1:0,lane==14?-1:0,lane==15?-1:0);
  return choose(a,_mm_set1_epi8((char)v),mask);
}
template<int lane> static inline int extract_epi8(__m128i a) {
  static_assert(lane>=0 && lane<16, "byte lane out of range");
  return _mm_cvtsi128_si32(_mm_srli_si128(a,lane)) & 0xff;
}
template<int lane> static inline __m128i insert_epi32(__m128i a,int v) {
  static_assert(lane>=0 && lane<4, "int32 lane out of range");
  const __m128i mask=_mm_setr_epi32(lane==0?-1:0,lane==1?-1:0,lane==2?-1:0,lane==3?-1:0);
  return choose(a,_mm_set1_epi32(v),mask);
}
template<int lane> static inline __m128i insert_epi64(__m128i a,long long v) {
  static_assert(lane>=0 && lane<2, "int64 lane out of range");
  const __m128i mask=_mm_set_epi64x(lane==1?-1LL:0LL,lane==0?-1LL:0LL);
  return choose(a,_mm_set1_epi64x(v),mask);
}
template<int lane> static inline long long extract_epi64(__m128i a) {
  static_assert(lane>=0 && lane<2, "int64 lane out of range");
  return _mm_cvtsi128_si64(_mm_srli_si128(a,lane*8));
}
} // namespace pcsx2_sse2_phase7
