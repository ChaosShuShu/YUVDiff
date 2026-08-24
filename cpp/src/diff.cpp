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

    // Generate combined threshold mask
    int64_t diff_cnt = 0;
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

            if (dy > threshold_ || du > threshold_ || dv > threshold_) {
                result.mask[y_idx] = 1;
                diff_cnt++;
            }
        }
    }
    result.diff_pixel_count = diff_cnt;

    return result;
}

} // namespace yuvdiff
