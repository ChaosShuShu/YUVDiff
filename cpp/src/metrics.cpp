#include "yuvdiff/metrics.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace yuvdiff {

namespace {

inline int reflect_index(int idx, int len) {
    if (len <= 0) return 0;
    while (idx < 0 || idx >= len) {
        if (idx < 0) {
            idx = -1 - idx;
        } else if (idx >= len) {
            idx = 2 * len - 1 - idx;
        }
    }
    return idx;
}

// 2D box filter (uniform filter) with 11x11 window and reflection boundary
std::vector<double> uniform_filter_2d(const std::vector<double>& input, int h, int w, int size = 11) {
    int radius = size / 2;
    std::vector<double> temp(h * w, 0.0);
    std::vector<double> output(h * w, 0.0);

    // Horizontal pass
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            double sum = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                int sc = reflect_index(c + k, w);
                sum += input[r * w + sc];
            }
            temp[r * w + c] = sum / size;
        }
    }

    // Vertical pass
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            double sum = 0.0;
            for (int k = -radius; k <= radius; ++k) {
                int sr = reflect_index(r + k, h);
                sum += temp[sr * w + c];
            }
            output[r * w + c] = sum / size;
        }
    }

    return output;
}

double compute_channel_mse(const YUVFrame& a, const YUVFrame& b, char channel) {
    int w = (channel == 'y') ? a.width : a.chroma_width();
    int h = (channel == 'y') ? a.height : a.chroma_height();
    size_t total = static_cast<size_t>(w) * h;
    if (total == 0) return 0.0;

    double sum_sq_diff = 0.0;
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            double val_a = 0.0;
            double val_b = 0.0;
            if (channel == 'y') {
                val_a = a.get_y(r, c);
                val_b = b.get_y(r, c);
            } else if (channel == 'u') {
                val_a = a.get_u(r, c);
                val_b = b.get_u(r, c);
            } else {
                val_a = a.get_v(r, c);
                val_b = b.get_v(r, c);
            }
            double diff = val_a - val_b;
            sum_sq_diff += diff * diff;
        }
    }
    return sum_sq_diff / total;
}

double mse_to_psnr(double mse, double max_val) {
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return 10.0 * std::log10((max_val * max_val) / mse);
}

} // anonymous namespace

PSNRResult MetricsCalculator::psnr(const YUVFrame& a, const YUVFrame& b) const {
    if (a.width != b.width || a.height != b.height) {
        throw std::invalid_argument("Frame size mismatch");
    }
    if (a.bit_depth != b.bit_depth) {
        throw std::invalid_argument("Bit depth mismatch");
    }

    double max_val = (a.bit_depth == 8) ? 255.0 : 1023.0;

    double mse_y = compute_channel_mse(a, b, 'y');
    double mse_u = compute_channel_mse(a, b, 'u');
    double mse_v = compute_channel_mse(a, b, 'v');

    double psnr_y = mse_to_psnr(mse_y, max_val);
    double psnr_u = mse_to_psnr(mse_u, max_val);
    double psnr_v = mse_to_psnr(mse_v, max_val);

    // Combined MSE with format-specific weights:
    // YUV420: 4:1:1
    // YUV422: 2:1:1
    // YUV444: 1:1:1
    double yw = 1.0, uw = 1.0, vw = 1.0;
    if (a.pixel_format == PixelFormat::YUV420P) {
        yw = 4.0;
    } else if (a.pixel_format == PixelFormat::YUV422P) {
        yw = 2.0;
    }

    double mse_total = (yw * mse_y + uw * mse_u + vw * mse_v) / (yw + uw + vw);
    double psnr_total = mse_to_psnr(mse_total, max_val);

    return {psnr_y, psnr_u, psnr_v, psnr_total};
}

SSIMResult MetricsCalculator::ssim(const YUVFrame& a, const YUVFrame& b) const {
    if (a.width != b.width || a.height != b.height) {
        throw std::invalid_argument("Frame size mismatch");
    }
    if (a.bit_depth != b.bit_depth) {
        throw std::invalid_argument("Bit depth mismatch");
    }

    int w = a.width;
    int h = a.height;
    size_t total = static_cast<size_t>(w) * h;
    if (total == 0) return {1.0};

    double max_val = (a.bit_depth == 8) ? 255.0 : 1023.0;
    double c1 = (0.01 * max_val) * (0.01 * max_val);
    double c2 = (0.03 * max_val) * (0.03 * max_val);

    std::vector<double> a_f(total);
    std::vector<double> b_f(total);
    std::vector<double> a_sq(total);
    std::vector<double> b_sq(total);
    std::vector<double> ab(total);

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            size_t idx = static_cast<size_t>(r) * w + c;
            double va = a.get_y(r, c);
            double vb = b.get_y(r, c);
            a_f[idx] = va;
            b_f[idx] = vb;
            a_sq[idx] = va * va;
            b_sq[idx] = vb * vb;
            ab[idx] = va * vb;
        }
    }

    std::vector<double> mu_a = uniform_filter_2d(a_f, h, w, 11);
    std::vector<double> mu_b = uniform_filter_2d(b_f, h, w, 11);
    std::vector<double> sigma_a_sq = uniform_filter_2d(a_sq, h, w, 11);
    std::vector<double> sigma_b_sq = uniform_filter_2d(b_sq, h, w, 11);
    std::vector<double> sigma_ab = uniform_filter_2d(ab, h, w, 11);

    double sum_ssim = 0.0;
    for (size_t i = 0; i < total; ++i) {
        double m_a = mu_a[i];
        double m_b = mu_b[i];
        double m_a_sq = m_a * m_a;
        double m_b_sq = m_b * m_b;
        double m_ab = m_a * m_b;

        double s_a_sq = sigma_a_sq[i] - m_a_sq;
        double s_b_sq = sigma_b_sq[i] - m_b_sq;
        double s_ab = sigma_ab[i] - m_ab;

        double num = (2.0 * m_ab + c1) * (2.0 * s_ab + c2);
        double den = (m_a_sq + m_b_sq + c1) * (s_a_sq + s_b_sq + c2);
        sum_ssim += (num / den);
    }

    return {sum_ssim / total};
}

} // namespace yuvdiff
