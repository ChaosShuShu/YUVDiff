#include "yuvdiff/theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QFontDatabase>

namespace yuvdiff {

void apply_studio_dark_theme(QApplication& app) {
    // 1. Set global font
    QFont font = app.font();
    font.setPointSize(9);
    app.setFont(font);

    // 2. Configure dark QPalette
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(18, 20, 26));
    pal.setColor(QPalette::WindowText, QColor(226, 232, 240));
    pal.setColor(QPalette::Base, QColor(22, 25, 34));
    pal.setColor(QPalette::AlternateBase, QColor(28, 32, 43));
    pal.setColor(QPalette::ToolTipBase, QColor(28, 32, 43));
    pal.setColor(QPalette::ToolTipText, QColor(241, 245, 249));
    pal.setColor(QPalette::Text, QColor(241, 245, 249));
    pal.setColor(QPalette::Button, QColor(30, 35, 47));
    pal.setColor(QPalette::ButtonText, QColor(226, 232, 240));
    pal.setColor(QPalette::BrightText, QColor(56, 189, 248));
    pal.setColor(QPalette::Highlight, QColor(14, 165, 233));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(pal);

    // 3. Studio Obsidian Dark QSS Stylesheet
    const char* qss = R"(
        /* Global Window and Central Widget */
        QMainWindow, QWidget#CentralWidget {
            background-color: #111318;
            color: #e2e8f0;
        }

        /* Toolbars & Bar Containers */
        QFrame#ToolBarFrame, QFrame#ConfigBarFrame, QFrame#BottomDeckFrame {
            background-color: #161922;
            border-bottom: 1px solid #242938;
            padding: 4px;
        }
        QFrame#BottomDeckFrame {
            border-top: 1px solid #242938;
            border-bottom: none;
        }

        /* Left Sidebar & Scroll Area */
        QScrollArea#SidebarScrollArea {
            background-color: #14161f;
            border-right: 1px solid #242938;
            border-top: none;
            border-bottom: none;
            border-left: none;
        }
        QWidget#SidebarContent {
            background-color: #14161f;
        }

        /* Card Panels (GroupBox / CardFrame) */
        QGroupBox {
            background-color: #171a24;
            border: 1px solid #282d3e;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 10px;
            padding-bottom: 6px;
            padding-left: 8px;
            padding-right: 8px;
            font-size: 11px;
            font-weight: bold;
            color: #38bdf8;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 8px;
            padding: 0 4px;
            background-color: #14161f;
            color: #38bdf8;
        }

        /* Buttons */
        QPushButton {
            background-color: #202431;
            color: #e2e8f0;
            border: 1px solid #31374b;
            border-radius: 4px;
            padding: 5px 12px;
            font-weight: 500;
            min-height: 18px;
        }
        QPushButton:hover {
            background-color: #2a3042;
            border-color: #0ea5e9;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #161922;
            border-color: #0284c7;
        }
        QPushButton:checked {
            background-color: #0369a1;
            border-color: #38bdf8;
            color: #ffffff;
        }
        QPushButton:disabled {
            background-color: #181a23;
            border-color: #242837;
            color: #64748b;
        }

        /* Primary Action Buttons */
        QPushButton#BtnOpenA, QPushButton#BtnOpenB {
            background-color: #1e293b;
            border: 1px solid #38bdf8;
            color: #38bdf8;
            font-weight: bold;
        }
        QPushButton#BtnOpenA:hover, QPushButton#BtnOpenB:hover {
            background-color: #0ea5e9;
            color: #ffffff;
        }
        QPushButton#BtnPlay {
            background-color: #0284c7;
            border: 1px solid #38bdf8;
            color: #ffffff;
            font-weight: bold;
            padding: 5px 16px;
        }
        QPushButton#BtnPlay:hover {
            background-color: #0ea5e9;
        }
        QPushButton#BtnPlay:checked {
            background-color: #ea580c;
            border-color: #fb923c;
            color: #ffffff;
        }

        /* ComboBox */
        QComboBox {
            background-color: #1c202c;
            color: #e2e8f0;
            border: 1px solid #2e3447;
            border-radius: 4px;
            padding: 4px 8px;
            min-height: 18px;
        }
        QComboBox:hover {
            border-color: #38bdf8;
        }
        QComboBox:focus {
            border-color: #0ea5e9;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 18px;
            border-left: 1px solid #2e3447;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
        }
        QComboBox QAbstractItemView {
            background-color: #161922;
            color: #e2e8f0;
            border: 1px solid #2e3447;
            selection-background-color: #0284c7;
            selection-color: #ffffff;
            outline: none;
            padding: 4px;
        }

        /* SpinBox */
        QSpinBox {
            background-color: #1c202c;
            color: #e2e8f0;
            border: 1px solid #2e3447;
            border-radius: 4px;
            padding: 3px 6px;
            min-height: 18px;
        }
        QSpinBox:hover {
            border-color: #38bdf8;
        }
        QSpinBox:focus {
            border-color: #0ea5e9;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #222634;
            border: none;
            width: 14px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #2e3447;
        }

        /* Slider */
        QSlider::groove:horizontal {
            height: 5px;
            background: #242938;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0284c7, stop:1 #38bdf8);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #f1f5f9;
            border: 2px solid #0284c7;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #38bdf8;
            border-color: #ffffff;
            width: 16px;
            margin-top: -6px;
            margin-bottom: -6px;
            border-radius: 8px;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            background: #14161f;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #282d3e;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #38bdf8;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* Key / Value Label Styling */
        QLabel#SidebarKey {
            color: #94a3b8;
            font-size: 11px;
        }
        QLabel#SidebarVal {
            color: #f1f5f9;
            font-size: 11px;
            font-family: monospace;
            font-weight: 500;
        }
        QLabel#CounterLabel {
            background-color: #141720;
            color: #38bdf8;
            border: 1px solid #282d3e;
            border-radius: 4px;
            padding: 3px 8px;
            font-family: monospace;
            font-size: 12px;
            font-weight: bold;
        }
        QLabel#ProgressPctLabel {
            color: #94a3b8;
            font-family: monospace;
            font-size: 11px;
            min-width: 45px;
        }

        /* Tooltip */
        QToolTip {
            background-color: #1a1d27;
            color: #f8fafc;
            border: 1px solid #38bdf8;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 11px;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #0f1117;
            color: #94a3b8;
            border-top: 1px solid #1f2330;
            font-size: 11px;
            min-height: 22px;
        }
    )";

    app.setStyleSheet(qss);
}

} // namespace yuvdiff
