#include "yuvdiff/gui.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QMessageBox>
#include <QPixmap>
#include <QVBoxLayout>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace yuvdiff {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("YUVdiff");
    resize(1280, 800);

    play_timer_ = new QTimer(this);
    connect(play_timer_, &QTimer::timeout, this, &MainWindow::on_play_tick);

    build_ui();
    build_shortcuts();
}

MainWindow::~MainWindow() {
    if (play_timer_->isActive()) {
        play_timer_->stop();
    }
}

void MainWindow::build_ui() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* root = new QVBoxLayout(central);

    // Toolbar Row 1
    QHBoxLayout* tb1 = new QHBoxLayout();
    btn_open_a_ = new QPushButton("Open A…", this);
    btn_open_b_ = new QPushButton("Open B…", this);
    connect(btn_open_a_, &QPushButton::clicked, this, &MainWindow::on_open_a);
    connect(btn_open_b_, &QPushButton::clicked, this, &MainWindow::on_open_b);

    combo_format_ = new QComboBox(this);
    combo_format_->addItems({
        "YUV420P8", "YUV422P8", "YUV444P8",
        "YUV420P10LE", "YUV422P10LE", "YUV444P10LE"
    });
    connect(combo_format_, &QComboBox::currentTextChanged, this, &MainWindow::on_format_changed);

    spin_w_ = new QSpinBox(this);
    spin_w_->setRange(1, 16384);
    spin_w_->setValue(1920);

    spin_h_ = new QSpinBox(this);
    spin_h_->setRange(1, 16384);
    spin_h_->setValue(1080);

    lbl_align_ = new QLabel("10-bit align:", this);
    combo_align_ = new QComboBox(this);
    combo_align_->addItem("Auto", "auto");
    combo_align_->addItem("MSB (HEVC/AV1)", "msb");
    combo_align_->addItem("LSB (some FFmpeg)", "lsb");

    tb1->addWidget(btn_open_a_);
    tb1->addWidget(btn_open_b_);
    tb1->addWidget(new QLabel("Format:", this));
    tb1->addWidget(combo_format_);
    tb1->addWidget(new QLabel("W:", this));
    tb1->addWidget(spin_w_);
    tb1->addWidget(new QLabel("H:", this));
    tb1->addWidget(spin_h_);
    tb1->addWidget(lbl_align_);
    tb1->addWidget(combo_align_);
    root->addLayout(tb1);

    // Toolbar Row 2
    QHBoxLayout* tb2 = new QHBoxLayout();
    spin_threshold_ = new QSpinBox(this);
    spin_threshold_->setRange(0, 1023);
    spin_threshold_->setValue(4);
    connect(spin_threshold_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_threshold_changed);

    combo_mode_ = new QComboBox(this);
    combo_mode_->addItem("ORIGINAL_A", static_cast<int>(RenderMode::ORIGINAL_A));
    combo_mode_->addItem("ORIGINAL_B", static_cast<int>(RenderMode::ORIGINAL_B));
    combo_mode_->addItem("HEATMAP", static_cast<int>(RenderMode::HEATMAP));
    combo_mode_->addItem("THRESHOLD_MASK", static_cast<int>(RenderMode::THRESHOLD_MASK));
    connect(combo_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_mode_changed);

    btn_export_current_ = new QPushButton("Export PNG", this);
    connect(btn_export_current_, &QPushButton::clicked, this, &MainWindow::on_export_current);

    btn_export_all_ = new QPushButton("Export All…", this);
    connect(btn_export_all_, &QPushButton::clicked, this, &MainWindow::on_export_all);

    btn_play_ = new QPushButton("▶ Play", this);
    btn_play_->setCheckable(true);
    connect(btn_play_, &QPushButton::toggled, this, &MainWindow::on_play_toggled);

    spin_fps_ = new QSpinBox(this);
    spin_fps_->setRange(1, 120);
    spin_fps_->setValue(25);

    tb2->addWidget(new QLabel("Threshold:", this));
    tb2->addWidget(spin_threshold_);
    tb2->addWidget(new QLabel("Mode:", this));
    tb2->addWidget(combo_mode_);
    tb2->addWidget(btn_export_current_);
    tb2->addWidget(btn_export_all_);
    tb2->addWidget(btn_play_);
    tb2->addWidget(new QLabel("FPS:", this));
    tb2->addWidget(spin_fps_);
    root->addLayout(tb2);

    // Frame Slider row
    QHBoxLayout* fr = new QHBoxLayout();
    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(0, 0);
    spin_frame_ = new QSpinBox(this);
    spin_frame_->setRange(0, 0);

    connect(slider_, &QSlider::valueChanged, spin_frame_, &QSpinBox::setValue);
    connect(spin_frame_, QOverload<int>::of(&QSpinBox::valueChanged), slider_, &QSlider::setValue);
    connect(slider_, &QSlider::valueChanged, this, &MainWindow::on_slider_changed);

    fr->addWidget(new QLabel("Frame:", this));
    fr->addWidget(slider_);
    fr->addWidget(spin_frame_);
    root->addLayout(fr);

    // Canvas
    canvas_ = new QLabel("Open A and B to begin.", this);
    canvas_->setAlignment(Qt::AlignCenter);
    canvas_->setMinimumSize(640, 360);
    canvas_->setStyleSheet("background: #222; color: #888;");
    root->addWidget(canvas_, 1);

    // Status bar
    lbl_metrics_ = new QLabel("—", this);
    statusBar()->addPermanentWidget(lbl_metrics_);

    on_format_changed(combo_format_->currentText());
}

