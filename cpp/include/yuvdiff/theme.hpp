#pragma once

class QApplication;

namespace yuvdiff {

enum class ThemeMode {
    Auto,
    Dark,
    Light
};

/**
 * @brief Applies the Apple visionOS Dark theme & QSS styling.
 */
void apply_visionos_dark_theme(QApplication& app);

/**
 * @brief Applies the Apple visionOS Light theme & QSS styling.
 */
void apply_visionos_light_theme(QApplication& app);

/**
 * @brief Applies visionOS theme based on theme mode (Auto/Dark/Light).
 */
void apply_theme(QApplication& app, ThemeMode mode = ThemeMode::Auto);

/**
 * @brief Sets up automatic listener for system color scheme changes (Qt6 styleHints).
 */
void setup_system_theme_listener(QApplication& app);

/**
 * @brief Backward-compatible alias for apply_visionos_dark_theme.
 */
void apply_studio_dark_theme(QApplication& app);

/**
 * @brief Returns true if currently running under dark theme.
 */
bool is_dark_theme();

} // namespace yuvdiff
