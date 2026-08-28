#include "yuvdiff/formats.hpp"

#include <regex>
#include <stdexcept>

namespace yuvdiff {

std::pair<PixelFormat, BitDepth> parse_format(std::string_view s) {
    if (s.empty()) {
        throw std::invalid_argument("Format string cannot be empty");
    }

    std::string lower(s);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    static const std::regex format_re(
        R"(^(?:yuv)?(420|422|444|420p|422p|444p|i420|yv12|nv12|nv21|nv16|nv24)(?:[_\-p])?(8|10|10le|10be)?$)"
    );
    std::smatch match;

    if (!std::regex_match(lower, match, format_re)) {
        throw std::invalid_argument(
            "Unsupported format: '" + std::string(s) + "'. Supported formats include: "
            "yuv420p (yuv420p8), yuv422p (yuv422p8), yuv444p (yuv444p8), "
            "yuv420p10le (yuv420p10), yuv422p10le (yuv422p10), yuv444p10le (yuv444p10)"
        );
    }

    std::string base = match[1].str();
    std::string suffix = match[2].matched ? match[2].str() : "";

    PixelFormat fmt;
    if (base == "420" || base == "420p" || base == "i420" || base == "yv12" || base == "nv12" || base == "nv21") {
        fmt = PixelFormat::YUV420P;
    } else if (base == "422" || base == "422p" || base == "nv16") {
        fmt = PixelFormat::YUV422P;
    } else if (base == "444" || base == "444p" || base == "nv24") {
        fmt = PixelFormat::YUV444P;
    } else {
        throw std::invalid_argument("Unknown pixel format in: '" + std::string(s) + "'");
    }

    BitDepth depth;
    if (suffix.empty() || suffix == "8") {
        depth = BitDepth::BIT8;
    } else if (suffix == "10" || suffix == "10le") {
        depth = BitDepth::BIT10LE;
    } else if (suffix == "10be") {
        throw std::invalid_argument("Big-endian 10-bit is not supported in v1");
    } else {
        throw std::invalid_argument("Unsupported bit depth suffix in: '" + std::string(s) + "'");
    }

    return {fmt, depth};
}

std::pair<int, int> chroma_subsampling(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::YUV420P:
            return {2, 2};
        case PixelFormat::YUV422P:
            return {2, 1};
        case PixelFormat::YUV444P:
            return {1, 1};
    }
    throw std::invalid_argument("Unknown pixel format enum");
}

size_t frame_bytes(PixelFormat fmt, int width, int height, BitDepth bit_depth) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Resolution must be positive, got " +
                                    std::to_string(width) + "x" + std::to_string(height));
    }

    size_t bytes_per_sample = (bit_depth == BitDepth::BIT8) ? 1 : 2;
    auto [h_factor, v_factor] = chroma_subsampling(fmt);

    size_t y_bytes = static_cast<size_t>(width) * height * bytes_per_sample;
    size_t chroma_w = static_cast<size_t>(width / h_factor);
    size_t chroma_h = static_cast<size_t>(height / v_factor);
    size_t uv_bytes = 2 * chroma_w * chroma_h * bytes_per_sample;

    return y_bytes + uv_bytes;
}

std::string to_string(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::YUV420P: return "yuv420p";
        case PixelFormat::YUV422P: return "yuv422p";
        case PixelFormat::YUV444P: return "yuv444p";
    }
    return "unknown";
}

std::string to_string(BitDepth depth) {
    switch (depth) {
        case BitDepth::BIT8: return "8";
        case BitDepth::BIT10LE: return "10le";
    }
    return "unknown";
}

std::string to_string(BitAlignment align) {
    switch (align) {
        case BitAlignment::MSB: return "msb";
        case BitAlignment::LSB: return "lsb";
    }
    return "unknown";
}

