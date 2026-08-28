#include "test_framework.hpp"
#include "yuvdiff/formats.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/simd.hpp"
#include "yuvdiff/tools.hpp"

#include <cstdio>
#include <fstream>
#include <vector>
#include <random>
#include <unistd.h>

namespace {

std::string create_temp_file(const std::string& prefix) {
    (void)prefix;
    char name_template[] = "/tmp/yuvdiff_test_XXXXXX";
    int fd = mkstemp(name_template);
    if (fd != -1) {
        ::close(fd);
    }
    return std::string(name_template);
}

} // anonymous namespace

TEST_CASE(test_yuvcut_8bit) {
    std::string in_path = create_temp_file("in");
    std::string out_path = create_temp_file("out");

    int w = 16;
    int h = 16;
    size_t fbytes = yuvdiff::frame_bytes(yuvdiff::PixelFormat::YUV420P, w, h, yuvdiff::BitDepth::BIT8);
    ASSERT_EQ(fbytes, 16 * 16 * 3 / 2); // 384 bytes

    // Write 5 distinct frames
    {
        std::ofstream ofs(in_path, std::ios::binary);
        for (int f = 0; f < 5; ++f) {
            std::vector<uint8_t> frame(fbytes, static_cast<uint8_t>(f * 20));
            ofs.write(reinterpret_cast<const char*>(frame.data()), fbytes);
        }
    }

    // Cut frames [1, 1+3) -> frames 1, 2, 3 (3 frames)
    yuvdiff::CutOptions opts;
    opts.input_path = in_path;
    opts.output_path = out_path;
    opts.width = w;
    opts.height = h;
    opts.format_str = "YUV420P8";
    opts.start_frame = 1;
    opts.num_frames = 3;

    size_t written = yuvdiff::yuvcut(opts);
    ASSERT_EQ(written, 3LL);

    // Verify output file contents
    {
        std::ifstream ifs(out_path, std::ios::binary);
        std::vector<uint8_t> buf(fbytes);
        for (int f = 0; f < 3; ++f) {
            ifs.read(reinterpret_cast<char*>(buf.data()), fbytes);
            ASSERT_EQ(ifs.gcount(), static_cast<std::streamsize>(fbytes));
            uint8_t expected_val = static_cast<uint8_t>((f + 1) * 20);
            for (size_t b = 0; b < fbytes; ++b) {
                ASSERT_EQ(buf[b], expected_val);
            }
        }
    }

    std::remove(in_path.c_str());
    std::remove(out_path.c_str());
}

TEST_CASE(test_simd_sq_diff_sum_8bit) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<size_t> test_sizes = {1, 7, 16, 31, 32, 33, 64, 100, 1024, 1920 * 1080};

    for (size_t n : test_sizes) {
        std::vector<uint8_t> a(n);
        std::vector<uint8_t> b(n);
        for (size_t i = 0; i < n; ++i) {
            a[i] = static_cast<uint8_t>(dist(rng));
            b[i] = static_cast<uint8_t>(dist(rng));
        }

        uint64_t expected_sum = 0;
        for (size_t i = 0; i < n; ++i) {
            int diff = static_cast<int>(a[i]) - static_cast<int>(b[i]);
            expected_sum += static_cast<uint64_t>(diff * diff);
        }

        uint64_t simd_sum = yuvdiff::simd::sq_diff_sum_8bit(a.data(), b.data(), n);
        ASSERT_EQ(simd_sum, expected_sum);
    }
}

TEST_CASE(test_simd_sq_diff_sum_16bit) {
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> dist(0, 1023);

    std::vector<size_t> test_sizes = {1, 7, 15, 16, 17, 32, 48, 100, 1024, 1920 * 1080};

    for (size_t n : test_sizes) {
        std::vector<uint16_t> a(n);
        std::vector<uint16_t> b(n);
        for (size_t i = 0; i < n; ++i) {
            a[i] = static_cast<uint16_t>(dist(rng));
            b[i] = static_cast<uint16_t>(dist(rng));
        }

        uint64_t expected_sum = 0;
        for (size_t i = 0; i < n; ++i) {
            int diff = static_cast<int>(a[i]) - static_cast<int>(b[i]);
            expected_sum += static_cast<uint64_t>(diff * diff);
        }

        uint64_t simd_sum = yuvdiff::simd::sq_diff_sum_16bit(a.data(), b.data(), n);
        ASSERT_EQ(simd_sum, expected_sum);
    }
}