void MainWindow::build_shortcuts() {
    // Mode hotkeys 1, 2, 3, 4
    for (int i = 0; i < combo_mode_->count(); ++i) {
        QAction* act = new QAction(this);
        act->setShortcut(QKeySequence(QString::number(i + 1)));
        connect(act, &QAction::triggered, this, [this, i]() {
            combo_mode_->setCurrentIndex(i);
        });
        addAction(act);
    }

    // Space to toggle play
    QAction* act_space = new QAction(this);
    act_space->setShortcut(QKeySequence(Qt::Key_Space));
    connect(act_space, &QAction::triggered, this, [this]() {
        btn_play_->toggle();
    });
    addAction(act_space);

    // Left / Right arrows
    QAction* act_left = new QAction(this);
    act_left->setShortcut(QKeySequence(Qt::Key_Left));
    connect(act_left, &QAction::triggered, this, [this]() { step_frame(-1); });
    addAction(act_left);

    QAction* act_right = new QAction(this);
    act_right->setShortcut(QKeySequence(Qt::Key_Right));
    connect(act_right, &QAction::triggered, this, [this]() { step_frame(1); });
    addAction(act_right);
}

void MainWindow::on_open_a() {
    open_file("a");
}

void MainWindow::on_open_b() {
    open_file("b");
}

