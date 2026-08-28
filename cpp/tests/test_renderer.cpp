#include "test_framework.hpp"
#include "yuvdiff/diff.hpp"
#include "yuvdiff/renderer.hpp"

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

TEST_CASE(test_renderer_yuv_to_rgb_neutral) {
    // Neutral gray: Y=128, U=128, V=128 -> RGB around (128, 128, 128)
    YUVFrame a = make_frame(16, 16, 128, 128, 128);

    Renderer r(16, 16);
    RgbImage img = r.render(a, nullptr, nullptr, RenderMode::ORIGINAL_A);

    ASSERT_EQ(img.width, 16);
    ASSERT_EQ(img.height, 16);
    ASSERT_EQ(img.data.size(), 16ULL * 16 * 3);

    // Check center pixel
    uint8_t red = img.data[0];
    uint8_t green = img.data[1];
    uint8_t blue = img.data[2];

    ASSERT_EQ(red, 128);
    ASSERT_EQ(green, 128);
    ASSERT_EQ(blue, 128);
}

TEST_CASE(test_renderer_heatmap_zero_diff) {
    YUVFrame a = make_frame(16, 16, 128, 128, 128);
    YUVFrame b = make_frame(16, 16, 128, 128, 128); // 0 diff -> dark background

    DiffEngine engine(4);
    DiffResult diff = engine.diff(a, b);

    Renderer r(16, 16);
    RgbImage img = r.render(a, &b, &diff, RenderMode::HEATMAP);

    ASSERT_EQ(img.width, 16);
    ASSERT_EQ(img.height, 16);

    // Zero diff heatmap: dark canvas
    ASSERT_EQ(img.data[0], 15);
    ASSERT_EQ(img.data[1], 15);
    ASSERT_EQ(img.data[2], 20);
}

TEST_CASE(test_renderer_heatmap_max_diff) {
    YUVFrame a = make_frame(16, 16, 0, 128, 128);
    YUVFrame b = make_frame(16, 16, 255, 128, 128); // max diff -> intense hot red/white

    DiffEngine engine(4);
    DiffResult diff = engine.diff(a, b);

    Renderer r(16, 16);
    RgbImage img = r.render(a, &b, &diff, RenderMode::HEATMAP);

    ASSERT_EQ(img.width, 16);
    ASSERT_EQ(img.height, 16);

    // Max diff heatmap: Red channel is saturated
    ASSERT_EQ(img.data[0], 255);
}

TEST_CASE(test_renderer_threshold_mask) {
    YUVFrame a = make_frame(16, 16, 100, 128, 128);
    YUVFrame b = make_frame(16, 16, 100, 128, 128);

    // Set top half diff
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 16; ++c) {
            b.y8[r * 16 + c] = 200; // diff = 100 > 4
        }
    }

    DiffEngine engine(4);
    DiffResult diff = engine.diff(a, b);

    Renderer r(16, 16);
    RgbImage img = r.render(a, &b, &diff, RenderMode::THRESHOLD_MASK);

    // Top pixel (r=4, c=4) is masked -> has red overlay
    size_t top_idx = (4 * 16 + 4) * 3;
    uint8_t top_r = img.data[top_idx + 0];
    uint8_t top_g = img.data[top_idx + 1];
    uint8_t top_b = img.data[top_idx + 2];
    ASSERT_TRUE(top_r > top_g);
    ASSERT_TRUE(top_r > top_b);

    // Bottom pixel (r=12, c=4) is not masked -> original neutral gray (100, 100, 100)
    size_t bot_idx = (12 * 16 + 4) * 3;
    uint8_t bot_r = img.data[bot_idx + 0];
    uint8_t bot_g = img.data[bot_idx + 1];
    uint8_t bot_b = img.data[bot_idx + 2];
    ASSERT_EQ(bot_r, 100);
    ASSERT_EQ(bot_g, 100);
    ASSERT_EQ(bot_b, 100);
}

#include "yuvdiff/cache.hpp"

TEST_CASE(test_frame_cache_lru) {
    FrameCache cache(3); // Capacity 3

    CachedFrame f1; f1.frame_idx = 1; f1.mode = RenderMode::ORIGINAL_A; f1.threshold = 4;
    CachedFrame f2; f2.frame_idx = 2; f2.mode = RenderMode::ORIGINAL_A; f2.threshold = 4;
    CachedFrame f3; f3.frame_idx = 3; f3.mode = RenderMode::ORIGINAL_A; f3.threshold = 4;
    CachedFrame f4; f4.frame_idx = 4; f4.mode = RenderMode::ORIGINAL_A; f4.threshold = 4;

    cache.put(f1);
    cache.put(f2);
    cache.put(f3);

    ASSERT_EQ(cache.size(), 3ULL);
    ASSERT_TRUE(cache.get(1, RenderMode::ORIGINAL_A, 4).has_value());

    // Insert 4th item -> should evict item 2 (because item 1 was just accessed, so 2 is oldest)
    cache.put(f4);
    ASSERT_EQ(cache.size(), 3ULL);
    ASSERT_TRUE(cache.get(1, RenderMode::ORIGINAL_A, 4).has_value());
    ASSERT_FALSE(cache.get(2, RenderMode::ORIGINAL_A, 4).has_value()); // Evicted!
    ASSERT_TRUE(cache.get(3, RenderMode::ORIGINAL_A, 4).has_value());
    ASSERT_TRUE(cache.get(4, RenderMode::ORIGINAL_A, 4).has_value());

    cache.clear();
    ASSERT_EQ(cache.size(), 0ULL);
}
