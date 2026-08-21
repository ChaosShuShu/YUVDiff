"""YUV format definitions and parsing.

Supported formats (6 total):
    YUV420P8, YUV422P8, YUV444P8,
    YUV420P10LE, YUV422P10LE, YUV444P10LE

10-bit samples are MSB-aligned in 16-bit LE words (H.264/HEVC/AV1
reference convention). Big-endian 10-bit is intentionally not supported
in v1.
"""
from __future__ import annotations

import re
from enum import Enum


class PixelFormat(Enum):
    """Chroma sub-sampling layout."""

    YUV420P = "YUV420P"
    YUV422P = "YUV422P"
    YUV444P = "YUV444P"


class BitDepth(Enum):
    """Bit depth and byte order."""

    BIT8 = 8
    BIT10LE = 10  # 16-bit LE storage, value placement depends on BitAlignment


class BitAlignment(Enum):
    """Where the 10-bit payload sits inside its 16-bit LE word.

    MSB-aligned (default; H.264/HEVC/AV1 reference convention):
        value lives in bits [15:6]; recover via `word >> 6`.
    LSB-aligned (some FFmpeg/x264 builds, certain encoders):
        value lives in bits [9:0]; recover via `word & 0x3FF`.
    """

    MSB = "msb"
    LSB = "lsb"


_FORMAT_RE = re.compile(r"^(YUV(?:420P|422P|444P))(8|10LE|10BE)?$")


def parse_format(s: str) -> tuple[PixelFormat, BitDepth]:
    """Parse a format string like 'YUV420P8' or 'YUV420P10LE'.

    Raises:
        ValueError: if the format is not in the supported list.
    """
    m = _FORMAT_RE.match(s)
    if not m:
        raise ValueError(
            f"Unsupported format: {s!r}. Supported: "
            "YUV420P8, YUV422P8, YUV444P8, YUV420P10LE, YUV422P10LE, YUV444P10LE"
        )
    pixel_str, suffix = m.group(1), m.group(2)
    if suffix is None or suffix == "8":
        return PixelFormat(pixel_str), BitDepth.BIT8
    if suffix == "10LE":
        return PixelFormat(pixel_str), BitDepth.BIT10LE
    if suffix == "10BE":
        raise ValueError("Big-endian 10-bit is not supported in v1")
    # Should not reach here.
    raise ValueError(f"Unsupported format: {s!r}")


def chroma_subsampling(fmt: PixelFormat) -> tuple[int, int]:
    """Return (horizontal_factor, vertical_factor) for the given format.

    YUV420P -> (2, 2)  |  YUV422P -> (2, 1)  |  YUV444P -> (1, 1)
    """
    if fmt == PixelFormat.YUV420P:
        return (2, 2)
    if fmt == PixelFormat.YUV422P:
        return (2, 1)
    if fmt == PixelFormat.YUV444P:
        return (1, 1)
    raise ValueError(f"Unknown pixel format: {fmt}")


def frame_bytes(fmt: PixelFormat, w: int, h: int, bit_depth: BitDepth) -> int:
    """Bytes per frame for the given format/resolution/bit-depth.

    8-bit: 1 byte per sample.
    10-bit: 2 bytes per sample (16-bit LE words), MSB-aligned.
    """
    if w <= 0 or h <= 0:
        raise ValueError(f"Resolution must be positive, got {w}x{h}")
    bytes_per_sample = 1 if bit_depth == BitDepth.BIT8 else 2
    h_factor, v_factor = chroma_subsampling(fmt)
    y_bytes = w * h * bytes_per_sample
    chroma_w = w // h_factor
    chroma_h = h // v_factor
    uv_bytes = 2 * chroma_w * chroma_h * bytes_per_sample
    return y_bytes + uv_bytes
