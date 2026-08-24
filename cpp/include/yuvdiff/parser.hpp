#pragma once

#include "yuvdiff/formats.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace yuvdiff {

struct YUVFrame {
    int width = 0;
    int height = 0;
    int bit_depth = 8;
    PixelFormat pixel_format = PixelFormat::YUV420P;

    // Data for 8-bit frames (empty if bit_depth == 10)
    std::vector<uint8_t> y8;
    std::vector<uint8_t> u8;
    std::vector<uint8_t> v8;

    // Data for 10-bit frames, stored as 16-bit integers in [0, 1023] (empty if bit_depth == 8)
    std::vector<uint16_t> y16;
    std::vector<uint16_t> u16;
    std::vector<uint16_t> v16;

    int chroma_width() const {
        auto [hf, vf] = chroma_subsampling(pixel_format);
        return width / hf;
    }

    int chroma_height() const {
        auto [hf, vf] = chroma_subsampling(pixel_format);
        return height / vf;
    }

    inline uint16_t get_y(int r, int c) const {
        return bit_depth == 8 ? y8[r * width + c] : y16[r * width + c];
    }

    inline uint16_t get_u(int r, int c) const {
        return bit_depth == 8 ? u8[r * chroma_width() + c] : u16[r * chroma_width() + c];
    }

    inline uint16_t get_v(int r, int c) const {
        return bit_depth == 8 ? v8[r * chroma_width() + c] : v16[r * chroma_width() + c];
    }
};

BitAlignment auto_detect_alignment(
    const std::string& path,
    std::string_view fmt,
    int width,
    int height,
    size_t sample_bytes = 4096
);

class YUVParser {
public:
    static constexpr size_t MMAP_THRESHOLD = 1024 * 1024; // 1 MB

    YUVParser(
        const std::string& path,
        std::string_view fmt,
        int width,
        int height,
        BitAlignment bit_alignment = BitAlignment::MSB
    );

    // Constructor supporting alignment string ("auto", "msb", "lsb")
    YUVParser(
        const std::string& path,
        std::string_view fmt,
        int width,
        int height,
        std::string_view bit_alignment_str
    );

    ~YUVParser();

    // Disable copy, enable move
    YUVParser(const YUVParser&) = delete;
    YUVParser& operator=(const YUVParser&) = delete;
    YUVParser(YUVParser&& other) noexcept;
    YUVParser& operator=(YUVParser&& other) noexcept;

    YUVFrame read_frame(size_t idx);

    size_t num_frames() const { return num_frames_; }
    int width() const { return width_; }
    int height() const { return height_; }
    PixelFormat pixel_format() const { return pixel_format_; }
    BitDepth bit_depth() const { return bit_depth_; }
    BitAlignment bit_alignment() const { return bit_alignment_; }
    const std::string& path() const { return path_; }

    void close();

private:
    std::string path_;
    int width_ = 0;
    int height_ = 0;
    PixelFormat pixel_format_ = PixelFormat::YUV420P;
    BitDepth bit_depth_ = BitDepth::BIT8;
    BitAlignment bit_alignment_ = BitAlignment::MSB;
    size_t frame_bytes_ = 0;
    size_t file_size_ = 0;
    size_t num_frames_ = 0;

    int fd_ = -1;
    void* mmap_data_ = nullptr;

    void init_file();
};

} // namespace yuvdiff
