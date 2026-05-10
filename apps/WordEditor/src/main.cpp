//! @file main.cpp CppWordKit Word Editor 应用程序入口

#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CppWordKit Word Editor");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("CppWordKit");

    MainWindow window;
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
