#pragma once

#include "yuvdiff/diff.hpp"
#include "yuvdiff/parser.hpp"

#include <cstdint>
#include <vector>

namespace yuvdiff {

enum class RenderMode {
    ORIGINAL_A,
    ORIGINAL_B,
    HEATMAP,
    THRESHOLD_MASK,
    SIDE_BY_SIDE,
    COMPARISON
};

struct RgbImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data; // RGB 24-bit (3 bytes per pixel: R, G, B)

    const uint8_t* scanline(int r) const {
        return data.data() + static_cast<size_t>(r) * width * 3;
    }
};

class Renderer {
public:
    Renderer(int width, int height);

    RgbImage render(
        const YUVFrame& frame_a,
        const YUVFrame* frame_b,
        const DiffResult* diff,
        RenderMode mode,
        int threshold = 4
    ) const;

    RgbImage yuv_to_rgb(const YUVFrame& frame) const;
    RgbImage render_heatmap(const DiffResult& diff, int bit_depth) const;
    RgbImage render_mask(const RgbImage& rgb_a, const DiffResult& diff) const;

private:
    int width_ = 0;
    int height_ = 0;
};

} // namespace yuvdiff
