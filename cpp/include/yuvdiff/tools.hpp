#pragma once

#include "yuvdiff/formats.hpp"

#include <cstddef>
#include <string>

namespace yuvdiff {

struct CutOptions {
    std::string input_path;
    std::string output_path;
    int width = 0;
    int height = 0;
    std::string format_str = "YUV420P8";
    size_t start_frame = 0;
    size_t num_frames = 0;
    std::string align_str = "auto";
};

// Cuts frames [start_frame, start_frame + num_frames) from input YUV file and writes to output YUV file.
// Returns the number of frames successfully written.
size_t yuvcut(const CutOptions& opts);

} // namespace yuvdiff
