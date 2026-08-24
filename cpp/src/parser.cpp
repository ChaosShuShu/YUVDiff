#include "yuvdiff/parser.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace yuvdiff {

BitAlignment auto_detect_alignment(
    const std::string& path,
    std::string_view fmt,
    int width,
    int height,
    size_t sample_bytes
) {
    (void)width;
    (void)height;
    auto [pixel_fmt, bit_depth] = parse_format(fmt);
    if (bit_depth != BitDepth::BIT10LE) {
        return BitAlignment::MSB;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return BitAlignment::MSB;
    }

    std::vector<uint8_t> buffer(sample_bytes);
    file.read(reinterpret_cast<char*>(buffer.data()), sample_bytes);
    size_t bytes_read = file.gcount();
    size_t num_words = bytes_read / 2;
    if (num_words == 0) {
        return BitAlignment::MSB;
    }

    const uint16_t* raw = reinterpret_cast<const uint16_t*>(buffer.data());

    uint32_t msb_max = 0;
    uint64_t msb_sum = 0;
    uint32_t lsb_max = 0;
    uint64_t lsb_sum = 0;

    for (size_t i = 0; i < num_words; ++i) {
        uint16_t w = raw[i];
        uint16_t msb_val = w >> 6;
        uint16_t lsb_val = w & 0x3FF;

        if (msb_val > msb_max) msb_max = msb_val;
        msb_sum += msb_val;

        if (lsb_val > lsb_max) lsb_max = lsb_val;
        lsb_sum += lsb_val;
    }

    double msb_mean = static_cast<double>(msb_sum) / num_words;
    double lsb_mean = static_cast<double>(lsb_sum) / num_words;

    bool msb_plausible = (msb_max > 100 && msb_mean > 50.0);
    bool lsb_plausible = (lsb_max > 100 && lsb_mean > 50.0);

    if (lsb_plausible && !msb_plausible) {
        return BitAlignment::LSB;
    }
    return BitAlignment::MSB;
}

YUVParser::YUVParser(
    const std::string& path,
    std::string_view fmt,
    int width,
    int height,
    BitAlignment bit_alignment
)
    : path_(path),
      width_(width),
      height_(height),
      bit_alignment_(bit_alignment) {
    auto [p_fmt, b_depth] = parse_format(fmt);
    pixel_format_ = p_fmt;
    bit_depth_ = b_depth;
    frame_bytes_ = frame_bytes(pixel_format_, width_, height_, bit_depth_);
    init_file();
}

YUVParser::YUVParser(
    const std::string& path,
    std::string_view fmt,
    int width,
    int height,
    std::string_view bit_alignment_str
)
    : path_(path),
      width_(width),
      height_(height) {
    auto [p_fmt, b_depth] = parse_format(fmt);
    pixel_format_ = p_fmt;
    bit_depth_ = b_depth;
    frame_bytes_ = frame_bytes(pixel_format_, width_, height_, bit_depth_);

    if (bit_alignment_str == "auto") {
        bit_alignment_ = auto_detect_alignment(path, fmt, width, height);
    } else if (bit_alignment_str == "msb") {
        bit_alignment_ = BitAlignment::MSB;
    } else if (bit_alignment_str == "lsb") {
        bit_alignment_ = BitAlignment::LSB;
    } else {
        throw std::invalid_argument("Unknown bit alignment: " + std::string(bit_alignment_str));
    }

    init_file();
}

void YUVParser::init_file() {
    fd_ = ::open(path_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("YUV file not found or cannot be opened: " + path_);
    }

    struct stat st;
    if (::fstat(fd_, &st) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Cannot get file status: " + path_);
    }
    file_size_ = static_cast<size_t>(st.st_size);
    num_frames_ = file_size_ / frame_bytes_;

    if (file_size_ >= MMAP_THRESHOLD) {
        mmap_data_ = ::mmap(nullptr, file_size_, PROT_READ, MAP_SHARED, fd_, 0);
        if (mmap_data_ == MAP_FAILED) {
            mmap_data_ = nullptr; // fallback to pread
        }
    }
}

YUVParser::~YUVParser() {
    close();
}

YUVParser::YUVParser(YUVParser&& other) noexcept
    : path_(std::move(other.path_)),
      width_(other.width_),
      height_(other.height_),
      pixel_format_(other.pixel_format_),
      bit_depth_(other.bit_depth_),
      bit_alignment_(other.bit_alignment_),
      frame_bytes_(other.frame_bytes_),
      file_size_(other.file_size_),
      num_frames_(other.num_frames_),
      fd_(other.fd_),
      mmap_data_(other.mmap_data_) {
    other.fd_ = -1;
    other.mmap_data_ = nullptr;
}

