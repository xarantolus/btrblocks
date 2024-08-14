#include "RLE.hpp"
#include "common/SIMD.hpp"
#include "common/Units.hpp"

namespace btrblocks {
template <typename data_type>
size_t compress_len(INTEGER* out_lengths,
                    data_type* out_values,
                    const data_type* data,
                    const BITMAP* nullmap,
                    const INTEGER N) {
  if (N == 0) {
    return 0;
  }

  INTEGER block_start_pos = 0;
  data_type previous = data[0];

  INTEGER out_len = 0;

  INTEGER i = 1;
  if (nullmap) {
#pragma GCC ivdep
#pragma clang loop vectorize(assume_safety) vectorize_width(scalable)
    for (; i < N; ++i) {
      data_type c = data[i];

      if (c == previous || !nullmap[i]) {
        continue;
      }

      // We have found an edge
      INTEGER block_len = i - block_start_pos;
      assert(block_len > 0);
      out_lengths[out_len] = block_len;
      out_values[out_len] = previous;
      out_len++;

      previous = c;
      block_start_pos = i;
    }
  } else {
#pragma GCC ivdep
#pragma clang loop vectorize(assume_safety) vectorize_width(scalable)
    for (; i < N; ++i) {
      data_type c = data[i];

      if (c == previous) {
        continue;
      }

      // We have found an edge
      INTEGER block_len = i - block_start_pos;
      assert(block_len > 0);
      out_lengths[out_len] = block_len;
      out_values[out_len] = previous;
      out_len++;

      previous = c;
      block_start_pos = i;
    }
  }

  INTEGER block_len = N - block_start_pos;
  assert(block_len > 0);

  out_lengths[out_len] = block_len;
  out_values[out_len] = previous;
  out_len++;

  return out_len;
}

template size_t compress_len(INTEGER* out_lengths,
                             INTEGER* out_values,
                             const INTEGER* data,
                             const BITMAP* nullmap,
                             const INTEGER N);
template size_t compress_len(INTEGER* out_lengths,
                             DOUBLE* out_values,
                             const DOUBLE* data,
                             const BITMAP* nullmap,
                             const INTEGER N);

#if defined(__aarch64__)
__attribute__((target("+sve"))) size_t compress_sve(INTEGER* out_lengths,
                                                    INTEGER* out_values,
                                                    const INTEGER* data,
                                                    const INTEGER N) {
  if (N == 0) {
    return 0;
  }

  size_t result_len = 1;

  INTEGER i = 0;
  INTEGER vl = static_cast<INTEGER>(svcntw());
  svbool_t pred = svwhilelt_b32(i, N);

  svint32_t current;
  svint32_t predecessors;

  INTEGER last_value = out_values[0] = data[0];
  INTEGER last_index = 0;
  svint32_t output_indices;
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

  out_lengths[result_len - 1] = N - last_index;

  return result_len;
}

__attribute__((target("+sve"))) size_t compress_sve(INTEGER* out_lengths,
                                                    DOUBLE* out_values,
                                                    const DOUBLE* data,
                                                    const INTEGER N) {
  if (N == 0) {
    return 0;
  }

  size_t result_len = 1;

  INTEGER i = 0;
  INTEGER vl = static_cast<INTEGER>(svcntd());
  svbool_t pred = svwhilelt_b64(i, N);

  svfloat64_t current;
  svfloat64_t predecessors;

  DOUBLE last_value = out_values[0] = data[0];
  int64_t last_index = 0;
  svint64_t output_indices;
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

  out_lengths[result_len - 1] = N - last_index;

  return result_len;
}
#endif
}  // namespace btrblocks
