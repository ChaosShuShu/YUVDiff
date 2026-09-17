#include "yuvdiff/theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QStyleHints>
#include <QtGlobal>

namespace yuvdiff {

static ThemeMode s_current_mode = ThemeMode::Auto;
static bool s_is_dark = true;

void apply_visionos_dark_theme(QApplication& app) {
    // 1. Set global font (Apple SF Pro style / system fallback)
    QFont font = app.font();
    font.setFamily("-apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Segoe UI', 'PingFang SC', sans-serif");
    font.setPointSize(9);
    app.setFont(font);

    // 2. Configure visionOS dark QPalette based on Figma tokens (node 0-1746)
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(22, 22, 26));          // #16161A (WindowBackground)
    pal.setColor(QPalette::WindowText, QColor(245, 245, 247));  // Primary Label (#F5F5F7)
    pal.setColor(QPalette::Base, QColor(18, 18, 22));            // #121216 (CanvasBackground)
    pal.setColor(QPalette::AlternateBase, QColor(32, 32, 38));   // Card / Platter
    pal.setColor(QPalette::ToolTipBase, QColor(34, 34, 40, 242));
    pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    pal.setColor(QPalette::Text, QColor(255, 255, 255));
    pal.setColor(QPalette::Button, QColor(255, 255, 255, 20));   // FillTertiary (8%)
    pal.setColor(QPalette::ButtonText, QColor(245, 245, 247));
    pal.setColor(QPalette::BrightText, QColor(90, 200, 245));    // #5AC8F5 (System Cyan)
    pal.setColor(QPalette::Highlight, QColor(10, 132, 255));     // #0A84FF (System Blue Accent)
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(pal);

    // 3. visionOS Dark Glassmorphism QSS
    const char* qss = R"(
        /* ===================================================================
         * Apple visionOS Dark Theme for YUVdiff Studio
         * Design System Tokens: Figma node 0-1746 (Colors) & 487-15286 (Toolbars)
         * =================================================================== */

