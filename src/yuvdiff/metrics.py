"""Quantitative metrics: PSNR and SSIM."""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy.ndimage import uniform_filter

from yuvdiff.formats import PixelFormat
from yuvdiff.parser import YUVFrame


@dataclass
class PSNRResult:
    """PSNR per channel and the sample-count-weighted total."""

    y: float
    u: float
    v: float
    total: float


@dataclass
class SSIMResult:
    """Mean SSIM. v1: Y channel only."""

    y: float


def _max_for(bit_depth: int) -> int:
    return 255 if bit_depth == 8 else 1023


def _psnr_channel(a: np.ndarray, b: np.ndarray, max_val: int) -> float:
    """PSNR for a single channel. Returns inf for identical inputs."""
    diff = a.astype(np.float64) - b.astype(np.float64)
    mse = float(np.mean(diff * diff))
    if mse == 0:
        return math.inf
    return 10.0 * math.log10((max_val * max_val) / mse)


def _mse_weight(pixel_format: PixelFormat) -> tuple[int, int, int]:
    """Return (y_weight, u_weight, v_weight) for MSE averaging.

    YUV420 -> 4:1:1 (per pixel: Y has 4 samples, U and V have 1 each)
    YUV422 -> 2:1:1
    YUV444 -> 1:1:1
    """
    if pixel_format == PixelFormat.YUV420P:
        return (4, 1, 1)
    if pixel_format == PixelFormat.YUV422P:
        return (2, 1, 1)
    return (1, 1, 1)


def _mse_channel(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.mean((a.astype(np.float64) - b.astype(np.float64)) ** 2))


class MetricsCalculator:
    """PSNR and SSIM. All stateless, methods are pure functions of inputs."""

    def psnr(self, a: YUVFrame, b: YUVFrame) -> PSNRResult:
        if (a.width, a.height) != (b.width, b.height):
            raise ValueError("Frame size mismatch")
        if a.bit_depth != b.bit_depth:
            raise ValueError("Bit depth mismatch")
        max_val = _max_for(a.bit_depth)
        psnr_y = _psnr_channel(a.y, b.y, max_val)
        psnr_u = _psnr_channel(a.u, b.u, max_val)
        psnr_v = _psnr_channel(a.v, b.v, max_val)
        # Combine MSEs sample-count weighted; do NOT average PSNRs.
        y_w, u_w, v_w = _mse_weight(a.pixel_format)
        mse_y = _mse_channel(a.y, b.y)
        mse_u = _mse_channel(a.u, b.u)
        mse_v = _mse_channel(a.v, b.v)
        mse_total = (y_w * mse_y + u_w * mse_u + v_w * mse_v) / (y_w + u_w + v_w)
        psnr_total = (
            math.inf if mse_total == 0
            else 10.0 * math.log10((max_val * max_val) / mse_total)
        )
        return PSNRResult(y=psnr_y, u=psnr_u, v=psnr_v, total=psnr_total)

    def ssim(self, a: YUVFrame, b: YUVFrame) -> SSIMResult:
        """Mean SSIM over the Y plane. v1: U/V SSIM not implemented."""
        if (a.width, a.height) != (b.width, b.height):
            raise ValueError("Frame size mismatch")
        if a.bit_depth != b.bit_depth:
            raise ValueError("Bit depth mismatch")
        max_val = _max_for(a.bit_depth)
        c1 = (0.01 * max_val) ** 2
        c2 = (0.03 * max_val) ** 2
        ssim_y = _ssim_channel(a.y, b.y, c1, c2)
        return SSIMResult(y=ssim_y)


def _ssim_channel(
    a: np.ndarray, b: np.ndarray, c1: float, c2: float
) -> float:
    """Compute mean SSIM over a single channel using 11x11 box filter."""
    a_f = a.astype(np.float64)
    b_f = b.astype(np.float64)
    size = 11
    mu_a = uniform_filter(a_f, size=size, mode="reflect")
    mu_b = uniform_filter(b_f, size=size, mode="reflect")
    mu_a_sq = mu_a * mu_a
    mu_b_sq = mu_b * mu_b
    mu_ab = mu_a * mu_b
    sigma_a_sq = uniform_filter(a_f * a_f, size=size, mode="reflect") - mu_a_sq
    sigma_b_sq = uniform_filter(b_f * b_f, size=size, mode="reflect") - mu_b_sq
    sigma_ab = uniform_filter(a_f * b_f, size=size, mode="reflect") - mu_ab
    numerator = (2 * mu_ab + c1) * (2 * sigma_ab + c2)
    denominator = (mu_a_sq + mu_b_sq + c1) * (sigma_a_sq + sigma_b_sq + c2)
    ssim_map = numerator / denominator
    return float(ssim_map.mean())
