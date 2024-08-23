#if defined(__aarch64__)
#include "RLE.hpp"

namespace btrblocks {
template <typename number_type>
__attribute__((target("+sve"))) size_t rle_decompress_len(const int *in_lengths, const number_type *in_values, const size_t N, number_type *data) {
    size_t total_len = 0;
    for (size_t i = 0; i < N; i++) {
        size_t len = in_lengths[i];
        number_type val = in_values[i];

        for (size_t j = 0; j < len; j++) {
            data[total_len + j] = val;
        }

        total_len += len;
    }

    return total_len;
}
// Explicit instantiation
template __attribute__((target("+sve"))) size_t rle_decompress_len(const int *in_lengths, const INTEGER *in_values, const size_t N, INTEGER *data);
template __attribute__((target("+sve"))) size_t rle_decompress_len(const int *in_lengths, const DOUBLE *in_values, const size_t N, DOUBLE *data);
}
#endif