void MainWindow::open_file(const QString& which) {
    QString path = QFileDialog::getOpenFileName(
        this, "Open " + which.toUpper() + " YUV", "", "YUV files (*.yuv);;All files (*)"
    );
    if (path.isEmpty()) return;

    std::string std_path = path.toStdString();

    // 1. Try auto-detecting resolution from filename
    auto auto_res = try_parse_resolution_from_filename(std_path);
    if (auto_res.has_value()) {
        spin_w_->setValue(auto_res->first);
        spin_h_->setValue(auto_res->second);
    }

    // 2. Try auto-detecting format from filename
    auto auto_fmt = try_parse_format_from_filename(std_path);
    if (auto_fmt.has_value()) {
        std::string fmt_str = to_string(auto_fmt->first) + to_string(auto_fmt->second);
        int idx = combo_format_->findText(QString::fromStdString(fmt_str));
        if (idx >= 0) {
            combo_format_->setCurrentIndex(idx);
        }
    }

    try {
        std::string fmt = combo_format_->currentText().toStdString();
        int w = spin_w_->value();
        int h = spin_h_->value();
        std::string align = combo_align_->currentData().toString().toStdString();

        auto parser = std::make_unique<YUVParser>(std_path, fmt, w, h, align);
        if (which == "a") {
            parser_a_ = std::move(parser);
        } else {
            parser_b_ = std::move(parser);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Open failed", e.what());
        return;
    }

    maybe_load_frame();
}

void MainWindow::on_format_changed(const QString& fmt) {
    bool is_10bit = fmt.contains("10LE");
    lbl_align_->setVisible(is_10bit);
    combo_align_->setVisible(is_10bit);
}

void MainWindow::maybe_load_frame() {
    if (!parser_a_ && !parser_b_) return;

    if (parser_a_ && parser_b_) {
        if (parser_a_->width() != parser_b_->width() || parser_a_->height() != parser_b_->height()) {
            QMessageBox::critical(
                this, "Resolution mismatch",
                QString("A=%1x%2 vs B=%3x%4")
                    .arg(parser_a_->width()).arg(parser_a_->height())
                    .arg(parser_b_->width()).arg(parser_b_->height())
            );
            return;
        }

        int n = static_cast<int>(std::min(parser_a_->num_frames(), parser_b_->num_frames()));
        slider_->setRange(0, std::max(0, n - 1));
        spin_frame_->setRange(0, std::max(0, n - 1));

        renderer_ = std::make_unique<Renderer>(parser_a_->width(), parser_a_->height());
        show_frame(0);
    } else if (parser_a_) { // Single Video A
        int n = static_cast<int>(parser_a_->num_frames());
        slider_->setRange(0, std::max(0, n - 1));
        spin_frame_->setRange(0, std::max(0, n - 1));

        combo_mode_->setCurrentIndex(0); // ORIGINAL_A
        renderer_ = std::make_unique<Renderer>(parser_a_->width(), parser_a_->height());
        show_frame(0);
    } else if (parser_b_) { // Single Video B
        int n = static_cast<int>(parser_b_->num_frames());
        slider_->setRange(0, std::max(0, n - 1));
        spin_frame_->setRange(0, std::max(0, n - 1));

        combo_mode_->setCurrentIndex(1); // ORIGINAL_B
        renderer_ = std::make_unique<Renderer>(parser_b_->width(), parser_b_->height());
        show_frame(0);
    }
}

void MainWindow::on_slider_changed(int idx) {
    show_frame(idx);
}

void MainWindow::on_mode_changed(int index) {
    (void)index;
    show_frame(slider_->value());
}

void MainWindow::on_threshold_changed(int val) {
    diff_engine_.set_threshold(val);
    show_frame(slider_->value());
}

void MainWindow::on_play_toggled(bool checked) {
    if (checked) {
        int fps = spin_fps_->value();
        play_timer_->start(1000 / fps);
        btn_play_->setText("⏸ Pause");
    } else {
        play_timer_->stop();
        btn_play_->setText("▶ Play");
    }
}

void MainWindow::on_play_tick() {
    int max_val = slider_->maximum();
    if (max_val <= 0) return;
    int next_val = (slider_->value() + 1) % (max_val + 1);
    slider_->setValue(next_val);
}

void MainWindow::step_frame(int delta) {
    int max_val = slider_->maximum();
    if (max_val <= 0) return;
    int next_val = (slider_->value() + delta + (max_val + 1)) % (max_val + 1);
    slider_->setValue(next_val);
}

void MainWindow::show_frame(int idx) {
    if (!renderer_) return;

    // Both videos loaded: Dual Diff Mode
    if (parser_a_ && parser_b_) {
        try {
            frame_a_ = parser_a_->read_frame(static_cast<size_t>(idx));
            frame_b_ = parser_b_->read_frame(static_cast<size_t>(idx));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Read failed", e.what());
            return;
        }

        diff_engine_.set_threshold(spin_threshold_->value());
        DiffResult diff = diff_engine_.diff(*frame_a_, *frame_b_);

        RenderMode mode = static_cast<RenderMode>(combo_mode_->currentData().toInt());
        RgbImage rgb = renderer_->render(*frame_a_, &(*frame_b_), &diff, mode, spin_threshold_->value());

        QImage qimg(rgb.data.data(), rgb.width, rgb.height, rgb.width * 3, QImage::Format_RGB888);
        canvas_->setPixmap(QPixmap::fromImage(qimg));

        update_metrics(diff);
    }
    // Single Video A Mode
    else if (parser_a_) {
        try {
            frame_a_ = parser_a_->read_frame(static_cast<size_t>(idx));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Read failed", e.what());
            return;
        }

        RgbImage rgb = renderer_->yuv_to_rgb(*frame_a_);
        QImage qimg(rgb.data.data(), rgb.width, rgb.height, rgb.width * 3, QImage::Format_RGB888);
        canvas_->setPixmap(QPixmap::fromImage(qimg));

        QString fname = QFileInfo(QString::fromStdString(parser_a_->path())).fileName();
        int n = slider_->maximum() + 1;
        lbl_metrics_->setText(
            QString("Frame %1/%2  |  Single Video A: %3  |  %4x%5 %6")
                .arg(idx).arg(n - 1)
                .arg(fname)
                .arg(parser_a_->width()).arg(parser_a_->height())
                .arg(combo_format_->currentText())
        );
    }
    // Single Video B Mode
    else if (parser_b_) {
        try {
            frame_b_ = parser_b_->read_frame(static_cast<size_t>(idx));
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Read failed", e.what());
            return;
        }

        RgbImage rgb = renderer_->yuv_to_rgb(*frame_b_);
        QImage qimg(rgb.data.data(), rgb.width, rgb.height, rgb.width * 3, QImage::Format_RGB888);
        canvas_->setPixmap(QPixmap::fromImage(qimg));

        QString fname = QFileInfo(QString::fromStdString(parser_b_->path())).fileName();
        int n = slider_->maximum() + 1;
        lbl_metrics_->setText(
            QString("Frame %1/%2  |  Single Video B: %3  |  %4x%5 %6")
                .arg(idx).arg(n - 1)
                .arg(fname)
                .arg(parser_b_->width()).arg(parser_b_->height())
                .arg(combo_format_->currentText())
        );
    }
}

void MainWindow::update_metrics(const DiffResult& diff) {
    if (!frame_a_ || !frame_b_) return;

    PSNRResult psnr = metrics_calc_.psnr(*frame_a_, *frame_b_);
    SSIMResult ssim = metrics_calc_.ssim(*frame_a_, *frame_b_);

    int n = slider_->maximum() + 1;
    int idx = slider_->value();
    double pct = (diff.total_pixel_count > 0)
        ? (100.0 * diff.diff_pixel_count / diff.total_pixel_count)
        : 0.0;

    QString align_info = "";
    if (parser_a_ && parser_a_->bit_depth() == BitDepth::BIT10LE) {
        align_info = QString("  |  Align=%1").arg(QString::fromStdString(to_string(parser_a_->bit_alignment())));
    }

    std::ostringstream oss;
    oss << "Frame " << idx << "/" << (n - 1) << "  |  "
        << std::fixed << std::setprecision(2)
        << "PSNR Y=" << psnr.y << " U=" << psnr.u << " V=" << psnr.v << " T=" << psnr.total << "  |  "
        << std::setprecision(4) << "SSIM Y=" << ssim.y << "  |  "
        << std::setprecision(2) << "Diff: " << pct << "% (" << diff.diff_pixel_count << "/" << diff.total_pixel_count << ")"
        << align_info.toStdString();

    lbl_metrics_->setText(QString::fromStdString(oss.str()));
}

void MainWindow::on_export_current() {
    if (canvas_->pixmap().isNull()) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Save current frame", "frame.png", "PNG (*.png)"
    );
    if (path.isEmpty()) return;

    canvas_->pixmap().save(path, "PNG");
}

