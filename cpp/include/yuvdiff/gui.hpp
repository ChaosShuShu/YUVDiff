#pragma once

#include "yuvdiff/diff.hpp"
#include "yuvdiff/formats.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/gl_widget.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/renderer.hpp"
#include "yuvdiff/worker.hpp"

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QStatusBar>

#include <memory>
#include <optional>

namespace yuvdiff {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_open_a();
    void on_open_b();
    void on_format_changed(const QString& fmt);
    void on_slider_changed(int idx);
    void on_mode_changed(int index);
    void on_threshold_changed(int val);
    void on_play_toggled(bool checked);
    void on_play_tick();
    void on_export_current();
    void on_export_all();
    void step_frame(int delta);

    // Worker slots
    void on_frame_ready(const yuvdiff::FrameReadyData& data);
    void on_render_error(int frame_idx, const QString& error_msg);

    // Pixel inspector slots
    void on_pixel_hovered(const yuvdiff::PixelInfo& info);
    void on_pixel_leave();

private:
    void build_ui();
    void build_shortcuts();
    void open_file(const QString& which);
    void maybe_load_frame();
    void request_current_frame();

    // State
    std::shared_ptr<YUVParser> parser_a_;
    std::shared_ptr<YUVParser> parser_b_;
    std::shared_ptr<Renderer> renderer_;
    std::unique_ptr<AsyncRenderWorker> worker_;

    QTimer* play_timer_ = nullptr;

    // UI Widgets
    QPushButton* btn_open_a_ = nullptr;
    QPushButton* btn_open_b_ = nullptr;
    QComboBox* combo_format_ = nullptr;
    QSpinBox* spin_w_ = nullptr;
    QSpinBox* spin_h_ = nullptr;
    QLabel* lbl_align_ = nullptr;
    QComboBox* combo_align_ = nullptr;

    QSpinBox* spin_threshold_ = nullptr;
    QComboBox* combo_mode_ = nullptr;
    QPushButton* btn_export_current_ = nullptr;
    QPushButton* btn_export_all_ = nullptr;
    QPushButton* btn_play_ = nullptr;
    QSpinBox* spin_fps_ = nullptr;

    QSlider* slider_ = nullptr;
    QSpinBox* spin_frame_ = nullptr;

    YUVGLWidget* canvas_ = nullptr;
    QLabel* lbl_pixel_info_ = nullptr;
    QLabel* lbl_metrics_ = nullptr;
};

} // namespace yuvdiff
