"""PySide6 main window for YUVdiff.

Layout:
    Toolbar:  [Open A] [Open B] [Format] [W] [H] [Threshold] [Mode] [Play]
    Center:   Visualization canvas (QLabel with QPixmap)
    Sidebar:  Frame slider, FPS spinbox, status labels
    Status:   Per-frame PSNR/SSIM/diff counts

This module is optional: the CLI works without it. On systems without
PySide6/libEGL (e.g. some headless sandboxes), importing this module
sets the symbols to None; instantiating MainWindow raises a clear error.
"""
from __future__ import annotations

import os
import sys
from typing import Optional

import numpy as np

from yuvdiff.diff import DiffEngine, DiffResult
from yuvdiff.formats import BitAlignment, BitDepth, parse_format
from yuvdiff.metrics import MetricsCalculator
from yuvdiff.parser import YUVFrame, YUVParser, auto_detect_alignment
from yuvdiff.renderer import RenderMode, Renderer

try:
    from PySide6.QtCore import Qt, QTimer
    from PySide6.QtGui import QAction, QImage, QKeySequence, QPixmap
    from PySide6.QtWidgets import (
        QApplication,
        QComboBox,
        QFileDialog,
        QHBoxLayout,
        QLabel,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QSlider,
        QSpinBox,
        QStatusBar,
        QVBoxLayout,
        QWidget,
    )
    _QT_AVAILABLE = True
    _QT_IMPORT_ERROR: Exception | None = None
except ImportError as e:
    _QT_AVAILABLE = False
    _QT_IMPORT_ERROR = e
    # Provide stand-in types so the module can be parsed/imported
    Qt = QTimer = QAction = QImage = QKeySequence = QPixmap = None  # type: ignore
    QApplication = QComboBox = QFileDialog = QHBoxLayout = QLabel = None  # type: ignore
    QMainWindow = QMessageBox = QPushButton = QSlider = QSpinBox = None  # type: ignore
    QStatusBar = QVBoxLayout = QWidget = None  # type: ignore


def _require_qt() -> None:
    if not _QT_AVAILABLE:
        raise RuntimeError(
            f"PySide6 is not available ({_QT_IMPORT_ERROR}); "
            "the GUI cannot run in this environment. Use the CLI instead."
        )


FORMAT_OPTIONS = [
    "YUV420P8", "YUV422P8", "YUV444P8",
    "YUV420P10LE", "YUV422P10LE", "YUV444P10LE",
]


