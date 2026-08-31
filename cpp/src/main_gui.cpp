#include "yuvdiff/gui.hpp"
#include "yuvdiff/theme.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    yuvdiff::apply_studio_dark_theme(app);
    yuvdiff::MainWindow window;
    window.show();
    return app.exec();
}
