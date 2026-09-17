#include "yuvdiff/gui.hpp"
#include "yuvdiff/theme.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSplitter>
#include <QStyleOptionSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace yuvdiff {

void TimelineSlider::paintEvent(QPaintEvent* ev) {
    QSlider::paintEvent(ev);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect gr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect hr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    QPainter p(this);

    // 1. Draw precision vertical needle "|"
    int center_x = hr.center().x();
    int needle_top = gr.top() - 4;
    int needle_bottom = gr.bottom() + 4;

    // Dark outline for contrast
    p.setPen(QPen(QColor(0, 0, 0, 160), 2.5));
    p.drawLine(center_x, needle_top, center_x, needle_bottom);

    // High-contrast bright needle center
    bool is_hover = (opt.state & QStyle::State_MouseOver) || (opt.state & QStyle::State_Sunken);
    QColor needle_col = is_hover ? QColor(90, 200, 245) : QColor(255, 255, 255);
    p.setPen(QPen(needle_col, 1.2));
    p.drawLine(center_x, needle_top, center_x, needle_bottom);

    // 2. Draw 25-frame tick marks below
    if (tickPosition() == NoTicks || tickInterval() <= 0 || maximum() <= minimum()) {
        return;
    }

    int min_val = minimum();
    int max_val = maximum();
    int interval = tickInterval();

    int left = gr.left();
    int right = gr.right();
    int span = right - left;
    if (span <= 0) return;

    int tick_y = gr.bottom() + 2;
    int tick_h = 3;

    int step = interval;
    while ((max_val - min_val) / step > 120) {
        step *= 2;
    }

    QColor base_col = palette().color(QPalette::WindowText);
    QColor minor_col(base_col.red(), base_col.green(), base_col.blue(), 70);
    QColor major_col(base_col.red(), base_col.green(), base_col.blue(), 160);

    for (int v = min_val; v <= max_val; v += step) {
        double ratio = double(v - min_val) / (max_val - min_val);
        int x = left + int(ratio * span);
        if (v % 100 == 0) {
            p.setPen(major_col);
            p.drawLine(x, tick_y, x, tick_y + tick_h + 2);
        } else {
            p.setPen(minor_col);
            p.drawLine(x, tick_y, x, tick_y + tick_h);
        }
    }
}

static QHBoxLayout* make_kv_row(QWidget* parent, const QString& key, QLabel*& out_val_label) {
    QHBoxLayout* row = new QHBoxLayout();
    row->setContentsMargins(0, 1, 0, 1);
    row->setSpacing(6);
    QLabel* k = new QLabel(key, parent);
    k->setObjectName("SidebarKey");
    k->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    out_val_label = new QLabel("—", parent);
    out_val_label->setObjectName("SidebarVal");
    out_val_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    out_val_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    out_val_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(k);
    row->addWidget(out_val_label);
    row->addStretch();
    return row;
}

static QFrame* make_v_separator(QWidget* parent) {
    QFrame* line = new QFrame(parent);
    line->setProperty("separator", true);
    line->setFixedWidth(1);
    return line;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("YUVdiff Studio");
    resize(1360, 860);

    worker_ = std::make_unique<AsyncRenderWorker>(this);
    connect(worker_.get(), &AsyncRenderWorker::frameReady, this, &MainWindow::on_frame_ready, Qt::QueuedConnection);
    connect(worker_.get(), &AsyncRenderWorker::renderError, this, &MainWindow::on_render_error, Qt::QueuedConnection);

    play_timer_ = new QTimer(this);
    connect(play_timer_, &QTimer::timeout, this, &MainWindow::on_play_tick);

    build_ui();
    build_menu_bar();
    build_shortcuts();
}

MainWindow::~MainWindow() {
    if (play_timer_->isActive()) {
        play_timer_->stop();
    }
    if (worker_) {
        worker_->stop();
    }
}

