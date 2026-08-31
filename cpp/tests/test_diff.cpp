#include "test_framework.hpp"
#include "yuvdiff/diff.hpp"

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

} // anonymous namespace

TEST_CASE(test_diff_identical_frames) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128);
    YUVFrame b = make_frame(16, 16, 100, 128, 128);

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);

    ASSERT_EQ(res.diff_pixel_count, 0LL);
    ASSERT_EQ(res.total_pixel_count, 256LL);
    for (int v : res.diff_y) ASSERT_EQ(v, 0);
    for (int v : res.diff_u) ASSERT_EQ(v, 0);
    for (int v : res.diff_v) ASSERT_EQ(v, 0);
    for (uint8_t m : res.mask) ASSERT_EQ(m, 0);
}

TEST_CASE(test_diff_black_vs_white) {
    YUVFrame a = make_frame(16, 16, 0, 0, 0);
    YUVFrame b = make_frame(16, 16, 255, 255, 255);

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);

    ASSERT_EQ(res.diff_pixel_count, 256LL);
    for (int v : res.diff_y) ASSERT_EQ(v, 255);
    for (uint8_t m : res.mask) ASSERT_EQ(m, 1);
}

TEST_CASE(test_diff_threshold_boundary) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128);
    YUVFrame b = make_frame(16, 16, 104, 128, 128); // diff == 4

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);
    ASSERT_EQ(res.diff_pixel_count, 256LL); // diff == 4 > 0
    ASSERT_EQ(res.diff_gt_2t, 0LL);        // diff == 4 is not > 8 (2t)
    ASSERT_EQ(res.diff_gt_t, 0LL);         // diff == 4 is not > 4 (t)
    ASSERT_EQ(res.diff_gt_half_t, 256LL);  // diff == 4 > 2 (t/2)

    YUVFrame c = make_frame(16, 16, 105, 128, 128); // diff == 5 > 4
    DiffResult res2 = engine.diff(a, c);
    ASSERT_EQ(res2.diff_pixel_count, 256LL);
    ASSERT_EQ(res2.diff_gt_t, 256LL);
    ASSERT_EQ(res2.diff_gt_2t, 0LL);       // diff == 5 is not > 8 (2t)

    YUVFrame d = make_frame(16, 16, 109, 128, 128); // diff == 9 > 8 (2t)
    DiffResult res3 = engine.diff(a, d);
    ASSERT_EQ(res3.diff_gt_2t, 256LL);
}

TEST_CASE(test_diff_chroma_upsampling) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128, PixelFormat::YUV420P);
    YUVFrame b = make_frame(16, 16, 100, 128, 128, PixelFormat::YUV420P);

    // Modify only one chroma pixel in U
    b.u8[0] = 200; // diff = 72 > 4

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);

    // In 420P, one chroma pixel corresponds to a 2x2 block in Y resolution = 4 pixels
    ASSERT_EQ(res.diff_pixel_count, 4LL);
    ASSERT_EQ(res.diff_gt_t, 4LL);
    ASSERT_EQ(res.diff_gt_2t, 4LL);
}

TEST_CASE(test_diff_cross_depth) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128); // 8-bit Y=100 -> equivalent 10-bit Y=400
    YUVFrame b;
    b.width = 16;
    b.height = 16;
    b.bit_depth = 10;
    b.pixel_format = PixelFormat::YUV420P;
    b.y16.resize(16 * 16, 400); // exactly matching
    b.u16.resize(8 * 8, 512);   // 128 * 4 = 512
    b.v16.resize(8 * 8, 512);

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);
    ASSERT_EQ(res.diff_pixel_count, 0LL);
}

TEST_CASE(test_diff_cross_chroma_format) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128, PixelFormat::YUV420P);
    YUVFrame b = make_frame(16, 16, 100, 128, 128, PixelFormat::YUV422P);

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);
    ASSERT_EQ(res.diff_pixel_count, 0LL);
}

TEST_CASE(test_diff_statistics) {
    YUVFrame a = make_frame(4, 4, 10, 128, 128);
    YUVFrame b = make_frame(4, 4, 10, 128, 128);

    // Set specific diffs: 0, 2, 4, 10 across 16 pixels
    // 4 pixels with diff 0 (10 vs 10)
    // 4 pixels with diff 2 (10 vs 12)
    // 4 pixels with diff 4 (10 vs 14)
    // 4 pixels with diff 10 (10 vs 20)
    for (int i = 4; i < 8; ++i) b.y8[i] = 12;
    for (int i = 8; i < 12; ++i) b.y8[i] = 14;
    for (int i = 12; i < 16; ++i) b.y8[i] = 20;

    DiffEngine engine(4); // t=4, t/2=2, 2t=8
    DiffResult res = engine.diff(a, b);

    // Non-zero diff pixels (d > 0): 12 pixels (values: 4x2, 4x4, 4x10)
    ASSERT_EQ(res.diff_pixel_count, 12LL);
    ASSERT_EQ(res.total_pixel_count, 16LL);
    ASSERT_NEAR(res.diff_ratio, 12.0 / 16.0, 0.0001);

    // d > 8 (2t): only the 4 pixels with diff 10
    ASSERT_EQ(res.diff_gt_2t, 4LL);
    ASSERT_NEAR(res.diff_gt_2t_ratio, 4.0 / 16.0, 0.0001);

    // d > 4 (t): only the 4 pixels with diff 10
    ASSERT_EQ(res.diff_gt_t, 4LL);
    ASSERT_NEAR(res.diff_gt_t_ratio, 4.0 / 16.0, 0.0001);

    // d > 2 (t/2): the 4 pixels with diff 4 and 4 pixels with diff 10 = 8 pixels
    ASSERT_EQ(res.diff_gt_half_t, 8LL);
    ASSERT_NEAR(res.diff_gt_half_t_ratio, 8.0 / 16.0, 0.0001);

    // Stats over d > 0 pixels:
    ASSERT_EQ(res.diff_min, 2);
    ASSERT_EQ(res.diff_max, 10);
    // Mean = (2*4 + 4*4 + 10*4) / 12 = (8 + 16 + 40) / 12 = 64 / 12 = 5.3333...
    ASSERT_NEAR(res.diff_mean, 64.0 / 12.0, 0.001);
    // Diffs sorted: [2,2,2,2, 4,4,4,4, 10,10,10,10] -> median = 4.0
    ASSERT_NEAR(res.diff_median, 4.0, 0.001);
}
