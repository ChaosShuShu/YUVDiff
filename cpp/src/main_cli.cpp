#include "yuvdiff/diff.hpp"
#include "yuvdiff/formats.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/parser.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <A.yuv> <B.yuv> --format <FMT> --width <W> --height <H> [options]\n\n"
              << "Options:\n"
              << "  --format <str>        YUV format (e.g. YUV420P8, YUV420P10LE, etc.) [Required]\n"
              << "  --width <int>         Width in pixels [Required]\n"
              << "  --height <int>        Height in pixels [Required]\n"
              << "  --frames <int>        Number of frames to compare (default: min of both)\n"
              << "  --threshold <int>     Pixel diff threshold for mask (default: 4)\n"
              << "  --10bit-align <align> 10-bit alignment: msb, lsb, auto (default: auto)\n"
              << "  -h, --help            Show this help message\n";
}

std::string format_float(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return (v > 0) ? "inf" : "-inf";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << v;
    return oss.str();
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    std::string path_a;
    std::string path_b;
    std::string format_str;
    int width = -1;
    int height = -1;
    std::optional<size_t> max_frames;
    int threshold = 4;
    std::string align_str = "auto";

    std::vector<std::string> positionals;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--format") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --format\n"; return 1; }
            format_str = argv[i];
        } else if (arg == "--width") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --width\n"; return 1; }
            width = std::stoi(argv[i]);
        } else if (arg == "--height") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --height\n"; return 1; }
            height = std::stoi(argv[i]);
        } else if (arg == "--frames") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --frames\n"; return 1; }
            max_frames = static_cast<size_t>(std::stoll(argv[i]));
        } else if (arg == "--threshold") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --threshold\n"; return 1; }
            threshold = std::stoi(argv[i]);
        } else if (arg == "--10bit-align") {
            if (++i >= argc) { std::cerr << "yuvdiff: missing value for --10bit-align\n"; return 1; }
            align_str = argv[i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "yuvdiff: unrecognized option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        } else {
            positionals.push_back(arg);
        }
    }

    if (positionals.empty()) {
        std::cerr << "yuvdiff: missing input video file(s)\n";
        print_usage(argv[0]);
        return 1;
    }

    path_a = positionals[0];
    if (positionals.size() >= 2) {
        path_b = positionals[1];
    }

    // Try auto-detecting resolution from filename if not provided
    if (width <= 0 || height <= 0) {
        auto auto_res = yuvdiff::try_parse_resolution_from_filename(path_a);
        if (auto_res.has_value()) {
            width = auto_res->first;
            height = auto_res->second;
            std::cerr << "# Auto-detected resolution: " << width << "x" << height << "\n";
        }
    }

    // Try auto-detecting format from filename if not provided
    if (format_str.empty()) {
        auto auto_fmt = yuvdiff::try_parse_format_from_filename(path_a);
        if (auto_fmt.has_value()) {
            format_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
            std::cerr << "# Auto-detected format: " << format_str << "\n";
        }
    }

    if (format_str.empty() || width <= 0 || height <= 0) {
        std::cerr << "yuvdiff: missing required arguments (--format, --width, --height)\n";
        print_usage(argv[0]);
        return 1;
    }

    yuvdiff::BitDepth bit_depth;
    try {
        auto [p_fmt, b_depth] = yuvdiff::parse_format(format_str);
        (void)p_fmt;
        bit_depth = b_depth;
    } catch (const std::exception& e) {
        std::cerr << "yuvdiff: " << e.what() << "\n";
        return 1;
    }

    std::string align_arg = (bit_depth == yuvdiff::BitDepth::BIT10LE) ? align_str : "msb";

    // Single Video CLI Inspection Mode
    if (positionals.size() == 1) {
        try {
            yuvdiff::YUVParser parser(path_a, format_str, width, height, align_arg);
            std::cout << "=== YUV Video Info ===\n"
                      << "File: " << path_a << "\n"
                      << "Format: " << format_str << "\n"
                      << "Resolution: " << width << "x" << height << "\n"
                      << "Bit Depth: " << yuvdiff::to_string(parser.bit_depth()) << "\n"
                      << "Total Frames: " << parser.num_frames() << "\n";
            if (bit_depth == yuvdiff::BitDepth::BIT10LE) {
                std::cout << "Alignment: " << yuvdiff::to_string(parser.bit_alignment()) << "\n";
            }
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "yuvdiff: " << e.what() << "\n";
            return 2;
        }
    }

    // Dual Video Diff Mode
    std::unique_ptr<yuvdiff::YUVParser> parser_a;
    std::unique_ptr<yuvdiff::YUVParser> parser_b;

    try {
        parser_a = std::make_unique<yuvdiff::YUVParser>(path_a, format_str, width, height, align_arg);
        parser_b = std::make_unique<yuvdiff::YUVParser>(path_b, format_str, width, height, align_arg);
    } catch (const std::exception& e) {
        std::cerr << "yuvdiff: " << e.what() << "\n";
        return 2;
    }

    if (align_str == "auto" && bit_depth == yuvdiff::BitDepth::BIT10LE) {
        std::cerr << "# 10-bit alignment auto-detected: "
                  << yuvdiff::to_string(parser_a->bit_alignment()) << "\n";
    }

    size_t num_frames = std::min(parser_a->num_frames(), parser_b->num_frames());
    if (max_frames.has_value()) {
        num_frames = std::min(num_frames, *max_frames);
    }

    yuvdiff::DiffEngine diff_engine(threshold);
    yuvdiff::MetricsCalculator metrics;

    std::cout << "frame,psnr_y,psnr_u,psnr_v,psnr_total,ssim_y,diff_pixels,total_pixels\n";

    for (size_t i = 0; i < num_frames; ++i) {
        yuvdiff::YUVFrame fa;
        yuvdiff::YUVFrame fb;
        try {
            fa = parser_a->read_frame(i);
            fb = parser_b->read_frame(i);
        } catch (const std::exception& e) {
            std::cerr << "yuvdiff: frame " << i << ": " << e.what() << "\n";
            return 2;
        }

        yuvdiff::PSNRResult psnr = metrics.psnr(fa, fb);
        yuvdiff::SSIMResult ssim = metrics.ssim(fa, fb);
        yuvdiff::DiffResult diff = diff_engine.diff(fa, fb);

        std::cout << i << ","
                  << format_float(psnr.y) << ","
                  << format_float(psnr.u) << ","
                  << format_float(psnr.v) << ","
                  << format_float(psnr.total) << ","
                  << format_float(ssim.y) << ","
                  << diff.diff_pixel_count << ","
                  << diff.total_pixel_count << "\n";
    }

    return 0;
}