namespace {

std::string get_basename_lower(std::string_view path) {
    size_t last_slash = path.find_last_of("/\\");
    std::string_view name = (last_slash == std::string_view::npos) ? path : path.substr(last_slash + 1);
    std::string lower(name);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

} // anonymous namespace

std::optional<std::pair<int, int>> try_parse_resolution_from_filename(std::string_view path) {
    std::string name = get_basename_lower(path);

    // 1. Prioritize explicit 'x' or 'X' resolution patterns like 1920x1080, 3840x2160, 352x288, 176x144
    static const std::regex res_x_re(R"((?:^|[^0-9a-zA-Z])([1-9][0-9]{1,4})[xX]([1-9][0-9]{1,4})(?:[^0-9a-zA-Z]|$))");
    auto x_begin = std::sregex_iterator(name.begin(), name.end(), res_x_re);
    auto x_end = std::sregex_iterator();

    for (std::sregex_iterator it = x_begin; it != x_end; ++it) {
        std::smatch match = *it;
        try {
            int w = std::stoi(match[1].str());
            int h = std::stoi(match[2].str());
            if (w >= 16 && w <= 16384 && h >= 16 && h <= 16384) {
                return std::pair<int, int>{w, h};
            }
        } catch (...) {}
    }

    // 2. Check common standard resolution keyword aliases
    if (name.find("qcif") != std::string::npos) return std::pair<int, int>{176, 144};
    if (name.find("4cif") != std::string::npos) return std::pair<int, int>{704, 576};
    if (name.find("cif") != std::string::npos) return std::pair<int, int>{352, 288};
    if (name.find("720p") != std::string::npos || name.find("720_") != std::string::npos) return std::pair<int, int>{1280, 720};
    if (name.find("1080p") != std::string::npos || name.find("1080i") != std::string::npos || name.find("fhd") != std::string::npos) return std::pair<int, int>{1920, 1080};
    if (name.find("1440p") != std::string::npos || name.find("2k") != std::string::npos || name.find("qhd") != std::string::npos) return std::pair<int, int>{2560, 1440};
    if (name.find("2160p") != std::string::npos || name.find("4k") != std::string::npos || name.find("uhd") != std::string::npos) return std::pair<int, int>{3840, 2160};
    if (name.find("4320p") != std::string::npos || name.find("8k") != std::string::npos) return std::pair<int, int>{7680, 4320};

    // 3. Fallback: check '_' separated resolutions (e.g. video_1920_1080.yuv)
    static const std::regex res_underscore_re(R"((?:^|[^0-9a-zA-Z])([1-9][0-9]{2,4})_([1-9][0-9]{2,4})(?:[^0-9a-zA-Z]|$))");
    auto u_begin = std::sregex_iterator(name.begin(), name.end(), res_underscore_re);
    auto u_end = std::sregex_iterator();

    for (std::sregex_iterator it = u_begin; it != u_end; ++it) {
        std::smatch match = *it;
        try {
            int w = std::stoi(match[1].str());
            int h = std::stoi(match[2].str());
            if (w >= 64 && w <= 16384 && h >= 64 && h <= 16384) {
                // Avoid date/timestamp patterns like 2026_0824
                if (w != 2024 && w != 2025 && w != 2026 && !(w > 2000 && h > 2000 && w == h + 1)) {
                    return std::pair<int, int>{w, h};
                }
            }
        } catch (...) {}
    }

    return std::nullopt;
}

std::optional<std::pair<PixelFormat, BitDepth>> try_parse_format_from_filename(std::string_view path) {
    std::string name = get_basename_lower(path);

    bool is_10bit = (name.find("10le") != std::string::npos ||
                     name.find("10bit") != std::string::npos ||
                     name.find("p10") != std::string::npos ||
                     name.find("_10_") != std::string::npos ||
                     name.find("_10.") != std::string::npos);

    BitDepth depth = is_10bit ? BitDepth::BIT10LE : BitDepth::BIT8;

    if (name.find("444") != std::string::npos || name.find("nv24") != std::string::npos) {
        return std::pair<PixelFormat, BitDepth>{PixelFormat::YUV444P, depth};
    }
    if (name.find("422") != std::string::npos || name.find("nv16") != std::string::npos) {
        return std::pair<PixelFormat, BitDepth>{PixelFormat::YUV422P, depth};
    }
    if (name.find("420") != std::string::npos || name.find("i420") != std::string::npos ||
        name.find("yv12") != std::string::npos || name.find("nv12") != std::string::npos) {
        return std::pair<PixelFormat, BitDepth>{PixelFormat::YUV420P, depth};
    }

    if (is_10bit) {
        // If 10-bit detected but no subsampling found, default to 420P
        return std::pair<PixelFormat, BitDepth>{PixelFormat::YUV420P, BitDepth::BIT10LE};
    }

    return std::nullopt;
}

} // namespace yuvdiff
