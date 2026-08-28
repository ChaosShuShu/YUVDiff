#include "yuvdiff/tools.hpp"
#include "yuvdiff/formats.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace yuvdiff {

size_t yuvcut(const CutOptions& opts) {
    if (opts.input_path.empty()) {
        throw std::invalid_argument("Input file path cannot be empty");
    }
    if (opts.output_path.empty()) {
        throw std::invalid_argument("Output file path cannot be empty");
    }
    if (opts.width <= 0 || opts.height <= 0) {
        throw std::invalid_argument("Width and height must be > 0");
    }
    if (opts.num_frames == 0) {
        return 0;
    }

    auto [fmt, depth] = parse_format(opts.format_str);
    size_t frame_sz = frame_bytes(fmt, opts.width, opts.height, depth);

    std::ifstream infile(opts.input_path, std::ios::binary);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open input file: " + opts.input_path);
    }

    std::ofstream outfile(opts.output_path, std::ios::binary);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + opts.output_path);
    }

    if (opts.start_frame > 0) {
        infile.seekg(static_cast<std::streamoff>(opts.start_frame * frame_sz), std::ios::beg);
        if (!infile) {
            throw std::runtime_error("Failed to seek to start frame " + std::to_string(opts.start_frame));
        }
    }

    std::vector<char> buffer(frame_sz);
    size_t written = 0;

    for (size_t i = 0; i < opts.num_frames; ++i) {
        infile.read(buffer.data(), static_cast<std::streamsize>(frame_sz));
        if (infile.gcount() < static_cast<std::streamsize>(frame_sz)) {
            break; // Reached EOF
        }
        outfile.write(buffer.data(), static_cast<std::streamsize>(frame_sz));
        if (!outfile) {
            throw std::runtime_error("Write error at frame " + std::to_string(opts.start_frame + i));
        }
        ++written;
    }

    return written;
}

} // namespace yuvdiff