        /* --- Global Window & Central Widget --- */
        QMainWindow, QWidget#CentralWidget {
            background-color: #16161a;
            color: #f5f5f7;
            outline: none;
        }

        /* --- Menu Bar & Menus (visionOS Industrial Glass) --- */
        QMenuBar {
            background-color: rgba(22, 22, 26, 0.98);
            color: #f5f5f7;
            border-bottom: 1px solid rgba(255, 255, 255, 0.10);
            padding: 2px 6px;
            font-size: 8.5pt;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 10px;
            border-radius: 6px;
            color: #f5f5f7;
        }
        QMenuBar::item:selected {
            background-color: rgba(255, 255, 255, 0.12);
            color: #ffffff;
        }
        QMenuBar::item:pressed {
            background-color: rgba(255, 255, 255, 0.20);
        }

        QMenu {
            background-color: #1e1e24;
            color: #f5f5f7;
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 8px;
            padding: 4px;
            font-size: 8.5pt;
        }
        QMenu::item {
            padding: 4px 22px 4px 18px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: #0a84ff;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: rgba(255, 255, 255, 0.12);
            margin: 3px 4px;
        }

        /* --- Integrated Industrial Toolbars & Decks --- */
        QFrame#ConfigBarFrame {
            background-color: rgba(22, 22, 26, 0.98);
            border-bottom: 1px solid rgba(255, 255, 255, 0.10);
            border-top: none;
            border-left: none;
            border-right: none;
            border-radius: 0px;
            padding: 2px 8px;
            margin: 0px;
        }
        QFrame#BottomDeckFrame {
            background-color: rgba(22, 22, 26, 0.98);
            border-top: 1px solid rgba(255, 255, 255, 0.10);
            border-bottom: none;
            border-left: none;
            border-right: none;
            border-radius: 0px;
            padding: 2px 8px;
            margin: 0px;
            min-height: 24px;
            max-height: 24px;
        }

        /* Compact Tool Buttons */
        QToolButton {
            background-color: transparent;
            color: #f5f5f7;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 1px 4px;
            min-width: 20px;
            min-height: 20px;
            max-height: 20px;
            font-size: 8.5pt;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: rgba(255, 255, 255, 0.12);
            border: 1px solid rgba(255, 255, 255, 0.22);
            color: #ffffff;
        }
        QToolButton:pressed {
            background-color: rgba(255, 255, 255, 0.22);
            border: 1px solid rgba(255, 255, 255, 0.35);
        }
        QToolButton:checked {
            background-color: #0a84ff;
            color: #ffffff;
            border: 1px solid #0a84ff;
            font-weight: 600;
        }
        QToolButton#BtnStepPrev, QToolButton#BtnStepNext {
            min-width: 22px;
            max-width: 22px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
            padding: 0px;
        }
        QToolButton#BtnPlay {
            min-width: 24px;
            max-width: 24px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
            background-color: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255, 255, 255, 0.18);
            padding: 0px;
        }
        QToolButton#BtnPlay:hover {
            background-color: rgba(255, 255, 255, 0.18);
            border: 1px solid rgba(255, 255, 255, 0.32);
        }
        QToolButton#BtnPlay:checked {
            background-color: #0a84ff;
            border: 1px solid rgba(255, 255, 255, 0.40);
        }
        QToolButton#BtnPlay:checked:hover {
            background-color: #2b95ff;
        }
        QToolButton#BtnResetZoom {
            font-size: 7.5pt;
            font-weight: 600;
            color: #d1d1d6;
            min-width: 26px;
            max-width: 26px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
        }

        /* Frame Counter Compact Badge */
        QFrame#FrameBadgeFrame {
            background-color: rgba(255, 255, 255, 0.05);
            border: 1px solid rgba(255, 255, 255, 0.12);
            border-radius: 4px;
            padding: 0px 4px;
            min-height: 20px;
            max-height: 20px;
        }
        QFrame#FrameBadgeFrame:hover {
            background-color: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(10, 132, 255, 0.60);
        }
        QFrame#FrameBadgeFrame QSpinBox {
            background: transparent;
            border: none;
            color: #ffffff;
            font-weight: 600;
            font-size: 8pt;
            padding: 0;
            min-height: 18px;
            max-height: 18px;
        }
        QFrame#FrameBadgeFrame QSpinBox::up-button,
        QFrame#FrameBadgeFrame QSpinBox::down-button {
            width: 0px;
            height: 0px;
            border: none;
        }
        QFrame#FrameBadgeFrame QLabel#TotalFramesLabel {
            color: #98989d;
            font-size: 8pt;
            font-weight: 500;
            padding-left: 2px;
        }

        /* Progress Percentage Badge */
        QLabel#ProgressPctLabel {
            background-color: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 4px;
            color: #98989d;
            font-size: 7.5pt;
            font-family: -apple-system, "SF Pro Text", monospace;
            font-weight: 500;
            padding: 0px 4px;
            min-height: 18px;
            max-height: 18px;
        }

        /* Toolbar Separator */
        QToolBar::separator, QFrame[separator="true"] {
            width: 1px;
            height: 14px;
            background-color: rgba(255, 255, 255, 0.12);
            margin: 2px 4px;
            border: none;
        }

        /* --- Sidebar & Scroll Area --- */
        QScrollArea#SidebarScrollArea {
            background-color: #141418;
            border-right: 1px solid rgba(255, 255, 255, 0.12);
            border-top: none;
            border-bottom: none;
            border-left: none;
        }
        QWidget#SidebarContent {
            background-color: #141418;
        }

        /* --- Glass Cards / GroupBoxes --- */
        QGroupBox {
            background-color: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.12);
            border-radius: 12px;
            margin-top: 12px;
            padding-top: 14px;
            padding-bottom: 6px;
            padding-left: 8px;
            padding-right: 8px;
            font-size: 8.5pt;
            font-weight: 600;
            color: #0a84ff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            top: 2px;
            padding: 0 4px;
            background-color: #141418;
            color: #0a84ff;
            font-size: 8.5pt;
            font-weight: 600;
        }

        /* --- Push Buttons (visionOS Platter Pill) --- */
        QPushButton {
            background-color: rgba(255, 255, 255, 0.08);
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 12px;
            padding: 4px 12px;
            font-size: 8.5pt;
            font-weight: 500;
            min-height: 18px;
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 0.18);
            border: 1px solid rgba(255, 255, 255, 0.32);
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: rgba(255, 255, 255, 0.28);
            border: 1px solid rgba(255, 255, 255, 0.45);
        }
        QPushButton:checked {
            background-color: #ffffff;
            color: #141416;
            border: 1px solid #ffffff;
            font-weight: 600;
        }
        QPushButton:disabled {
            background-color: rgba(255, 255, 255, 0.03);
            color: rgba(255, 255, 255, 0.25);
            border: 1px solid rgba(255, 255, 255, 0.06);
        }

        /* Primary / Accent Buttons (Open A, Open B, etc.) */
        QPushButton#BtnOpenA, QPushButton#BtnOpenB, QPushButton[accent="true"] {
            background-color: #0a84ff;
            border: 1px solid rgba(255, 255, 255, 0.35);
            color: #ffffff;
            font-weight: 600;
            border-radius: 12px;
        }
        QPushButton#BtnOpenA:hover, QPushButton#BtnOpenB:hover, QPushButton[accent="true"]:hover {
            background-color: #2b95ff;
            border: 1px solid rgba(255, 255, 255, 0.55);
            color: #ffffff;
        }
        QPushButton#BtnOpenA:pressed, QPushButton#BtnOpenB:pressed, QPushButton[accent="true"]:pressed {
            background-color: #0071e3;
        }

        /* Play Button (visionOS Capsule Highlight) */
        QPushButton#BtnPlay {
            background-color: #0a84ff;
            border: 1px solid rgba(255, 255, 255, 0.35);
            color: #ffffff;
            font-weight: 600;
            padding: 4px 14px;
            border-radius: 12px;
        }
        QPushButton#BtnPlay:hover {
            background-color: #2b95ff;
            border: 1px solid rgba(255, 255, 255, 0.55);
        }
        QPushButton#BtnPlay:checked {
            background-color: #ff9f0a;
            border: 1px solid rgba(255, 255, 255, 0.40);
            color: #ffffff;
        }

        /* Capsule variant */
        QPushButton[capsule="true"] {
            border-radius: 15px;
            padding: 4px 16px;
        }

        /* Step Previous / Next Buttons */
        QPushButton#BtnStepPrev, QPushButton#BtnStepNext {
            padding: 0px;
            min-width: 32px;
            max-width: 32px;
            font-size: 10pt;
            font-weight: bold;
        }

        /* --- ComboBox (Recessed Glass Platter) --- */
        QComboBox {
            background-color: rgba(255, 255, 255, 0.08);
            color: #f5f5f7;
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 7px;
            padding: 1px 8px 1px 8px;
            font-size: 8.5pt;
            min-height: 20px;
            max-height: 20px;
        }
        QComboBox:hover {
            background-color: rgba(255, 255, 255, 0.16);
            border: 1px solid rgba(255, 255, 255, 0.30);
        }
        QComboBox:focus, QComboBox:on {
            border: 1px solid #0a84ff;
        }
        QComboBox::drop-down {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 18px;
            border-left: 1px solid rgba(255, 255, 255, 0.10);
            border-top-right-radius: 7px;
            border-bottom-right-radius: 7px;
            background-color: rgba(255, 255, 255, 0.04);
        }
        QComboBox::drop-down:hover {
            background-color: rgba(255, 255, 255, 0.14);
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron_down_dark.png);
            width: 8px;
            height: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #202026;
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.18);
            border-radius: 10px;
            padding: 4px;
            selection-background-color: #0a84ff;
            selection-color: #ffffff;
            outline: none;
        }

        /* --- SpinBox (Recessed Glass Field) --- */
        QSpinBox, QDoubleSpinBox {
            background-color: rgba(255, 255, 255, 0.06);
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.12);
            border-radius: 7px;
            padding: 1px 2px 1px 6px;
            font-size: 8.5pt;
            min-height: 20px;
            max-height: 20px;
        }
        QSpinBox:hover, QDoubleSpinBox:hover {
            border: 1px solid rgba(255, 255, 255, 0.25);
            background-color: rgba(255, 255, 255, 0.09);
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #0a84ff;
        }
        QSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 15px;
            border-left: 1px solid rgba(255, 255, 255, 0.10);
            border-bottom: 0.5px solid rgba(255, 255, 255, 0.08);
            border-top-right-radius: 6px;
            background-color: rgba(255, 255, 255, 0.04);
            margin: 0px;
        }
        QSpinBox::up-button:hover {
            background-color: rgba(255, 255, 255, 0.18);
        }
        QSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 15px;
            border-left: 1px solid rgba(255, 255, 255, 0.10);
            border-bottom-right-radius: 6px;
            background-color: rgba(255, 255, 255, 0.04);
            margin: 0px;
        }
        QSpinBox::down-button:hover {
            background-color: rgba(255, 255, 255, 0.18);
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url(:/icons/arrow_up_dark.png);
            width: 7px;
            height: 7px;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url(:/icons/arrow_down_dark.png);
            width: 7px;
            height: 7px;
        }

        /* --- Professional Timeline Slider (Needle Playhead) --- */
        QSlider {
            min-height: 18px;
            max-height: 18px;
            padding-bottom: 2px;
        }
        QSlider::groove:horizontal {
            height: 3px;
            background: rgba(255, 255, 255, 0.16);
            border-radius: 1.5px;
        }
        QSlider::sub-page:horizontal {
            background: #0a84ff;
            border-radius: 1.5px;
        }
        QSlider::handle:horizontal {
            background: transparent;
            border: none;
            width: 6px;
            margin: -5px 0;
        }

        /* --- ScrollBar (Minimal Floating Capsule) --- */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px 1px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 0.22);
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(255, 255, 255, 0.45);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0px;
        }

        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 1px 2px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(255, 255, 255, 0.22);
            min-width: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(255, 255, 255, 0.45);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            width: 0px;
        }

        /* --- Key / Value & Information Labels --- */
        QLabel#SidebarKey {
            color: #98989d;
            font-size: 9pt;
        }
        QLabel#SidebarVal {
            color: #f5f5f7;
            font-size: 9pt;
            font-family: monospace;
            font-weight: 500;
        }
        QLabel#ProgressPctLabel {
            color: #98989d;
            font-family: monospace;
            font-size: 9pt;
            min-width: 45px;
        }

        /* --- Splitter --- */
        QSplitter::handle:horizontal {
            background-color: rgba(255, 255, 255, 0.08);
            width: 2px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #0a84ff;
        }

        /* --- Tooltip (Thick Glass Platter) --- */
        QToolTip {
            background-color: rgba(34, 34, 40, 0.95);
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.22);
            border-radius: 8px;
            padding: 4px 10px;
            font-size: 9pt;
        }

        /* --- Status Bar --- */
        QStatusBar {
            background-color: #121216;
            color: #98989d;
            border-top: 1px solid rgba(255, 255, 255, 0.10);
            font-size: 9pt;
            min-height: 22px;
        }
    )";

    app.setStyleSheet(qss);
}

