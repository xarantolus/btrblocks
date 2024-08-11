// -------------------------------------------------------------------------------------
#include "OneValue.hpp"
// -------------------------------------------------------------------------------------
#include "common/SIMD.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::doubles {
// -------------------------------------------------------------------------------------
double OneValue::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue::compress(const DOUBLE* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       DoubleStats& stats,
                       u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<OneValueStructure*>(dest);
  if (src != nullptr) {
    col_struct.one_value = stats.distinct_values.begin()->first;
  } else {
    col_struct.one_value = NULL_CODE;
  }
  return sizeof(DOUBLE);
}
// -------------------------------------------------------------------------------------

#if defined(__aarch64__)
__attribute__((target_clones("default", "+sve2", "+sve"))) inline void
decompress_sve_loop(DOUBLE* dest, const OneValueStructure& col_struct, u32 tuple_count) {
#pragma GCC ivdep
#pragma clang loop vectorize(assume_safety) vectorize_width(scalable) interleave(enable)
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.one_value;
  }
}
#endif

inline void decompress_non_sve_loop(DOUBLE* dest,
                                    const OneValueStructure& col_struct,
                                    u32 tuple_count) {
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.one_value;
  }
}
void OneValue::decompress(DOUBLE* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  const auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  BTR_IFELSEARM_SVE(
      { decompress_sve_loop(dest, col_struct, tuple_count); },
      { decompress_non_sve_loop(dest, col_struct, tuple_count); });
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::doubles
// -------------------------------------------------------------------------------------
