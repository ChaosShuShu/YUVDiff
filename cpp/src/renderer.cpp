#include "yuvdiff/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace yuvdiff {

namespace {

inline uint8_t clamp_u8(double v) {
    if (v < 0.0) return 0;
    if (v > 255.0) return 255;
    return static_cast<uint8_t>(std::round(v));
}

} // anonymous namespace

Renderer::Renderer(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Renderer dimensions must be positive");
    }
}

RgbImage Renderer::render(
    const YUVFrame& frame_a,
    const YUVFrame* frame_b,
    const DiffResult* diff,
    RenderMode mode,
    int threshold
) const {
    (void)threshold;
    switch (mode) {
        case RenderMode::ORIGINAL_A:
            return yuv_to_rgb(frame_a);
        case RenderMode::ORIGINAL_B:
            if (!frame_b) {
                throw std::invalid_argument("Frame B is required for ORIGINAL_B render mode");
            }
            return yuv_to_rgb(*frame_b);
        case RenderMode::HEATMAP:
            if (!diff) {
                throw std::invalid_argument("DiffResult is required for HEATMAP render mode");
            }
            return render_heatmap(*diff, frame_a.bit_depth);
        case RenderMode::THRESHOLD_MASK:
            if (!diff) {
                throw std::invalid_argument("DiffResult is required for THRESHOLD_MASK render mode");
            }
            RgbImage rgb_a = yuv_to_rgb(frame_a);
            return render_mask(rgb_a, *diff);
    }
    throw std::invalid_argument("Unknown RenderMode");
}

RgbImage Renderer::yuv_to_rgb(const YUVFrame& frame) const {
    int w = frame.width;
    int h = frame.height;
    auto [hf, vf] = chroma_subsampling(frame.pixel_format);

    RgbImage img;
    img.width = w;
    img.height = h;
    img.data.resize(static_cast<size_t>(w) * h * 3);

    for (int r = 0; r < h; ++r) {
        int cr = r / vf;
        for (int c = 0; c < w; ++c) {
            int cc = c / hf;
            size_t idx = (static_cast<size_t>(r) * w + c) * 3;

            double y_val = frame.get_y(r, c);
            double u_val = frame.get_u(cr, cc);
            double v_val = frame.get_v(cr, cc);

            if (frame.bit_depth == 10) {
                y_val = static_cast<double>(static_cast<uint16_t>(y_val) >> 2);
                u_val = static_cast<double>(static_cast<uint16_t>(u_val) >> 2);
                v_val = static_cast<double>(static_cast<uint16_t>(v_val) >> 2);
            }

            double u_c = u_val - 128.0;
            double v_c = v_val - 128.0;

            double r_val = y_val + 1.402 * v_c;
            double g_val = y_val - 0.344136 * u_c - 0.714136 * v_c;
            double b_val = y_val + 1.772 * u_c;

            img.data[idx + 0] = clamp_u8(r_val);
            img.data[idx + 1] = clamp_u8(g_val);
            img.data[idx + 2] = clamp_u8(b_val);
        }
    }

    return img;
}

RgbImage Renderer::render_heatmap(const DiffResult& diff, int bit_depth) const {
    int w = width_;
    int h = height_;
    double max_val = (bit_depth == 8) ? 255.0 : 1023.0;

    RgbImage img;
    img.width = w;
    img.height = h;
    img.data.resize(static_cast<size_t>(w) * h * 3);

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            size_t p_idx = static_cast<size_t>(r) * w + c;
            size_t idx = p_idx * 3;

            double diff_val = static_cast<double>(diff.diff_y[p_idx]);
            double norm = std::clamp(diff_val / max_val, 0.0, 1.0);

            // 0 error -> Gray (128, 128, 128), Max error -> Pure Red (255, 0, 0)
            double r_val = 128.0 + 127.0 * norm;
            double g_val = 128.0 * (1.0 - norm);
            double b_val = 128.0 * (1.0 - norm);

            img.data[idx + 0] = clamp_u8(r_val);
            img.data[idx + 1] = clamp_u8(g_val);
            img.data[idx + 2] = clamp_u8(b_val);
        }
    }

    return img;
}

RgbImage Renderer::render_mask(const RgbImage& rgb_a, const DiffResult& diff) const {
    int w = rgb_a.width;
    int h = rgb_a.height;

    RgbImage img = rgb_a; // deep copy

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            size_t p_idx = static_cast<size_t>(r) * w + c;
            if (diff.mask[p_idx] != 0) {
                size_t idx = p_idx * 3;
                double r_orig = img.data[idx + 0];
                double g_orig = img.data[idx + 1];
                double b_orig = img.data[idx + 2];

                img.data[idx + 0] = clamp_u8(0.6 * 255.0 + 0.4 * r_orig);
                img.data[idx + 1] = clamp_u8(0.4 * g_orig);
                img.data[idx + 2] = clamp_u8(0.4 * b_orig);
            }
        }
    }

    return img;
}

} // namespace yuvdiff
