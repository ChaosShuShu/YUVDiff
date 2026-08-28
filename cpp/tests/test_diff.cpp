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
    ASSERT_EQ(res.diff_pixel_count, 0LL); // diff == 4 is not > 4

    YUVFrame c = make_frame(16, 16, 105, 128, 128); // diff == 5 > 4
    DiffResult res2 = engine.diff(a, c);
    ASSERT_EQ(res2.diff_pixel_count, 256LL);
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

    // Set specific diffs: 0, 10, 20, 30 across 16 pixels
    // 4 pixels with diff 0 (10 vs 10)
    // 4 pixels with diff 10 (10 vs 20)
    // 4 pixels with diff 20 (10 vs 30)
    // 4 pixels with diff 30 (10 vs 40)
    for (int i = 4; i < 8; ++i) b.y8[i] = 20;
    for (int i = 8; i < 12; ++i) b.y8[i] = 30;
    for (int i = 12; i < 16; ++i) b.y8[i] = 40;

    DiffEngine engine(4);
    DiffResult res = engine.diff(a, b);

    ASSERT_EQ(res.diff_min, 0);
    ASSERT_EQ(res.diff_max, 30);
    // Mean = (0*4 + 10*4 + 20*4 + 30*4) / 16 = 240 / 16 = 15.0
    ASSERT_NEAR(res.diff_mean, 15.0, 0.001);
    // Diffs: [0,0,0,0, 10,10,10,10, 20,20,20,20, 30,30,30,30]
    // 16 elements: element 8 is 10, element 9 is 20 -> median = 15.0
    ASSERT_NEAR(res.diff_median, 15.0, 0.001);
    ASSERT_EQ(res.diff_pixel_count, 12LL); // 12 pixels > 4
    ASSERT_EQ(res.total_pixel_count, 16LL);
}
