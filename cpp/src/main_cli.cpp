#include "yuvdiff/CLI11.hpp"
#include "yuvdiff/diff.hpp"
#include "yuvdiff/formats.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/tools.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string format_float(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return (v > 0) ? "inf" : "-inf";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << v;
    return oss.str();
}

int run_info(const std::string& input_path, int width, int height, std::string format_str, const std::string& align_str) {
    if (width <= 0 || height <= 0) {
        auto auto_res = yuvdiff::try_parse_resolution_from_filename(input_path);
        if (auto_res.has_value()) {
            width = auto_res->first;
            height = auto_res->second;
        }
    }
    if (format_str.empty()) {
        auto auto_fmt = yuvdiff::try_parse_format_from_filename(input_path);
        if (auto_fmt.has_value()) {
            format_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
        } else {
            format_str = "YUV420P8";
        }
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "Error: resolution could not be determined. Please specify --width and --height.\n";
        return 2;
    }

    try {
        yuvdiff::YUVParser parser(input_path, format_str, width, height, align_str);
        std::cout << "=== YUV Video Info ===\n"
                  << "File: " << input_path << "\n"
                  << "Format: " << format_str << "\n"
                  << "Resolution: " << width << "x" << height << "\n"
                  << "Bit Depth: " << yuvdiff::to_string(parser.bit_depth()) << "\n"
                  << "Total Frames: " << parser.num_frames() << "\n";
        if (parser.bit_depth() == yuvdiff::BitDepth::BIT10LE) {
            std::cout << "Alignment: " << yuvdiff::to_string(parser.bit_alignment()) << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}

int run_cut(const yuvdiff::CutOptions& in_opts) {
    yuvdiff::CutOptions opts = in_opts;
    if (opts.width <= 0 || opts.height <= 0) {
        auto auto_res = yuvdiff::try_parse_resolution_from_filename(opts.input_path);
        if (auto_res.has_value()) {
            opts.width = auto_res->first;
            opts.height = auto_res->second;
        }
    }
    if (opts.format_str.empty()) {
        auto auto_fmt = yuvdiff::try_parse_format_from_filename(opts.input_path);
        if (auto_fmt.has_value()) {
            opts.format_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
        } else {
            opts.format_str = "YUV420P8";
        }
    }

    if (opts.width <= 0 || opts.height <= 0) {
        std::cerr << "Error: resolution could not be determined. Please specify --width and --height.\n";
        return 2;
    }

    try {
        size_t written = yuvdiff::yuvcut(opts);
        std::cout << "Successfully cut " << written << " frames to " << opts.output_path << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}

int run_diff(
    const std::string& path_a,
    const std::string& path_b,
    int width,
    int height,
    std::string format_str,
    std::string format_a_str,
    std::string format_b_str,
    std::optional<size_t> max_frames,
    int threshold,
    const std::string& align_str,
    bool stop_on_diff,
    bool quiet
) {
    // If only 1 input file provided in diff command, delegate to info
    if (path_b.empty()) {
        return run_info(path_a, width, height, format_str.empty() ? format_a_str : format_str, align_str);
    }

    if (width <= 0 || height <= 0) {
        auto auto_res = yuvdiff::try_parse_resolution_from_filename(path_a);
        if (auto_res.has_value()) {
            width = auto_res->first;
            height = auto_res->second;
            if (!quiet) std::cerr << "# Auto-detected resolution: " << width << "x" << height << "\n";
        }
    }

    if (format_a_str.empty()) {
        if (!format_str.empty()) {
            format_a_str = format_str;
        } else {
            auto auto_fmt = yuvdiff::try_parse_format_from_filename(path_a);
            if (auto_fmt.has_value()) {
                format_a_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
                if (!quiet) std::cerr << "# Auto-detected format A: " << format_a_str << "\n";
            } else {
                format_a_str = "YUV420P8";
            }
        }
    }

    if (format_b_str.empty()) {
        if (!format_str.empty()) {
            format_b_str = format_str;
        } else {
            auto auto_fmt = yuvdiff::try_parse_format_from_filename(path_b);
            if (auto_fmt.has_value()) {
                format_b_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
                if (!quiet) std::cerr << "# Auto-detected format B: " << format_b_str << "\n";
            } else {
                format_b_str = format_a_str;
            }
        }
    }

    if (format_a_str.empty() || width <= 0 || height <= 0) {
        std::cerr << "Error: missing required arguments (--format / --format-a, --width, --height)\n";
        return 2;
    }

    std::unique_ptr<yuvdiff::YUVParser> parser_a;
    std::unique_ptr<yuvdiff::YUVParser> parser_b;

    try {
        parser_a = std::make_unique<yuvdiff::YUVParser>(path_a, format_a_str, width, height, align_str);
        parser_b = std::make_unique<yuvdiff::YUVParser>(path_b, format_b_str, width, height, align_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    if (align_str == "auto" && parser_a->bit_depth() == yuvdiff::BitDepth::BIT10LE && !quiet) {
        std::cerr << "# 10-bit alignment auto-detected for A: "
                  << yuvdiff::to_string(parser_a->bit_alignment()) << "\n";
    }

    size_t num_frames = std::min(parser_a->num_frames(), parser_b->num_frames());
    if (max_frames.has_value()) {
        num_frames = std::min(num_frames, *max_frames);
    }

    yuvdiff::DiffEngine diff_engine(threshold);
    yuvdiff::MetricsCalculator metrics;

    if (!quiet) {
        std::cout << "frame,psnr_y,psnr_u,psnr_v,psnr_total,ssim_y,diff_pixels,total_pixels,diff_mean,diff_median,diff_max,diff_min\n";
    }

    bool has_any_diff = false;

    for (size_t i = 0; i < num_frames; ++i) {
        yuvdiff::YUVFrame fa;
        yuvdiff::YUVFrame fb;
        try {
            fa = parser_a->read_frame(i);
            fb = parser_b->read_frame(i);
        } catch (const std::exception& e) {
            std::cerr << "Error frame " << i << ": " << e.what() << "\n";
            return 2;
        }

        yuvdiff::PSNRResult psnr = metrics.psnr(fa, fb);
        yuvdiff::SSIMResult ssim = metrics.ssim(fa, fb);
        yuvdiff::DiffResult diff = diff_engine.diff(fa, fb);

        if (diff.diff_pixel_count > 0) {
            has_any_diff = true;
        }

        if (!quiet) {
            std::cout << i << ","
                      << format_float(psnr.y) << ","
                      << format_float(psnr.u) << ","
                      << format_float(psnr.v) << ","
                      << format_float(psnr.total) << ","
                      << format_float(ssim.y) << ","
                      << diff.diff_pixel_count << ","
                      << diff.total_pixel_count << ","
                      << format_float(diff.diff_mean) << ","
                      << format_float(diff.diff_median) << ","
                      << diff.diff_max << ","
                      << diff.diff_min << "\n";
        }

        if (stop_on_diff && diff.diff_pixel_count > 0) {
            if (!quiet) {
                std::cerr << "# Stop on diff: Difference found at frame " << i
                          << " (diff_pixels=" << diff.diff_pixel_count
                          << "/" << diff.total_pixel_count
                          << ", max_diff=" << diff.diff_max << ")\n";
            }
            return 1;
        }
    }

    return has_any_diff ? 1 : 0;
}

#include <fstream>

int run_cmp(
    const std::string& path_a,
    const std::string& path_b,
    int width,
    int height,
    std::string format_str,
    std::string format_a_str,
    std::string format_b_str,
    std::optional<size_t> max_frames,
    int threshold,
    const std::string& align_str,
    const std::string& output_file
) {
    if (path_a.empty() || path_b.empty()) {
        std::cerr << "Error: cmp requires two input video files\n";
        return 2;
    }

    if (width <= 0 || height <= 0) {
        auto auto_res = yuvdiff::try_parse_resolution_from_filename(path_a);
        if (auto_res.has_value()) {
            width = auto_res->first;
            height = auto_res->second;
        }
    }

    if (format_a_str.empty()) {
        if (!format_str.empty()) {
            format_a_str = format_str;
        } else {
            auto auto_fmt = yuvdiff::try_parse_format_from_filename(path_a);
            if (auto_fmt.has_value()) {
                format_a_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
            } else {
                format_a_str = "YUV420P8";
            }
        }
    }

    if (format_b_str.empty()) {
        if (!format_str.empty()) {
            format_b_str = format_str;
        } else {
            auto auto_fmt = yuvdiff::try_parse_format_from_filename(path_b);
            if (auto_fmt.has_value()) {
                format_b_str = yuvdiff::to_string(auto_fmt->first) + yuvdiff::to_string(auto_fmt->second);
            } else {
                format_b_str = format_a_str;
            }
        }
    }

    if (format_a_str.empty() || width <= 0 || height <= 0) {
        std::cerr << "Error: missing required arguments (--format / --format-a, --width, --height)\n";
        return 2;
    }

    std::unique_ptr<yuvdiff::YUVParser> parser_a;
    std::unique_ptr<yuvdiff::YUVParser> parser_b;

    try {
        parser_a = std::make_unique<yuvdiff::YUVParser>(path_a, format_a_str, width, height, align_str);
        parser_b = std::make_unique<yuvdiff::YUVParser>(path_b, format_b_str, width, height, align_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    size_t num_frames = std::min(parser_a->num_frames(), parser_b->num_frames());
    if (max_frames.has_value()) {
        num_frames = std::min(num_frames, *max_frames);
    }

    std::unique_ptr<std::ofstream> out_stream;
    if (!output_file.empty()) {
        out_stream = std::make_unique<std::ofstream>(output_file, std::ios::out);
        if (!out_stream->is_open()) {
            std::cerr << "Error: Failed to open output file: " << output_file << "\n";
            return 2;
        }
    }

    yuvdiff::DiffEngine diff_engine(threshold);

    int64_t total_diff_frames = 0;
    int64_t total_diff_pixels = 0;
    int64_t total_all_pixels = 0;
    double sum_mean_weighted = 0.0;
    int32_t global_max = 0;
    int32_t global_min = (num_frames > 0) ? 1000000 : 0;
    std::vector<uint64_t> global_hist(1024, 0);

    for (size_t i = 0; i < num_frames; ++i) {
        yuvdiff::YUVFrame fa;
        yuvdiff::YUVFrame fb;
        try {
            fa = parser_a->read_frame(i);
            fb = parser_b->read_frame(i);
        } catch (const std::exception& e) {
            std::cerr << "Error frame " << i << ": " << e.what() << "\n";
            return 2;
        }

        yuvdiff::DiffResult diff = diff_engine.diff(fa, fb);

        if (diff.diff_pixel_count > 0) {
            total_diff_frames++;
        }
        total_diff_pixels += diff.diff_pixel_count;
        total_all_pixels += diff.total_pixel_count;
        sum_mean_weighted += diff.diff_mean * diff.total_pixel_count;
        if (diff.diff_max > global_max) global_max = diff.diff_max;
        if (diff.diff_min < global_min) global_min = diff.diff_min;

        for (int32_t d : diff.diff_pixel) {
            if (d >= 0 && d < 1024) global_hist[d]++;
            else if (d >= 1024) global_hist[1023]++;
        }

        double diff_ratio = (diff.total_pixel_count > 0)
            ? (static_cast<double>(diff.diff_pixel_count) / diff.total_pixel_count)
            : 0.0;

        if (out_stream) {
            (*out_stream) << "frame=" << i
                          << " diff_pixels=" << diff.diff_pixel_count
                          << " total_pixels=" << diff.total_pixel_count
                          << " diff_ratio=" << format_float(diff_ratio)
                          << " diff_mean=" << format_float(diff.diff_mean)
                          << " diff_median=" << format_float(diff.diff_median)
                          << " diff_max=" << diff.diff_max
                          << " diff_min=" << diff.diff_min << "\n";
        }
    }

    if (num_frames == 0) {
        global_min = 0;
    }

    double global_diff_ratio = (total_all_pixels > 0)
        ? (static_cast<double>(total_diff_pixels) / total_all_pixels)
        : 0.0;
    double global_mean = (total_all_pixels > 0)
        ? (sum_mean_weighted / total_all_pixels)
        : 0.0;

    double global_median = 0.0;
    if (total_all_pixels > 0) {
        uint64_t target = total_all_pixels / 2 + 1;
        uint64_t accum = 0;
        for (int k = 0; k < 1024; ++k) {
            accum += global_hist[k];
            if (accum >= target) {
                global_median = static_cast<double>(k);
                break;
            }
        }
    }

    std::cout << "total_frames=" << num_frames
              << " diff_frames=" << total_diff_frames
              << " diff_pixels=" << total_diff_pixels
              << " total_pixels=" << total_all_pixels
              << " diff_ratio=" << format_float(global_diff_ratio)
              << " diff_mean=" << format_float(global_mean)
              << " diff_median=" << format_float(global_median)
              << " diff_max=" << global_max
              << " diff_min=" << global_min << "\n";

    return (total_diff_frames > 0) ? 1 : 0;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    CLI::App app{"YUVDiff - High Performance YUV Video Quality & Diff Analysis Tool"};
    app.set_help_flag("-?,--help", "Print this help message and exit");
    app.require_subcommand(0, 1);

    // =========================================================================
    // Subcommand: diff (also default fallback for root app)
    // =========================================================================
    std::string diff_path_a;
    std::string diff_path_b;
    int diff_width = -1;
    int diff_height = -1;
    std::string diff_format;
    std::string diff_format_a;
    std::string diff_format_b;
    std::optional<size_t> diff_frames;
    int diff_threshold = 4;
    std::string diff_align = "auto";
    bool diff_stop_on_diff = false;
    bool diff_quiet = false;

    auto setup_diff_options = [&](CLI::App* sub) {
        sub->set_help_flag("-?,--help", "Print this help message and exit");
        sub->add_option("file_a", diff_path_a, "First YUV video file (Video A)");
        sub->add_option("file_b", diff_path_b, "Second YUV video file (Video B)");
        sub->add_option("-w,--width", diff_width, "Width in pixels");
        sub->add_option("-h,--height", diff_height, "Height in pixels");
        sub->add_option("-f,--format", diff_format, "YUV format (e.g. YUV420P8, YUV422P10LE)");
        sub->add_option("--format-a", diff_format_a, "YUV format for Video A");
        sub->add_option("--format-b", diff_format_b, "YUV format for Video B");
        sub->add_option("-n,--frames", diff_frames, "Number of frames to compare");
        sub->add_option("-t,--threshold", diff_threshold, "Pixel diff threshold for mask (default: 4)");
        sub->add_option("--10bit-align", diff_align, "10-bit alignment: msb, lsb, auto (default: auto)");
        sub->add_flag("--stop-on-diff", diff_stop_on_diff, "Stop on first frame with pixel differences");
        sub->add_flag("-q,--quiet", diff_quiet, "Quiet mode: suppress CSV output, exit with 0 (identical) or 1 (differs)");
    };

    CLI::App* diff_cmd = app.add_subcommand("diff", "Compare two YUV videos and output quality metrics / statistics");
    setup_diff_options(diff_cmd);

    // =========================================================================
    // Subcommand: cmp
    // =========================================================================
    std::string cmp_path_a;
    std::string cmp_path_b;
    int cmp_width = -1;
    int cmp_height = -1;
    std::string cmp_format;
    std::string cmp_format_a;
    std::string cmp_format_b;
    std::optional<size_t> cmp_frames;
    int cmp_threshold = 4;
    std::string cmp_align = "auto";
    std::string cmp_output;

    CLI::App* cmp_cmd = app.add_subcommand("cmp", "Compare two YUV videos and output K=V formatted summary and frame statistics");
    cmp_cmd->set_help_flag("-?,--help", "Print this help message and exit");
    cmp_cmd->add_option("file_a", cmp_path_a, "First YUV video file (Video A)")->required();
    cmp_cmd->add_option("file_b", cmp_path_b, "Second YUV video file (Video B)")->required();
    cmp_cmd->add_option("-w,--width", cmp_width, "Width in pixels");
    cmp_cmd->add_option("-h,--height", cmp_height, "Height in pixels");
    cmp_cmd->add_option("-f,--format", cmp_format, "YUV format (e.g. YUV420P8, YUV422P10LE)");
    cmp_cmd->add_option("--format-a", cmp_format_a, "YUV format for Video A");
    cmp_cmd->add_option("--format-b", cmp_format_b, "YUV format for Video B");
    cmp_cmd->add_option("-n,--frames", cmp_frames, "Number of frames to compare");
    cmp_cmd->add_option("-t,--threshold", cmp_threshold, "Pixel diff threshold for mask (default: 4)");
    cmp_cmd->add_option("--10bit-align", cmp_align, "10-bit alignment: msb, lsb, auto (default: auto)");
    cmp_cmd->add_option("-o,--output", cmp_output, "Output file path to save per-frame K=V statistics");

    // =========================================================================
    // Subcommand: cut
    // =========================================================================
    yuvdiff::CutOptions cut_opts;
    CLI::App* cut_cmd = app.add_subcommand("cut", "Cut / extract frame slice from a YUV video");
    cut_cmd->set_help_flag("-?,--help", "Print this help message and exit");
    cut_cmd->add_option("-i,--input", cut_opts.input_path, "Input YUV file path")->required();
    cut_cmd->add_option("-o,--output", cut_opts.output_path, "Output YUV file path")->required();
    cut_cmd->add_option("-n,--frames", cut_opts.num_frames, "Number of frames to extract")->required();
    cut_cmd->add_option("-s,--start", cut_opts.start_frame, "Start frame index (default: 0)");
    cut_cmd->add_option("-w,--width", cut_opts.width, "Width in pixels");
    cut_cmd->add_option("-h,--height", cut_opts.height, "Height in pixels");
    cut_cmd->add_option("-f,--format", cut_opts.format_str, "YUV format (e.g. YUV420P8, YUV420P10LE)");
    cut_cmd->add_option("--10bit-align", cut_opts.align_str, "10-bit alignment: msb, lsb, auto");

    // =========================================================================
    // Subcommand: info
    // =========================================================================
    std::string info_path;
    int info_width = -1;
    int info_height = -1;
    std::string info_format;
    std::string info_align = "auto";
    CLI::App* info_cmd = app.add_subcommand("info", "Inspect YUV video metadata and alignment");
    info_cmd->set_help_flag("-?,--help", "Print this help message and exit");
    info_cmd->add_option("file", info_path, "YUV video file path")->required();
    info_cmd->add_option("-w,--width", info_width, "Width in pixels");
    info_cmd->add_option("-h,--height", info_height, "Height in pixels");
    info_cmd->add_option("-f,--format", info_format, "YUV format");
    info_cmd->add_option("--10bit-align", info_align, "10-bit alignment: msb, lsb, auto");

    // Setup root options for direct invocation (backward compatibility: yuvdiff-cli A.yuv B.yuv ...)
    setup_diff_options(&app);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (cmp_cmd->parsed()) {
        return run_cmp(
            cmp_path_a,
            cmp_path_b,
            cmp_width,
            cmp_height,
            cmp_format,
            cmp_format_a,
            cmp_format_b,
            cmp_frames,
            cmp_threshold,
            cmp_align,
            cmp_output
        );
    } else if (cut_cmd->parsed()) {
        return run_cut(cut_opts);
    } else if (info_cmd->parsed()) {
        return run_info(info_path, info_width, info_height, info_format, info_align);
    } else {
        // Run diff (either via 'diff' subcommand or direct invocation)
        if (diff_path_a.empty()) {
            std::cout << app.help();
            return 1;
        }
        return run_diff(
            diff_path_a,
            diff_path_b,
            diff_width,
            diff_height,
            diff_format,
            diff_format_a,
            diff_format_b,
            diff_frames,
            diff_threshold,
            diff_align,
            diff_stop_on_diff,
            diff_quiet
        );
    }
}