void apply_visionos_light_theme(QApplication& app) {
    // 1. Set global font
    QFont font = app.font();
    font.setFamily("-apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Segoe UI', 'PingFang SC', sans-serif");
    font.setPointSize(9);
    app.setFont(font);

    // 2. Configure visionOS light QPalette based on Figma tokens (node 0-1746 Light)
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(242, 242, 247));         // #F2F2F7 (WindowBackground)
    pal.setColor(QPalette::WindowText, QColor(28, 28, 30));         // Primary Label (#1C1C1E)
    pal.setColor(QPalette::Base, QColor(255, 255, 255));           // #FFFFFF (CanvasBackground)
    pal.setColor(QPalette::AlternateBase, QColor(255, 255, 255, 224)); // Card / Platter
    pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 255, 242));
    pal.setColor(QPalette::ToolTipText, QColor(28, 28, 30));
    pal.setColor(QPalette::Text, QColor(28, 28, 30));
    pal.setColor(QPalette::Button, QColor(0, 0, 0, 15));           // FillTertiary (6%)
    pal.setColor(QPalette::ButtonText, QColor(28, 28, 30));
    pal.setColor(QPalette::BrightText, QColor(0, 122, 255));       // #007AFF
    pal.setColor(QPalette::Highlight, QColor(0, 122, 255));        // #007AFF
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(pal);

    // 3. visionOS Light Glassmorphism QSS
    const char* qss = R"(
        /* ===================================================================
         * Apple visionOS Light Theme for YUVdiff Studio
         * Design System Tokens: Figma node 0-1746 (Colors) & 487-15286 (Toolbars)
         * =================================================================== */

        /* --- Global Window & Central Widget --- */
        QMainWindow, QWidget#CentralWidget {
            background-color: #f2f2f7;
            color: #1c1c1e;
            outline: none;
        }

        /* --- Menu Bar & Menus (visionOS Industrial Glass) --- */
        QMenuBar {
            background-color: rgba(242, 242, 247, 0.98);
            color: #1c1c1e;
            border-bottom: 1px solid rgba(0, 0, 0, 0.08);
            padding: 2px 6px;
            font-size: 8.5pt;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 10px;
            border-radius: 6px;
            color: #1c1c1e;
        }
        QMenuBar::item:selected {
            background-color: rgba(0, 0, 0, 0.08);
            color: #1c1c1e;
        }
        QMenuBar::item:pressed {
            background-color: rgba(0, 0, 0, 0.14);
        }

        QMenu {
            background-color: #ffffff;
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.12);
            border-radius: 8px;
            padding: 4px;
            font-size: 8.5pt;
        }
        QMenu::item {
            padding: 4px 22px 4px 18px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: #007aff;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: rgba(0, 0, 0, 0.08);
            margin: 3px 4px;
        }

        /* --- Integrated Industrial Toolbars & Decks (Light Mode) --- */
        QFrame#ConfigBarFrame {
            background-color: rgba(242, 242, 247, 0.98);
            border-bottom: 1px solid rgba(0, 0, 0, 0.10);
            border-top: none;
            border-left: none;
            border-right: none;
            border-radius: 0px;
            padding: 2px 8px;
            margin: 0px;
        }
        QFrame#BottomDeckFrame {
            background-color: rgba(242, 242, 247, 0.98);
            border-top: 1px solid rgba(0, 0, 0, 0.10);
            border-bottom: none;
            border-left: none;
            border-right: none;
            border-radius: 0px;
            padding: 2px 8px;
            margin: 0px;
            min-height: 24px;
            max-height: 24px;
        }

        /* Compact Tool Buttons */
        QToolButton {
            background-color: transparent;
            color: #1c1c1e;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 1px 4px;
            min-width: 20px;
            min-height: 20px;
            max-height: 20px;
            font-size: 8.5pt;
            font-weight: 500;
        }
        QToolButton:hover {
            background-color: rgba(0, 0, 0, 0.06);
            border: 1px solid rgba(0, 0, 0, 0.14);
        }
        QToolButton:pressed {
            background-color: rgba(0, 0, 0, 0.12);
        }
        QToolButton:checked {
            background-color: #007aff;
            color: #ffffff;
            border: 1px solid #007aff;
            font-weight: 600;
        }
        QToolButton#BtnStepPrev, QToolButton#BtnStepNext {
            min-width: 22px;
            max-width: 22px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
            padding: 0px;
        }
        QToolButton#BtnPlay {
            min-width: 24px;
            max-width: 24px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
            background-color: rgba(0, 0, 0, 0.05);
            border: 1px solid rgba(0, 0, 0, 0.14);
            padding: 0px;
        }
        QToolButton#BtnPlay:hover {
            background-color: rgba(0, 0, 0, 0.10);
            border: 1px solid rgba(0, 0, 0, 0.22);
        }
        QToolButton#BtnPlay:checked {
            background-color: #007aff;
            border: 1px solid #007aff;
            color: #ffffff;
        }
        QToolButton#BtnPlay:checked:hover {
            background-color: #0066d6;
        }
        QToolButton#BtnResetZoom {
            font-size: 7.5pt;
            font-weight: 600;
            color: #48484a;
            min-width: 26px;
            max-width: 26px;
            min-height: 20px;
            max-height: 20px;
            border-radius: 4px;
        }

        /* Frame Counter Compact Badge (Light Mode) */
        QFrame#FrameBadgeFrame {
            background-color: rgba(0, 0, 0, 0.04);
            border: 1px solid rgba(0, 0, 0, 0.10);
            border-radius: 4px;
            padding: 0px 4px;
            min-height: 20px;
            max-height: 20px;
        }
        QFrame#FrameBadgeFrame:hover {
            background-color: rgba(0, 0, 0, 0.07);
            border: 1px solid rgba(0, 122, 255, 0.60);
        }
        QFrame#FrameBadgeFrame QSpinBox {
            background: transparent;
            border: none;
            color: #1c1c1e;
            font-weight: 600;
            font-size: 8pt;
            padding: 0;
            min-height: 18px;
            max-height: 18px;
        }
        QFrame#FrameBadgeFrame QSpinBox::up-button,
        QFrame#FrameBadgeFrame QSpinBox::down-button {
            width: 0px;
            height: 0px;
            border: none;
        }
        QFrame#FrameBadgeFrame QLabel#TotalFramesLabel {
            color: #636366;
            font-size: 8pt;
            font-weight: 500;
            padding-left: 2px;
        }

        /* Progress Percentage Badge (Light Mode) */
        QLabel#ProgressPctLabel {
            background-color: rgba(0, 0, 0, 0.04);
            border: 1px solid rgba(0, 0, 0, 0.08);
            border-radius: 4px;
            color: #636366;
            font-size: 7.5pt;
            font-family: -apple-system, "SF Pro Text", monospace;
            font-weight: 500;
            padding: 0px 4px;
            min-height: 18px;
            max-height: 18px;
        }

        /* Toolbar Separator */
        QToolBar::separator, QFrame[separator="true"] {
            width: 1px;
            height: 14px;
            background-color: rgba(0, 0, 0, 0.10);
            margin: 2px 4px;
            border: none;
        }

        /* --- Sidebar & Scroll Area --- */
        QScrollArea#SidebarScrollArea {
            background-color: #e5e5ea;
            border-right: 1px solid rgba(0, 0, 0, 0.10);
            border-top: none;
            border-bottom: none;
            border-left: none;
        }
        QWidget#SidebarContent {
            background-color: #e5e5ea;
        }

        /* --- Glass Cards / GroupBoxes --- */
        QGroupBox {
            background-color: rgba(255, 255, 255, 0.75);
            border: 1px solid rgba(0, 0, 0, 0.08);
            border-radius: 12px;
            margin-top: 12px;
            padding-top: 14px;
            padding-bottom: 6px;
            padding-left: 8px;
            padding-right: 8px;
            font-size: 8.5pt;
            font-weight: 600;
            color: #007aff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            top: 2px;
            padding: 0 4px;
            background-color: #e5e5ea;
            color: #007aff;
            font-size: 8.5pt;
            font-weight: 600;
        }

        /* --- Push Buttons (visionOS Platter Pill) --- */
        QPushButton {
            background-color: rgba(0, 0, 0, 0.05);
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.10);
            border-radius: 12px;
            padding: 4px 12px;
            font-size: 8.5pt;
            font-weight: 500;
            min-height: 18px;
        }
        QPushButton:hover {
            background-color: rgba(0, 0, 0, 0.09);
            border: 1px solid rgba(0, 0, 0, 0.18);
            color: #1c1c1e;
        }
        QPushButton:pressed {
            background-color: rgba(0, 0, 0, 0.14);
            border: 1px solid rgba(0, 0, 0, 0.25);
        }
        QPushButton:checked {
            background-color: #007aff;
            color: #ffffff;
            border: 1px solid #007aff;
            font-weight: 600;
        }
        QPushButton:disabled {
            background-color: rgba(0, 0, 0, 0.02);
            color: rgba(60, 60, 67, 0.30);
            border: 1px solid rgba(0, 0, 0, 0.04);
        }

        /* Primary / Accent Buttons (Open A, Open B, etc.) */
        QPushButton#BtnOpenA, QPushButton#BtnOpenB, QPushButton[accent="true"] {
            background-color: #007aff;
            border: 1px solid rgba(0, 0, 0, 0.15);
            color: #ffffff;
            font-weight: 600;
            border-radius: 12px;
        }
        QPushButton#BtnOpenA:hover, QPushButton#BtnOpenB:hover, QPushButton[accent="true"]:hover {
            background-color: #0066d6;
            border: 1px solid rgba(0, 0, 0, 0.25);
            color: #ffffff;
        }
        QPushButton#BtnOpenA:pressed, QPushButton#BtnOpenB:pressed, QPushButton[accent="true"]:pressed {
            background-color: #0051a8;
        }

        /* Play Button (visionOS Capsule Highlight) */
        QPushButton#BtnPlay {
            background-color: #007aff;
            border: 1px solid rgba(0, 0, 0, 0.15);
            color: #ffffff;
            font-weight: 600;
            padding: 4px 14px;
            border-radius: 12px;
        }
        QPushButton#BtnPlay:hover {
            background-color: #0066d6;
        }
        QPushButton#BtnPlay:checked {
            background-color: #ff9500;
            border: 1px solid rgba(0, 0, 0, 0.20);
            color: #ffffff;
        }

        /* Capsule variant */
        QPushButton[capsule="true"] {
            border-radius: 15px;
            padding: 4px 16px;
        }

        /* Step Previous / Next Buttons */
        QPushButton#BtnStepPrev, QPushButton#BtnStepNext {
            padding: 0px;
            min-width: 32px;
            max-width: 32px;
            font-size: 10pt;
            font-weight: bold;
        }

        /* --- ComboBox (Recessed Glass Platter) --- */
        QComboBox {
            background-color: rgba(0, 0, 0, 0.05);
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.10);
            border-radius: 7px;
            padding: 1px 8px 1px 8px;
            font-size: 8.5pt;
            min-height: 20px;
            max-height: 20px;
        }
        QComboBox:hover {
            background-color: rgba(0, 0, 0, 0.08);
            border: 1px solid rgba(0, 0, 0, 0.16);
        }
        QComboBox:focus, QComboBox:on {
            border: 1px solid #007aff;
        }
        QComboBox::drop-down {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 18px;
            border-left: 1px solid rgba(0, 0, 0, 0.08);
            border-top-right-radius: 7px;
            border-bottom-right-radius: 7px;
            background-color: rgba(0, 0, 0, 0.03);
        }
        QComboBox::drop-down:hover {
            background-color: rgba(0, 0, 0, 0.08);
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron_down_light.png);
            width: 8px;
            height: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.12);
            border-radius: 10px;
            padding: 4px;
            selection-background-color: #007aff;
            selection-color: #ffffff;
            outline: none;
        }

        /* --- SpinBox (Recessed Glass Field) --- */
        QSpinBox, QDoubleSpinBox {
            background-color: rgba(0, 0, 0, 0.04);
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.10);
            border-radius: 7px;
            padding: 1px 2px 1px 6px;
            font-size: 8.5pt;
            min-height: 20px;
            max-height: 20px;
        }
        QSpinBox:hover, QDoubleSpinBox:hover {
            border: 1px solid rgba(0, 0, 0, 0.18);
            background-color: rgba(0, 0, 0, 0.06);
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #007aff;
        }
        QSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 15px;
            border-left: 1px solid rgba(0, 0, 0, 0.08);
            border-bottom: 0.5px solid rgba(0, 0, 0, 0.06);
            border-top-right-radius: 6px;
            background-color: rgba(0, 0, 0, 0.03);
            margin: 0px;
        }
        QSpinBox::up-button:hover {
            background-color: rgba(0, 0, 0, 0.10);
        }
        QSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 15px;
            border-left: 1px solid rgba(0, 0, 0, 0.08);
            border-bottom-right-radius: 6px;
            background-color: rgba(0, 0, 0, 0.03);
            margin: 0px;
        }
        QSpinBox::down-button:hover {
            background-color: rgba(0, 0, 0, 0.10);
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url(:/icons/arrow_up_light.png);
            width: 7px;
            height: 7px;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url(:/icons/arrow_down_light.png);
            width: 7px;
            height: 7px;
        }

        /* --- Professional Timeline Slider (Needle Playhead - Light Mode) --- */
        QSlider {
            min-height: 18px;
            max-height: 18px;
            padding-bottom: 2px;
        }
        QSlider::groove:horizontal {
            height: 3px;
            background: rgba(0, 0, 0, 0.12);
            border-radius: 1.5px;
        }
        QSlider::sub-page:horizontal {
            background: #007aff;
            border-radius: 1.5px;
        }
        QSlider::handle:horizontal {
            background: transparent;
            border: none;
            width: 6px;
            margin: -5px 0;
        }

        /* --- ScrollBar (Minimal Floating Capsule) --- */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px 1px;
        }
        QScrollBar::handle:vertical {
            background: rgba(0, 0, 0, 0.18);
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(0, 0, 0, 0.35);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0px;
        }

        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 1px 2px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(0, 0, 0, 0.18);
            min-width: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(0, 0, 0, 0.35);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            width: 0px;
        }

        /* --- Key / Value & Information Labels --- */
        QLabel#SidebarKey {
            color: #636366;
            font-size: 9pt;
        }
        QLabel#SidebarVal {
            color: #1c1c1e;
            font-size: 9pt;
            font-family: monospace;
            font-weight: 500;
        }
        QLabel#ProgressPctLabel {
            color: #636366;
            font-family: monospace;
            font-size: 9pt;
            min-width: 45px;
        }

        /* --- Splitter --- */
        QSplitter::handle:horizontal {
            background-color: rgba(0, 0, 0, 0.08);
            width: 2px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #007aff;
        }

        /* --- Tooltip (Thick Glass Platter) --- */
        QToolTip {
            background-color: rgba(255, 255, 255, 0.95);
            color: #1c1c1e;
            border: 1px solid rgba(0, 0, 0, 0.12);
            border-radius: 8px;
            padding: 4px 10px;
            font-size: 9pt;
        }

        /* --- Status Bar --- */
        QStatusBar {
            background-color: #f2f2f7;
            color: #636366;
            border-top: 1px solid rgba(0, 0, 0, 0.10);
            font-size: 9pt;
            min-height: 22px;
        }
    )";

    app.setStyleSheet(qss);
}

