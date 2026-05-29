#include <QApplication>
#include "controller/appcontroller.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MindPalace::Controller::AppController appController;
    appController.start();

    return QApplication::exec();
}
