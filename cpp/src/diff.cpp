#include "yuvdiff/diff.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace yuvdiff {

DiffEngine::DiffEngine(int threshold) : threshold_(threshold) {
    if (threshold < 0) {
        throw std::invalid_argument("threshold must be >= 0, got " + std::to_string(threshold));
    }
}

DiffResult DiffEngine::diff(const YUVFrame& a, const YUVFrame& b) const {
    if (a.width != b.width || a.height != b.height) {
        throw std::invalid_argument(
            "Frame size mismatch: A=(" + std::to_string(a.width) + "x" + std::to_string(a.height) +
            ") vs B=(" + std::to_string(b.width) + "x" + std::to_string(b.height) + ")"
        );
    }

    int w = a.width;
    int h = a.height;
    auto [hf_a, vf_a] = chroma_subsampling(a.pixel_format);
    auto [hf_b, vf_b] = chroma_subsampling(b.pixel_format);

    int hf = std::min(hf_a, hf_b);
    int vf = std::min(vf_a, vf_b);
    int cw = w / hf;
    int ch = h / vf;

    int scale_a = (a.bit_depth == 8 && b.bit_depth == 10) ? 4 : 1;
    int scale_b = (b.bit_depth == 8 && a.bit_depth == 10) ? 4 : 1;

    DiffResult result;
    result.diff_y.resize(static_cast<size_t>(w) * h);
    result.diff_u.resize(static_cast<size_t>(cw) * ch);
    result.diff_v.resize(static_cast<size_t>(cw) * ch);
    result.diff_pixel.resize(static_cast<size_t>(w) * h);
    result.mask.resize(static_cast<size_t>(w) * h, 0);
    result.total_pixel_count = static_cast<int64_t>(w) * h;
    result.diff_pixel_count = 0;

    // Diff Y plane
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int idx = r * w + c;
            int32_t val_a = static_cast<int32_t>(a.get_y(r, c)) * scale_a;
            int32_t val_b = static_cast<int32_t>(b.get_y(r, c)) * scale_b;
            result.diff_y[idx] = std::abs(val_a - val_b);
        }
    }

    // Diff U and V planes
    for (int r = 0; r < ch; ++r) {
        int vr = r * vf;
        for (int c = 0; c < cw; ++c) {
            int vc = c * hf;
            int idx = r * cw + c;
            int32_t u_a = static_cast<int32_t>(a.get_u(vr / vf_a, vc / hf_a)) * scale_a;
            int32_t u_b = static_cast<int32_t>(b.get_u(vr / vf_b, vc / hf_b)) * scale_b;
            result.diff_u[idx] = std::abs(u_a - u_b);

            int32_t v_a = static_cast<int32_t>(a.get_v(vr / vf_a, vc / hf_a)) * scale_a;
            int32_t v_b = static_cast<int32_t>(b.get_v(vr / vf_b, vc / hf_b)) * scale_b;
            result.diff_v[idx] = std::abs(v_a - v_b);
        }
    }

    // Generate combined threshold mask & combined pixel difference stats (calculated for d > 0)
    int64_t non_zero_cnt = 0;
    int64_t sum_diff_non_zero = 0;
    int32_t min_diff_non_zero = 1000000;
    int32_t max_diff_non_zero = 0;
    std::vector<int> hist_non_zero(1024, 0);

    result.threshold = threshold_;
    result.threshold_half = threshold_ / 2;
    result.threshold_2t = threshold_ * 2;

    int64_t diff_gt_t_cnt = 0;
    int64_t diff_gt_half_t_cnt = 0;
    int64_t diff_gt_2t_cnt = 0;

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int y_idx = r * w + c;
            int32_t dy = result.diff_y[y_idx];
            int32_t du = std::abs(
                static_cast<int32_t>(a.get_u(r / vf_a, c / hf_a)) * scale_a -
                static_cast<int32_t>(b.get_u(r / vf_b, c / hf_b)) * scale_b
            );
            int32_t dv = std::abs(
                static_cast<int32_t>(a.get_v(r / vf_a, c / hf_a)) * scale_a -
                static_cast<int32_t>(b.get_v(r / vf_b, c / hf_b)) * scale_b
            );

            int32_t d_pixel = std::max({dy, du, dv});
            result.diff_pixel[y_idx] = d_pixel;

            if (d_pixel > 0) {
                non_zero_cnt++;
                sum_diff_non_zero += d_pixel;
                if (d_pixel < min_diff_non_zero) min_diff_non_zero = d_pixel;
                if (d_pixel > max_diff_non_zero) max_diff_non_zero = d_pixel;
                if (d_pixel < 1024) {
                    hist_non_zero[d_pixel]++;
                } else {
                    hist_non_zero[1023]++;
                }

                if (d_pixel > result.threshold_half) {
                    diff_gt_half_t_cnt++;
                }
                if (d_pixel > result.threshold) {
                    diff_gt_t_cnt++;
                }
                if (d_pixel > result.threshold_2t) {
                    diff_gt_2t_cnt++;
                }
            }

            if (d_pixel > threshold_) {
                result.mask[y_idx] = 1;
            }
        }
    }

    result.diff_pixel_count = non_zero_cnt;
    result.diff_ratio = (result.total_pixel_count > 0)
        ? (static_cast<double>(result.diff_pixel_count) / result.total_pixel_count)
        : 0.0;

    result.diff_gt_half_t = diff_gt_half_t_cnt;
    result.diff_gt_half_t_ratio = (result.total_pixel_count > 0)
        ? (static_cast<double>(result.diff_gt_half_t) / result.total_pixel_count)
        : 0.0;

    result.diff_gt_t = diff_gt_t_cnt;
    result.diff_gt_t_ratio = (result.total_pixel_count > 0)
        ? (static_cast<double>(result.diff_gt_t) / result.total_pixel_count)
        : 0.0;

    result.diff_gt_2t = diff_gt_2t_cnt;
    result.diff_gt_2t_ratio = (result.total_pixel_count > 0)
        ? (static_cast<double>(result.diff_gt_2t) / result.total_pixel_count)
        : 0.0;

    if (non_zero_cnt > 0) {
        result.diff_min = min_diff_non_zero;
        result.diff_max = max_diff_non_zero;
        result.diff_mean = static_cast<double>(sum_diff_non_zero) / non_zero_cnt;

        // Fast and exact median from non-zero histogram
        if (non_zero_cnt % 2 == 1) {
            size_t target = non_zero_cnt / 2 + 1;
            size_t accum = 0;
            for (int i = 1; i < 1024; ++i) {
                accum += hist_non_zero[i];
                if (accum >= target) {
                    result.diff_median = static_cast<double>(i);
                    break;
                }
            }
        } else {
            size_t target1 = non_zero_cnt / 2;
            size_t target2 = non_zero_cnt / 2 + 1;
            size_t accum = 0;
            int val1 = -1;
            int val2 = -1;
            for (int i = 1; i < 1024; ++i) {
                accum += hist_non_zero[i];
                if (val1 == -1 && accum >= target1) {
                    val1 = i;
                }
                if (val2 == -1 && accum >= target2) {
                    val2 = i;
                    break;
                }
            }
            result.diff_median = (val1 + val2) / 2.0;
        }
    } else {
        result.diff_min = 0;
        result.diff_max = 0;
        result.diff_mean = 0.0;
        result.diff_median = 0.0;
    }

    return result;
}

} // namespace yuvdiff