void MainWindow::build_ui() {
    QWidget* central = new QWidget(this);
    central->setObjectName("CentralWidget");
    setCentralWidget(central);
    QVBoxLayout* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ==========================================
    // TOP: Primary Configuration Toolbar
    // ==========================================
    QFrame* frame_tb2 = new QFrame(this);
    frame_tb2->setObjectName("ConfigBarFrame");
    QHBoxLayout* tb2 = new QHBoxLayout(frame_tb2);
    tb2->setContentsMargins(10, 3, 10, 3);
    tb2->setSpacing(6);

    spin_threshold_ = new QSpinBox(this);
    spin_threshold_->setRange(0, 1023);
    spin_threshold_->setValue(4);
    spin_threshold_->setFixedWidth(64);
    connect(spin_threshold_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_threshold_changed);

    spin_w_ = new QSpinBox(this);
    spin_w_->setRange(1, 16384);
    spin_w_->setValue(1920);
    spin_w_->setFixedWidth(74);

    spin_h_ = new QSpinBox(this);
    spin_h_->setRange(1, 16384);
    spin_h_->setValue(1080);
    spin_h_->setFixedWidth(74);

    combo_format_a_ = new QComboBox(this);
    combo_format_a_->addItems({
        "yuv420p8", "yuv422p8", "yuv444p8",
        "yuv420p10le", "yuv422p10le", "yuv444p10le"
    });
    connect(combo_format_a_, &QComboBox::currentTextChanged, this, &MainWindow::on_format_a_changed);

    combo_format_b_ = new QComboBox(this);
    combo_format_b_->addItems({
        "yuv420p8", "yuv422p8", "yuv444p8",
        "yuv420p10le", "yuv422p10le", "yuv444p10le"
    });
    connect(combo_format_b_, &QComboBox::currentTextChanged, this, &MainWindow::on_format_b_changed);

    lbl_align_ = new QLabel("Align:", this);
    lbl_align_->setObjectName("SidebarKey");
    combo_align_ = new QComboBox(this);
    combo_align_->addItem("Auto", "auto");
    combo_align_->addItem("MSB (HEVC/AV1)", "msb");
    combo_align_->addItem("LSB (FFmpeg)", "lsb");

    spin_fps_ = new QSpinBox(this);
    spin_fps_->setRange(1, 120);
    spin_fps_->setValue(25);
    spin_fps_->setFixedWidth(56);

    tb2->addWidget(new QLabel("Threshold:", this));
    tb2->addWidget(spin_threshold_);
    tb2->addWidget(make_v_separator(this));
    tb2->addWidget(new QLabel("W:", this));
    tb2->addWidget(spin_w_);
    tb2->addWidget(new QLabel("H:", this));
    tb2->addWidget(spin_h_);
    tb2->addWidget(make_v_separator(this));
    tb2->addWidget(new QLabel("Fmt A:", this));
    tb2->addWidget(combo_format_a_);
    tb2->addWidget(new QLabel("Fmt B:", this));
    tb2->addWidget(combo_format_b_);
    tb2->addWidget(lbl_align_);
    tb2->addWidget(combo_align_);
    tb2->addWidget(make_v_separator(this));
    tb2->addWidget(new QLabel("FPS:", this));
    tb2->addWidget(spin_fps_);
    tb2->addStretch();
    root->addWidget(frame_tb2);

    // ==========================================
    // MIDDLE: Left Sidebar + Central Viewport (Splitter)
    // ==========================================
    QSplitter* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setObjectName("MainSplitter");
    splitter->setChildrenCollapsible(false);

    // Left Info & Stats Sidebar (Compact, Multi-line, Narrow)
    QScrollArea* scroll = new QScrollArea(splitter);
    scroll->setObjectName("SidebarScrollArea");
    scroll->setMinimumWidth(180);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget* sidebar_content = new QWidget(scroll);
    sidebar_content->setObjectName("SidebarContent");
    QVBoxLayout* sb_layout = new QVBoxLayout(sidebar_content);
    sb_layout->setContentsMargins(6, 4, 6, 6);
    sb_layout->setSpacing(5);

    // ==========================================
    // SECTION 1: Static Metadata (静态信息)
    // ==========================================
    QGroupBox* grp_static = new QGroupBox("SOURCE METADATA / 静态信息", sidebar_content);
    QVBoxLayout* lay_static = new QVBoxLayout(grp_static);
    lay_static->setContentsMargins(8, 6, 8, 6);
    lay_static->setSpacing(3);

    lay_static->addLayout(make_kv_row(grp_static, "Video A:", lbl_info_a_file_));
    lay_static->addLayout(make_kv_row(grp_static, "  Details:", lbl_info_a_dim_));
    lay_static->addLayout(make_kv_row(grp_static, "  Frames:", lbl_info_a_frames_));
    lay_static->addWidget(make_v_separator(grp_static));
    lay_static->addLayout(make_kv_row(grp_static, "Video B:", lbl_info_b_file_));
    lay_static->addLayout(make_kv_row(grp_static, "  Details:", lbl_info_b_dim_));
    lay_static->addLayout(make_kv_row(grp_static, "  Frames/Align:", lbl_info_b_frames_));
    sb_layout->addWidget(grp_static);

    // ==========================================
    // SECTION 2: Dynamic Analysis (动态分析)
    // ==========================================
    QGroupBox* grp_dyn = new QGroupBox("FRAME ANALYSIS / 动态分析", sidebar_content);
    QVBoxLayout* lay_dyn = new QVBoxLayout(grp_dyn);
    lay_dyn->setContentsMargins(8, 6, 8, 6);
    lay_dyn->setSpacing(3);

    lay_dyn->addLayout(make_kv_row(grp_dyn, "PSNR Total:", lbl_psnr_total_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "Planes (Y/U/V):", lbl_psnr_channels_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "SSIM (Y):", lbl_ssim_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "Diff(d>0):", lbl_diff_basic_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "  > 2t:", lbl_diff_gt_2t_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "  >  t:", lbl_diff_gt_t_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "  > t/2:", lbl_diff_gt_half_t_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "Mean / Median Δ:", lbl_diff_mean_));
    lay_dyn->addLayout(make_kv_row(grp_dyn, "Max / Min Δ:", lbl_diff_max_));
    sb_layout->addWidget(grp_dyn);

    // ==========================================
    // SECTION 3: Pixel Inspector (像素探测)
    // ==========================================
    QGroupBox* grp_insp = new QGroupBox("PIXEL INSPECTOR / 像素探测", sidebar_content);
    QVBoxLayout* lay_insp = new QVBoxLayout(grp_insp);
    lay_insp->setContentsMargins(8, 6, 8, 6);
    lay_insp->setSpacing(3);

    lay_insp->addLayout(make_kv_row(grp_insp, "Coord (x,y):", lbl_insp_pos_));
    lay_insp->addLayout(make_kv_row(grp_insp, "Pixel A (YUV):", lbl_insp_val_a_));
    lay_insp->addLayout(make_kv_row(grp_insp, "Pixel B (YUV):", lbl_insp_val_b_));
    lay_insp->addLayout(make_kv_row(grp_insp, "Diff Δ:", lbl_insp_diff_));
    sb_layout->addWidget(grp_insp);

    sb_layout->addStretch();
    scroll->setWidget(sidebar_content);

    // Center Canvas (OpenGL GPU Accelerated)
    canvas_ = new YUVGLWidget(splitter);
    connect(canvas_, &YUVGLWidget::pixelHovered, this, &MainWindow::on_pixel_hovered);
    connect(canvas_, &YUVGLWidget::pixelLeave, this, &MainWindow::on_pixel_leave);

    splitter->addWidget(scroll);
    splitter->addWidget(canvas_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 1150});

    root->addWidget(splitter, 1);

    // ==========================================
    // BOTTOM: Transport & Timeline Deck (Compact Industrial Docked)
    // ==========================================
    QFrame* frame_bottom = new QFrame(this);
    frame_bottom->setObjectName("BottomDeckFrame");
    QHBoxLayout* deck = new QHBoxLayout(frame_bottom);
    deck->setContentsMargins(6, 1, 6, 1);
    deck->setSpacing(5);

    btn_step_prev_ = new QToolButton(this);
    btn_step_prev_->setObjectName("BtnStepPrev");
    btn_step_prev_->setIconSize(QSize(13, 13));
    btn_step_prev_->setToolTip("Previous Frame (Left Arrow / [)");
    connect(btn_step_prev_, &QToolButton::clicked, this, [this]() { step_frame(-1); });

    btn_play_ = new QToolButton(this);
    btn_play_->setObjectName("BtnPlay");
    btn_play_->setCheckable(true);
    btn_play_->setIconSize(QSize(13, 13));
    btn_play_->setToolTip("Play / Pause (Space)");
    connect(btn_play_, &QToolButton::toggled, this, &MainWindow::on_play_toggled);

    btn_step_next_ = new QToolButton(this);
    btn_step_next_->setObjectName("BtnStepNext");
    btn_step_next_->setIconSize(QSize(13, 13));
    btn_step_next_->setToolTip("Next Frame (Right Arrow / ])");
    connect(btn_step_next_, &QToolButton::clicked, this, [this]() { step_frame(1); });

    QFrame* sep1 = make_v_separator(this);

    // Frame Counter Capsule Badge
    QFrame* frame_badge = new QFrame(this);
    frame_badge->setObjectName("FrameBadgeFrame");
    QHBoxLayout* badge_layout = new QHBoxLayout(frame_badge);
    badge_layout->setContentsMargins(4, 0, 4, 0);
    badge_layout->setSpacing(2);

    spin_frame_ = new QSpinBox(frame_badge);
    spin_frame_->setObjectName("SpinFrame");
    spin_frame_->setRange(0, 0);
    spin_frame_->setValue(0);
    spin_frame_->setFixedWidth(42);
    spin_frame_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    spin_frame_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin_frame_->setToolTip("Current Frame (Type number and press Enter to jump)");
    connect(spin_frame_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_spin_frame_changed);

    lbl_total_frames_ = new QLabel("/ 0", frame_badge);
    lbl_total_frames_->setObjectName("TotalFramesLabel");
    lbl_total_frames_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    badge_layout->addWidget(spin_frame_);
    badge_layout->addWidget(lbl_total_frames_);

    slider_ = new TimelineSlider(Qt::Horizontal, this);
    slider_->setObjectName("TimelineSlider");
    slider_->setRange(0, 0);
    slider_->setTickPosition(QSlider::TicksBelow);
    slider_->setTickInterval(25);
    connect(slider_, &QSlider::valueChanged, this, &MainWindow::on_slider_changed);

    lbl_progress_pct_ = new QLabel("0.0%", this);
    lbl_progress_pct_->setObjectName("ProgressPctLabel");
    lbl_progress_pct_->setFixedWidth(42);
    lbl_progress_pct_->setAlignment(Qt::AlignCenter);

    QFrame* sep2 = make_v_separator(this);

    btn_reset_zoom_ = new QToolButton(this);
    btn_reset_zoom_->setObjectName("BtnResetZoom");
    btn_reset_zoom_->setText("1:1");
    btn_reset_zoom_->setToolTip("Reset Zoom & Pan to 100% (R / Double-click)");
    connect(btn_reset_zoom_, &QToolButton::clicked, this, &MainWindow::on_reset_zoom_clicked);

    deck->addWidget(btn_step_prev_);
    deck->addWidget(btn_play_);
    deck->addWidget(btn_step_next_);
    deck->addWidget(sep1);
    deck->addWidget(frame_badge);
    deck->addWidget(slider_, 1);
    deck->addWidget(lbl_progress_pct_);
    deck->addWidget(sep2);
    deck->addWidget(btn_reset_zoom_);
    root->addWidget(frame_bottom);

    update_transport_icons();

    // ==========================================
    // STATUS BAR
    // ==========================================
    lbl_metrics_ = new QLabel("Ready", this);
    statusBar()->addWidget(lbl_metrics_);

    on_format_a_changed(combo_format_a_->currentText());
}

