#pragma once

#include "yuvdiff/parser.hpp"

#include <cstdint>
#include <vector>

namespace yuvdiff {

struct DiffResult {
    std::vector<int32_t> diff_y;     // Native Y resolution (H x W)
    std::vector<int32_t> diff_u;     // Native chroma resolution (H_c x W_c)
    std::vector<int32_t> diff_v;     // Native chroma resolution (H_c x W_c)
    std::vector<int32_t> diff_pixel; // Full-channel combined max diff at Y resolution (H x W)
    std::vector<uint8_t> mask;       // Upsampled binary mask at Y resolution (1 if diff > threshold, 0 otherwise)

    int64_t diff_pixel_count = 0;    // Strictly d > 0 pixels count
    int64_t total_pixel_count = 0;   // H * W
    double diff_ratio = 0.0;         // diff_pixel_count / total_pixel_count

    // Multi-threshold stepped metrics
    int threshold = 4;
    int threshold_half = 2;          // threshold / 2
    int threshold_2t = 8;            // threshold * 2

    int64_t diff_gt_t = 0;           // d > threshold count
    double diff_gt_t_ratio = 0.0;    // diff_gt_t / total_pixel_count

    int64_t diff_gt_half_t = 0;      // d > (threshold / 2) count
    double diff_gt_half_t_ratio = 0.0; // diff_gt_half_t / total_pixel_count

    int64_t diff_gt_2t = 0;          // d > (threshold * 2) count
    double diff_gt_2t_ratio = 0.0;   // diff_gt_2t / total_pixel_count

    // Distribution statistics over d > 0 pixels
    double diff_mean = 0.0;
    double diff_median = 0.0;
    int32_t diff_max = 0;
    int32_t diff_min = 0;
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
