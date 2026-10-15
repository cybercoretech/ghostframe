// Engine, recipe and history tests. Fixture images are generated at runtime,
// so no binary samples live in the repo.
#include "edithistory.h"
#include "imageeditor.h"
#include "previewprovider.h"
#include "editrecipe.h"
#include "imageengine.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QDir>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using vips::VImage;

namespace {

// 400×300 RGB gradient: red rises left→right, green rises top→bottom, blue fixed.
QString writeFixture(const QString &dir, const QString &name = "fixture.png", QSize size = {400, 300}) {
    QImage image(size, QImage::Format_RGB888);
    for (int y = 0; y < size.height(); ++y)
        for (int x = 0; x < size.width(); ++x)
            image.setPixelColor(x, y, QColor(x * 255 / (size.width() - 1), y * 255 / (size.height() - 1), 96));
    const QString path = dir + "/" + name;
    if (!image.save(path)) qFatal("could not write fixture %s", qPrintable(path));
    return path;
}

double mean(const VImage &image) { return image.avg(); }

}  // namespace

class TestGhostframe : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QString m_fixture;

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_fixture = writeFixture(m_dir.path());
    }

    // ── recipe ──────────────────────────────────────────────────────────
    void recipeIdentityIsEmpty() {
        const EditRecipe r;
        QVERIFY(r.isIdentity());
        QCOMPARE(r.toJson().value("ops").toArray().size(), 0);
        QCOMPARE(r.toJson().value("version").toInt(), EditRecipe::Version);
    }

    void recipeRoundTrips() {
        EditRecipe r;
        r.rotation = 270;
        r.hasCrop = true; r.cropX = 0.1; r.cropY = 0.2; r.cropWidth = 0.5; r.cropHeight = 0.6;
        r.brightness = 12.5; r.contrast = -20; r.saturation = 140;
        EditRecipe back;
        QVERIFY(EditRecipe::fromJson(QJsonDocument::fromJson(r.toJsonString().toUtf8()).object(), &back));
        QCOMPARE(back, r);
    }

    void recipeClampsAndIgnoresUnknownOps() {
        const auto json = QJsonDocument::fromJson(R"({"version":1,"ops":[
            {"op":"rotate","degrees":-90},
            {"op":"saturation","percent":999},
            {"op":"crop","x":0.8,"y":0,"width":0.9,"height":1},
            {"op":"neonBloom","amount":3}]})").object();
        EditRecipe r;
        QVERIFY(EditRecipe::fromJson(json, &r));
        QCOMPARE(r.rotation, 270);
        QCOMPARE(r.saturation, 200.0);
        QVERIFY(r.hasCrop);
        QVERIFY(qFuzzyCompare(r.cropX + r.cropWidth, 1.0));
    }

    void recipeRejectsNewerVersion() {
        EditRecipe r;
        QVERIFY(!EditRecipe::fromJson(QJsonObject{{"version", EditRecipe::Version + 1}, {"ops", QJsonArray()}}, &r));
        QVERIFY(!EditRecipe::fromJson(QJsonObject{{"ops", QJsonArray()}}, &r));
    }

    // ── history ─────────────────────────────────────────────────────────
    void historyUndoRedo() {
        EditHistory h;
        EditRecipe a; a.rotation = 90;
        EditRecipe b = a; b.brightness = 30;
        QVERIFY(!h.canUndo());
        QVERIFY(h.commit(a));
        QVERIFY(h.commit(b));
        QVERIFY(!h.commit(b));  // unchanged state is not a new step
        QCOMPARE(h.undo(), a);
        QCOMPARE(h.undo(), EditRecipe{});
        QVERIFY(!h.canUndo());
        QCOMPARE(h.redo(), a);
        EditRecipe c = a; c.saturation = 0;
        QVERIFY(h.commit(c));  // drops the redo branch (b)
        QVERIFY(!h.canRedo());
        QCOMPARE(h.undo(), a);
        QCOMPARE(h.redo(), c);
    }

    void historyRespectsLimit() {
        EditHistory h(5);
        for (int i = 1; i <= 20; ++i) { EditRecipe r; r.brightness = i; h.commit(r); }
        QCOMPARE(h.size(), 5);
        QCOMPARE(h.current().brightness, 20.0);
    }

    // ── engine ──────────────────────────────────────────────────────────
    void sourceSizeAndProxy() {
        QCOMPARE(ImageEngine::sourceSize(m_fixture), QSize(400, 300));
        const VImage proxy = ImageEngine::loadProxy(m_fixture, {200, 200});
        QCOMPARE(proxy.width(), 200);
        QCOMPARE(proxy.height(), 150);
        // Never upscales.
        QCOMPARE(ImageEngine::loadProxy(m_fixture, {4000, 4000}).width(), 400);
    }

    void rotateAndCrop() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe r;
        r.rotation = 90;
        VImage out = ImageEngine::apply(original, r);
        QCOMPARE(out.width(), 300);
        QCOMPARE(out.height(), 400);
        // Clockwise: the original's bottom-left (green=255, red=0) is now top-left.
        const std::vector<double> topLeft = out.getpoint(0, 0);
        QVERIFY(topLeft[0] < 5 && topLeft[1] > 250);

        r.hasCrop = true; r.cropX = 0.5; r.cropY = 0.25; r.cropWidth = 0.5; r.cropHeight = 0.5;
        out = ImageEngine::apply(original, r);
        QCOMPARE(out.width(), 150);
        QCOMPARE(out.height(), 200);
    }

    void brightnessAndContrastMatchImageMagickFormula() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe brighter; brighter.brightness = 40;
        QVERIFY(mean(ImageEngine::apply(original, brighter)) > mean(original) + 20);

        // Identity contrast (slope 1, intercept 0) must not shift anything.
        EditRecipe flat; flat.brightness = 0; flat.contrast = 0;
        QCOMPARE(mean(ImageEngine::apply(original, flat)), mean(original));

        // +100 brightness saturates to white and must clip, not wrap around.
        EditRecipe white; white.brightness = 100;
        const VImage clipped = ImageEngine::apply(original, white);
        QCOMPARE(clipped.format(), VIPS_FORMAT_UCHAR);
        QVERIFY(clipped.min() >= 250);

        EditRecipe moreContrast; moreContrast.contrast = 60;
        QVERIFY(ImageEngine::apply(original, moreContrast).deviate() > original.deviate());
    }

    // Reference values sampled from `magick ramp.png -brightness-contrast BxC`
    // (ImageMagick 7.1.2) on a 0..255 grey ramp.
    void brightnessContrastMatchesImageMagickSamples_data() {
        QTest::addColumn<double>("brightness");
        QTest::addColumn<double>("contrast");
        QTest::addColumn<QList<int>>("inputs");
        QTest::addColumn<QList<int>>("expected");
        QTest::newRow("0x40") << 0.0 << 40.0 << QList<int>{0, 64, 128, 192, 255} << QList<int>{0, 22, 128, 235, 255};
        QTest::newRow("0x-40") << 0.0 << -40.0 << QList<int>{0, 64, 128, 192, 255} << QList<int>{51, 89, 128, 166, 204};
        QTest::newRow("25x40") << 25.0 << 40.0 << QList<int>{0, 64, 128, 192, 255} << QList<int>{21, 128, 235, 255, 255};
        QTest::newRow("-30x-20") << -30.0 << -20.0 << QList<int>{0, 64, 128, 192, 255} << QList<int>{0, 16, 67, 118, 168};
    }

    void brightnessContrastMatchesImageMagickSamples() {
        QFETCH(double, brightness);
        QFETCH(double, contrast);
        QFETCH(QList<int>, inputs);
        QFETCH(QList<int>, expected);
        QImage ramp(256, 1, QImage::Format_RGB888);
        for (int x = 0; x < 256; ++x) ramp.setPixelColor(x, 0, QColor(x, x, x));
        const QString path = m_dir.path() + "/ramp.png";
        QVERIFY(ramp.save(path));
        EditRecipe r; r.brightness = brightness; r.contrast = contrast;
        const VImage out = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        for (int i = 0; i < inputs.size(); ++i) {
            const int value = int(out.getpoint(inputs[i], 0)[0]);
            QVERIFY2(std::abs(value - expected[i]) <= 1,
                     qPrintable(QString("in %1: got %2, ImageMagick %3").arg(inputs[i]).arg(value).arg(expected[i])));
        }
    }

    void saturationZeroIsNeutralGrey() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe grey; grey.saturation = 0;
        const VImage out = ImageEngine::apply(original, grey);
        const std::vector<double> p = out.getpoint(300, 50);
        QVERIFY(std::abs(p[0] - p[1]) <= 2 && std::abs(p[1] - p[2]) <= 2);
    }

    void alphaIsPreserved() {
        QImage rgba(64, 64, QImage::Format_RGBA8888);
        rgba.fill(QColor(200, 40, 40, 128));
        const QString path = m_dir.path() + "/alpha.png";
        QVERIFY(rgba.save(path));
        EditRecipe r; r.brightness = 30; r.saturation = 50;
        const VImage out = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        QCOMPARE(out.bands(), 4);
        QCOMPARE(out.extract_band(3).avg(), 128.0);
    }

    void toQImageFormats() {
        const QImage rgb = ImageEngine::toQImage(ImageEngine::loadOriginal(m_fixture));
        QCOMPARE(rgb.size(), QSize(400, 300));
        QCOMPARE(rgb.format(), QImage::Format_RGB888);
        QCOMPARE(rgb.pixelColor(399, 0).red(), 255);
    }

    void exportWritesFullResolution() {
        EditRecipe r; r.rotation = 90; r.brightness = 10;
        const QString out = m_dir.path() + "/export.jpg";
        QString error;
        QVERIFY2(ImageEngine::exportImage(m_fixture, r, out, &error), qPrintable(error));
        QCOMPARE(ImageEngine::sourceSize(out), QSize(300, 400));

        QVERIFY(!ImageEngine::exportImage(m_dir.path() + "/missing.png", r, m_dir.path() + "/x.jpg", &error));
        QVERIFY(!error.isEmpty());
    }

    // ── editor (end to end, no QML) ─────────────────────────────────────
    void editorEditUndoExport() {
        PreviewProvider provider;
        ImageEditor editor;
        editor.setPreviewProvider(&provider);
        const QString exports = m_dir.path() + "/exports";
        editor.setExportDirectory(exports);

        QSignalSpy previews(&editor, &ImageEditor::previewChanged);
        editor.openLocalFile(m_fixture);
        QTRY_VERIFY(editor.ready());
        QTRY_VERIFY(!editor.previewUrl().isEmpty());
        QCOMPARE(editor.displayWidth(), 400);
        QVERIFY(!editor.canUndo());

        editor.rotateBy(90);
        QTRY_COMPARE(editor.displayWidth(), 300);
        QCOMPARE(editor.displayHeight(), 400);
        QCOMPARE(provider.requestImage("preview", nullptr, {}).size(), QSize(300, 400));

        // A slider drag is live, then one undo step on release.
        editor.setAdjustments(10, 0, 100);
        editor.setAdjustments(20, 0, 100);
        editor.setAdjustments(30, 5, 120);
        QVERIFY(editor.canUndo());
        editor.commitEdit();
        editor.undo();
        QCOMPARE(editor.brightness(), 0.0);
        QVERIFY(editor.canRedo());
        editor.redo();
        QCOMPARE(editor.brightness(), 30.0);
        QCOMPARE(editor.saturation(), 120.0);
        editor.undo();
        editor.undo();
        QTRY_COMPARE(editor.displayWidth(), 400);  // rotation undone too
        editor.redo();
        editor.redo();

        // Crop to the right half, then export.
        editor.setCrop(0.5, 0, 0.5, 1);
        QTRY_COMPARE(editor.displayWidth(), 150);
        QSignalSpy saved(&editor, &ImageEditor::saved);
        editor.save("Fixture", "A test export", "test, ghostframe");
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 10000);
        const QString path = saved.first().first().toString();
        QCOMPARE(ImageEngine::sourceSize(path), QSize(150, 400));
        QFile sidecar(path + ".xmp");
        QVERIFY(sidecar.open(QIODevice::ReadOnly));
        const QString xmp = QString::fromUtf8(sidecar.readAll());
        QVERIFY(xmp.contains("<cybermeta:recipe>"));
        QVERIFY(xmp.contains("&quot;op&quot;:&quot;rotate&quot;") || xmp.contains("\"op\":\"rotate\""));
        QVERIFY(xmp.contains("ghostframe"));
        // No staging files are left behind.
        QCOMPARE(QDir(exports).entryList(QDir::Files | QDir::Hidden).size(), 2);

        // A user's own file is never deleted when the session ends.
        editor.cancel();
        QVERIFY(QFileInfo::exists(m_fixture));
        QVERIFY(!editor.ready());
    }

    // Phase 0 target: a preview render on the proxy well under 100 ms.
    void previewIsFast() {
        const QString big = writeFixture(m_dir.path(), "big.png", {6000, 4000});
        const VImage proxy = ImageEngine::loadProxy(big);
        EditRecipe r; r.rotation = 90; r.brightness = 20; r.contrast = 15; r.saturation = 130;
        r.hasCrop = true; r.cropX = 0.1; r.cropY = 0.1; r.cropWidth = 0.8; r.cropHeight = 0.8;
        ImageEngine::toQImage(ImageEngine::apply(proxy, r));  // warm-up
        QElapsedTimer timer;
        timer.start();
        constexpr int runs = 10;
        for (int i = 0; i < runs; ++i) {
            r.brightness = i;
            ImageEngine::toQImage(ImageEngine::apply(proxy, r));
        }
        const double perRender = double(timer.elapsed()) / runs;
        qInfo("preview render on a %dx%d proxy: %.1f ms", proxy.width(), proxy.height(), perRender);
        QVERIFY2(perRender < 100, qPrintable(QString("%1 ms per render").arg(perRender)));
    }
};

int main(int argc, char *argv[]) {
    QString error;
    if (!ImageEngine::initialize(argv[0], &error)) qFatal("libvips: %s", qPrintable(error));
    int status = 0;
    {
        QGuiApplication app(argc, argv);
        TestGhostframe tests;
        status = QTest::qExec(&tests, argc, argv);
    }
    ImageEngine::shutdown();
    return status;
}

#include "tst_ghostframe.moc"
