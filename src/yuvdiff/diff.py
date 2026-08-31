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
    diff_pixel_count: int  # Strictly d > 0 pixels count
    total_pixel_count: int  # H * W
    diff_ratio: float = 0.0
    diff_gt_2t: int = 0
    diff_gt_2t_ratio: float = 0.0
    diff_gt_t: int = 0
    diff_gt_t_ratio: float = 0.0
    diff_gt_half_t: int = 0
    diff_gt_half_t_ratio: float = 0.0
    diff_mean: float = 0.0
    diff_median: float = 0.0
    diff_max: int = 0
    diff_min: int = 0


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

        h, w = a.y.shape
        diff_u_full = self._upsample_plane(diff_u, a.pixel_format, h, w)
        diff_v_full = self._upsample_plane(diff_v, a.pixel_format, h, w)
        diff_pixel = np.maximum(diff_y, np.maximum(diff_u_full, diff_v_full))

        total = h * w
        non_zero_mask = diff_pixel > 0
        diff_pixel_count = int(non_zero_mask.sum())
        diff_ratio = diff_pixel_count / total if total > 0 else 0.0

        t = self.threshold
        t_half = t // 2
        t_2t = t * 2

        diff_gt_2t = int((diff_pixel > t_2t).sum())
        diff_gt_2t_ratio = diff_gt_2t / total if total > 0 else 0.0

        diff_gt_t = int((diff_pixel > t).sum())
        diff_gt_t_ratio = diff_gt_t / total if total > 0 else 0.0

        diff_gt_half_t = int((diff_pixel > t_half).sum())
        diff_gt_half_t_ratio = diff_gt_half_t / total if total > 0 else 0.0

        mask = diff_pixel > t

        if diff_pixel_count > 0:
            non_zero_diffs = diff_pixel[non_zero_mask]
            diff_mean = float(np.mean(non_zero_diffs))
            diff_median = float(np.median(non_zero_diffs))
            diff_max = int(np.max(non_zero_diffs))
            diff_min = int(np.min(non_zero_diffs))
        else:
            diff_mean = 0.0
            diff_median = 0.0
            diff_max = 0
            diff_min = 0

        return DiffResult(
            diff_y=diff_y,
            diff_u=diff_u,
            diff_v=diff_v,
            mask=mask,
            diff_pixel_count=diff_pixel_count,
            total_pixel_count=total,
            diff_ratio=diff_ratio,
            diff_gt_2t=diff_gt_2t,
            diff_gt_2t_ratio=diff_gt_2t_ratio,
            diff_gt_t=diff_gt_t,
            diff_gt_t_ratio=diff_gt_t_ratio,
            diff_gt_half_t=diff_gt_half_t,
            diff_gt_half_t_ratio=diff_gt_half_t_ratio,
            diff_mean=diff_mean,
            diff_median=diff_median,
            diff_max=diff_max,
            diff_min=diff_min,
        )

    @staticmethod
    def _upsample_plane(
        plane: np.ndarray, fmt: PixelFormat, h: int, w: int
    ) -> np.ndarray:
        """Nearest-neighbor upsample a chroma plane to Y's resolution."""
        if fmt == PixelFormat.YUV444P:
            return plane
        if fmt == PixelFormat.YUV422P:
            return np.kron(plane, np.ones((1, 2), dtype=plane.dtype))
        return np.kron(plane, np.ones((2, 2), dtype=plane.dtype))
