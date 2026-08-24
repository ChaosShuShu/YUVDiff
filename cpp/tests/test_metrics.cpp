#include "test_framework.hpp"
#include "yuvdiff/metrics.hpp"

#include <cmath>

using namespace yuvdiff;

namespace {

YUVFrame make_frame(int w, int h, uint8_t y_val, uint8_t u_val, uint8_t v_val, PixelFormat fmt = PixelFormat::YUV420P) {
    YUVFrame f;
    f.width = w;
    f.height = h;
    f.bit_depth = 8;
    f.pixel_format = fmt;

    auto [hf, vf] = chroma_subsampling(fmt);
    int cw = w / hf;
    int ch = h / vf;

    f.y8.resize(w * h, y_val);
    f.u8.resize(cw * ch, u_val);
    f.v8.resize(cw * ch, v_val);
    return f;
}

YUVFrame make_frame10(int w, int h, uint16_t y_val, uint16_t u_val, uint16_t v_val, PixelFormat fmt = PixelFormat::YUV420P) {
    YUVFrame f;
    f.width = w;
    f.height = h;
    f.bit_depth = 10;
    f.pixel_format = fmt;

    auto [hf, vf] = chroma_subsampling(fmt);
    int cw = w / hf;
    int ch = h / vf;

    f.y16.resize(w * h, y_val);
    f.u16.resize(cw * ch, u_val);
    f.v16.resize(cw * ch, v_val);
    return f;
}

} // anonymous namespace

TEST_CASE(test_metrics_identical_frames) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128);
    YUVFrame b = make_frame(16, 16, 100, 128, 128);

    MetricsCalculator calc;
    PSNRResult psnr = calc.psnr(a, b);
    ASSERT_TRUE(std::isinf(psnr.y));
    ASSERT_TRUE(std::isinf(psnr.u));
    ASSERT_TRUE(std::isinf(psnr.v));
    ASSERT_TRUE(std::isinf(psnr.total));

    SSIMResult ssim = calc.ssim(a, b);
    ASSERT_NEAR(ssim.y, 1.0, 1e-6);
}

TEST_CASE(test_metrics_known_psnr_8bit) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128);
    YUVFrame b = make_frame(16, 16, 110, 128, 128); // Y diff = 10, MSE = 100

    MetricsCalculator calc;
    PSNRResult psnr = calc.psnr(a, b);

    // 10 * log10(255^2 / 100) = 28.1308036...
    double expected_psnr_y = 10.0 * std::log10(65025.0 / 100.0);
    ASSERT_NEAR(psnr.y, expected_psnr_y, 1e-4);
    ASSERT_TRUE(std::isinf(psnr.u));
    ASSERT_TRUE(std::isinf(psnr.v));

    // For 420P: MSE_total = (4 * 100 + 0 + 0) / 6 = 400 / 6 = 66.66666...
    double expected_psnr_total = 10.0 * std::log10(65025.0 / (400.0 / 6.0));
    ASSERT_NEAR(psnr.total, expected_psnr_total, 1e-4);
}

TEST_CASE(test_metrics_known_psnr_10bit) {
    YUVFrame a = make_frame10(16, 16, 500, 512, 512);
    YUVFrame b = make_frame10(16, 16, 510, 512, 512); // diff = 10, MAX = 1023

    MetricsCalculator calc;
    PSNRResult psnr = calc.psnr(a, b);

    // 10 * log10(1023^2 / 100) = 10 * log10(1046529 / 100) = 40.1975
    double expected_psnr_y = 10.0 * std::log10(1046529.0 / 100.0);
    ASSERT_NEAR(psnr.y, expected_psnr_y, 1e-4);
}

TEST_CASE(test_metrics_ssim_small_diff) {
    YUVFrame a = make_frame(32, 32, 128, 128, 128);
    YUVFrame b = make_frame(32, 32, 129, 128, 128); // diff = 1

    MetricsCalculator calc;
    SSIMResult ssim = calc.ssim(a, b);

    ASSERT_TRUE(ssim.y > 0.99);
}
