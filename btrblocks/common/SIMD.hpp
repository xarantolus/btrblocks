#pragma once

// ------------------------------- Not Using SIMD -------------------------------
#if defined(BTR_FLAG_NO_SIMD) and BTR_FLAG_NO_SIMD
// ------------------------------------------------------------------------------

#undef BTR_USE_SIMD
#define BTR_IFSIMD(x...)
#define BTR_IFELSESIMD(a, b) b
#define SIMD_EXTRA_BYTES 0
#define SIMD_EXTRA_ELEMENTS(TYPE) 0
#define SVE_ENABLED false
// -------------------------------- Using SIMD ----------------------------------
#else  // USE_SIMD
// ------------------------------------------------------------------------------

#if (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#define SVE_ENABLED false
#elif defined(__aarch64__)
#include <simde/x86/avx512.h>
// There are some places where we might want to use SVE instead - check if normal if(SVE_ENABLED)
// Note that this should be done as runtime check
#include <arm_sve.h>
#include <sys/auxv.h>
#define SVE_ENABLED (getauxval(AT_HWCAP) & HWCAP_SVE)
#endif

#define BTR_IFSIMD(x...) x
#define BTR_IFELSESIMD(a, b) a
#define BTR_USE_SIMD 1

// SIMD instruction can become faster when they are allowed to make writes out
// of bounds. This spares us any out of bound checks and therefore many
// branches. The extra data simply gets overwritten or ignored.
#define SIMD_EXTRA_BYTES (sizeof(__m256i) * 4)
#define SIMD_EXTRA_ELEMENTS(TYPE) (SIMD_EXTRA_BYTES / sizeof(TYPE))

#endif  // BTR_FLAG_NO_SIMD
