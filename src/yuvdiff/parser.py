"""YUV file parser with mmap-backed lazy frame reads.

Reads planar YUV files (YUV420P/422P/444P, 8-bit or 10-bit LE).
Only the requested frame is read into memory; the underlying file is
mmap'd for large files (>1 MB) to avoid loading the whole sequence.

10-bit support:
    Two byte alignments are common in the wild:
      - MSB-aligned (H.264/HEVC/AV1 reference convention, the default
        for this tool): the 10-bit value occupies bits [15:6] of each
        16-bit LE word, recovered via `word >> 6`.
      - LSB-aligned (some encoders / older FFmpeg): the value occupies
        bits [9:0], recovered via `word & 0x3FF`.
    Use `bit_alignment="auto"` to detect from a small header sample, or
    pass an explicit `BitAlignment.MSB` / `BitAlignment.LSB`.
"""
from __future__ import annotations

import mmap
import os
from dataclasses import dataclass
from typing import Optional

import numpy as np

from yuvdiff.formats import (
    BitAlignment,
    BitDepth,
    PixelFormat,
    chroma_subsampling,
    frame_bytes,
    parse_format,
)


@dataclass
class YUVFrame:
    """A single YUV frame, planar layout."""

    y: np.ndarray
    u: np.ndarray
    v: np.ndarray
    bit_depth: int
    width: int
    height: int
    pixel_format: PixelFormat


def _extract_10bit(raw: np.ndarray, alignment: BitAlignment) -> np.ndarray:
    """Pull the 10-bit payload out of a uint16 LE word array."""
    if alignment == BitAlignment.MSB:
        return (raw >> 6).astype(np.uint16)
    return (raw & 0x3FF).astype(np.uint16)


def auto_detect_alignment(
    path: str, fmt: str, width: int, height: int, sample_bytes: int = 4096
) -> BitAlignment:
    """Inspect the first `sample_bytes` of a 10-bit YUV file and pick
    the byte alignment that yields plausible 10-bit content.

    Heuristic: the "correct" interpretation should produce values in a
    typical content range (max > 100, mean > 50). The "wrong"
    interpretation will collapse the values to a small range because
    the unused half of the 16-bit word is zero. If only one of the
    two interpretations is plausible, that wins; otherwise default MSB.
    """
    pixel_fmt, bit_depth = parse_format(fmt)
    if bit_depth != BitDepth.BIT10LE:
        # 8-bit files have no alignment question.
        return BitAlignment.MSB
    with open(path, "rb") as f:
        raw = np.frombuffer(f.read(sample_bytes), dtype="<u2")
    if raw.size == 0:
        return BitAlignment.MSB
    msb = raw >> 6
    lsb = raw & 0x3FF
    msb_plausible = msb.size > 0 and msb.max() > 100 and msb.mean() > 50
    lsb_plausible = lsb.size > 0 and lsb.max() > 100 and lsb.mean() > 50
    if lsb_plausible and not msb_plausible:
        return BitAlignment.LSB
    return BitAlignment.MSB


class YUVParser:
    """mmap-backed reader for a raw YUV file.

    Frames are read lazily; only the requested frame's bytes are copied
    out of the mmap window into a numpy array.
    """

    _MMAP_THRESHOLD = 1 << 20  # 1 MB

    def __init__(
        self,
        path: str,
        fmt: str,
        width: int,
        height: int,
        bit_alignment: BitAlignment | str = BitAlignment.MSB,
    ):
        if not os.path.exists(path):
            raise FileNotFoundError(f"YUV file not found: {path}")
        self.path = path
        self.width = width
        self.height = height
        self.pixel_format, self.bit_depth = parse_format(fmt)

        # Resolve alignment: accept enum or string; "auto" runs detection.
        if isinstance(bit_alignment, str):
            if bit_alignment == "auto":
                self.bit_alignment = auto_detect_alignment(path, fmt, width, height)
            else:
                self.bit_alignment = BitAlignment(bit_alignment)
        else:
            self.bit_alignment = bit_alignment

        self._frame_bytes = frame_bytes(
            self.pixel_format, width, height, self.bit_depth
        )
        self._file_size = os.path.getsize(path)
        self.num_frames = self._file_size // self._frame_bytes
        self._mmap: Optional[mmap.mmap] = None
        if self._file_size >= self._MMAP_THRESHOLD:
            with open(path, "rb") as f:
                self._mmap = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)

    def read_frame(self, idx: int) -> YUVFrame:
        if idx < 0 or idx >= self.num_frames:
            raise IndexError(
                f"Frame {idx} out of range [0, {self.num_frames})"
            )
        h_f, v_f = chroma_subsampling(self.pixel_format)
        cy, cx = self.height // v_f, self.width // h_f

        if self.bit_depth == BitDepth.BIT8:
            return self._read_8bit(idx, cy, cx)
        return self._read_10bit(idx, cy, cx)

    def _read_8bit(self, idx: int, cy: int, cx: int) -> YUVFrame:
        offset = idx * self._frame_bytes
        y_size = self.width * self.height
        uv_size = cy * cx
        y = self._view(offset, y_size, np.uint8).reshape(self.height, self.width)
        u = self._view(offset + y_size, uv_size, np.uint8).reshape(cy, cx)
        v = self._view(
            offset + y_size + uv_size, uv_size, np.uint8
        ).reshape(cy, cx)
        return YUVFrame(
            y=y.copy(),
            u=u.copy(),
            v=v.copy(),
            bit_depth=8,
            width=self.width,
            height=self.height,
            pixel_format=self.pixel_format,
        )

    def _read_10bit(self, idx: int, cy: int, cx: int) -> YUVFrame:
        offset = idx * self._frame_bytes
        y_bytes = self.width * self.height * 2
        uv_bytes = cy * cx * 2
        y_raw = self._view(offset, y_bytes, np.uint16).reshape(
            self.height, self.width
        )
        u_raw = self._view(offset + y_bytes, uv_bytes, np.uint16).reshape(cy, cx)
        v_raw = self._view(
            offset + y_bytes + uv_bytes, uv_bytes, np.uint16
        ).reshape(cy, cx)
        y = _extract_10bit(y_raw, self.bit_alignment)
        u = _extract_10bit(u_raw, self.bit_alignment)
        v = _extract_10bit(v_raw, self.bit_alignment)
        return YUVFrame(
            y=y, u=u, v=v,
            bit_depth=10,
            width=self.width,
            height=self.height,
            pixel_format=self.pixel_format,
        )

    def _view(self, offset: int, count_bytes: int, dtype) -> np.ndarray:
        """Return a numpy view of `count_bytes` starting at `offset`.

        For mmap files this is a zero-copy slice; for small files this is
        a fresh read.
        """
        if self._mmap is not None:
            return np.frombuffer(
                self._mmap[offset : offset + count_bytes], dtype=dtype
            )
        with open(self.path, "rb") as f:
            f.seek(offset)
            return np.frombuffer(f.read(count_bytes), dtype=dtype)

    def close(self) -> None:
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None

    def __enter__(self) -> "YUVParser":
        return self

    def __exit__(self, *args) -> None:
        self.close()