YUVParser& YUVParser::operator=(YUVParser&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        width_ = other.width_;
        height_ = other.height_;
        pixel_format_ = other.pixel_format_;
        bit_depth_ = other.bit_depth_;
        bit_alignment_ = other.bit_alignment_;
        frame_bytes_ = other.frame_bytes_;
        file_size_ = other.file_size_;
        num_frames_ = other.num_frames_;
        fd_ = other.fd_;
        mmap_data_ = other.mmap_data_;

        other.fd_ = -1;
        other.mmap_data_ = nullptr;
    }
    return *this;
}

void YUVParser::close() {
    if (mmap_data_ != nullptr) {
        ::munmap(mmap_data_, file_size_);
        mmap_data_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

YUVFrame YUVParser::read_frame(size_t idx) {
    if (idx >= num_frames_) {
        throw std::out_of_range("Frame " + std::to_string(idx) +
                                " out of range [0, " + std::to_string(num_frames_) + ")");
    }

    YUVFrame frame;
    frame.width = width_;
    frame.height = height_;
    frame.bit_depth = (bit_depth_ == BitDepth::BIT8) ? 8 : 10;
    frame.pixel_format = pixel_format_;

    auto [hf, vf] = chroma_subsampling(pixel_format_);
    int cy = height_ / vf;
    int cx = width_ / hf;

    size_t offset = idx * frame_bytes_;

    if (bit_depth_ == BitDepth::BIT8) {
        size_t y_size = static_cast<size_t>(width_) * height_;
        size_t uv_size = static_cast<size_t>(cy) * cx;

        frame.y8.resize(y_size);
        frame.u8.resize(uv_size);
        frame.v8.resize(uv_size);

        if (mmap_data_ != nullptr) {
            const uint8_t* ptr = static_cast<const uint8_t*>(mmap_data_) + offset;
            std::memcpy(frame.y8.data(), ptr, y_size);
            std::memcpy(frame.u8.data(), ptr + y_size, uv_size);
            std::memcpy(frame.v8.data(), ptr + y_size + uv_size, uv_size);
        } else {
            ssize_t r1 = ::pread(fd_, frame.y8.data(), y_size, offset);
            ssize_t r2 = ::pread(fd_, frame.u8.data(), uv_size, offset + y_size);
            ssize_t r3 = ::pread(fd_, frame.v8.data(), uv_size, offset + y_size + uv_size);
            if (r1 < 0 || r2 < 0 || r3 < 0) {
                throw std::runtime_error("I/O error reading frame " + std::to_string(idx));
            }
        }
    } else {
        size_t y_samples = static_cast<size_t>(width_) * height_;
        size_t uv_samples = static_cast<size_t>(cy) * cx;
        size_t total_samples = y_samples + 2 * uv_samples;
        size_t total_bytes = total_samples * 2;

        std::vector<uint16_t> raw_buf;
        const uint16_t* raw_ptr = nullptr;

        if (mmap_data_ != nullptr) {
            raw_ptr = reinterpret_cast<const uint16_t*>(
                static_cast<const uint8_t*>(mmap_data_) + offset
            );
        } else {
            raw_buf.resize(total_samples);
            ssize_t r = ::pread(fd_, raw_buf.data(), total_bytes, offset);
            if (r < 0 || static_cast<size_t>(r) < total_bytes) {
                throw std::runtime_error("I/O error reading 10-bit frame " + std::to_string(idx));
            }
            raw_ptr = raw_buf.data();
        }

        frame.y16.resize(y_samples);
        frame.u16.resize(uv_samples);
        frame.v16.resize(uv_samples);

        const uint16_t* y_src = raw_ptr;
        const uint16_t* u_src = raw_ptr + y_samples;
        const uint16_t* v_src = raw_ptr + y_samples + uv_samples;

        if (bit_alignment_ == BitAlignment::MSB) {
            for (size_t i = 0; i < y_samples; ++i) {
                frame.y16[i] = y_src[i] >> 6;
            }
            for (size_t i = 0; i < uv_samples; ++i) {
                frame.u16[i] = u_src[i] >> 6;
                frame.v16[i] = v_src[i] >> 6;
            }
        } else { // LSB
            for (size_t i = 0; i < y_samples; ++i) {
                frame.y16[i] = y_src[i] & 0x3FF;
            }
            for (size_t i = 0; i < uv_samples; ++i) {
                frame.u16[i] = u_src[i] & 0x3FF;
                frame.v16[i] = v_src[i] & 0x3FF;
            }
        }
    }

    return frame;
}

} // namespace yuvdiff
