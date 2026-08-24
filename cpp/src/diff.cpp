#include "yuvdiff/diff.hpp"

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
    if (a.pixel_format != b.pixel_format) {
        throw std::invalid_argument("Pixel format mismatch: A vs B");
    }

    int w = a.width;
    int h = a.height;
    auto [hf, vf] = chroma_subsampling(a.pixel_format);
    int cw = w / hf;
    int ch = h / vf;

    DiffResult result;
    result.diff_y.resize(static_cast<size_t>(w) * h);
    result.diff_u.resize(static_cast<size_t>(cw) * ch);
    result.diff_v.resize(static_cast<size_t>(cw) * ch);
    result.mask.resize(static_cast<size_t>(w) * h, 0);
    result.total_pixel_count = static_cast<int64_t>(w) * h;
    result.diff_pixel_count = 0;

    // Diff Y plane
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int idx = r * w + c;
            int32_t val_a = static_cast<int32_t>(a.get_y(r, c));
            int32_t val_b = static_cast<int32_t>(b.get_y(r, c));
            result.diff_y[idx] = std::abs(val_a - val_b);
        }
    }

    // Diff U and V planes
    for (int r = 0; r < ch; ++r) {
        for (int c = 0; c < cw; ++c) {
            int idx = r * cw + c;
            int32_t u_a = static_cast<int32_t>(a.get_u(r, c));
            int32_t u_b = static_cast<int32_t>(b.get_u(r, c));
            result.diff_u[idx] = std::abs(u_a - u_b);

            int32_t v_a = static_cast<int32_t>(a.get_v(r, c));
            int32_t v_b = static_cast<int32_t>(b.get_v(r, c));
            result.diff_v[idx] = std::abs(v_a - v_b);
        }
    }

    // Generate combined threshold mask with nearest-neighbor chroma upsampling
    int64_t diff_cnt = 0;
    for (int r = 0; r < h; ++r) {
        int cr = r / vf;
        for (int c = 0; c < w; ++c) {
            int cc = c / hf;
            int y_idx = r * w + c;
            int c_idx = cr * cw + cc;

            bool is_diff = (result.diff_y[y_idx] > threshold_) ||
                           (result.diff_u[c_idx] > threshold_) ||
                           (result.diff_v[c_idx] > threshold_);
            if (is_diff) {
                result.mask[y_idx] = 1;
                diff_cnt++;
            }
        }
    }
    result.diff_pixel_count = diff_cnt;

    return result;
}

} // namespace yuvdiff
