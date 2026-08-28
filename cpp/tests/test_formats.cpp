#include "test_framework.hpp"
#include "yuvdiff/formats.hpp"

using namespace yuvdiff;

TEST_CASE(test_format_parse_8bit) {
    // 1. Case-insensitivity
    auto [f1, d1] = parse_format("YUV420P8");
    ASSERT_TRUE(f1 == PixelFormat::YUV420P);
    ASSERT_TRUE(d1 == BitDepth::BIT8);

    auto [f1_lower, d1_lower] = parse_format("yuv420p8");
    ASSERT_TRUE(f1_lower == PixelFormat::YUV420P);
    ASSERT_TRUE(d1_lower == BitDepth::BIT8);

    auto [f1_mixed, d1_mixed] = parse_format("yUv420p8");
    ASSERT_TRUE(f1_mixed == PixelFormat::YUV420P);
    ASSERT_TRUE(d1_mixed == BitDepth::BIT8);

    auto [f2, d2] = parse_format("yuv422p8");
    ASSERT_TRUE(f2 == PixelFormat::YUV422P);
    ASSERT_TRUE(d2 == BitDepth::BIT8);

    auto [f3, d3] = parse_format("yuv444p8");
    ASSERT_TRUE(f3 == PixelFormat::YUV444P);
    ASSERT_TRUE(d3 == BitDepth::BIT8);

    // 2. Default bit depth to 8-bit when bit depth is omitted
    auto [f4, d4] = parse_format("yuv420p");
    ASSERT_TRUE(f4 == PixelFormat::YUV420P);
    ASSERT_TRUE(d4 == BitDepth::BIT8);

    auto [f5, d5] = parse_format("YUV422P");
    ASSERT_TRUE(f5 == PixelFormat::YUV422P);
    ASSERT_TRUE(d5 == BitDepth::BIT8);

    auto [f6, d6] = parse_format("444p");
    ASSERT_TRUE(f6 == PixelFormat::YUV444P);
    ASSERT_TRUE(d6 == BitDepth::BIT8);

    auto [f7, d7] = parse_format("i420");
    ASSERT_TRUE(f7 == PixelFormat::YUV420P);
    ASSERT_TRUE(d7 == BitDepth::BIT8);

    auto [f8, d8] = parse_format("nv12");
    ASSERT_TRUE(f8 == PixelFormat::YUV420P);
    ASSERT_TRUE(d8 == BitDepth::BIT8);
}

TEST_CASE(test_format_parse_10bit_le) {
    // 1. Case-insensitivity with 10le
    auto [f1, d1] = parse_format("YUV420P10LE");
    ASSERT_TRUE(f1 == PixelFormat::YUV420P);
    ASSERT_TRUE(d1 == BitDepth::BIT10LE);

    auto [f1_lower, d1_lower] = parse_format("yuv420p10le");
    ASSERT_TRUE(f1_lower == PixelFormat::YUV420P);
    ASSERT_TRUE(d1_lower == BitDepth::BIT10LE);

    auto [f1_mixed, d1_mixed] = parse_format("yUv420p10Le");
    ASSERT_TRUE(f1_mixed == PixelFormat::YUV420P);
    ASSERT_TRUE(d1_mixed == BitDepth::BIT10LE);

    // 2. Default endianness to LE when omitted
    auto [f2_no_endian, d2_no_endian] = parse_format("yuv420p10");
    ASSERT_TRUE(f2_no_endian == PixelFormat::YUV420P);
    ASSERT_TRUE(d2_no_endian == BitDepth::BIT10LE);

    auto [f3_no_endian, d3_no_endian] = parse_format("YUV422P10");
    ASSERT_TRUE(f3_no_endian == PixelFormat::YUV422P);
    ASSERT_TRUE(d3_no_endian == BitDepth::BIT10LE);

    auto [f4_no_endian, d4_no_endian] = parse_format("444p10");
    ASSERT_TRUE(f4_no_endian == PixelFormat::YUV444P);
    ASSERT_TRUE(d4_no_endian == BitDepth::BIT10LE);

    // 3. String conversions produce lowercase
    ASSERT_EQ(to_string(PixelFormat::YUV420P), "yuv420p");
    ASSERT_EQ(to_string(BitDepth::BIT10LE), "10le");
}

TEST_CASE(test_format_parse_invalid) {
    ASSERT_THROWS(parse_format("YUV420P10BE"), std::invalid_argument);
    ASSERT_THROWS(parse_format("yuv420p10be"), std::invalid_argument);
    ASSERT_THROWS(parse_format("RGB24"), std::invalid_argument);
    ASSERT_THROWS(parse_format(""), std::invalid_argument);
    ASSERT_THROWS(parse_format("invalid_fmt"), std::invalid_argument);
}

TEST_CASE(test_chroma_subsampling) {
    auto [h420, v420] = chroma_subsampling(PixelFormat::YUV420P);
    ASSERT_EQ(h420, 2);
    ASSERT_EQ(v420, 2);

    auto [h422, v422] = chroma_subsampling(PixelFormat::YUV422P);
    ASSERT_EQ(h422, 2);
    ASSERT_EQ(v422, 1);

    auto [h444, v444] = chroma_subsampling(PixelFormat::YUV444P);
    ASSERT_EQ(h444, 1);
    ASSERT_EQ(v444, 1);
}

