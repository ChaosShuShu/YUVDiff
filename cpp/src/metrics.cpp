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

// O(1) sliding-window 2D box filter with reflection boundary
void uniform_filter_2d(
    const double* input,
    double* output,
    double* temp,
    int h,
    int w,
    int size = 11
) {
    int radius = size / 2;
    double inv_size = 1.0 / size;

    // Horizontal pass
    for (int r = 0; r < h; ++r) {
        const double* in_row = input + r * w;
        double* tmp_row = temp + r * w;

        // Initialize sliding sum for c = 0 (window [-radius, +radius])
        double sum = 0.0;
        for (int k = -radius; k <= radius; ++k) {
            sum += in_row[reflect_index(k, w)];
        }
        tmp_row[0] = sum * inv_size;

        // Slide window horizontally across row
        for (int c = 1; c < w; ++c) {
            int in_idx = reflect_index(c + radius, w);
            int out_idx = reflect_index(c - radius - 1, w);
            sum += in_row[in_idx] - in_row[out_idx];
            tmp_row[c] = sum * inv_size;
        }
    }

    // Vertical pass
    for (int c = 0; c < w; ++c) {
        // Initialize sliding sum for r = 0 (window [-radius, +radius])
        double sum = 0.0;
        for (int k = -radius; k <= radius; ++k) {
            int sr = reflect_index(k, h);
            sum += temp[sr * w + c];
        }
        output[0 * w + c] = sum * inv_size;

        // Slide window vertically down column
        for (int r = 1; r < h; ++r) {
            int in_r = reflect_index(r + radius, h);
            int out_r = reflect_index(r - radius - 1, h);
            sum += temp[in_r * w + c] - temp[out_r * w + c];
            output[r * w + c] = sum * inv_size;
        }
    }
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

    std::vector<double> buf_in(total);
    std::vector<double> temp(total);
    std::vector<double> mu_a(total);
    std::vector<double> mu_b(total);
    std::vector<double> sigma_a_sq(total);
    std::vector<double> sigma_b_sq(total);
    std::vector<double> sigma_ab(total);

    // 1. mu_a = E[A]
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            buf_in[r * w + c] = static_cast<double>(a.get_y(r, c));
        }
    }
    uniform_filter_2d(buf_in.data(), mu_a.data(), temp.data(), h, w, 11);

    // 2. E[A^2]
    for (size_t i = 0; i < total; ++i) {
        buf_in[i] = buf_in[i] * buf_in[i];
    }
    uniform_filter_2d(buf_in.data(), sigma_a_sq.data(), temp.data(), h, w, 11);

    // 3. mu_b = E[B]
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            buf_in[r * w + c] = static_cast<double>(b.get_y(r, c));
        }
    }
    uniform_filter_2d(buf_in.data(), mu_b.data(), temp.data(), h, w, 11);

    // 4. E[B^2]
    for (size_t i = 0; i < total; ++i) {
        buf_in[i] = buf_in[i] * buf_in[i];
    }
    uniform_filter_2d(buf_in.data(), sigma_b_sq.data(), temp.data(), h, w, 11);

    // 5. E[AB]
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            buf_in[r * w + c] = static_cast<double>(a.get_y(r, c)) * static_cast<double>(b.get_y(r, c));
        }
    }
    uniform_filter_2d(buf_in.data(), sigma_ab.data(), temp.data(), h, w, 11);

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