void apply_theme(QApplication& app, ThemeMode mode) {
    s_current_mode = mode;
    bool is_dark = true;

    if (mode == ThemeMode::Dark) {
        is_dark = true;
    } else if (mode == ThemeMode::Light) {
        is_dark = false;
    } else {
        // ThemeMode::Auto: detect from system styleHints (Qt 6.5+)
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        auto scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Light) {
            is_dark = false;
        } else if (scheme == Qt::ColorScheme::Dark) {
            is_dark = true;
        } else {
            // Fallback: check window text brightness
            QColor win_col = app.palette().color(QPalette::Window);
            is_dark = (win_col.value() < 128);
        }
#else
        // For Qt < 6.5: detect from system palette brightness
        QColor win_col = app.palette().color(QPalette::Window);
        is_dark = (win_col.value() < 128);
#endif
    }

    s_is_dark = is_dark;
    if (is_dark) {
        apply_visionos_dark_theme(app);
    } else {
        apply_visionos_light_theme(app);
    }
}

bool is_dark_theme() {
    return s_is_dark;
}

void setup_system_theme_listener(QApplication& app) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto* hints = QGuiApplication::styleHints();
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, &app, [&app](Qt::ColorScheme /*scheme*/) {
        if (s_current_mode == ThemeMode::Auto) {
            apply_theme(app, ThemeMode::Auto);
        }
    });
#else
    // QStyleHints::colorSchemeChanged was introduced in Qt 6.5
    (void)app;
#endif
}

void apply_studio_dark_theme(QApplication& app) {
    apply_theme(app, ThemeMode::Auto);
}

} // namespace yuvdiff
