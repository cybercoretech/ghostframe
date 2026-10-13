#include "photoservice.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Ghostframe");
    QCoreApplication::setOrganizationName("Cybercore Systems");
    QGuiApplication::setDesktopFileName("ghostframe");
    QQuickStyle::setStyle("Basic");

    PhotoService photos;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("photoService", &photos);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