void MainWindow::on_export_all() {
    if (!parser_a_ && !parser_b_) return;

    QString out_dir = QFileDialog::getExistingDirectory(this, "Choose output directory");
    if (out_dir.isEmpty()) return;

    if (parser_a_ && parser_b_) {
        QString a_base = QFileInfo(QString::fromStdString(parser_a_->path())).baseName();
        QString b_base = QFileInfo(QString::fromStdString(parser_b_->path())).baseName();
        int n = static_cast<int>(std::min(parser_a_->num_frames(), parser_b_->num_frames()));

        for (int i = 0; i < n; ++i) {
            show_frame(i);
            QApplication::processEvents();

            QString fname = QString("%1_vs_%2_frame_%3.png")
                .arg(a_base)
                .arg(b_base)
                .arg(i, 5, 10, QChar('0'));
            QString full_path = QDir(out_dir).filePath(fname);
            canvas_->pixmap().save(full_path, "PNG");
        }
        statusBar()->showMessage(QString("Exported %1 frames to %2").arg(n).arg(out_dir), 5000);
    } else {
        auto& parser = parser_a_ ? parser_a_ : parser_b_;
        QString base = QFileInfo(QString::fromStdString(parser->path())).baseName();
        int n = static_cast<int>(parser->num_frames());

        for (int i = 0; i < n; ++i) {
            show_frame(i);
            QApplication::processEvents();

            QString fname = QString("%1_frame_%2.png")
                .arg(base)
                .arg(i, 5, 10, QChar('0'));
            QString full_path = QDir(out_dir).filePath(fname);
            canvas_->pixmap().save(full_path, "PNG");
        }
        statusBar()->showMessage(QString("Exported %1 frames to %2").arg(n).arg(out_dir), 5000);
    }
}

} // namespace yuvdiff
