#include <QApplication>
#include "controller/appcontroller.h"
#include "view/DesktopPetWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    if (QApplication::arguments().contains(QStringLiteral("--desktop-pet"))) {
        return MindPalace::DesktopPet::runDesktopPetMode();
    }

    MindPalace::Controller::AppController appController;
    if (QApplication::arguments().contains(QStringLiteral("--review-reminder-startup"))) {
        appController.showStartupReviewReminder();
        return 0;
    }

    appController.start();

    return QApplication::exec();
}