if _QT_AVAILABLE:
    class MainWindowImpl(QMainWindow):
        def __init__(self) -> None:
            super().__init__()
            self.setWindowTitle("YUVdiff")
            self.resize(1280, 800)

            # State
            self.parser_a: Optional[YUVParser] = None
            self.parser_b: Optional[YUVParser] = None
            self.frame_a: Optional[YUVFrame] = None
            self.frame_b: Optional[YUVFrame] = None
            self.diff_engine = DiffEngine(threshold=4)
            self.metrics = MetricsCalculator()
            self.renderer: Optional[Renderer] = None
            self._play_timer = QTimer(self)
            self._play_timer.timeout.connect(self._on_play_tick)

            self._build_ui()
            self._build_menu()

        # ---------- UI construction ----------

        def _build_ui(self) -> None:
            central = QWidget()
            self.setCentralWidget(central)
            root = QVBoxLayout(central)

            # Toolbar row 1: file open + format + W/H
            tb1 = QHBoxLayout()
            self.btn_open_a = QPushButton("Open A…")
            self.btn_open_b = QPushButton("Open B…")
            self.btn_open_a.clicked.connect(lambda: self._on_open("a"))
            self.btn_open_b.clicked.connect(lambda: self._on_open("b"))
            self.combo_format = QComboBox()
            self.combo_format.addItems(FORMAT_OPTIONS)
            self.combo_format.currentTextChanged.connect(self._on_format_changed)
            self.spin_w = QSpinBox()
            self.spin_w.setRange(1, 16384)
            self.spin_w.setValue(1920)
            self.spin_h = QSpinBox()
            self.spin_h.setRange(1, 16384)
            self.spin_h.setValue(1080)
            # 10-bit alignment dropdown (only relevant for *10LE formats)
            self.combo_align = QComboBox()
            self.combo_align.addItem("Auto", "auto")
            self.combo_align.addItem("MSB (HEVC/AV1)", BitAlignment.MSB)
            self.combo_align.addItem("LSB (some FFmpeg)", BitAlignment.LSB)
            self.lbl_align = QLabel("10-bit align:")
            tb1.addWidget(self.btn_open_a)
            tb1.addWidget(self.btn_open_b)
            tb1.addWidget(QLabel("Format:"))
            tb1.addWidget(self.combo_format)
            tb1.addWidget(QLabel("W:"))
            tb1.addWidget(self.spin_w)
            tb1.addWidget(QLabel("H:"))
            tb1.addWidget(self.spin_h)
            tb1.addWidget(self.lbl_align)
            tb1.addWidget(self.combo_align)
            root.addLayout(tb1)
            self._on_format_changed(self.combo_format.currentText())

            # Toolbar row 2: threshold + mode + play + export
            tb2 = QHBoxLayout()
            self.spin_threshold = QSpinBox()
            self.spin_threshold.setRange(0, 1023)
            self.spin_threshold.setValue(4)
            self.combo_mode = QComboBox()
            for mode in RenderMode:
                self.combo_mode.addItem(mode.name, mode)
            self.btn_export_current = QPushButton("Export PNG")
            self.btn_export_current.clicked.connect(self._on_export_current)
            self.btn_export_all = QPushButton("Export All…")
            self.btn_export_all.clicked.connect(self._on_export_all)
            self.btn_play = QPushButton("▶ Play")
            self.btn_play.setCheckable(True)
            self.btn_play.toggled.connect(self._on_play_toggle)
            self.spin_fps = QSpinBox()
            self.spin_fps.setRange(1, 120)
            self.spin_fps.setValue(25)
            tb2.addWidget(QLabel("Threshold:"))
            tb2.addWidget(self.spin_threshold)
            tb2.addWidget(QLabel("Mode:"))
            tb2.addWidget(self.combo_mode)
            tb2.addWidget(self.btn_export_current)
            tb2.addWidget(self.btn_export_all)
            tb2.addWidget(self.btn_play)
            tb2.addWidget(QLabel("FPS:"))
            tb2.addWidget(self.spin_fps)
            root.addLayout(tb2)

            # Frame slider
            fr = QHBoxLayout()
            self.slider = QSlider(Qt.Horizontal)
            self.slider.setRange(0, 0)
            self.slider.valueChanged.connect(self._on_slider_change)
            self.spin_frame = QSpinBox()
            self.spin_frame.setRange(0, 0)
            self.spin_frame.valueChanged.connect(self.slider.setValue)
            self.slider.valueChanged.connect(self.spin_frame.setValue)
            fr.addWidget(QLabel("Frame:"))
            fr.addWidget(self.slider)
            fr.addWidget(self.spin_frame)
            root.addLayout(fr)

            # Canvas
            self.canvas = QLabel("Open A and B to begin.")
            self.canvas.setAlignment(Qt.AlignCenter)
            self.canvas.setMinimumSize(640, 360)
            self.canvas.setStyleSheet("background: #222; color: #888;")
            root.addWidget(self.canvas, stretch=1)

            # Status bar with metrics
            self.status = QStatusBar()
            self.setStatusBar(self.status)
            self.lbl_metrics = QLabel("—")
            self.status.addPermanentWidget(self.lbl_metrics)

        def _build_menu(self) -> None:
            for i, mode in enumerate(RenderMode, start=1):
                act = QAction(self)
                act.setShortcut(QKeySequence(str(i)))
                act.triggered.connect(lambda checked=False, m=mode: self._set_mode(m))
                self.addAction(act)
            act_space = QAction(self)
            act_space.setShortcut(QKeySequence(Qt.Key_Space))
            act_space.triggered.connect(self.btn_play.toggle)
            self.addAction(act_space)
            act_left = QAction(self)
            act_left.setShortcut(QKeySequence(Qt.Key_Left))
            act_left.triggered.connect(lambda: self._step(-1))
            self.addAction(act_left)
            act_right = QAction(self)
            act_right.setShortcut(QKeySequence(Qt.Key_Right))
            act_right.triggered.connect(lambda: self._step(+1))
            self.addAction(act_right)

        # ---------- Slots ----------

        def _on_open(self, which: str) -> None:
            path, _ = QFileDialog.getOpenFileName(
                self, f"Open {which.upper()} YUV", "", "YUV files (*.yuv);;All files (*)"
            )
            if not path:
                return
            try:
                fmt = self.combo_format.currentText()
                w = self.spin_w.value()
                h = self.spin_h.value()
                align = self.combo_align.currentData()
                parser = YUVParser(path, fmt, w, h, bit_alignment=align)
            except Exception as e:
                QMessageBox.critical(self, "Open failed", str(e))
                return
            if which == "a":
                if self.parser_a:
                    self.parser_a.close()
                self.parser_a = parser
            else:
                if self.parser_b:
                    self.parser_b.close()
                self.parser_b = parser
            self._maybe_load_frame()

        def _maybe_load_frame(self) -> None:
            if not (self.parser_a and self.parser_b):
                return
            if (self.parser_a.width, self.parser_a.height) != (
                self.parser_b.width, self.parser_b.height
            ):
                QMessageBox.critical(
                    self, "Resolution mismatch",
                    f"A={self.parser_a.width}x{self.parser_a.height}, "
                    f"B={self.parser_b.width}x{self.parser_b.height}",
                )
                return
            n = min(self.parser_a.num_frames, self.parser_b.num_frames)
            self.slider.setRange(0, max(0, n - 1))
            self.spin_frame.setRange(0, max(0, n - 1))
            self.renderer = Renderer(self.parser_a.width, self.parser_a.height)
            self._show_frame(0)

        def _on_slider_change(self, idx: int) -> None:
            self._show_frame(idx)

        def _set_mode(self, mode: RenderMode) -> None:
            self.combo_mode.setCurrentIndex(list(RenderMode).index(mode))
            self._show_frame(self.slider.value())

        def _on_format_changed(self, fmt: str) -> None:
            """Show the 10-bit-align dropdown only for *10LE formats."""
            is_10bit = "10LE" in fmt
            self.lbl_align.setVisible(is_10bit)
            self.combo_align.setVisible(is_10bit)

        def _on_play_toggle(self, on: bool) -> None:
            if on:
                fps = self.spin_fps.value()
                self._play_timer.start(int(1000 / fps))
                self.btn_play.setText("⏸ Pause")
            else:
                self._play_timer.stop()
                self.btn_play.setText("▶ Play")

        def _on_play_tick(self) -> None:
            n = self.slider.maximum() + 1
            if n == 0:
                return
            idx = (self.slider.value() + 1) % n
            self.slider.setValue(idx)

        def _step(self, delta: int) -> None:
            n = self.slider.maximum() + 1
            if n == 0:
                return
            self.slider.setValue((self.slider.value() + delta) % n)

        # ---------- Rendering ----------

        def _show_frame(self, idx: int) -> None:
            if not (self.parser_a and self.parser_b and self.renderer):
                return
            try:
                self.frame_a = self.parser_a.read_frame(idx)
                self.frame_b = self.parser_b.read_frame(idx)
            except Exception as e:
                QMessageBox.critical(self, "Read failed", str(e))
                return
            threshold = self.spin_threshold.value()
            self.diff_engine = DiffEngine(threshold=threshold)
            diff: DiffResult = self.diff_engine.diff(self.frame_a, self.frame_b)
            mode: RenderMode = self.combo_mode.currentData()
            img: QImage = self.renderer.render(
                self.frame_a, self.frame_b, diff, mode, threshold
            )
            pix = QPixmap.fromImage(img)
            self.canvas.setPixmap(pix)
            self._update_metrics(diff)

        def _update_metrics(self, diff: DiffResult) -> None:
            psnr = self.metrics.psnr(self.frame_a, self.frame_b)
            ssim = self.metrics.ssim(self.frame_a, self.frame_b)
            n = self.slider.maximum() + 1
            idx = self.slider.value()
            pct = 100.0 * diff.diff_pixel_count / max(1, diff.total_pixel_count)
            align = ""
            if self.parser_a and self.parser_a.bit_depth == 10:
                align = f"  |  Align={self.parser_a.bit_alignment.value}"
            self.lbl_metrics.setText(
                f"Frame {idx}/{n-1}  |  "
                f"PSNR Y={psnr.y:.2f} U={psnr.u:.2f} V={psnr.v:.2f} T={psnr.total:.2f}  |  "
                f"SSIM Y={ssim.y:.4f}  |  "
                f"Diff: {pct:.2f}% ({diff.diff_pixel_count}/{diff.total_pixel_count})"
                f"{align}"
            )

        def _on_export_current(self) -> None:
            if self.canvas.pixmap() is None:
                return
            path, _ = QFileDialog.getSaveFileName(
                self, "Save current frame", "frame.png", "PNG (*.png)"
            )
            if not path:
                return
            self.canvas.pixmap().save(path, "PNG")

        def _on_export_all(self) -> None:
            if not (self.parser_a and self.parser_b):
                return
            out_dir = QFileDialog.getExistingDirectory(self, "Choose output directory")
            if not out_dir:
                return
            a_base = os.path.splitext(os.path.basename(self.parser_a.path))[0]
            b_base = os.path.splitext(os.path.basename(self.parser_b.path))[0]
            n = min(self.parser_a.num_frames, self.parser_b.num_frames)
            app = QApplication.instance()
            for i in range(n):
                self._show_frame(i)
                if app is not None:
                    app.processEvents()
                fname = f"{a_base}_vs_{b_base}_frame_{i:05d}.png"
                self.canvas.pixmap().save(os.path.join(out_dir, fname), "PNG")
            self.status.showMessage(f"Exported {n} frames to {out_dir}", 5000)

        # ---------- Cleanup ----------

        def closeEvent(self, event) -> None:
            if self.parser_a:
                self.parser_a.close()
            if self.parser_b:
                self.parser_b.close()
            super().closeEvent(event)

    MainWindow = MainWindowImpl
else:  # not _QT_AVAILABLE

    class MainWindow:  # type: ignore[no-redef]
        """Stub MainWindow for environments without PySide6.

        Instantiating this raises a clear error directing the user to
        the CLI. Importing the module does not fail, so other code can
        safely reference `yuvdiff.gui.MainWindow`.
        """

        def __init__(self, *args, **kwargs):
            _require_qt()


def main() -> int:
    """Entry point for `python -m yuvdiff.gui`."""
    _require_qt()
    app = QApplication.instance() or QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
