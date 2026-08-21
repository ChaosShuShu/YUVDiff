"""Pixel-level absolute diff between two YUV frames."""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from yuvdiff.formats import PixelFormat
from yuvdiff.parser import YUVFrame


@dataclass
class DiffResult:
    """Output of DiffEngine.diff()."""

    diff_y: np.ndarray  # absolute diff of Y plane, shape (H, W)
    diff_u: np.ndarray  # shape (H/v, W/h) — native chroma resolution
    diff_v: np.ndarray
    mask: np.ndarray    # bool, shape (H, W) — True where any channel > threshold
    diff_pixel_count: int
    total_pixel_count: int  # H * W


class DiffEngine:
    """Per-channel absolute diff + threshold-based mask.

    The mask combines Y and upsampled U/V with a logical OR. Threshold
    is uniform across channels in v1.
    """

    def __init__(self, threshold: int = 4):
        if threshold < 0:
            raise ValueError(f"threshold must be >= 0, got {threshold}")
        self.threshold = threshold

    def diff(self, a: YUVFrame, b: YUVFrame) -> DiffResult:
        if (a.width, a.height) != (b.width, b.height):
            raise ValueError(
                f"Frame size mismatch: A=({a.width}x{a.height}) "
                f"vs B=({b.width}x{b.height})"
            )
        if a.pixel_format != b.pixel_format:
            raise ValueError(
                f"Pixel format mismatch: A={a.pixel_format} vs B={b.pixel_format}"
            )

        diff_y = np.abs(a.y.astype(np.int32) - b.y.astype(np.int32)).astype(np.int32)
        diff_u = np.abs(a.u.astype(np.int32) - b.u.astype(np.int32)).astype(np.int32)
        diff_v = np.abs(a.v.astype(np.int32) - b.v.astype(np.int32)).astype(np.int32)

        mask_y = diff_y > self.threshold
        h, w = a.y.shape

        mask_u_full = self._upsample_mask(diff_u > self.threshold, a.pixel_format, h, w)
        mask_v_full = self._upsample_mask(diff_v > self.threshold, a.pixel_format, h, w)
        mask = mask_y | mask_u_full | mask_v_full

        total = h * w
        return DiffResult(
            diff_y=diff_y,
            diff_u=diff_u,
            diff_v=diff_v,
            mask=mask,
            diff_pixel_count=int(mask.sum()),
            total_pixel_count=total,
        )

    @staticmethod
    def _upsample_mask(
        sub_mask: np.ndarray, fmt: PixelFormat, h: int, w: int
    ) -> np.ndarray:
        """Nearest-neighbor upsample a chroma mask to Y's resolution."""
        if fmt == PixelFormat.YUV444P:
            return sub_mask
        if fmt == PixelFormat.YUV422P:
            return np.kron(sub_mask, np.ones((1, 2), dtype=bool))
        return np.kron(sub_mask, np.ones((2, 2), dtype=bool))
