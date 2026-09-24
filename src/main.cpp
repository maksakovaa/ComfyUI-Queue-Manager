#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ComfyUI Queue Manager");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("ComfyUIQueueManager");
    if (QStyleFactory::keys().contains("Fusion"))
        app.setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.show();
    return app.exec();
}
