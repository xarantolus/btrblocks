#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
#include <map>
#include <random>
// -------------------------------------------------------------------------------------
namespace btrblocks {
template <typename T>
struct NumberStats;

#if defined(__aarch64__)
template <typename T>
NumberStats<T> generateStatsSVE(const T* src, const BITMAP* nullmap, u32 tuple_count);
#endif

// -------------------------------------------------------------------------------------
template <typename T>
struct NumberStats {
 public:
  NumberStats(const T* src, const BITMAP* bitmap, u32 tuple_count)
      : src(src), bitmap(bitmap), tuple_count(tuple_count) {}
  // -------------------------------------------------------------------------------------
  const T* src;
  const BITMAP* bitmap;
  std::map<T, u32> distinct_values;
  T min;
  T max;
  NumberStats() = delete;
  // -------------------------------------------------------------------------------------
  u32 tuple_count;
  u32 total_size;
  u32 null_count;
  u32 unique_count;
  u32 set_count;
  u32 average_run_length;
  u32 run_count;
  bool is_sorted;
  // -------------------------------------------------------------------------------------
  tuple<vector<T>, vector<BITMAP>> samples(u32 n, u32 length) {
    // -------------------------------------------------------------------------------------
    std::random_device rd;   // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with
                             // rd()
    // -------------------------------------------------------------------------------------
    // TODO : Construction Site !! need a better theory and algorithm for
    // sampling Constraints: RLE(runs), nulls, uniqueness, naive approach to
    // pick enough elements that approximate stratified sample run ~8 normal
    // rounds, then check if the
    vector<T> compiled_values;
    vector<BITMAP> compiled_bitmap;
    // -------------------------------------------------------------------------------------
    if (tuple_count <= n * length) {
      compiled_values.insert(compiled_values.end(), src, src + tuple_count);
      if (bitmap != nullptr) {
        compiled_bitmap.insert(compiled_bitmap.end(), bitmap, bitmap + tuple_count);
      } else {
        compiled_bitmap.insert(compiled_bitmap.end(), tuple_count, 1);
      }
    } else {
      u32 separator =
          tuple_count / n;  // how big is the slice of the input, of which we take a part....
      u32 remainder = tuple_count % n;
      for (u32 sample_i = 0; sample_i < n; sample_i++) {
        u32 range_end = ((sample_i == n - 1) ? (separator + remainder) : separator) - length;
        std::uniform_int_distribution<> dis(0, range_end);
        u32 partition_begin = sample_i * separator + dis(gen);
        // (sample_i * separator, (sample_i + 1 ) * separator) range to pick
        // from
        compiled_values.insert(compiled_values.end(), src + partition_begin,
                               src + partition_begin + length);
        if (bitmap == nullptr) {
          compiled_bitmap.insert(compiled_bitmap.end(), length, 1);
        } else {
          compiled_bitmap.insert(compiled_bitmap.end(), bitmap + partition_begin,
                                 bitmap + partition_begin + length);
        }
      }
    }

    return std::make_tuple(compiled_values, compiled_bitmap);
  }
  // -------------------------------------------------------------------------------------
  static NumberStats generateStats(const T* src, const BITMAP* nullmap, u32 tuple_count) {
    NumberStats stats(src, nullmap, tuple_count);
    // -------------------------------------------------------------------------------------
    stats.tuple_count = tuple_count;
    stats.total_size = tuple_count * sizeof(T);
    stats.null_count = 0;
    stats.average_run_length = 0;
    stats.is_sorted = true;
    // -------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------
    // Let NULL_CODE (0) of null values also taken into stats consideration
    // -------------------------------------------------------------------------------------
    T last_value;
    if (tuple_count > 0) {
      stats.min = stats.max = last_value = src[0];
    }
    u32 run_count = 0;
    u64 run_start_idx = 0;
    u64 nulls_in_run = 0;
    T current_value;
        // -------------------------------------------------------------------------------------
    if (nullmap) {
      for (u64 row_i = 0; row_i < tuple_count; row_i++) {
        // -------------------------------------------------------------------------------------
        current_value = src[row_i];
        if (current_value != last_value) {
          if (nullmap[row_i]) {
            if (current_value < last_value) {
              stats.is_sorted = false;
            }
            run_count++;
            auto len = (row_i - run_start_idx);
            stats.null_count += nulls_in_run;
            assert(len > 0);
            auto it = stats.distinct_values.find(last_value);
            if (it != stats.distinct_values.end()) {
              it->second += len;
            } else {
              stats.distinct_values.insert({last_value, len});
            }
            run_start_idx = row_i;
            nulls_in_run = 0;
            last_value = current_value;
          } else {
            nulls_in_run++;
          }
        }
      }
    }
    else {
      for (u64 row_i = 0; row_i < tuple_count; row_i++) {
        current_value = src[row_i];
        if (current_value != last_value) {
          if (current_value < last_value) {
            stats.is_sorted = false;
          }
          run_count++;
          auto len = (row_i - run_start_idx);
          assert(len > 0);
          auto it = stats.distinct_values.find(last_value);
          if (it != stats.distinct_values.end()) {
            it->second += len;
          } else {
            stats.distinct_values.insert({last_value, len});
          }
          run_start_idx = row_i;
          last_value = current_value;
        }
      }
    }
    run_count++;

    // -------------------------------------------------------------------------------------
    if (tuple_count > 0) {
      auto run_len = (tuple_count - run_start_idx);
      if (run_len > 0) {
        auto it = stats.distinct_values.find(current_value);
        if (it != stats.distinct_values.end()) {
            it->second += run_len;
        } else {
            stats.distinct_values.insert({current_value, run_len});
        }
      }
      stats.null_count += nulls_in_run;

      // Since maps are sorted, we can easily access min/max
      stats.min = stats.distinct_values.begin()->first;
      stats.max = stats.distinct_values.rbegin()->first;
    }
    stats.average_run_length = CD(tuple_count) / CD(run_count);
    stats.unique_count = stats.distinct_values.size();
    stats.set_count = stats.tuple_count - stats.null_count;
    stats.run_count = run_count;

    return stats;
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
