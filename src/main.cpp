#include "photoservice.h"
#include "imageeditor.h"
#include "imageengine.h"
#include "previewprovider.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QQuickWindow>
#include <QTimer>
#include <QtQuickControls2/QQuickStyle>

int main(int argc, char *argv[]) {
    QString vipsError;
    if (!ImageEngine::initialize(argv[0], &vipsError)) {
        qCritical().noquote() << "Ghostframe could not start libvips:" << vipsError;
        return 1;
    }
    const int code = [&] {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Ghostframe");
    QCoreApplication::setOrganizationName("Cybercore Systems");
    QGuiApplication::setDesktopFileName("ghostframe");
    QQuickStyle::setStyle("Basic");

    PhotoService photos;
    ImageEditor imageEditor;
    QQmlApplicationEngine engine;
    auto *previewProvider = new PreviewProvider;  // owned by the engine
    engine.addImageProvider("ghostframe-edit", previewProvider);
    imageEditor.setPreviewProvider(previewProvider);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) qCritical().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty("photoService", &photos);
    engine.rootContext()->setContextProperty("imageEditor", &imageEditor);
    const QUrl mainUrl(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [mainUrl](QObject *object, const QUrl &url) {
        if (!object && url == mainUrl) QCoreApplication::exit(1);
    });
    engine.load(mainUrl);

    // Development aid: GHOSTFRAME_EDIT=/path/to/image opens the editor on a
    // local file (never modified or deleted), without PhotoPrism.
    const QString editPath = qEnvironmentVariable("GHOSTFRAME_EDIT");
    if (!editPath.isEmpty() && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().first()->setProperty("showEditor", true);
        imageEditor.openLocalFile(editPath);
    }

    // Development aid: GHOSTFRAME_GRAB=/path.png[:ms] saves a screenshot of the
    // window after the delay (default 6000 ms) and quits. Works offscreen.
    const QString grab = qEnvironmentVariable("GHOSTFRAME_GRAB");
    if (!grab.isEmpty() && !engine.rootObjects().isEmpty()) {
        const qsizetype colon = grab.lastIndexOf(':');
        bool ok = false;
        const int delay = colon > 0 ? grab.mid(colon + 1).toInt(&ok) : 0;
        const QString path = ok ? grab.left(colon) : grab;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QTimer::singleShot(ok ? delay : 6000, &app, [window, path] {
            if (window) window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }
    return app.exec();
    }();
    // Every ImageEditor worker has finished by now (its destructor waits).
    ImageEngine::shutdown();
    return code;
}
