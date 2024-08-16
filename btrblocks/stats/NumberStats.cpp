// -------------------------------------------------------------------------------------
#include "stats/NumberStats.hpp"
#include <arm_sve.h>
#include "common/SIMD.hpp"
#include "common/Units.hpp"
#include "common/SIMD.hpp"
#include "common/Units.hpp"
#include "scheme/templated/RLE.hpp"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>
// -------------------------------------------------------------------------------------
namespace btrblocks {

#if defined(__aarch64__)
__attribute__((target("+sve"))) size_t stats_sve(INTEGER* out_lengths,
                                                 INTEGER* out_values,
                                                    const INTEGER* data,
                                                    const BITMAP* nullmap,
                                                    u32* null_count,
                                                    const INTEGER N) {
  if (N == 0) {
    return 0;
  }
  u32 nullcount = 0;

  size_t result_len = 1;

  INTEGER i = 0;
  INTEGER vl = static_cast<INTEGER>(svcntw());
  svbool_t pred = svwhilelt_b32(i, N);

  svint32_t current;
  svint32_t predecessors;

  INTEGER last_value = out_values[0] = data[0];
  INTEGER last_index = 0;
  svint32_t output_indices;
  // nullmap[i] == 0 if null
  if (nullmap) {
    const svint32_t nullmap_comparator = svdup_s32(0);
    do {
      assert(i % vl == 0);
      svint32_t index_reg = svindex_s32(i, 1);

      current = svld1_s32(pred, &data[i]);
      svint32_t nulls_locs = svld1ub_s32(pred, &nullmap[i]);

      predecessors = svinsr_n_s32(current, last_value);
      svbool_t nulls = svcmpeq_s32(pred, nulls_locs, nullmap_comparator);
      nullcount += svcntp_b32(pred, nulls);

      // null values are simply ignored, so just add them to pred
      pred = svbic_b_z(pred, pred, nulls);

      // Find changes
      svbool_t cmp = svcmpne_s32(pred, current, predecessors);

      // Move the found changes to the front of the vector
      svint32_t output_values = svcompact_s32(cmp, current);
      output_indices = svcompact_s32(cmp, index_reg);

      // Find lengths
      svint32_t indices_shifted = svinsr_n_s32(output_indices, last_index);
      svint32_t lengths = svsub_s32_x(cmp, output_indices, indices_shifted);

      // Now if we e.g. compressed 5 items, we want a predicate with the lowest 5 items active
      uint32_t result_count = svcntp_b32(cmp, cmp);
      svbool_t store_pred = svwhilelt_b32(static_cast<uint32_t>(0), result_count);

      // We now need to store them in values
      svst1_s32(store_pred, out_values + result_len, output_values);
      svst1_s32(store_pred, out_lengths + result_len - 1, lengths);

      i += vl;
      last_value = svlastb_s32(pred, current);
      last_index = svclastb_n_s32(store_pred, last_index, output_indices);
      pred = svwhilelt_b32(i, N);

      result_len += result_count;
    } while (svptest_first(pred, pred));
  } else {
    do {
      assert(i % vl == 0);
      svint32_t index_reg = svindex_s32(i, 1);

      current = svld1_s32(pred, &data[i]);
      predecessors = svinsr_n_s32(current, last_value);

      // Find changes
      svbool_t cmp = svcmpne_s32(pred, current, predecessors);

      // Move the found changes to the front of the vector
      svint32_t output_values = svcompact_s32(cmp, current);
      output_indices = svcompact_s32(cmp, index_reg);

      // Find lengths
      svint32_t indices_shifted = svinsr_n_s32(output_indices, last_index);
      svint32_t lengths = svsub_s32_x(cmp, output_indices, indices_shifted);

      // Now if we e.g. compressed 5 items, we want a predicate with the lowest 5 items active
      uint32_t result_count = svcntp_b32(cmp, cmp);
      svbool_t store_pred = svwhilelt_b32(static_cast<uint32_t>(0), result_count);

      // We now need to store them in values
      svst1_s32(store_pred, out_values + result_len, output_values);
      svst1_s32(store_pred, out_lengths + result_len - 1, lengths);

      i += vl;
      last_value = svlastb_s32(pred, current);
      last_index = svclastb_n_s32(store_pred, last_index, output_indices);
      pred = svwhilelt_b32(i, N);

      result_len += result_count;
    } while (svptest_first(pred, pred));
  }

  out_lengths[result_len - 1] = N - last_index;
  *null_count = nullcount;

  return result_len;
}

__attribute__((target("+sve"))) size_t stats_sve(INTEGER* out_lengths,
                                                 DOUBLE* out_values,
                                                 const DOUBLE* data,
                                                 const BITMAP* nullmap,
                                                 u32* null_count,
                                                 const INTEGER N) {
  if (N == 0) {
    return 0;
  }
  u32 nullcount = 0;

  size_t result_len = 1;

  INTEGER i = 0;
  INTEGER vl = static_cast<INTEGER>(svcntd());
  svbool_t pred = svwhilelt_b64(i, N);

  svfloat64_t current;
  svfloat64_t predecessors;

  DOUBLE last_value = out_values[0] = data[0];
  int64_t last_index = 0;
  svint64_t output_indices;
  if (nullmap) {
    // nullmap[i] == 0 if null
    const svuint64_t nullmap_comparator = svdup_u64(0);
    do {
      assert(i % vl == 0);
      svint64_t index_reg = svindex_s64(i, 1);

      current = svld1_f64(pred, &data[i]);
      svuint64_t nulls_locs = svld1ub_u64(pred, &nullmap[i]);

      predecessors = svinsr_n_f64(current, last_value);
      svbool_t nulls = svcmpeq_u64(pred, nulls_locs, nullmap_comparator);
      nullcount += svcntp_b64(pred, nulls);

      // null values are simply ignored, so just add them to pred
      pred = svbic_b_z(pred, pred, nulls);

      // Find changes
      svbool_t cmp = svcmpne_f64(pred, current, predecessors);

      // Move the found changes to the front of the vector
      svfloat64_t output_values = svcompact_f64(cmp, current);
      output_indices = svcompact_s64(cmp, index_reg);

      // Find lengths
      svint64_t indices_shifted = svinsr_n_s64(output_indices, last_index);
      svint64_t lengths = svsub_s64_x(cmp, output_indices, indices_shifted);

      // Now if we e.g. compressed 5 items, we want a predicate with the lowest 5 items active
      uint64_t result_count = svcntp_b64(cmp, cmp);
      svbool_t store_pred = svwhilelt_b64(static_cast<uint64_t>(0), result_count);

      // We now need to store them in values
      svst1_f64(store_pred, out_values + result_len, output_values);
      svst1w_s64(store_pred, out_lengths + result_len - 1, lengths);

      i += vl;
      last_value = svlastb_f64(pred, current);
      last_index = svclastb_n_s64(store_pred, last_index, output_indices);
      pred = svwhilelt_b64(i, N);

      result_len += result_count;
    } while (svptest_first(pred, pred));
  } else {
    do {
      assert(i % vl == 0);
      svint64_t index_reg = svindex_s64(i, 1);

      current = svld1_f64(pred, &data[i]);
      predecessors = svinsr_n_f64(current, last_value);

      // Find changes
      svbool_t cmp = svcmpne_f64(pred, current, predecessors);

      // Move the found changes to the front of the vector
      svfloat64_t output_values = svcompact_f64(cmp, current);
      output_indices = svcompact_s64(cmp, index_reg);

      // Find lengths
      svint64_t indices_shifted = svinsr_n_s64(output_indices, last_index);
      svint64_t lengths = svsub_s64_x(cmp, output_indices, indices_shifted);

      // Now if we e.g. compressed 5 items, we want a predicate with the lowest 5 items active
      uint64_t result_count = svcntp_b64(cmp, cmp);
      svbool_t store_pred = svwhilelt_b64(static_cast<uint64_t>(0), result_count);

      // We now need to store them in values
      svst1_f64(store_pred, out_values + result_len, output_values);
      svst1w_s64(store_pred, out_lengths + result_len - 1, lengths);

      i += vl;
      last_value = svlastb_f64(pred, current);
      last_index = svclastb_n_s64(store_pred, last_index, output_indices);
      pred = svwhilelt_b64(i, N);

      result_len += result_count;
    } while (svptest_first(pred, pred));
  }

  out_lengths[result_len - 1] = N - last_index;
  *null_count = nullcount;

  return result_len;
}

template <typename T>
NumberStats<T> generateStatsSVE(const T* src, const BITMAP* nullmap, u32 tuple_count) {
  NumberStats stats(src, nullmap, tuple_count);
  stats.tuple_count = tuple_count;
  stats.total_size = tuple_count * sizeof(T);
  stats.null_count = 0;
  stats.average_run_length = 0;
  stats.is_sorted = std::is_sorted(src, src + tuple_count);

  std::vector<T> out_values(tuple_count);
  std::vector<INTEGER> out_lengths(tuple_count);

  stats.run_count = stats_sve(out_lengths.data(), out_values.data(), src, nullmap, &stats.null_count, tuple_count);

  T last_value;
  if (tuple_count > 0) {
      stats.min = stats.max = last_value = out_values[0];
  }

  for (u64 row_i = 0; row_i < stats.run_count; row_i++) {
    auto current_value = out_values[row_i];
      if (stats.is_sorted && current_value < last_value) {
        stats.is_sorted = false;
      }
    if (stats.distinct_values.find(current_value) == stats.distinct_values.end()) {
      stats.distinct_values.insert({current_value, out_lengths[row_i]});
    } else {
      stats.distinct_values[current_value] += out_lengths[row_i];
    }
    if (current_value > stats.max) {
      stats.max = current_value;
    } else if (current_value < stats.min) {
      stats.min = current_value;
    }
    last_value = current_value;
  }

  stats.average_run_length = CD(tuple_count) / CD(stats.run_count);
  stats.unique_count = stats.distinct_values.size();
  stats.set_count = stats.tuple_count - stats.null_count;

  return stats;
}

template NumberStats<INTEGER> generateStatsSVE(const INTEGER* src, const BITMAP* nullmap, u32 tuple_count);
template NumberStats<DOUBLE> generateStatsSVE(const DOUBLE* src, const BITMAP* nullmap, u32 tuple_count);
#endif

// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
