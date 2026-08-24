#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace yuvdiff {

enum class PixelFormat {
    YUV420P,
    YUV422P,
    YUV444P
};

enum class BitDepth {
    BIT8 = 8,
    BIT10LE = 10
};

enum class BitAlignment {
    MSB, // High 10 bits [15:6] in 16-bit word
    LSB  // Low 10 bits [9:0] in 16-bit word
};

// Parse strings like "YUV420P8", "YUV420P10LE", etc.
// Throws std::invalid_argument on unsupported formats.
std::pair<PixelFormat, BitDepth> parse_format(std::string_view s);

// Return (horizontal_subsampling, vertical_subsampling)
// 420 -> (2, 2), 422 -> (2, 1), 444 -> (1, 1)
std::pair<int, int> chroma_subsampling(PixelFormat fmt);

// Calculate the number of bytes for a single frame.
// Throws std::invalid_argument on non-positive width/height.
size_t frame_bytes(PixelFormat fmt, int width, int height, BitDepth bit_depth);

// Convert enum to string representation
std::string to_string(PixelFormat fmt);
std::string to_string(BitDepth depth);
std::string to_string(BitAlignment align);

// Heuristically extract resolution (width, height) from a filename/path.
// Returns std::nullopt if no resolution pattern is matched.
std::optional<std::pair<int, int>> try_parse_resolution_from_filename(std::string_view path);

// Heuristically extract format (PixelFormat, BitDepth) from a filename/path.
// Returns std::nullopt if no known format pattern is matched.
std::optional<std::pair<PixelFormat, BitDepth>> try_parse_format_from_filename(std::string_view path);

} // namespace yuvdiff
