import os
import subprocess
import sys

import numpy as np
import pytest

from tests.conftest import _write_synthetic_yuv


def _run_cli(args, cwd=None):
    result = subprocess.run(
        ["yuvdiff", *args],
        capture_output=True,
        text=True,
        cwd=cwd or os.getcwd(),
    )
    return result


class TestCliHelp:
    def test_no_args_prints_usage_and_exits_nonzero(self):
        result = _run_cli([])
        assert result.returncode != 0
        assert "usage" in result.stderr.lower() or "usage" in result.stdout.lower()


class TestCliEndToEnd:
    def test_identical_files_produce_inf_psnr(self, tmp_path, rng):
        path = tmp_path / "a.yuv"
        _write_synthetic_yuv(path, "YUV420P8", 32, 16, 3, rng)
        path_b = tmp_path / "b.yuv"
        path_b.write_bytes(path.read_bytes())
        result = _run_cli([
            str(path), str(path_b),
            "--format", "YUV420P8",
            "--width", "32", "--height", "16",
            "--frames", "3",
        ])
        assert result.returncode == 0, f"stderr: {result.stderr}"
        lines = [l for l in result.stdout.strip().split("\n") if l]
        assert len(lines) == 4
        assert "psnr_y" in lines[0]
        for line in lines[1:]:
            assert "inf" in line

    def test_different_files_produce_finite_psnr(self, tmp_path, rng):
        path_a = tmp_path / "a.yuv"
        _write_synthetic_yuv(path_a, "YUV420P8", 32, 16, 2, rng)
        path_b = tmp_path / "b.yuv"
        _write_synthetic_yuv(path_b, "YUV420P8", 32, 16, 2, np.random.default_rng(99))
        result = _run_cli([
            str(path_a), str(path_b),
            "--format", "YUV420P8",
            "--width", "32", "--height", "16",
        ])
        assert result.returncode == 0, f"stderr: {result.stderr}"
        lines = [l for l in result.stdout.strip().split("\n") if l]
        assert len(lines) == 3

    def test_format_mismatch_exits_1(self, tmp_path, rng):
        path = tmp_path / "a.yuv"
        _write_synthetic_yuv(path, "YUV420P8", 32, 16, 1, rng)
        result = _run_cli([
            str(path), str(path),
            "--format", "YUV420P10BE",
            "--width", "32", "--height", "16",
        ])
        assert result.returncode == 1
