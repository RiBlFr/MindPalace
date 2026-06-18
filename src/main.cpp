#include <QApplication>
#include <QLockFile>
#include "controller/appcontroller.h"
#include "view/DesktopPetWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    if (QApplication::arguments().contains(QStringLiteral("--desktop-pet"))) {
        return MindPalace::DesktopPet::runDesktopPetMode();
    }

    if (QApplication::arguments().contains(QStringLiteral("--review-reminder-startup"))) {
        MindPalace::Controller::AppController appController;
        appController.showStartupReviewReminder();
        return 0;
    }

    QLockFile mainWindowLock(MindPalace::DesktopPet::mainWindowLockPath());
    if (!mainWindowLock.tryLock(0)) {
        return 0;
    }

    MindPalace::Controller::AppController appController;
    appController.start();

    return QApplication::exec();
}
