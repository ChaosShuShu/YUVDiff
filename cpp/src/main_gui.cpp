#include "yuvdiff/gui.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    yuvdiff::MainWindow window;
    window.show();
    return app.exec();
}
