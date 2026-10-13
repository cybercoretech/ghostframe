#include "photoservice.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QtQuickControls2/QQuickStyle>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Ghostframe");
    QCoreApplication::setOrganizationName("Cybercore Systems");
    QGuiApplication::setDesktopFileName("ghostframe");
    QQuickStyle::setStyle("Basic");

    PhotoService photos;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) qCritical().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty("photoService", &photos);
    const QUrl mainUrl(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [mainUrl](QObject *object, const QUrl &url) {
        if (!object && url == mainUrl) QCoreApplication::exit(1);
    });
    engine.load(mainUrl);
    return app.exec();
}