TEST_CASE(test_frame_bytes) {
    // 1920x1080 YUV420P8 = 1920*1080 + 2*(960*540) = 2073600 + 1036800 = 3110400
    ASSERT_EQ(frame_bytes(PixelFormat::YUV420P, 1920, 1080, BitDepth::BIT8), 3110400ULL);

    // 1920x1080 YUV422P8 = 1920*1080 + 2*(960*1080) = 2073600 + 2073600 = 4147200
    ASSERT_EQ(frame_bytes(PixelFormat::YUV422P, 1920, 1080, BitDepth::BIT8), 4147200ULL);

    // 1920x1080 YUV444P8 = 1920*1080 + 2*(1920*1080) = 2073600 + 4147200 = 6220800
    ASSERT_EQ(frame_bytes(PixelFormat::YUV444P, 1920, 1080, BitDepth::BIT8), 6220800ULL);

    // 1920x1080 YUV420P10LE = 3110400 * 2 = 6220800
    ASSERT_EQ(frame_bytes(PixelFormat::YUV420P, 1920, 1080, BitDepth::BIT10LE), 6220800ULL);

    ASSERT_THROWS(frame_bytes(PixelFormat::YUV420P, 0, 1080, BitDepth::BIT8), std::invalid_argument);
    ASSERT_THROWS(frame_bytes(PixelFormat::YUV420P, 1920, -1, BitDepth::BIT8), std::invalid_argument);
}

TEST_CASE(test_try_parse_resolution_from_filename) {
    auto r1 = try_parse_resolution_from_filename("BlowingBubbles_416x240_50.yuv");
    ASSERT_TRUE(r1.has_value());
    ASSERT_EQ(r1->first, 416);
    ASSERT_EQ(r1->second, 240);

    auto r2 = try_parse_resolution_from_filename("/path/to/Kimono1_1920x1080_120.yuv");
    ASSERT_TRUE(r2.has_value());
    ASSERT_EQ(r2->first, 1920);
    ASSERT_EQ(r2->second, 1080);

    auto r3 = try_parse_resolution_from_filename("football_cif.yuv");
    ASSERT_TRUE(r3.has_value());
    ASSERT_EQ(r3->first, 352);
    ASSERT_EQ(r3->second, 288);

    auto r4 = try_parse_resolution_from_filename("foreman_qcif.yuv");
    ASSERT_TRUE(r4.has_value());
    ASSERT_EQ(r4->first, 176);
    ASSERT_EQ(r4->second, 144);

    auto r5 = try_parse_resolution_from_filename("traffic_3840x2160_60.yuv");
    ASSERT_TRUE(r5.has_value());
    ASSERT_EQ(r5->first, 3840);
    ASSERT_EQ(r5->second, 2160);

    auto r6 = try_parse_resolution_from_filename("output_720p.yuv");
    ASSERT_TRUE(r6.has_value());
    ASSERT_EQ(r6->first, 1280);
    ASSERT_EQ(r6->second, 720);

    auto r7 = try_parse_resolution_from_filename("no_res_here.yuv");
    ASSERT_FALSE(r7.has_value());

    // Test case from user bug report: prefix number with underscore
    auto r8 = try_parse_resolution_from_filename("out44_3840x2160_yuv420p10le.yuv");
    ASSERT_TRUE(r8.has_value());
    ASSERT_EQ(r8->first, 3840);
    ASSERT_EQ(r8->second, 2160);

    auto r9 = try_parse_resolution_from_filename("video_1920_1080.yuv");
    ASSERT_TRUE(r9.has_value());
    ASSERT_EQ(r9->first, 1920);
    ASSERT_EQ(r9->second, 1080);
}

TEST_CASE(test_try_parse_format_from_filename) {
    auto f1 = try_parse_format_from_filename("park_joy_420p10le_3840x2160.yuv");
    ASSERT_TRUE(f1.has_value());
    ASSERT_TRUE(f1->first == PixelFormat::YUV420P);
    ASSERT_TRUE(f1->second == BitDepth::BIT10LE);

    auto f2 = try_parse_format_from_filename("test_yuv422p_1920x1080.yuv");
    ASSERT_TRUE(f2.has_value());
    ASSERT_TRUE(f2->first == PixelFormat::YUV422P);
    ASSERT_TRUE(f2->second == BitDepth::BIT8);

    auto f3 = try_parse_format_from_filename("sequence_444p10_1280x720.yuv");
    ASSERT_TRUE(f3.has_value());
    ASSERT_TRUE(f3->first == PixelFormat::YUV444P);
    ASSERT_TRUE(f3->second == BitDepth::BIT10LE);

    auto f4 = try_parse_format_from_filename("video_i420.yuv");
    ASSERT_TRUE(f4.has_value());
    ASSERT_TRUE(f4->first == PixelFormat::YUV420P);
    ASSERT_TRUE(f4->second == BitDepth::BIT8);

    auto f5 = try_parse_format_from_filename("out44_3840x2160_yuv420p10le.yuv");
    ASSERT_TRUE(f5.has_value());
    ASSERT_TRUE(f5->first == PixelFormat::YUV420P);
    ASSERT_TRUE(f5->second == BitDepth::BIT10LE);

    auto f6 = try_parse_format_from_filename("unknown_video.yuv");
    ASSERT_FALSE(f6.has_value());
}
