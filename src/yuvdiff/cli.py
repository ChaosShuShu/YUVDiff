"""Command-line entry point for yuvdiff.

Usage:
    yuvdiff A.yuv B.yuv --format YUV420P8 --width 1920 --height 1080

Prints per-frame CSV to stdout with columns:
    frame, psnr_y, psnr_u, psnr_v, psnr_total, ssim_y, diff_pixels, total_pixels

Exit codes:
    0 success
    1 format / argument error
    2 file I/O error
"""
from __future__ import annotations

import argparse
import csv
import math
import sys
from typing import Optional

from yuvdiff.diff import DiffEngine
from yuvdiff.formats import BitDepth, parse_format
from yuvdiff.metrics import MetricsCalculator
from yuvdiff.parser import YUVParser


def _build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="yuvdiff",
        description="Pixel-level diff for raw YUV video sequences (A vs B).",
    )
    p.add_argument("a", help="Path to reference YUV (A)")
    p.add_argument("b", help="Path to test YUV (B)")
    p.add_argument(
        "--format", required=True,
        help="YUV format string, e.g. YUV420P8 or YUV422P10LE",
    )
    p.add_argument("--width", type=int, required=True)
    p.add_argument("--height", type=int, required=True)
    p.add_argument(
        "--frames", type=int, default=None,
        help="Number of frames to compare (default: min of both files)",
    )
    p.add_argument(
        "--threshold", type=int, default=4,
        help="Pixel diff threshold for mask (default: 4)",
    )
    p.add_argument(
        "--10bit-align", dest="tenbit_align",
        choices=["msb", "lsb", "auto"], default="auto",
        help="10-bit byte alignment (default: auto-detect from header)",
    )
    return p


def main(argv: Optional[list[str]] = None) -> int:
    args = _build_arg_parser().parse_args(argv)
    try:
        _, bit_depth = parse_format(args.format)
    except ValueError as e:
        print(f"yuvdiff: {e}", file=sys.stderr)
        return 1

    # 8-bit files ignore the alignment flag; force msb.
    align = args.tenbit_align if bit_depth == BitDepth.BIT10LE else "msb"

    try:
        parser_a = YUVParser(
            args.a, args.format, args.width, args.height, bit_alignment=align
        )
        parser_b = YUVParser(
            args.b, args.format, args.width, args.height, bit_alignment=align
        )
    except (FileNotFoundError, ValueError, OSError) as e:
        print(f"yuvdiff: {e}", file=sys.stderr)
        return 2

    if args.tenbit_align == "auto" and bit_depth == BitDepth.BIT10LE:
        print(
            f"# 10-bit alignment auto-detected: {parser_a.bit_alignment.value}",
            file=sys.stderr,
        )

    n = args.frames
    if n is None:
        n = min(parser_a.num_frames, parser_b.num_frames)
    n = min(n, parser_a.num_frames, parser_b.num_frames)

    diff_engine = DiffEngine(threshold=args.threshold)
    metrics = MetricsCalculator()

    writer = csv.writer(sys.stdout)
    writer.writerow([
        "frame", "psnr_y", "psnr_u", "psnr_v", "psnr_total",
        "ssim_y", "diff_pixels", "total_pixels",
    ])

    for i in range(n):
        try:
            fa = parser_a.read_frame(i)
            fb = parser_b.read_frame(i)
        except (IndexError, ValueError) as e:
            print(f"yuvdiff: frame {i}: {e}", file=sys.stderr)
            return 2
        psnr = metrics.psnr(fa, fb)
        ssim = metrics.ssim(fa, fb)
        diff = diff_engine.diff(fa, fb)
        writer.writerow([
            i,
            _fmt(psnr.y), _fmt(psnr.u), _fmt(psnr.v), _fmt(psnr.total),
            _fmt(ssim.y),
            diff.diff_pixel_count, diff.total_pixel_count,
        ])

    return 0


def _fmt(v: float) -> str:
    """Format a float for CSV. inf -> 'inf'."""
    if math.isnan(v):
        return "nan"
    if math.isinf(v) and v > 0:
        return "inf"
    if math.isinf(v) and v < 0:
        return "-inf"
    return f"{v:.4f}"


if __name__ == "__main__":
    sys.exit(main())
