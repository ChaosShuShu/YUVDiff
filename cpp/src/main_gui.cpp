#include "yuvdiff/gui.hpp"
#include "yuvdiff/theme.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QPixmap>
#include <QTimer>

#include <QScreen>
#include <QPainter>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("YUVdiff GUI & UI Preview Tool");
    parser.addHelpOption();

    QCommandLineOption themeOption(QStringList() << "t" << "theme", "Theme mode: auto, dark, light", "mode", "auto");
    QCommandLineOption previewOption(QStringList() << "p" << "preview", "Run in mock UI preview mode (no video file needed).");
    QCommandLineOption snapOption("snapshot", "Save screenshot to path and exit.", "path");
    parser.addOption(themeOption);
    parser.addOption(previewOption);
    parser.addOption(snapOption);
    parser.process(app);

    QString themeStr = parser.value(themeOption).toLower();
    yuvdiff::ThemeMode tMode = yuvdiff::ThemeMode::Auto;
    if (themeStr == "dark") tMode = yuvdiff::ThemeMode::Dark;
    else if (themeStr == "light") tMode = yuvdiff::ThemeMode::Light;

    yuvdiff::apply_theme(app, tMode);
    if (tMode == yuvdiff::ThemeMode::Auto) {
        yuvdiff::setup_system_theme_listener(app);
    }

    yuvdiff::MainWindow window;

    // In preview mode or snapshot mode, populate with high-fidelity mock data
    if (parser.isSet(previewOption) || parser.isSet(snapOption)) {
        window.populate_mock_data();
    }

    window.show();

    if (parser.isSet(snapOption)) {
        QString snapPath = parser.value(snapOption);
        QTimer::singleShot(200, [&window, snapPath, &app]() {
            QPixmap pix = window.grab();
            QFileInfo fi(snapPath);
            QDir().mkpath(fi.dir().absolutePath());
            pix.save(snapPath, "PNG");
            app.quit();
        });
    }

    return app.exec();
}
