#include "test_framework.hpp"
#include "yuvdiff/parser.hpp"

#include <cstdio>
#include <fstream>
#include <vector>

using namespace yuvdiff;

namespace {

std::string create_temp_yuv(
    const std::string& filename,
    PixelFormat fmt,
    BitDepth depth,
    int width,
    int height,
    size_t num_frames,
    BitAlignment align = BitAlignment::MSB
) {
    size_t fb = frame_bytes(fmt, width, height, depth);
    std::vector<uint8_t> frame_buf(fb);

    auto [hf, vf] = chroma_subsampling(fmt);
    int cw = width / hf;
    int ch = height / vf;

    if (depth == BitDepth::BIT8) {
        // Y = 120, U = 130, V = 140
        size_t y_sz = width * height;
        size_t uv_sz = cw * ch;
        std::fill(frame_buf.begin(), frame_buf.begin() + y_sz, 120);
        std::fill(frame_buf.begin() + y_sz, frame_buf.begin() + y_sz + uv_sz, 130);
        std::fill(frame_buf.begin() + y_sz + uv_sz, frame_buf.end(), 140);
    } else {
        uint16_t* ptr = reinterpret_cast<uint16_t*>(frame_buf.data());
        size_t y_sz = width * height;
        size_t uv_sz = cw * ch;
        uint16_t y_val = (align == BitAlignment::MSB) ? (500 << 6) : 500;
        uint16_t u_val = (align == BitAlignment::MSB) ? (600 << 6) : 600;
        uint16_t v_val = (align == BitAlignment::MSB) ? (700 << 6) : 700;

        std::fill(ptr, ptr + y_sz, y_val);
        std::fill(ptr + y_sz, ptr + y_sz + uv_sz, u_val);
        std::fill(ptr + y_sz + uv_sz, ptr + y_sz + 2 * uv_sz, v_val);
    }

    std::ofstream out(filename, std::ios::binary);
    for (size_t i = 0; i < num_frames; ++i) {
        out.write(reinterpret_cast<const char*>(frame_buf.data()), frame_buf.size());
    }
    out.close();
    return filename;
}

} // anonymous namespace

TEST_CASE(test_parser_8bit_read) {
    std::string path = "/tmp/test_yuv_8bit.yuv";
    create_temp_yuv(path, PixelFormat::YUV420P, BitDepth::BIT8, 32, 16, 3);

    YUVParser parser(path, "YUV420P8", 32, 16);
    ASSERT_EQ(parser.num_frames(), 3ULL);
    ASSERT_EQ(parser.width(), 32);
    ASSERT_EQ(parser.height(), 16);

    YUVFrame frame = parser.read_frame(0);
    ASSERT_EQ(frame.width, 32);
    ASSERT_EQ(frame.height, 16);
    ASSERT_EQ(frame.bit_depth, 8);
    ASSERT_EQ(frame.y8.size(), 32ULL * 16);
    ASSERT_EQ(frame.u8.size(), 16ULL * 8);
    ASSERT_EQ(frame.v8.size(), 16ULL * 8);

    ASSERT_EQ(frame.get_y(0, 0), 120);
    ASSERT_EQ(frame.get_u(0, 0), 130);
    ASSERT_EQ(frame.get_v(0, 0), 140);

    ASSERT_THROWS(parser.read_frame(3), std::out_of_range);

    parser.close();
    std::remove(path.c_str());
}

TEST_CASE(test_parser_10bit_msb_read) {
    std::string path = "/tmp/test_yuv_10bit_msb.yuv";
    create_temp_yuv(path, PixelFormat::YUV420P, BitDepth::BIT10LE, 32, 16, 2, BitAlignment::MSB);

    YUVParser parser(path, "YUV420P10LE", 32, 16, BitAlignment::MSB);
    ASSERT_EQ(parser.num_frames(), 2ULL);

    YUVFrame frame = parser.read_frame(0);
    ASSERT_EQ(frame.bit_depth, 10);
    ASSERT_EQ(frame.get_y(0, 0), 500);
    ASSERT_EQ(frame.get_u(0, 0), 600);
    ASSERT_EQ(frame.get_v(0, 0), 700);

    parser.close();
    std::remove(path.c_str());
}

TEST_CASE(test_parser_10bit_lsb_read) {
    std::string path = "/tmp/test_yuv_10bit_lsb.yuv";
    create_temp_yuv(path, PixelFormat::YUV420P, BitDepth::BIT10LE, 32, 16, 2, BitAlignment::LSB);

    YUVParser parser(path, "YUV420P10LE", 32, 16, BitAlignment::LSB);
    ASSERT_EQ(parser.num_frames(), 2ULL);

    YUVFrame frame = parser.read_frame(0);
    ASSERT_EQ(frame.bit_depth, 10);
    ASSERT_EQ(frame.get_y(0, 0), 500);
    ASSERT_EQ(frame.get_u(0, 0), 600);
    ASSERT_EQ(frame.get_v(0, 0), 700);

    parser.close();
    std::remove(path.c_str());
}

TEST_CASE(test_auto_detect_alignment) {
    std::string path_msb = "/tmp/test_autodetect_msb.yuv";
    create_temp_yuv(path_msb, PixelFormat::YUV420P, BitDepth::BIT10LE, 32, 16, 5, BitAlignment::MSB);
    ASSERT_TRUE(auto_detect_alignment(path_msb, "YUV420P10LE", 32, 16) == BitAlignment::MSB);
    std::remove(path_msb.c_str());

    std::string path_lsb = "/tmp/test_autodetect_lsb.yuv";
    create_temp_yuv(path_lsb, PixelFormat::YUV420P, BitDepth::BIT10LE, 32, 16, 5, BitAlignment::LSB);
    ASSERT_TRUE(auto_detect_alignment(path_lsb, "YUV420P10LE", 32, 16) == BitAlignment::LSB);
    std::remove(path_lsb.c_str());
}
