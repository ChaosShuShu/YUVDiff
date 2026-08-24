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
