"""Render YUV frames + diff results to QImage for the GUI.

Four modes (RenderMode):
    ORIGINAL_A       — show frame A
    ORIGINAL_B       — show frame B
    HEATMAP          — diff intensity → color gradient
    THRESHOLD_MASK   — original A with red overlay on pixels above threshold
"""
from __future__ import annotations

from enum import Enum

import numpy as np

try:
    from PySide6.QtGui import QImage
except ImportError as e:  # pragma: no cover - GUI optional
    QImage = None  # type: ignore
    _IMPORT_ERROR = e
else:
    _IMPORT_ERROR = None

from yuvdiff.parser import YUVFrame


class RenderMode(Enum):
    ORIGINAL_A = "a"
    ORIGINAL_B = "b"
    HEATMAP = "heatmap"
    THRESHOLD_MASK = "mask"


# BT.601 limited-range YUV -> RGB matrix.
# R = Y + 1.402 * (V - 128)
# G = Y - 0.344136 * (U - 128) - 0.714136 * (V - 128)
# B = Y + 1.772 * (U - 128)
_BT601 = np.array([
    [1.0,  0.0,       1.402],
    [1.0, -0.344136, -0.714136],
    [1.0,  1.772,     0.0],
], dtype=np.float64)


def _require_qt() -> None:
    if QImage is None:
        raise RuntimeError(
            f"PySide6 QImage is not available ({_IMPORT_ERROR}); "
            "renderer requires Qt for image output"
        )


def _upsample_chroma(plane: np.ndarray, fmt) -> np.ndarray:
    """Nearest-neighbor upsample chroma to Y's resolution."""
    from yuvdiff.formats import PixelFormat
    if fmt == PixelFormat.YUV444P:
        return plane
    if fmt == PixelFormat.YUV422P:
        return np.kron(plane, np.ones((1, 2), dtype=plane.dtype))
    return np.kron(plane, np.ones((2, 2), dtype=plane.dtype))


class Renderer:
    """Convert (YUV frame [+ diff]) to a QImage in the chosen mode."""

    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height

    def render(
        self,
        frame_a: YUVFrame,
        frame_b: YUVFrame | None,
        diff,                       # DiffResult or None
        mode: RenderMode,
        threshold: int,
    ) -> "QImage":
        _require_qt()
        if mode == RenderMode.ORIGINAL_A:
            rgb = self._yuv_to_rgb(frame_a)
            return self._rgb_to_qimage(rgb)
        if mode == RenderMode.ORIGINAL_B:
            assert frame_b is not None
            rgb = self._yuv_to_rgb(frame_b)
            return self._rgb_to_qimage(rgb)
        if mode == RenderMode.HEATMAP:
            assert diff is not None
            return self._heatmap_to_qimage(diff, frame_a)
        if mode == RenderMode.THRESHOLD_MASK:
            assert diff is not None
            rgb = self._yuv_to_rgb(frame_a)
            return self._mask_to_qimage(rgb, diff, threshold)
        raise ValueError(f"Unknown render mode: {mode}")

    def _yuv_to_rgb(self, frame: YUVFrame) -> np.ndarray:
        if frame.bit_depth == 10:
            y = (frame.y >> 2).astype(np.uint8)
            u = (_upsample_chroma(frame.u, frame.pixel_format) >> 2).astype(np.uint8)
            v = (_upsample_chroma(frame.v, frame.pixel_format) >> 2).astype(np.uint8)
        else:
            y = frame.y.astype(np.uint8)
            u = _upsample_chroma(frame.u, frame.pixel_format).astype(np.uint8)
            v = _upsample_chroma(frame.v, frame.pixel_format).astype(np.uint8)
        yuv = np.stack([
            y.astype(np.float64),
            u.astype(np.float64),
            v.astype(np.float64),
        ], axis=-1)
        yuv_centered = yuv.copy()
        yuv_centered[..., 1] -= 128.0
        yuv_centered[..., 2] -= 128.0
        rgb = yuv_centered @ _BT601.T
        rgb = np.clip(rgb, 0, 255).astype(np.uint8)
        return rgb

    def _rgb_to_qimage(self, rgb: np.ndarray) -> "QImage":
        h, w, _ = rgb.shape
        return QImage(rgb.data, w, h, w * 3, QImage.Format_RGB888).copy()

    def _heatmap_to_qimage(self, diff, frame: YUVFrame) -> "QImage":
        max_val = 255 if frame.bit_depth == 8 else 1023
        norm = np.clip(diff.diff_y.astype(np.float64) / max_val, 0.0, 1.0)
        # 0 error -> Gray (128, 128, 128), Max error -> Pure Red (255, 0, 0)
        r = 128.0 + 127.0 * norm
        g = 128.0 * (1.0 - norm)
        b = 128.0 * (1.0 - norm)
        rgb = np.stack([r, g, b], axis=-1).astype(np.uint8)
        h, w = rgb.shape[:2]
        return QImage(rgb.data, w, h, w * 3, QImage.Format_RGB888).copy()

    def _mask_to_qimage(
        self, rgb_a: np.ndarray, diff, threshold: int
    ) -> "QImage":
        rgb = rgb_a.copy()
        mask = diff.mask
        overlay_r = 0.6 * 255 + 0.4 * rgb[..., 0]
        overlay_g = 0.4 * rgb[..., 1]
        overlay_b = 0.4 * rgb[..., 2]
        rgb[..., 0] = np.where(mask, overlay_r, rgb[..., 0]).astype(np.uint8)
        rgb[..., 1] = np.where(mask, overlay_g, rgb[..., 1]).astype(np.uint8)
        rgb[..., 2] = np.where(mask, overlay_b, rgb[..., 2]).astype(np.uint8)
        h, w = rgb.shape[:2]
        return QImage(rgb.data, w, h, w * 3, QImage.Format_RGB888).copy()
