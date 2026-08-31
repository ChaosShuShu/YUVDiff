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
    void on_format_a_changed(const QString& fmt);
    void on_format_b_changed(const QString& fmt);
    void on_slider_changed(int idx);
    void on_spin_frame_changed(int idx);
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
    void on_reset_zoom_clicked();

private:
    void build_ui();
    void build_shortcuts();
    void open_file(const QString& which);
    void reload_parser(const QString& which);
    void maybe_load_frame();
    void request_current_frame();
    void refresh_source_info();

    // State
    std::shared_ptr<YUVParser> parser_a_;
    std::shared_ptr<YUVParser> parser_b_;
    std::shared_ptr<Renderer> renderer_;
    std::unique_ptr<AsyncRenderWorker> worker_;

    QTimer* play_timer_ = nullptr;

    // Top Toolbar (Row 1)
    QPushButton* btn_open_a_ = nullptr;
    QPushButton* btn_open_b_ = nullptr;
    QPushButton* btn_export_current_ = nullptr;
    QPushButton* btn_export_all_ = nullptr;

    // Top Config Bar (Row 2)
    QComboBox* combo_mode_ = nullptr;
    QSpinBox* spin_threshold_ = nullptr;
    QSpinBox* spin_w_ = nullptr;
    QSpinBox* spin_h_ = nullptr;
    QComboBox* combo_format_a_ = nullptr;
    QComboBox* combo_format_b_ = nullptr;
    QLabel* lbl_align_ = nullptr;
    QComboBox* combo_align_ = nullptr;
    QSpinBox* spin_fps_ = nullptr;

    // Left Sidebar Cards
    QLabel* lbl_info_a_file_ = nullptr;
    QLabel* lbl_info_a_dim_ = nullptr;
    QLabel* lbl_info_a_frames_ = nullptr;

    QLabel* lbl_info_b_file_ = nullptr;
    QLabel* lbl_info_b_dim_ = nullptr;
    QLabel* lbl_info_b_frames_ = nullptr;

    QLabel* lbl_psnr_total_ = nullptr;
    QLabel* lbl_psnr_channels_ = nullptr;
    QLabel* lbl_ssim_ = nullptr;

    QLabel* lbl_diff_basic_ = nullptr;
    QLabel* lbl_diff_gt_2t_ = nullptr;
    QLabel* lbl_diff_gt_t_ = nullptr;
    QLabel* lbl_diff_gt_half_t_ = nullptr;

    QLabel* lbl_diff_mean_ = nullptr;
    QLabel* lbl_diff_median_ = nullptr;
    QLabel* lbl_diff_max_ = nullptr;
    QLabel* lbl_diff_min_ = nullptr;

    QLabel* lbl_insp_pos_ = nullptr;
    QLabel* lbl_insp_val_a_ = nullptr;
    QLabel* lbl_insp_val_b_ = nullptr;
    QLabel* lbl_insp_diff_ = nullptr;

    // Center Viewport
    YUVGLWidget* canvas_ = nullptr;

    // Bottom Transport & Timeline Deck
    QPushButton* btn_step_prev_ = nullptr;
    QPushButton* btn_play_ = nullptr;
    QPushButton* btn_step_next_ = nullptr;
    QSpinBox* spin_frame_ = nullptr;
    QLabel* lbl_total_frames_ = nullptr;
    QSlider* slider_ = nullptr;
    QLabel* lbl_progress_pct_ = nullptr;
    QPushButton* btn_reset_zoom_ = nullptr;

    // Status bar
    QLabel* lbl_metrics_ = nullptr;
};

} // namespace yuvdiff
