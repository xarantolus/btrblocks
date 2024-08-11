#include "OneValue.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
double OneValue::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<OneValueStructure*>(dest);
  if (src != nullptr) {
    col_struct.one_value = stats.distinct_values.begin()->first;
  } else {
    col_struct.one_value = NULL_CODE;
  }
  return sizeof(UINTEGER);
}
// -------------------------------------------------------------------------------------
#if defined(__aarch64__)
__attribute__((target_clones("+sve2", "+sve", "default"))) inline void
decompress_sve_loop(INTEGER* dest, const OneValueStructure& col_struct, u32 tuple_count) {
#pragma GCC ivdep
#pragma clang loop vectorize(assume_safety) vectorize_width(scalable) interleave(enable)
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.one_value;
  }
}
#endif

inline void decompress_non_sve_loop(INTEGER* dest,
                                    const OneValueStructure& col_struct,
                                    u32 tuple_count) {
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.one_value;
  }
}

void OneValue::decompress(INTEGER* dest,
                          BitmapWrapper*,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) {
  const auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  BTR_IFELSEARM_SVE(
      { decompress_sve_loop(dest, col_struct, tuple_count); },
      { decompress_non_sve_loop(dest, col_struct, tuple_count); });
}
// -------------------------------------------------------------------------------------
INTEGER OneValue::lookup(u32) {
  UNREACHABLE();
}
void OneValue::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