void MainWindow::build_menu_bar() {
    QMenuBar* mb = menuBar();
    mb->setNativeMenuBar(false);

    // ==========================================
    // File Menu (文件)
    // ==========================================
    QMenu* menu_file = mb->addMenu("文件 (&F)");

    act_open_a_ = menu_file->addAction("打开视频 A (&Open A)...", QKeySequence::Open, this, &MainWindow::on_open_a);
    act_open_a_->setStatusTip("打开基准视频 A (YUV 序列)");

    act_open_b_ = menu_file->addAction("打开视频 B (&Open B)...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this, &MainWindow::on_open_b);
    act_open_b_->setStatusTip("打开对比视频 B (YUV 序列)");

    menu_file->addSeparator();

    act_export_current_ = menu_file->addAction("保存当前帧 (&Save Frame)...", QKeySequence::Save, this, &MainWindow::on_export_current);
    act_export_current_->setStatusTip("将当前视口渲染帧保存为图片 (PNG)");

    act_export_all_ = menu_file->addAction("保存/导出序列 (&Export Sequence)...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, &MainWindow::on_export_all);
    act_export_all_->setStatusTip("批量导出整个比对序列帧");

    menu_file->addSeparator();

    QAction* act_exit = menu_file->addAction("退出 (&Exit)", QKeySequence::Quit, this, &MainWindow::close);
    act_exit->setStatusTip("退出 YUVdiff Studio");

    // ==========================================
    // View Menu (视图)
    // ==========================================
    QMenu* menu_view = mb->addMenu("视图 (&V)");
    mode_group_ = new QActionGroup(this);
    mode_group_->setExclusive(true);

    struct ModeConfig {
        RenderMode mode;
        QString text;
        QString shortcut_str;
        QString tip;
    };

    const std::vector<ModeConfig> mode_list = {
        {RenderMode::ORIGINAL_A, "原始画面 A (ORIGINAL_A)", "1", "显示基准视频 A 原始画面"},
        {RenderMode::ORIGINAL_B, "原始画面 B (ORIGINAL_B)", "2", "显示对比视频 B 原始画面"},
        {RenderMode::HEATMAP, "热力图 (HEATMAP)", "3", "显示像素差异热力分布"},
        {RenderMode::THRESHOLD_MASK, "阈值掩膜 (THRESHOLD_MASK)", "4", "显示差异超过阈值的二值化掩膜"},
        {RenderMode::SIDE_BY_SIDE, "并排对比 (SIDE_BY_SIDE)", "5", "左右并排同步比对画面"},
        {RenderMode::COMPARISON, "差分对比 (COMPARISON)", "6", "混合差分视觉对比模式"}
    };

    for (const auto& item : mode_list) {
        QAction* act = menu_view->addAction(item.text);
        act->setCheckable(true);
        act->setShortcut(QKeySequence(item.shortcut_str));
        act->setStatusTip(item.tip);
        mode_group_->addAction(act);
        mode_actions_[item.mode] = act;

        if (item.mode == current_mode_) {
            act->setChecked(true);
        }

        RenderMode m = item.mode;
        connect(act, &QAction::triggered, this, [this, m]() {
            set_render_mode(m);
        });
    }

    menu_view->addSeparator();

    QAction* act_reset_zoom = menu_view->addAction("重置缩放与平移 (1:1) (&R)", QKeySequence(Qt::Key_R), this, &MainWindow::on_reset_zoom_clicked);
    act_reset_zoom->setStatusTip("重置视口缩放为 1:1 像素居中显示");

    // ==========================================
    // Playback Menu (播放)
    // ==========================================
    QMenu* menu_play = mb->addMenu("播放 (&P)");
    QAction* act_play = menu_play->addAction("播放 / 暂停", QKeySequence(Qt::Key_Space), this, [this]() { btn_play_->toggle(); });
    act_play->setStatusTip("空格键切换播放/暂停");

    menu_play->addSeparator();
    menu_play->addAction("上一帧", QKeySequence(Qt::Key_Left), this, [this]() { step_frame(-1); });
    menu_play->addAction("下一帧", QKeySequence(Qt::Key_Right), this, [this]() { step_frame(1); });
    menu_play->addAction("后退 10 帧", QKeySequence(Qt::SHIFT | Qt::Key_Left), this, [this]() { step_frame(-10); });
    menu_play->addAction("前进 10 帧", QKeySequence(Qt::SHIFT | Qt::Key_Right), this, [this]() { step_frame(10); });

    menu_play->addSeparator();
    menu_play->addAction("跳转至首帧", QKeySequence(Qt::Key_Home), this, [this]() { if (slider_) slider_->setValue(0); });
    menu_play->addAction("跳转至尾帧", QKeySequence(Qt::Key_End), this, [this]() { if (slider_) slider_->setValue(slider_->maximum()); });

    // ==========================================
    // Help Menu (帮助)
    // ==========================================
    QMenu* menu_help = mb->addMenu("帮助 (&H)");
    menu_help->addAction("快捷键说明...", QKeySequence::HelpContents, this, [this]() {
        QMessageBox::information(this, "快捷键说明",
            "<b>模式切换:</b> 1 ~ 6<br>"
            "<b>播放/暂停:</b> Space<br>"
            "<b>单帧步进:</b> Left / Right (或 [ / ])<br>"
            "<b>十帧快进:</b> Shift + Left / Right<br>"
            "<b>首尾跳转:</b> Home / End<br>"
            "<b>缩放还原:</b> R 或双击视口<br>"
            "<b>打开文件:</b> Ctrl+O (A), Ctrl+Shift+O (B)<br>"
            "<b>保存导出:</b> Ctrl+S (单帧), Ctrl+Shift+S (序列)");
    });

    menu_help->addAction("关于 YUVdiff Studio...", this, [this]() {
        QMessageBox::about(this, "关于 YUVdiff Studio",
            "<b>YUVdiff Studio v0.1.0</b><br>"
            "专业级 Raw YUV 视频差异对比与分析工具。<br>"
            "支持多格式、色彩空间解析与硬件加速差分渲染。<br>"
            "Shut Out to TQ , KF");
    });
}

void MainWindow::build_shortcuts() {
    // Bracket shortcuts [ and ]
    QAction* act_bracket_left = new QAction(this);
    act_bracket_left->setShortcut(QKeySequence(Qt::Key_BracketLeft));
    connect(act_bracket_left, &QAction::triggered, this, [this]() { step_frame(-1); });
    addAction(act_bracket_left);

    QAction* act_bracket_right = new QAction(this);
    act_bracket_right->setShortcut(QKeySequence(Qt::Key_BracketRight));
    connect(act_bracket_right, &QAction::triggered, this, [this]() { step_frame(1); });
    addAction(act_bracket_right);
}

void MainWindow::on_reset_zoom_clicked() {
    if (canvas_) {
        canvas_->reset_zoom_pan();
    }
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

    // 1. Stop playback if active
    if (play_timer_->isActive()) {
        play_timer_->stop();
        btn_play_->setChecked(false);
    }

    std::string std_path = path.toStdString();

    // 2. Try auto-detecting resolution from filename
    auto auto_res = try_parse_resolution_from_filename(std_path);
    if (auto_res.has_value()) {
        QSignalBlocker b1(spin_w_);
        QSignalBlocker b2(spin_h_);
        spin_w_->setValue(auto_res->first);
        spin_h_->setValue(auto_res->second);
    }

    // 3. Try auto-detecting format from filename
    auto auto_fmt = try_parse_format_from_filename(std_path);
    auto combo = (which == "a") ? combo_format_a_ : combo_format_b_;
    if (auto_fmt.has_value()) {
        std::string fmt_str = to_string(auto_fmt->first) + to_string(auto_fmt->second);
        int idx = combo->findText(QString::fromStdString(fmt_str), Qt::MatchFixedString);
        if (idx < 0) {
            idx = combo->findText(QString::fromStdString(fmt_str), Qt::MatchContains);
        }
        if (idx >= 0) {
            QSignalBlocker b(combo);
            combo->setCurrentIndex(idx);
        }
    }

    try {
        std::string fmt = combo->currentText().toStdString();
        int w = spin_w_->value();
        int h = spin_h_->value();
        std::string align = combo_align_->currentData().toString().toStdString();

        auto parser = std::make_shared<YUVParser>(std_path, fmt, w, h, align);
        if (which == "a") {
            if (parser_b_ && (parser->width() != parser_b_->width() || parser->height() != parser_b_->height())) {
                parser_b_.reset();
            }
            parser_a_ = parser;
        } else {
            if (parser_a_ && (parser->width() != parser_a_->width() || parser->height() != parser_a_->height())) {
                parser_a_.reset();
            }
            parser_b_ = parser;
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Open failed", e.what());
        return;
    }

    // Reset slider to 0
    {
        QSignalBlocker b_sl(slider_);
        slider_->setValue(0);
    }

    refresh_source_info();
    maybe_load_frame();
}

void MainWindow::refresh_source_info() {
    if (parser_a_) {
        QString fname = QFileInfo(QString::fromStdString(parser_a_->path())).fileName();
        lbl_info_a_file_->setText(fname);
        lbl_info_a_file_->setToolTip(QString::fromStdString(parser_a_->path()));
        lbl_info_a_dim_->setText(QString("%1 × %2 (%3)")
            .arg(parser_a_->width()).arg(parser_a_->height())
            .arg(combo_format_a_->currentText()));
        lbl_info_a_frames_->setText(QString("%1 frames").arg(parser_a_->num_frames()));
    } else {
        lbl_info_a_file_->setText("Not loaded");
        lbl_info_a_file_->setToolTip("");
        lbl_info_a_dim_->setText("—");
        lbl_info_a_frames_->setText("—");
    }

    if (parser_b_) {
        QString fname = QFileInfo(QString::fromStdString(parser_b_->path())).fileName();
        lbl_info_b_file_->setText(fname);
        lbl_info_b_file_->setToolTip(QString::fromStdString(parser_b_->path()));
        lbl_info_b_dim_->setText(QString("%1 × %2 (%3)")
            .arg(parser_b_->width()).arg(parser_b_->height())
            .arg(combo_format_b_->currentText()));
        QString align_str = (parser_b_->bit_depth() == BitDepth::BIT10LE)
            ? QString(" (%1)").arg(QString::fromStdString(to_string(parser_b_->bit_alignment())))
            : "";
        lbl_info_b_frames_->setText(QString("%1 frames%2").arg(parser_b_->num_frames()).arg(align_str));
    } else {
        lbl_info_b_file_->setText("Not loaded");
        lbl_info_b_file_->setToolTip("");
        lbl_info_b_dim_->setText("—");
        lbl_info_b_frames_->setText("—");
    }
}

void MainWindow::reload_parser(const QString& which) {
    if (which == "a" && parser_a_) {
        try {
            std::string path = parser_a_->path();
            std::string fmt = combo_format_a_->currentText().toStdString();
            int w = spin_w_->value();
            int h = spin_h_->value();
            std::string align = combo_align_->currentData().toString().toStdString();
            parser_a_ = std::make_shared<YUVParser>(path, fmt, w, h, align);
            refresh_source_info();
            maybe_load_frame();
        } catch (...) {}
    } else if (which == "b" && parser_b_) {
        try {
            std::string path = parser_b_->path();
            std::string fmt = combo_format_b_->currentText().toStdString();
            int w = spin_w_->value();
            int h = spin_h_->value();
            std::string align = combo_align_->currentData().toString().toStdString();
            parser_b_ = std::make_shared<YUVParser>(path, fmt, w, h, align);
            refresh_source_info();
            maybe_load_frame();
        } catch (...) {}
    }
}

void MainWindow::on_format_a_changed(const QString& fmt) {
    bool has_10bit = combo_format_a_->currentText().contains("10le", Qt::CaseInsensitive) ||
                     combo_format_b_->currentText().contains("10le", Qt::CaseInsensitive);
    lbl_align_->setVisible(has_10bit);
    combo_align_->setVisible(has_10bit);
    reload_parser("a");
}

void MainWindow::on_format_b_changed(const QString& fmt) {
    bool has_10bit = combo_format_a_->currentText().contains("10le", Qt::CaseInsensitive) ||
                     combo_format_b_->currentText().contains("10le", Qt::CaseInsensitive);
    lbl_align_->setVisible(has_10bit);
    combo_align_->setVisible(has_10bit);
    reload_parser("b");
}

void MainWindow::maybe_load_frame() {
    if (!parser_a_ && !parser_b_) {
        if (canvas_) canvas_->set_frames(nullptr, nullptr, RenderMode::ORIGINAL_A, 4);
        if (worker_) worker_->set_parsers(nullptr, nullptr, nullptr);
        if (spin_frame_) spin_frame_->setRange(0, 0);
        if (lbl_total_frames_) lbl_total_frames_->setText("/ 0");
        if (lbl_progress_pct_) lbl_progress_pct_->setText("0.0%");
        return;
    }

    int n = 0;
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

        n = static_cast<int>(std::min(parser_a_->num_frames(), parser_b_->num_frames()));
        renderer_ = std::make_shared<Renderer>(parser_a_->width(), parser_a_->height());
    } else if (parser_a_) {
        n = static_cast<int>(parser_a_->num_frames());
        set_render_mode(RenderMode::ORIGINAL_A);
        renderer_ = std::make_shared<Renderer>(parser_a_->width(), parser_a_->height());
    } else if (parser_b_) {
        n = static_cast<int>(parser_b_->num_frames());
        set_render_mode(RenderMode::ORIGINAL_B);
        renderer_ = std::make_shared<Renderer>(parser_b_->width(), parser_b_->height());
    }

    int max_idx = std::max(0, n - 1);
    slider_->setRange(0, max_idx);
    spin_frame_->setRange(0, max_idx);
    lbl_total_frames_->setText(QString("/ %1").arg(max_idx));

    worker_->set_parsers(parser_a_, parser_b_, renderer_);
    request_current_frame();
}

void MainWindow::on_slider_changed(int idx) {
    if (spin_frame_ && spin_frame_->value() != idx) {
        QSignalBlocker b(spin_frame_);
        spin_frame_->setValue(idx);
    }
    request_current_frame();
}

void MainWindow::on_spin_frame_changed(int idx) {
    if (slider_ && slider_->value() != idx) {
        slider_->setValue(idx);
    }
}

void MainWindow::set_render_mode(RenderMode mode) {
    current_mode_ = mode;
    auto it = mode_actions_.find(mode);
    if (it != mode_actions_.end() && it->second) {
        it->second->setChecked(true);
    }
    request_current_frame();
}

void MainWindow::on_threshold_changed(int val) {
    (void)val;
    if (worker_) {
        worker_->clear_cache();
    }
    request_current_frame();
}

void MainWindow::request_current_frame() {
    if (!renderer_ || (!parser_a_ && !parser_b_)) return;

    int idx = slider_->value();
    RenderMode mode = current_mode_;
    int threshold = spin_threshold_->value();
    bool is_playing = btn_play_->isChecked();

    worker_->request_frame(idx, mode, threshold, is_playing);
}

void MainWindow::on_play_toggled(bool checked) {
    if (checked) {
        int fps = spin_fps_->value();
        play_timer_->start(1000 / fps);
    } else {
        play_timer_->stop();
    }
    update_transport_icons();
}

void MainWindow::update_transport_icons() {
    if (!btn_step_prev_ || !btn_play_ || !btn_step_next_) return;
    bool is_dark = yuvdiff::is_dark_theme();
    QString suffix = is_dark ? "dark" : "light";
    btn_step_prev_->setIcon(QIcon(QString(":/icons/media_prev_%1.png").arg(suffix)));
    btn_step_next_->setIcon(QIcon(QString(":/icons/media_next_%1.png").arg(suffix)));
    if (btn_play_->isChecked()) {
        btn_play_->setIcon(QIcon(QString(":/icons/media_pause_%1.png").arg(suffix)));
        btn_play_->setToolTip("Pause (Space)");
    } else {
        btn_play_->setIcon(QIcon(QString(":/icons/media_play_%1.png").arg(suffix)));
        btn_play_->setToolTip("Play (Space)");
    }
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        update_transport_icons();
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

void MainWindow::on_frame_ready(const yuvdiff::FrameReadyData& data) {
    if (!data.frame_a && !data.frame_b) return;

    canvas_->set_frames(data.frame_a, data.frame_b, data.mode, data.threshold);

    int total_f = slider_->maximum() + 1;
    if (spin_frame_ && spin_frame_->value() != data.frame_idx) {
        QSignalBlocker b(spin_frame_);
        spin_frame_->setValue(data.frame_idx);
    }
    if (lbl_total_frames_) {
        lbl_total_frames_->setText(QString("/ %1").arg(std::max(0, total_f - 1)));
    }

    double progress = (total_f > 1) ? (100.0 * data.frame_idx / (total_f - 1)) : 0.0;
    lbl_progress_pct_->setText(QString("%1%").arg(progress, 4, 'f', 1));

    if (data.is_dual) {
        double pct = (data.total_pixels > 0)
            ? (100.0 * data.diff_pixels / data.total_pixels)
            : 0.0;

        // 1. Update Quality Metrics Card
        QString psnr_color = (data.psnr.total >= 40.0) ? "#22c55e" : (data.psnr.total >= 30.0 ? "#f59e0b" : "#ef4444");
        lbl_psnr_total_->setText(QString("<span style='color:%1; font-weight:bold;'>%2 dB</span>")
            .arg(psnr_color).arg(data.psnr.total, 0, 'f', 2));
        lbl_psnr_channels_->setText(QString("Y:%1 U:%2 V:%3")
            .arg(data.psnr.y, 0, 'f', 2)
            .arg(data.psnr.u, 0, 'f', 2)
            .arg(data.psnr.v, 0, 'f', 2));
        lbl_ssim_->setText(QString::number(data.ssim.y, 'f', 4));

        // 2. Update Diff Distribution Card
        lbl_diff_basic_->setText(QString("%1% (%2 px)").arg(pct, 0, 'f', 2).arg(data.diff_pixels));
        lbl_diff_gt_2t_->setText(QString("%1% (%2 px)")
            .arg(data.diff_gt_2t_ratio * 100.0, 0, 'f', 2).arg(data.diff_gt_2t));
        lbl_diff_gt_t_->setText(QString("%1% (%2 px)")
            .arg(data.diff_gt_t_ratio * 100.0, 0, 'f', 2).arg(data.diff_gt_t));
        lbl_diff_gt_half_t_->setText(QString("%1% (%2 px)")
            .arg(data.diff_gt_half_t_ratio * 100.0, 0, 'f', 2).arg(data.diff_gt_half_t));

        // 3. Update Non-Zero Statistics Card
        lbl_diff_mean_->setText(QString("%1 / %2")
            .arg(data.diff_mean, 0, 'f', 2)
            .arg(data.diff_median, 0, 'f', 2));
        lbl_diff_max_->setText(QString("%1 / %2")
            .arg(data.diff_max)
            .arg(data.diff_min));

        // 4. Update Status Bar
        std::ostringstream oss;
        oss << "Frame " << data.frame_idx << "/" << (total_f - 1) << "  |  "
            << std::fixed << std::setprecision(2)
            << "PSNR Total=" << data.psnr.total << " dB  |  SSIM=" << std::setprecision(4) << data.ssim.y << "  |  "
            << "Diff(d>0): " << std::setprecision(2) << pct << "% (" << data.diff_pixels << "/" << data.total_pixels << ") "
            << "[>2t:" << data.diff_gt_2t << ", >t:" << data.diff_gt_t << ", >t/2:" << data.diff_gt_half_t << "]";
        lbl_metrics_->setText(QString::fromStdString(oss.str()));
    } else {
        auto& parser = (data.single_channel == "a") ? parser_a_ : parser_b_;
        QString label_ch = (data.single_channel == "a") ? "A" : "B";
        QString fname = parser ? QFileInfo(QString::fromStdString(parser->path())).fileName() : "";

        lbl_psnr_total_->setText("<span style='color:#64748b;'>Single Video (No Diff)</span>");
        lbl_psnr_channels_->setText("—");
        lbl_ssim_->setText("—");
        lbl_diff_basic_->setText("—");
        lbl_diff_gt_2t_->setText("—");
        lbl_diff_gt_t_->setText("—");
        lbl_diff_gt_half_t_->setText("—");
        lbl_diff_mean_->setText("—");
        lbl_diff_max_->setText("—");

        lbl_metrics_->setText(QString("Frame %1/%2  |  Single Video %3: %4")
            .arg(data.frame_idx).arg(total_f - 1).arg(label_ch).arg(fname));
    }
}

void MainWindow::on_render_error(int frame_idx, const QString& error_msg) {
    statusBar()->showMessage(QString("Frame %1 render error: %2").arg(frame_idx).arg(error_msg), 4000);
}

void MainWindow::on_pixel_hovered(const yuvdiff::PixelInfo& info) {
    if (info.x < 0 || info.y < 0) {
        on_pixel_leave();
        return;
    }

    lbl_insp_pos_->setText(QString("(%1, %2)").arg(info.x).arg(info.y));

    if (info.has_a) {
        lbl_insp_val_a_->setText(QString("Y:%1 U:%2 V:%3").arg(info.y_a).arg(info.u_a).arg(info.v_a));
    } else {
        lbl_insp_val_a_->setText("—");
    }

    if (info.has_b) {
        lbl_insp_val_b_->setText(QString("Y:%1 U:%2 V:%3").arg(info.y_b).arg(info.u_b).arg(info.v_b));
    } else {
        lbl_insp_val_b_->setText("—");
    }

    if (info.has_a && info.has_b) {
        lbl_insp_diff_->setText(QString("Y:%1 U:%2 V:%3 (max=%4)")
            .arg(info.diff_y).arg(info.diff_u).arg(info.diff_v).arg(info.diff_max));
    } else {
        lbl_insp_diff_->setText("—");
    }
}

void MainWindow::on_pixel_leave() {
    lbl_insp_pos_->setText("(—, —)");
    lbl_insp_val_a_->setText("—");
    lbl_insp_val_b_->setText("—");
    lbl_insp_diff_->setText("—");
}

void MainWindow::on_export_current() {
    QImage img = canvas_->grabFramebuffer();
    if (img.isNull()) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Save current frame", "frame.png", "PNG (*.png)"
    );
    if (path.isEmpty()) return;

    img.save(path, "PNG");
}

void MainWindow::on_export_all() {
    if (!parser_a_ && !parser_b_) return;

    QString out_dir = QFileDialog::getExistingDirectory(this, "Choose output directory");
    if (out_dir.isEmpty()) return;

    DiffEngine diff_engine(spin_threshold_->value());
    RenderMode mode = current_mode_;
    int threshold = spin_threshold_->value();

    if (parser_a_ && parser_b_) {
        QString a_base = QFileInfo(QString::fromStdString(parser_a_->path())).baseName();
        QString b_base = QFileInfo(QString::fromStdString(parser_b_->path())).baseName();
        int n = static_cast<int>(std::min(parser_a_->num_frames(), parser_b_->num_frames()));

        for (int i = 0; i < n; ++i) {
            auto fa = parser_a_->read_frame(i);
            auto fb = parser_b_->read_frame(i);
            auto diff = diff_engine.diff(fa, fb);
            auto rgb = renderer_->render(fa, &fb, &diff, mode, threshold);

            QImage qimg(rgb.data.data(), rgb.width, rgb.height, rgb.width * 3, QImage::Format_RGB888);
            QString fname = QString("%1_vs_%2_frame_%3.png")
                .arg(a_base)
                .arg(b_base)
                .arg(i, 5, 10, QChar('0'));
            qimg.save(QDir(out_dir).filePath(fname), "PNG");
            QApplication::processEvents();
        }
        statusBar()->showMessage(QString("Exported %1 frames to %2").arg(n).arg(out_dir), 5000);
    } else {
        auto& parser = parser_a_ ? parser_a_ : parser_b_;
        QString base = QFileInfo(QString::fromStdString(parser_a_->path())).baseName();
        int n = static_cast<int>(parser->num_frames());

        for (int i = 0; i < n; ++i) {
            auto f = parser->read_frame(i);
            auto rgb = renderer_->yuv_to_rgb(f);
            QImage qimg(rgb.data.data(), rgb.width, rgb.height, rgb.width * 3, QImage::Format_RGB888);
            QString fname = QString("%1_frame_%2.png")
                .arg(base)
                .arg(i, 5, 10, QChar('0'));
            qimg.save(QDir(out_dir).filePath(fname), "PNG");
            QApplication::processEvents();
        }
        statusBar()->showMessage(QString("Exported %1 frames to %2").arg(n).arg(out_dir), 5000);
    }
}

void MainWindow::populate_mock_data() {
    lbl_info_a_file_->setText("sample_4k_source.yuv");
    lbl_info_a_dim_->setText("3840 × 2160 (420p8)");
    lbl_info_a_frames_->setText("300 frames");

    lbl_info_b_file_->setText("sample_4k_encoded.yuv");
    lbl_info_b_dim_->setText("3840 × 2160 (420p10le)");
    lbl_info_b_frames_->setText("300 frames (MSB)");

    lbl_psnr_total_->setText("42.35 dB");
    lbl_psnr_channels_->setText("Y:41.20 U:45.60 V:45.80");
    lbl_ssim_->setText("0.9876");

    lbl_diff_basic_->setText("5.12% (424,560 px)");
    lbl_diff_gt_2t_->setText("0.64% (53,070 px)");
    lbl_diff_gt_t_->setText("1.85% (153,320 px)");
    lbl_diff_gt_half_t_->setText("2.63% (218,170 px)");

    lbl_diff_mean_->setText("2.14 / 2.00");
    lbl_diff_max_->setText("14 / 1");

    lbl_insp_pos_->setText("(1920, 1080)");
    lbl_insp_val_a_->setText("Y:142 U:128 V:129");
    lbl_insp_val_b_->setText("Y:145 U:128 V:128");
    lbl_insp_diff_->setText("ΔY:3 ΔU:0 ΔV:1 (max:3)");

    spin_frame_->blockSignals(true);
    slider_->blockSignals(true);
    spin_frame_->setRange(0, 299);
    slider_->setRange(0, 299);
    spin_frame_->setValue(42);
    slider_->setValue(42);
    spin_frame_->blockSignals(false);
    slider_->blockSignals(false);

    lbl_total_frames_->setText("/ 299");
    lbl_progress_pct_->setText("14.0%");
    set_render_mode(RenderMode::COMPARISON);
    lbl_metrics_->setText("Preview Mock Mode | Frame 42 / 300 | Mode: COMPARISON | Threshold: 4");
}

} // namespace yuvdiff
