#pragma once
#include <arm_sve.h>
#include <cassert>
#include <cstdint>
#include "common/SIMD.hpp"
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks {
struct RLEStructure {
  u32 runs_count;
  u32 runs_count_offset;
  u8 values_scheme_code;
  u8 counts_scheme_code;
  u8 data[];
};

#if defined(__aarch64__)
// RLE.hpp
template <typename elem_type>
__attribute__((target("+sve"))) inline uint64_t sve_vector_width();

template <typename int_type>
size_t compress_len(INTEGER* out_lengths,
                    int_type* out_values,
                    const int_type* data,
                    const INTEGER N);

extern template size_t compress_len(INTEGER* out_lengths,
                                    s32* out_values,
                                    const s32* data,
                                    const INTEGER N);
extern template size_t compress_len(INTEGER* out_lengths,
                                    DOUBLE* out_values,
                                    const DOUBLE* data,
                                    const INTEGER N);

__attribute__((target("+sve"))) size_t compress_sve(INTEGER* out_lengths,
                                                    INTEGER* out_values,
                                                    const INTEGER* data,
                                                    const INTEGER N);

__attribute__((target("+sve"))) size_t compress_sve(INTEGER* out_lengths,
                                                    DOUBLE* out_values,
                                                    const DOUBLE* data,
                                                    const INTEGER N);
#endif

// -------------------------------------------------------------------------------------
template <typename NumberType, typename SchemeType, typename StatsType, typename SchemeCodeType>
class TRLE {
 public:
  // -------------------------------------------------------------------------------------
  static inline u32 compressColumn(const NumberType* src,
                                   const BITMAP* nullmap,
                                   u8* dest,
                                   StatsType& stats,
                                   u8 allowed_cascading_level,
                                   u8 force_values = autoScheme(),
                                   u8 force_counts = autoScheme()) {
    auto& col_struct = *reinterpret_cast<RLEStructure*>(dest);
    // -------------------------------------------------------------------------------------
    std::vector<NumberType> rle_values;
    rle_values.resize(stats.run_count);
    std::vector<INTEGER> rle_count;
    rle_count.resize(stats.run_count);
    // -------------------------------------------------------------------------------------
    // RLE encoding
    BTR_IFELSEARM_SVE(
        {
          // TODO: Is stats only the sampling, or also for the full compression? if yes, we can't
          // rely on null_count or run_count etc.
          if (nullmap == nullptr || stats.null_count) {
            // arm_sve_len is better when there are many smaller runs, otherwise
            // compress_len is better
            if (stats.average_run_length < sve_vector_width<NumberType>()) {
              // Probably better to use the implementation that handles compress better
              assert(compress_sve(rle_count.data(), rle_values.data(), src, stats.tuple_count) ==
                     stats.run_count);
            } else {
              assert(compress_len(rle_count.data(), rle_values.data(), src, stats.tuple_count) ==
                     stats.run_count);
            }
          } else {
            NumberType last_item = src[0];
            INTEGER count = 1;
            for (uint32_t row_i = 1; row_i < stats.tuple_count; row_i++) {
              if (src[row_i] == last_item ||
                  (nullmap != nullptr && !nullmap[row_i])) {  // the null trick brought
                                                              // no compression benefits
                count++;
              } else {
                rle_count.push_back(count);
                rle_values.push_back(last_item);
                last_item = src[row_i];
                count = 1;
              }
            }
            rle_count.push_back(count);
            rle_values.push_back(last_item);
          }
        },
        {
          NumberType last_item = src[0];
          INTEGER count = 1;
          for (uint32_t row_i = 1; row_i < stats.tuple_count; row_i++) {
            if (src[row_i] == last_item ||
                (nullmap != nullptr && !nullmap[row_i])) {  // the null trick brought
                                                            // no compression benefits
              count++;
            } else {
              rle_count.push_back(count);
              rle_values.push_back(last_item);
              last_item = src[row_i];
              count = 1;
            }
          }
          rle_count.push_back(count);
          rle_values.push_back(last_item);
        });
    // -------------------------------------------------------------------------------------
    col_struct.runs_count = rle_count.size();
    die_if(rle_count.size() == rle_values.size());
    // -------------------------------------------------------------------------------------
    auto write_ptr = col_struct.data;
    // -------------------------------------------------------------------------------------
    // Compress values
    {
      u32 used_space;
      CSchemePicker<NumberType, SchemeType, StatsType, SchemeCodeType>::compress(
          rle_values.data(), nullptr, write_ptr, rle_values.size(), allowed_cascading_level - 1,
          used_space, col_struct.values_scheme_code, autoScheme(), "values");
      write_ptr += used_space;
      // -------------------------------------------------------------------------------------
      Log::debug("X_RLE: values_c = {} values_s = {}", CI(col_struct.values_scheme_code),
                 CI(used_space));
      // -------------------------------------------------------------------------------------
    }
    // -------------------------------------------------------------------------------------
    // Compress counts
    {
      col_struct.runs_count_offset = write_ptr - col_struct.data;
      u32 used_space;
      IntegerSchemePicker::compress(rle_count.data(), nullptr, write_ptr, rle_count.size(),
                                    allowed_cascading_level - 1, used_space,
                                    col_struct.counts_scheme_code, force_counts, "counts");
      write_ptr += used_space;
      // TODO why is used_space added 2 times????
      write_ptr += used_space;
      // -------------------------------------------------------------------------------------
      Log::debug("X_RLE: count_c = {} count_s = {}", CI(col_struct.counts_scheme_code),
                 CI(used_space));
      // -------------------------------------------------------------------------------------
    }
    // -------------------------------------------------------------------------------------
    return write_ptr - dest;
  }

  static inline string fullDescription(const u8* src, const string& selfDescription) {
    const auto& col_struct = *reinterpret_cast<const RLEStructure*>(src);
    string result = selfDescription;

    auto& value_scheme =
        TypeWrapper<SchemeType, SchemeCodeType>::getScheme(col_struct.values_scheme_code);
    result += "\n\t-> ([valueType] values) " + value_scheme.fullDescription(col_struct.data);

    IntegerScheme& counts_scheme =
        TypeWrapper<IntegerScheme, IntegerSchemeType>::getScheme(col_struct.counts_scheme_code);
    result += "\n\t-> ([int] counts) " +
              counts_scheme.fullDescription(col_struct.data + col_struct.runs_count_offset);

    return result;
  }
  // -------------------------------------------------------------------------------------
  static inline void decompressColumn(NumberType* dest,
                                      BitmapWrapper*,
                                      const u8* src,
                                      u32 tuple_count,
                                      u32 level) {
    throw Generic_Exception("Unsupported templated specialization");
  }

  static inline u32 decompressRuns(NumberType* values,
                                   INTEGER* counts,
                                   BitmapWrapper*,
                                   const u8* src,
                                   u32 tuple_count,
                                   u32 level) {
    const auto& col_struct = *reinterpret_cast<const RLEStructure*>(src);
    auto& value_scheme =
        TypeWrapper<SchemeType, SchemeCodeType>::getScheme(col_struct.values_scheme_code);
    value_scheme.decompress(values, nullptr, col_struct.data, col_struct.runs_count, level + 1);

    IntegerScheme& counts_scheme =
        TypeWrapper<IntegerScheme, IntegerSchemeType>::getScheme(col_struct.counts_scheme_code);
    counts_scheme.decompress(counts, nullptr, col_struct.data + col_struct.runs_count_offset,
                             col_struct.runs_count, level + 1);

    return col_struct.runs_count;
  }
  // -------------------------------------------------------------------------------------
};

#if defined(__aarch64__)
template <typename T>
inline T* decompress_sve_loop(T* dest,
                              const RLEStructure col_struct,
                              const T* values,
                              int* counts) {
  for (u32 run_i = 0; run_i < col_struct.runs_count; run_i++) {
    auto value = values[run_i];
    auto count = counts[run_i];
    std::fill(dest, dest + count, value);
    dest += count;
  }
  return dest;
}
#endif

template <>
inline void TRLE<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>::decompressColumn(
    INTEGER* dest,
    BitmapWrapper*,
    const u8* src,
    u32 tuple_count,
    u32 level) {
  static_assert(sizeof(*dest) == 4);

  const auto& col_struct = *reinterpret_cast<const RLEStructure*>(src);
  // -------------------------------------------------------------------------------------
  // Decompress values
  thread_local std::vector<std::vector<INTEGER>> values_v;
  auto values =
      get_level_data(values_v, col_struct.runs_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  {
    IntegerScheme& scheme =
        TypeWrapper<IntegerScheme, IntegerSchemeType>::getScheme(col_struct.values_scheme_code);
    scheme.decompress(values, nullptr, col_struct.data, col_struct.runs_count, level + 1);
  }
  // -------------------------------------------------------------------------------------
  // Decompress counts
  thread_local std::vector<std::vector<INTEGER>> counts_v;
  auto counts =
      get_level_data(counts_v, col_struct.runs_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  {
    IntegerScheme& scheme =
        TypeWrapper<IntegerScheme, IntegerSchemeType>::getScheme(col_struct.counts_scheme_code);
    scheme.decompress(counts, nullptr, col_struct.data + col_struct.runs_count_offset,
                      col_struct.runs_count, level + 1);
  }
  // -------------------------------------------------------------------------------------
  auto write_ptr = dest;
#ifdef BTR_USE_SIMD
  BTR_IFELSEARM_SVE(
      { write_ptr = decompress_sve_loop<INTEGER>(write_ptr, col_struct, values, counts); },
      {
        // On non-SVE ARM machines, this generates NEON code
        for (u32 run_i = 0; run_i < col_struct.runs_count; run_i++) {
          auto target_ptr = write_ptr + counts[run_i];

          /*
           * I tried several variation for vectorizing this. Using AVX2 directly is
           * the fastest even when there are many very short runs. The penalty of
           * branching simply outweighs the few instructions saved by not using AVX2
           * for short runs
           */
          // set is a sequential operation
          __m256i vec = _mm256_set1_epi32(values[run_i]);
          while (write_ptr < target_ptr) {
            // store is performed in a single cycle
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(write_ptr), vec);
            write_ptr += 8;
          }
          write_ptr = target_ptr;
        }
      });
#else
  for (u32 run_i = 0; run_i < col_struct.runs_count; run_i++) {
    auto val = values[run_i];
    auto target_ptr = write_ptr + counts[run_i];
    while (write_ptr != target_ptr) {
      *write_ptr++ = val;
    }
  }
#endif
}

template <>
inline void TRLE<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>::decompressColumn(
    DOUBLE* dest,
    BitmapWrapper*,
    const u8* src,
    u32 tuple_count,
    u32 level) {
  static_assert(sizeof(*dest) == 8);

  const auto& col_struct = *reinterpret_cast<const RLEStructure*>(src);
  // -------------------------------------------------------------------------------------
  // Decompress values
  thread_local std::vector<std::vector<DOUBLE>> values_v;
  auto values =
      get_level_data(values_v, col_struct.runs_count + SIMD_EXTRA_ELEMENTS(DOUBLE), level);
  {
    DoubleScheme& scheme =
        TypeWrapper<DoubleScheme, DoubleSchemeType>::getScheme(col_struct.values_scheme_code);
    scheme.decompress(values, nullptr, col_struct.data, col_struct.runs_count, level + 1);
  }
  // -------------------------------------------------------------------------------------
  // Decompress counts
  thread_local std::vector<std::vector<INTEGER>> counts_v;
  auto counts =
      get_level_data(counts_v, col_struct.runs_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  {
    IntegerScheme& scheme =
        TypeWrapper<IntegerScheme, IntegerSchemeType>::getScheme(col_struct.counts_scheme_code);
    scheme.decompress(counts, nullptr, col_struct.data + col_struct.runs_count_offset,
                      col_struct.runs_count, level + 1);
  }
  // -------------------------------------------------------------------------------------
  auto write_ptr = dest;
#ifdef BTR_USE_SIMD
  BTR_IFELSEARM_SVE(
      { write_ptr = decompress_sve_loop<DOUBLE>(write_ptr, col_struct, values, counts); },
      {
        for (u32 run_i = 0; run_i < col_struct.runs_count; run_i++) {
          auto target_ptr = write_ptr + counts[run_i];

          // set is a sequential operation
          __m256d vec = _mm256_set1_pd(values[run_i]);
          while (write_ptr < target_ptr) {
            // store is performed in a single cycle
            _mm256_storeu_pd(write_ptr, vec);
            write_ptr += 4;
          }
          write_ptr = target_ptr;
        }
      });
#else
  for (u32 run_i = 0; run_i < col_struct.runs_count; run_i++) {
    auto val = values[run_i];
    auto target_ptr = write_ptr + counts[run_i];
    while (write_ptr != target_ptr) {
      *write_ptr++ = val;
    }
  }
#endif
}
}  // namespace btrblocks
