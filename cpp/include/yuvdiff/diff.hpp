#pragma once

#include "yuvdiff/parser.hpp"

#include <cstdint>
#include <vector>

namespace yuvdiff {

struct DiffResult {
    std::vector<int32_t> diff_y; // Native Y resolution (H x W)
    std::vector<int32_t> diff_u; // Native chroma resolution (H_c x W_c)
    std::vector<int32_t> diff_v; // Native chroma resolution (H_c x W_c)
    std::vector<uint8_t> mask;   // Upsampled binary mask at Y resolution (1 if diff > threshold, 0 otherwise)
    int64_t diff_pixel_count = 0;
    int64_t total_pixel_count = 0;
};

class DiffEngine {
public:
    explicit DiffEngine(int threshold = 4);

    DiffResult diff(const YUVFrame& a, const YUVFrame& b) const;

    int threshold() const { return threshold_; }
    void set_threshold(int t) { threshold_ = t; }

private:
    int threshold_ = 4;
};

} // namespace yuvdiff
