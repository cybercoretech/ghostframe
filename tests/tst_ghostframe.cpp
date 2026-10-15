// Engine, recipe and history tests. Fixture images are generated at runtime,
// so no binary samples live in the repo.
#include "cubelut.h"
#include "cybermetaclient.h"
#include "cybergrid.h"
#include "edithistory.h"
#include "fx.h"
#include "looklibrary.h"
#include "imageeditor.h"
#include "previewprovider.h"
#include "recipestore.h"
#include "editrecipe.h"
#include "imageengine.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <random>

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

// exiftool output for these tags (for checking exports independently of cybermeta).
QString exiftool(const QStringList &args) {
    QProcess p;
    p.start("exiftool", args);
    p.waitForFinished(20000);
    return QString::fromUtf8(p.readAllStandardOutput());
}
bool haveExiftool() { return !QStandardPaths::findExecutable("exiftool").isEmpty(); }

// A JPEG with camera, owner and GPS metadata (and optionally a rotation).
QString writeCameraJpeg(const QString &dir, const QString &name, int orientation = 1) {
    QImage image(300, 200, QImage::Format_RGB888);
    image.fill(QColor(90, 120, 160));
    for (int x = 0; x < 30; ++x) for (int y = 0; y < 30; ++y) image.setPixelColor(x, y, Qt::red);  // top-left marker
    const QString path = dir + "/" + name;
    image.save(path, "JPG", 95);
    exiftool({"-q", "-overwrite_original", "-Make=Fujifilm", "-Model=X-T5", "-LensModel=XF23mm", "-FNumber=2.8",
              "-ExposureTime=1/250", "-ISO=400", "-Artist=Raven", "-DateTimeOriginal=2026:10:01 21:14:05",
              "-GPSLatitude=45.5152", "-GPSLatitudeRef=N", "-GPSLongitude=122.6784", "-GPSLongitudeRef=W",
              QString("-Orientation#=%1").arg(orientation), path});
    return path;
}

double mean(const VImage &image) { return image.avg(); }

// A solid-colour 8-bit sRGB fixture.
QString writeSolid(const QString &dir, const QString &name, QColor colour, QSize size = {64, 64}) {
    QImage image(size, QImage::Format_RGB888);
    image.fill(colour);
    const QString path = dir + "/" + name;
    if (!image.save(path)) qFatal("could not write %s", qPrintable(path));
    return path;
}

std::vector<double> pixel(const VImage &image, int x = 0, int y = 0) { return image.getpoint(x, y); }

// LCh chroma of an 8-bit sRGB pixel, for colour assertions.
double chromaOf(const std::vector<double> &rgb) {
    const VImage one = VImage::black(1, 1).new_from_image(rgb).cast(VIPS_FORMAT_UCHAR)
                           .copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    return one.colourspace(VIPS_INTERPRETATION_LCH).getpoint(0, 0)[1];
}

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
        editor.setRecipeStore(RecipeStore(m_dir.path() + "/recipes"));

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

    // ── Phase 1: recipe ────────────────────────────────────────────────
    void fullRecipeRoundTrips() {
        EditRecipe r;
        r.rotation = 90; r.flipHorizontal = true; r.straighten = -3.5;
        r.perspectiveVertical = 20; r.perspectiveHorizontal = -10;
        r.hasCrop = true; r.cropX = 0.1; r.cropY = 0.1; r.cropWidth = 0.7; r.cropHeight = 0.6;
        r.temperature = 25; r.tint = -10; r.exposure = 0.7;
        r.highlights = -40; r.shadows = 35; r.whites = 10; r.blacks = -15;
        r.brightness = 5; r.contrast = 12;
        r.levelsBlack = 0.05; r.levelsWhite = 0.95; r.levelsGamma = 1.2;
        r.curveMaster = {{0, 0}, {0.25, 0.2}, {0.75, 0.82}, {1, 1}};
        r.curveBlue = {{0, 0.05}, {1, 0.95}};
        r.vibrance = 30; r.saturation = 110;
        r.hsl[0] = {10, -20, 5}; r.hsl[5] = {-15, 40, -10};
        r.sharpenAmount = 80; r.sharpenRadius = 1.4; r.sharpenThreshold = 10;
        r.noiseLuminance = 20; r.noiseColor = 35;
        EditRecipe back;
        QVERIFY(EditRecipe::fromJson(QJsonDocument::fromJson(r.toJsonString().toUtf8()).object(), &back));
        QCOMPARE(back, r);
        QVERIFY(!r.geometryOnly().isIdentity());
        QCOMPARE(r.geometryOnly().exposure, 0.0);
        QCOMPARE(r.geometryOnly().straighten, -3.5);
    }

    void parametersMatchVariantMap() {
        EditRecipe r;
        const QVariantMap map = r.toVariantMap();
        // Every key the map exposes (except computed crop/rotation) can be set.
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (it.key().startsWith("crop") || it.key() == "rotation" || it.key() == "hasCrop") continue;
            QVERIFY2(r.setParameter(it.key(), it.value()), qPrintable(it.key()));
        }
        QCOMPARE(r, EditRecipe{});
        QVERIFY(!r.setParameter("noSuchThing", 1));
        QVERIFY(r.setParameter("exposure", 99));
        QCOMPARE(r.exposure, 5.0);  // clamped
        QVERIFY(r.setParameter("hsl.aqua.hue", -40));
        QCOMPARE(r.hsl[4].hue, -40.0);
        QVERIFY(r.setParameter("curve.red", QVariantList{QVariantList{0.0, 0.0}, QVariantList{0.5, 0.6}, QVariantList{1.0, 1.0}}));
        QCOMPARE(r.curveRed.size(), 3);
        // Levels can't cross.
        r.setParameter("levelsWhite", 0.3);
        r.setParameter("levelsBlack", 0.5);
        QVERIFY(r.levelsBlack < r.levelsWhite);
    }

    void curvesAreMonotonicAndHitTheirPoints() {
        const QList<QPointF> points{{0, 0}, {0.2, 0.05}, {0.3, 0.6}, {0.7, 0.65}, {1, 1}};
        double previous = -1;
        for (int i = 0; i <= 1000; ++i) {
            const double v = ImageEngine::curveValue(points, i / 1000.0);
            QVERIFY2(v >= previous - 1e-9, qPrintable(QString("dip at %1").arg(i / 1000.0)));
            previous = v;
        }
        for (const auto &p : points) QVERIFY(std::abs(ImageEngine::curveValue(points, p.x()) - p.y()) < 1e-9);
        QCOMPARE(ImageEngine::curveValue({}, 0.37), 0.37);
    }

    void toneControlsStayMonotonic() {
        const char *keys[] = {"shadows", "highlights", "whites", "blacks"};
        for (const char *key : keys) {
            for (double amount : {-100.0, 100.0}) {
                EditRecipe r;
                r.setParameter(key, amount);
                double previous = -1;
                for (int i = 0; i <= 256; ++i) {
                    const double v = ImageEngine::toneValue(r, -1, i / 256.0);
                    QVERIFY2(v >= previous - 1e-9, qPrintable(QString("%1 %2 at %3").arg(key).arg(amount).arg(i)));
                    previous = v;
                }
            }
        }
        EditRecipe lift; lift.shadows = 80;
        QVERIFY(ImageEngine::toneValue(lift, -1, 0.25) > 0.3);
        QCOMPARE(ImageEngine::toneValue(lift, -1, 1.0), 1.0);
        EditRecipe recover; recover.highlights = -80;
        QVERIFY(ImageEngine::toneValue(recover, -1, 0.75) < 0.7);
        QCOMPARE(ImageEngine::toneValue(recover, -1, 0.0), 0.0);
    }

    // ── Phase 1: light and colour ──────────────────────────────────────
    void exposureDoublesLinearLight() {
        // sRGB 118 ≈ 0.18 linear; +1 EV → 0.36 linear ≈ sRGB 161.
        const QString grey = writeSolid(m_dir.path(), "grey118.png", QColor(118, 118, 118));
        EditRecipe r; r.exposure = 1;
        const auto p = pixel(ImageEngine::apply(ImageEngine::loadOriginal(grey), r));
        QVERIFY2(std::abs(p[0] - 161) <= 2, qPrintable(QString::number(p[0])));
    }

    void whiteBalanceWarmsAndKeepsBrightness() {
        const QString grey = writeSolid(m_dir.path(), "grey.png", QColor(128, 128, 128));
        const VImage original = ImageEngine::loadOriginal(grey);
        EditRecipe warm; warm.temperature = 60;
        const auto p = pixel(ImageEngine::apply(original, warm));
        QVERIFY(p[0] > p[2] + 20);  // red up, blue down
        const double luma = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        QVERIFY2(std::abs(luma - 128) < 8, qPrintable(QString::number(luma)));
        EditRecipe magenta; magenta.tint = 60;
        const auto q = pixel(ImageEngine::apply(original, magenta));
        QVERIFY(q[1] < q[0] && q[1] < q[2]);
    }

    void vibranceFavoursMutedColours() {
        const QString muted = writeSolid(m_dir.path(), "muted.png", QColor(140, 120, 110));
        const QString vivid = writeSolid(m_dir.path(), "vivid.png", QColor(230, 30, 40));
        EditRecipe r; r.vibrance = 80;
        const double mutedBefore = chromaOf(pixel(ImageEngine::loadOriginal(muted)));
        const double mutedAfter = chromaOf(pixel(ImageEngine::apply(ImageEngine::loadOriginal(muted), r)));
        const double vividBefore = chromaOf(pixel(ImageEngine::loadOriginal(vivid)));
        const double vividAfter = chromaOf(pixel(ImageEngine::apply(ImageEngine::loadOriginal(vivid), r)));
        QVERIFY(mutedAfter / mutedBefore > 1.3);
        QVERIFY(vividAfter / vividBefore < mutedAfter / mutedBefore);
    }

    void hslBandsAreSelective() {
        const QString red = writeSolid(m_dir.path(), "red.png", QColor(210, 40, 40));
        const QString blue = writeSolid(m_dir.path(), "blue.png", QColor(40, 70, 220));
        EditRecipe r; r.hsl[0].saturation = -100;  // desaturate reds only
        const auto redOut = pixel(ImageEngine::apply(ImageEngine::loadOriginal(red), r));
        const auto blueOut = pixel(ImageEngine::apply(ImageEngine::loadOriginal(blue), r));
        QVERIFY(chromaOf(redOut) < chromaOf({210, 40, 40}) * 0.3);
        QVERIFY(std::abs(chromaOf(blueOut) - chromaOf({40, 70, 220})) < 3);
    }

    // ── Phase 1: geometry ──────────────────────────────────────────────
    void flipsMirror() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);  // red rises left → right
        EditRecipe h; h.flipHorizontal = true;
        QVERIFY(pixel(ImageEngine::apply(original, h), 0, 0)[0] > 250);
        EditRecipe v; v.flipVertical = true;  // green rises top → bottom
        QVERIFY(pixel(ImageEngine::apply(original, v), 0, 0)[1] > 250);
    }

    void straightenAndPerspectiveLeaveNoEmptyCorners() {
        const QString white = writeSolid(m_dir.path(), "white.png", Qt::white, {400, 300});
        const VImage original = ImageEngine::loadOriginal(white);
        for (int which = 0; which < 3; ++which) {
            EditRecipe r;
            if (which == 0) r.straighten = 12;
            if (which == 1) r.perspectiveVertical = 100;
            if (which == 2) { r.straighten = -7; r.perspectiveHorizontal = -80; }
            const VImage out = ImageEngine::apply(original, r);
            QCOMPARE(QSize(out.width(), out.height()), ImageEngine::geometryFrame({400, 300}, r));
            const int corners[4][2] = {{0, 0}, {out.width() - 1, 0}, {0, out.height() - 1}, {out.width() - 1, out.height() - 1}};
            for (const auto &c : corners)
                QVERIFY2(pixel(out, c[0], c[1])[0] > 240, qPrintable(QString("case %1 corner %2,%3").arg(which).arg(c[0]).arg(c[1])));
        }
        EditRecipe r; r.straighten = 10;
        const QSize frame = ImageEngine::geometryFrame({400, 300}, r);
        QVERIFY(frame.width() < 400 && frame.height() < 300);
        QVERIFY(std::abs(double(frame.width()) / frame.height() - 400.0 / 300.0) < 0.02);  // aspect kept
    }

    void regionRenderMatchesFullRender() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe r; r.rotation = 90; r.exposure = 0.3;
        const VImage full = ImageEngine::apply(original, r);
        ImageEngine::ApplyOptions options;
        options.region = QRectF(0.5, 0.25, 0.25, 0.5);
        const VImage part = ImageEngine::apply(original, r, options);
        QCOMPARE(part.width(), 75);
        QCOMPARE(part.height(), 200);
        QCOMPARE(pixel(part, 10, 10), pixel(full, 150 + 10, 100 + 10));
    }

    // ── Phase 1: detail, clipping, histogram, 16-bit ───────────────────
    void sharpenAndNoiseReduction() {
        // A soft vertical edge: sharpening should raise local contrast.
        QImage edge(120, 40, QImage::Format_RGB888);
        for (int x = 0; x < 120; ++x) for (int y = 0; y < 40; ++y) {
            const int v = std::clamp((x - 50) * 255 / 20, 0, 255);
            edge.setPixelColor(x, y, QColor(v, v, v));
        }
        const QString edgePath = m_dir.path() + "/edge.png";
        QVERIFY(edge.save(edgePath));
        EditRecipe sharp; sharp.sharpenAmount = 150; sharp.sharpenRadius = 2;
        const VImage original = ImageEngine::loadOriginal(edgePath);
        QVERIFY(ImageEngine::apply(original, sharp).deviate() > original.deviate());

        QImage noise(120, 120, QImage::Format_RGB888);
        std::mt19937 rng(7);
        std::uniform_int_distribution<int> jitter(-40, 40);
        for (int x = 0; x < 120; ++x) for (int y = 0; y < 120; ++y) {
            const int v = 128 + jitter(rng);
            noise.setPixelColor(x, y, QColor(v, std::clamp(v + jitter(rng), 0, 255), v));
        }
        const QString noisePath = m_dir.path() + "/noise.png";
        QVERIFY(noise.save(noisePath));
        EditRecipe smooth; smooth.noiseLuminance = 100; smooth.noiseColor = 100;
        const VImage noisy = ImageEngine::loadOriginal(noisePath);
        QVERIFY(ImageEngine::apply(noisy, smooth).deviate() < noisy.deviate() * 0.7);
    }

    void clippingOverlayMarksExtremes() {
        QImage image(2, 1, QImage::Format_RGB888);
        image.setPixelColor(0, 0, Qt::white);
        image.setPixelColor(1, 0, Qt::black);
        const QString path = m_dir.path() + "/extremes.png";
        QVERIFY(image.save(path));
        const VImage marked = ImageEngine::clippingOverlay(ImageEngine::loadOriginal(path));
        QCOMPARE(pixel(marked, 0, 0), (std::vector<double>{255, 54, 168}));
        QCOMPARE(pixel(marked, 1, 0), (std::vector<double>{53, 244, 255}));
        const QImage histogram = ImageEngine::histogramImage(ImageEngine::loadOriginal(m_fixture));
        QCOMPARE(histogram.size(), QSize(256, 90));
    }

    void sixteenBitStaysSixteenBit() {
        const VImage ramp = (VImage::xyz(256, 4)[0] * 256.0).cast(VIPS_FORMAT_USHORT)
                                .bandjoin(VImage::xyz(256, 4)[0] * 256.0).bandjoin(VImage::xyz(256, 4)[0] * 256.0)
                                .cast(VIPS_FORMAT_USHORT)
                                .copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_RGB16));
        const QString path = m_dir.path() + "/ramp16.png";
        ramp.write_to_file(path.toUtf8().constData());
        EditRecipe r; r.exposure = 0.5; r.saturation = 120; r.shadows = 20;
        const VImage out = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        QCOMPARE(out.format(), VIPS_FORMAT_USHORT);
        QVERIFY(out.max() > 60000);
        QVERIFY(out.avg() > ramp.avg());
    }

    // ── Phase 1: recipe store ──────────────────────────────────────────
    void recipeStoreRemembersPerPhoto() {
        RecipeStore store(m_dir.path() + "/store");
        const QString key = RecipeStore::keyForHash("ABCdef0123");
        QCOMPARE(key, QString("pp-abcdef0123"));
        EditRecipe r; r.exposure = 1.25; r.curveMaster = {{0, 0}, {0.5, 0.6}, {1, 1}};
        QVERIFY(store.save(key, r));
        EditRecipe back;
        QVERIFY(store.load(key, &back));
        QCOMPARE(back, r);
        QVERIFY(store.save(key, EditRecipe{}));  // identity forgets the photo
        QVERIFY(!store.load(key, &back));
        QVERIFY(RecipeStore::keyForHash("../../etc").startsWith("pp-etc"));
    }

    void editorHistoryRestoreZoomAndBefore() {
        PreviewProvider provider;
        const RecipeStore store(m_dir.path() + "/editor-recipes");
        {
            ImageEditor editor;
            editor.setPreviewProvider(&provider);
            editor.setRecipeStore(store);
            editor.openLocalFile(m_fixture);
            QTRY_VERIFY(editor.ready());
            QTRY_VERIFY(!editor.histogramUrl().isEmpty());
            QCOMPARE(provider.requestImage("histogram/1", nullptr, {}).size(), QSize(256, 90));

            editor.setParameter("exposure", 0.5);   // live → draft render
            QTRY_VERIFY(editor.status().startsWith("Draft"));
            editor.commitEdit();                    // released → full render
            QTRY_VERIFY(editor.status().startsWith("Preview"));
            editor.applyParameter("hsl.blue.saturation", 30, QString());
            editor.flip(true);
            QCOMPARE(editor.historyLabels(), (QStringList{"Original", "Exposure", "Blue saturation", "Flip horizontal"}));
            editor.jumpToHistory(1);
            QCOMPARE(editor.currentRecipe().exposure, 0.5);
            QVERIFY(!editor.currentRecipe().flipHorizontal);
            QVERIFY(editor.canRedo());
            editor.redo();
            editor.redo();

            editor.setShowBefore(true);
            QTRY_VERIFY(!editor.beforeUrl().isEmpty());
            QCOMPARE(provider.requestImage("before/x", nullptr, {}).size(), QSize(400, 300));

            editor.setZoom(true, 0.5, 0.5, 120, 80);
            QTRY_VERIFY(!editor.zoomUrl().isEmpty());
            QCOMPARE(provider.requestImage("zoom/x", nullptr, {}).size(), QSize(120, 80));
            editor.panZoom(-1000, 0);  // clamps at the right edge
            QTRY_VERIFY(editor.zoomRegion().right() > 0.999);
            editor.setZoom(false, 0, 0, 0, 0);
            QVERIFY(editor.zoomUrl().isEmpty());
            editor.cancel();
        }
        {
            // Reopening the same file restores the last committed edit.
            ImageEditor editor;
            editor.setRecipeStore(store);
            editor.openLocalFile(m_fixture);
            QCOMPARE(editor.currentRecipe().exposure, 0.5);
            QVERIFY(editor.currentRecipe().flipHorizontal);
            QCOMPARE(editor.historyLabels().first(), QString("Opened saved edit"));
            editor.resetEdits();  // identity: the store forgets the photo
            editor.cancel();
            EditRecipe none;
            QVERIFY(!store.load(RecipeStore::keyForPath(QFileInfo(m_fixture).absoluteFilePath()), &none));
        }
    }

    // ── Phase 2: recipe, looks ─────────────────────────────────────────
    void fxRecipeRoundTripsAndLooksSplit() {
        EditRecipe r;
        r.hasCrop = true; r.cropWidth = 0.5; r.straighten = 2;
        r.gradientAmount = 60; r.gradientName = "Neon"; r.gradientColors = {"05010a", "ff36a8", "35f4ff"};
        r.lutName = "teal.cube"; r.lutAmount = 70;
        r.bloomAmount = 40; r.bloomThreshold = 55; r.bloomRadius = 30; r.aberrationAmount = 20;
        r.glitchAmount = 35; r.glitchSeed = 9; r.pixelSortAmount = 25; r.pixelSortVertical = true;
        r.halftoneAmount = 50; r.halftoneSize = 12; r.scanlineAmount = 30; r.scanlineSpacing = 6;
        r.grainAmount = 20; r.grainSize = 2; r.vignetteAmount = -40; r.vignetteMidpoint = 30;
        EditRecipe::TextLayer t; t.text = "NEON <COFFIN> & co"; t.anchor = 4; t.glow = 50; t.color = "ff36a8";
        r.textLayers = {t};
        EditRecipe back;
        QVERIFY(EditRecipe::fromJson(QJsonDocument::fromJson(r.toJsonString().toUtf8()).object(), &back));
        QCOMPARE(back, r);
        QVERIFY(r.hasFx());

        // A look carries no geometry or text; applying keeps the target's.
        const QJsonObject look = r.lookJson();
        const QString lookText = QJsonDocument(look).toJson();
        QVERIFY(!lookText.contains("\"crop\"") && !lookText.contains("\"text\"") && !lookText.contains("straighten"));
        EditRecipe target; target.rotation = 90; EditRecipe::TextLayer mark; mark.text = "©"; target.textLayers = {mark};
        const EditRecipe styled = target.withLook(look);
        QCOMPARE(styled.rotation, 90);
        QCOMPARE(styled.textLayers.size(), 1);
        QCOMPARE(styled.textLayers.first().text, QString("©"));
        QCOMPARE(styled.bloomAmount, 40.0);
        QVERIFY(!styled.hasCrop);

        // Hostile input is cleaned up.
        // (An ordinary string: moc's lexer mis-reads this JSON as a raw string.)
        const auto hostile = QJsonDocument::fromJson(
            "{\"version\":1,\"ops\":["
            "{\"op\":\"lut\",\"name\":\"../../etc/passwd\"},"
            "{\"op\":\"gradientMap\",\"amount\":500,\"colors\":[\"#FF0000\",\"nothex\",\"00ff00\"]},"
            "{\"op\":\"text\",\"layers\":[{\"text\":\"x\",\"size\":9,\"anchor\":42,\"color\":\"zzz\",\"opacity\":7}]}]}").object();
        QVERIFY(EditRecipe::fromJson(hostile, &back));
        QCOMPARE(back.lutName, QString("passwd"));
        QCOMPARE(back.gradientAmount, 100.0);
        QCOMPARE(back.gradientColors, (QStringList{"ff0000", "00ff00"}));
        QCOMPARE(back.textLayers.first().size, 0.5);
        QCOMPARE(back.textLayers.first().anchor, 8);
        QCOMPARE(back.textLayers.first().color, QString("35f4ff"));
        QCOMPARE(back.textLayers.first().opacity, 1.0);
    }

    // ── Phase 2: effects ───────────────────────────────────────────────
    void gradientMapFollowsItsStops() {
        const VImage table = Fx::gradientTable({"000080", "ffff00"});
        QCOMPARE(table.width(), 256);
        QVERIFY(std::abs(pixel(table, 0)[2] - 128.0 / 255) < 0.01);
        QVERIFY(std::abs(pixel(table, 255)[0] - 1.0) < 0.01);
        QImage ramp(256, 4, QImage::Format_RGB888);
        for (int x = 0; x < 256; ++x) for (int y = 0; y < 4; ++y) ramp.setPixelColor(x, y, QColor(x, x, x));
        const QString path = m_dir.path() + "/gramp.png";
        QVERIFY(ramp.save(path));
        EditRecipe r; r.gradientAmount = 100; r.gradientColors = {"000080", "ffff00"};
        const VImage out = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        QVERIFY(std::abs(pixel(out, 0)[2] - 128) < 3 && pixel(out, 0)[0] < 3);   // dark → navy
        QVERIFY(pixel(out, 255)[0] > 250 && pixel(out, 255)[2] < 5);            // light → yellow
    }

    void cubeLutParsesAndApplies() {
        // A 2×2×2 inverting LUT.
        QByteArray cube = "TITLE \"invert\"\nLUT_3D_SIZE 2\n";
        for (int b = 0; b < 2; ++b) for (int g = 0; g < 2; ++g) for (int r = 0; r < 2; ++r)
            cube += QByteArray::number(1 - r) + " " + QByteArray::number(1 - g) + " " + QByteArray::number(1 - b) + "\n";
        CubeLut lut;
        QString error;
        QVERIFY2(CubeLut::parse(cube, &lut, &error), qPrintable(error));
        float px[3] = {0.25f, 0.5f, 1.0f};
        lut.apply(px, 1);
        QVERIFY(std::abs(px[0] - 0.75f) < 1e-5 && std::abs(px[1] - 0.5f) < 1e-5 && std::abs(px[2]) < 1e-5);
        QVERIFY(!CubeLut::parse("LUT_1D_SIZE 4\n0 0 0\n", &lut, &error));
        QVERIFY(!CubeLut::parse("LUT_3D_SIZE 2\n0 0 0\n", &lut, &error));
        QVERIFY(error.contains("Expected 8"));

        // Through the library and a render.
        const QString cubePath = m_dir.path() + "/invert.cube";
        QFile f(cubePath); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(cube); f.close();
        LutLibrary::setDirectory(m_dir.path() + "/luts");
        QCOMPARE(LutLibrary::import(cubePath, &error), QString("invert.cube"));
        QCOMPARE(LutLibrary::names(), QStringList{"invert.cube"});
        const QString solid = writeSolid(m_dir.path(), "lutin.png", QColor(40, 120, 200));
        EditRecipe r; r.lutName = "invert.cube";
        const auto p = pixel(ImageEngine::apply(ImageEngine::loadOriginal(solid), r));
        QVERIFY(std::abs(p[0] - 215) <= 2 && std::abs(p[1] - 135) <= 2 && std::abs(p[2] - 55) <= 2);
        r.lutName = "missing.cube";  // renders as no LUT, never fails
        QCOMPARE(pixel(ImageEngine::apply(ImageEngine::loadOriginal(solid), r)), (std::vector<double>{40, 120, 200}));
    }

    void effectsDoWhatTheySay() {
        // A dark frame with one bright square in the middle.
        QImage spot(200, 200, QImage::Format_RGB888);
        spot.fill(QColor(10, 10, 10));
        for (int x = 90; x < 110; ++x) for (int y = 90; y < 110; ++y) spot.setPixelColor(x, y, Qt::white);
        const QString spotPath = m_dir.path() + "/spot.png";
        QVERIFY(spot.save(spotPath));
        const VImage original = ImageEngine::loadOriginal(spotPath);

        EditRecipe bloom; bloom.bloomAmount = 100; bloom.bloomThreshold = 50; bloom.bloomRadius = 12;
        const VImage glowing = ImageEngine::apply(original, bloom);
        QVERIFY(pixel(glowing, 120, 100)[0] > pixel(original, 120, 100)[0] + 15);  // glow beside the square
        QVERIFY(pixel(glowing, 5, 5)[0] < 20);                                       // far corner stays dark

        EditRecipe vignette; vignette.vignetteAmount = -100;
        const VImage grey = ImageEngine::loadOriginal(writeSolid(m_dir.path(), "g200.png", QColor(200, 200, 200), {200, 200}));
        const VImage vig = ImageEngine::apply(grey, vignette);
        QVERIFY(pixel(vig, 0, 0)[0] < 60);
        QVERIFY(pixel(vig, 100, 100)[0] > 195);

        EditRecipe lines; lines.scanlineAmount = 100; lines.scanlineSpacing = 4;
        const VImage lined = ImageEngine::apply(grey, lines);
        QVERIFY(pixel(lined, 50, 0)[0] < pixel(lined, 50, 2)[0] - 50);

        EditRecipe aberration; aberration.aberrationAmount = 100;
        const VImage split = ImageEngine::apply(original, aberration);
        QCOMPARE(split.width(), 200);
        const auto edge = pixel(split, 110, 100);
        QVERIFY(std::abs(edge[0] - edge[2]) > 20);  // red and blue no longer line up at the edge

        EditRecipe halftone; halftone.halftoneAmount = 100; halftone.halftoneSize = 10;
        QVERIFY(ImageEngine::apply(ImageEngine::loadOriginal(m_fixture), halftone).deviate()
                > ImageEngine::loadOriginal(m_fixture).deviate());
    }

    // Regression: resizing a channel can round a pixel short in one dimension.
    void aberrationWorksAtAnySize() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        for (int amount : {1, 7, 15, 33, 100})
            for (double cw : {0.5, 0.37, 0.731, 0.999}) {
                EditRecipe r; r.aberrationAmount = amount;
                r.hasCrop = true; r.cropWidth = cw; r.cropHeight = 1.0 - cw / 3;
                const VImage out = ImageEngine::apply(original, r);
                QCOMPARE(QSize(out.width(), out.height()), QSize(qRound(400 * cw), qRound(300 * (1.0 - cw / 3))));
            }
    }

    void randomEffectsAreRepeatable() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe a; a.glitchAmount = 80; a.glitchSeed = 5; a.grainAmount = 60;
        const VImage one = ImageEngine::apply(original, a), two = ImageEngine::apply(original, a);
        QCOMPARE((one - two).abs().max(), 0.0);
        EditRecipe b = a; b.glitchSeed = 6;
        QVERIFY((one - ImageEngine::apply(original, b)).abs().max() > 0);
        QCOMPARE(one.width(), 400);
        EditRecipe grainOnly; grainOnly.grainAmount = 50;
        QVERIFY(std::abs(ImageEngine::apply(original, grainOnly).avg() - original.avg()) < 2);
    }

    void pixelSortOrdersBrightRuns() {
        QImage noise(64, 16, QImage::Format_RGB888);
        std::mt19937 rng(3);
        std::uniform_int_distribution<int> value(140, 255);
        for (int x = 0; x < 64; ++x) for (int y = 0; y < 16; ++y) { const int v = value(rng); noise.setPixelColor(x, y, QColor(v, v, v)); }
        const QString path = m_dir.path() + "/sortme.png";
        QVERIFY(noise.save(path));
        EditRecipe r; r.pixelSortAmount = 100;  // threshold 0.1: everything sorts
        const VImage sorted = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        for (int y = 0; y < 16; ++y)
            for (int x = 1; x < 64; ++x) QVERIFY(pixel(sorted, x, y)[0] >= pixel(sorted, x - 1, y)[0] - 1);
    }

    void textLayersLandWhereAnchored() {
        const VImage black = ImageEngine::loadOriginal(writeSolid(m_dir.path(), "black.png", Qt::black, {400, 200}));
        EditRecipe r;
        EditRecipe::TextLayer t; t.text = "GHOST"; t.color = "ff36a8"; t.size = 0.2; t.anchor = 8; t.margin = 0.02;
        r.textLayers = {t};
        const VImage out = ImageEngine::apply(black, r);
        // Pink appears in the bottom-right quarter and nowhere in the top-left.
        QVERIFY(out.extract_area(200, 100, 200, 100)[0].max() > 200);
        QCOMPARE(out.extract_area(0, 0, 200, 100).max(), 0.0);
        EditRecipe empty; EditRecipe::TextLayer blank; blank.text = "   "; empty.textLayers = {blank};
        QCOMPARE(ImageEngine::apply(black, empty).max(), 0.0);
    }

    void regionMatchesFullRenderWithFx() {
        const VImage original = ImageEngine::loadOriginal(m_fixture);
        EditRecipe r; r.vignetteAmount = -60; r.glitchAmount = 50; r.scanlineAmount = 40; r.bloomAmount = 30;
        EditRecipe::TextLayer t; t.text = "ZOOM"; t.anchor = 4; r.textLayers = {t};
        const VImage full = ImageEngine::apply(original, r);
        ImageEngine::ApplyOptions options;
        options.region = QRectF(0.25, 0.25, 0.5, 0.5);
        const VImage part = ImageEngine::apply(original, r, options);
        QCOMPARE((part - full.extract_area(100, 75, 200, 150)).abs().max(), 0.0);
    }

    void greyImagesBecomeColourForFx() {
        QImage g(32, 32, QImage::Format_Grayscale8);
        g.fill(128);
        const QString path = m_dir.path() + "/grey8.png";
        QVERIFY(g.save(path));
        EditRecipe r; r.gradientAmount = 100; r.gradientColors = {"000000", "ff0000"};
        const VImage out = ImageEngine::apply(ImageEngine::loadOriginal(path), r);
        QCOMPARE(out.bands(), 3);
        QVERIFY(pixel(out)[0] > 100 && pixel(out)[1] < 10);
    }

    // ── Phase 2: export presets ────────────────────────────────────────
    void exportPresetsSizeAndFormat() {
        EditRecipe r;
        QString error;
        auto exportAs = [&](const QString &name, ImageEngine::ExportOptions o) {
            const QString out = m_dir.path() + "/" + name;
            if (!ImageEngine::exportImage(m_fixture, r, out, o, &error)) qWarning("%s", qPrintable(error));
            return out;
        };
        ImageEngine::ExportOptions shorts; shorts.size = {1080, 1920};
        QCOMPARE(ImageEngine::sourceSize(exportAs("shorts.jpg", shorts)), QSize(1080, 1920));
        shorts.fit = ImageEngine::ExportOptions::FitBlur;
        QCOMPARE(ImageEngine::sourceSize(exportAs("shorts-blur.webp", shorts)), QSize(1080, 1920));
        ImageEngine::ExportOptions padded; padded.size = {300, 300}; padded.fit = ImageEngine::ExportOptions::FitPad; padded.padColor = "ff0000";
        const VImage pad = ImageEngine::loadOriginal(exportAs("padded.png", padded));
        QCOMPARE(QSize(pad.width(), pad.height()), QSize(300, 300));
        QCOMPARE(pixel(pad, 150, 2), (std::vector<double>{255, 0, 0}));  // bar above the 300×225 image
        ImageEngine::ExportOptions web; web.quality = 60;
        const QString avif = exportAs("web.avif", web);
        QVERIFY2(QFileInfo(avif).size() > 0, qPrintable(error));
        QCOMPARE(ImageEngine::sourceSize(avif), QSize(400, 300));
    }

    // ── Phase 2: CYBERGRID and looks ───────────────────────────────────
    void cybergridPalettesAndGradients() {
        QTemporaryDir config;
        QDir().mkpath(config.path() + "/themes");
        QFile custom(config.path() + "/themes/my-neon.json");
        QVERIFY(custom.open(QIODevice::WriteOnly));
        custom.write(R"({"id":"my-neon","palette":{"bg":"000000","white":"ffffff","acid_green":"00ff00","hot_pink":"ff00ff","purple":"440088","cyan":"00ffff","orange":"ff8800","red":"ff0000","panel":"111111","line":"222222","muted":"777777"}})");
        custom.close();
        qputenv("CYBERCORE_CONFIG_DIR", config.path().toUtf8());
        const CyberGrid grid;
        qunsetenv("CYBERCORE_CONFIG_DIR");
        QVERIFY(grid.themes().size() >= 82);
        QCOMPARE(grid.gradient("my-neon", "neon"), (QStringList{"000000", "440088", "ff00ff", "00ffff", "ffffff"}));
        QCOMPARE(grid.gradient("cyberdyne-hunter", "duo-pink").size(), 2);
        QVERIFY(grid.gradient("nope", "neon").isEmpty());
        QVERIFY(grid.styles().size() >= 6);
    }

    void lookLibraryBuiltInsAndYours() {
        const LookLibrary looks(m_dir.path() + "/looks");
        QStringList names;
        for (const auto &l : looks.list()) names.append(l.toMap().value("name").toString());
        for (const char *expected : {"Neon Noir", "Neon Coffin", "Blackbox Terminal", "Synthwave Sunset", "Ghost Signal"})
            QVERIFY2(names.contains(expected), expected);
        // Every built-in parses and has an effect.
        for (const QString &name : names) {
            EditRecipe r;
            QVERIFY2(EditRecipe::fromJson(looks.recipe(name), &r), qPrintable(name));
            QVERIFY2(!r.isIdentity(), qPrintable(name));
        }
        EditRecipe mine; mine.bloomAmount = 77;
        QVERIFY(looks.save("Neon Noir", mine.lookJson()));  // yours shadows the built-in
        EditRecipe loaded;
        QVERIFY(EditRecipe::fromJson(looks.recipe("Neon Noir"), &loaded));
        QCOMPARE(loaded.bloomAmount, 77.0);
        QVERIFY(looks.remove("Neon Noir"));
        QVERIFY(EditRecipe::fromJson(looks.recipe("Neon Noir"), &loaded));
        QVERIFY(loaded.bloomAmount != 77.0);  // built-in again
    }

    void editorLooksTextAndPresetExport() {
        ImageEditor editor;
        editor.setRecipeStore(RecipeStore(m_dir.path() + "/p2-recipes"));
        editor.setLookLibrary(LookLibrary(m_dir.path() + "/p2-looks"));
        const QString exports = m_dir.path() + "/p2-exports";
        editor.setExportDirectory(exports);
        editor.openLocalFile(m_fixture);
        QTRY_VERIFY(editor.ready());
        editor.setCrop(0, 0, 0.5, 1);
        editor.applyLook("Blackbox Terminal");
        QVERIFY(editor.currentRecipe().hasCrop);
        QVERIFY(editor.currentRecipe().scanlineAmount > 0);
        editor.addTextLayer("NEON COFFIN");
        editor.setTextLayer(0, "anchor", 4, true);
        QCOMPARE(editor.currentRecipe().textLayers.first().anchor, 4);
        QCOMPARE(editor.historyLabels().mid(1), (QStringList{"Crop", "Look: Blackbox Terminal", "Add text", "Text anchor"}));
        editor.removeTextLayer(0);
        QVERIFY(editor.currentRecipe().textLayers.isEmpty());

        QSignalSpy saved(&editor, &ImageEditor::saved);
        editor.save("t", "", "", QVariantMap{{"preset", "Shorts"}, {"width", 1080}, {"height", 1920}, {"fit", "blur"}, {"format", "webp"}, {"quality", 80}});
        QTRY_VERIFY2_WITH_TIMEOUT(saved.count() == 1, qPrintable(editor.status()), 15000);
        const QString path = saved.first().first().toString();
        QVERIFY(path.contains("-Ghostframe-shorts-") && path.endsWith(".webp"));
        QCOMPARE(ImageEngine::sourceSize(path), QSize(1080, 1920));
        editor.cancel();
    }

    // ── Phase 3: cybermeta ─────────────────────────────────────────────
    void cybermetaSummary() {
        if (!Cybermeta::available()) QSKIP("cybermeta 0.2+ not installed");
        if (!haveExiftool()) QSKIP("exiftool not installed");
        const QString photo = writeCameraJpeg(m_dir.path(), "camera.jpg");
        QString error;
        const QVariantMap summary = Cybermeta::summary(photo, &error);
        QVERIFY2(!summary.isEmpty(), qPrintable(error));
        QCOMPARE(summary.value("camera").toString(), QString("Fujifilm X-T5"));
        QCOMPARE(summary.value("shutter").toString(), QString("1/250 s"));
        QCOMPARE(summary.value("iso").toInt(), 400);
        const QVariantMap gps = summary.value("gps").toMap();
        QVERIFY(std::abs(gps.value("latitude").toDouble() - 45.5152) < 1e-3);
        QVERIFY(gps.value("longitude").toDouble() < 0);
        QVERIFY(gps.value("map_url").toString().startsWith("https://www.openstreetmap.org/"));
    }

    void exportPrivacyAndEmbeddedMetadata_data() {
        QTest::addColumn<QString>("privacy");
        QTest::addColumn<QString>("format");
        QTest::addColumn<bool>("gps");
        QTest::addColumn<QString>("make");
        QTest::newRow("keep") << "keep" << "jpg" << true << "Fujifilm";
        QTest::newRow("strip gps (default)") << "gps" << "jpg" << false << "Fujifilm";
        QTest::newRow("anonymous") << "anonymous" << "jpg" << false << "Generic";
        QTest::newRow("strip all") << "all" << "jpg" << false << "";
        QTest::newRow("webp strip gps") << "gps" << "webp" << false << "Fujifilm";
        QTest::newRow("png strip gps") << "gps" << "png" << false << "Fujifilm";
    }

    void exportPrivacyAndEmbeddedMetadata() {
        QFETCH(QString, privacy);
        QFETCH(QString, format);
        QFETCH(bool, gps);
        QFETCH(QString, make);
        if (!Cybermeta::available()) QSKIP("cybermeta 0.2+ not installed");
        if (!haveExiftool()) QSKIP("exiftool not installed");
        const QString photo = writeCameraJpeg(m_dir.path(), "privacy-src.jpg");
        ImageEditor editor;
        editor.setRecipeStore(RecipeStore(m_dir.path() + "/p3-recipes"));
        const QString exports = m_dir.path() + "/p3-" + privacy + "-" + format;
        editor.setExportDirectory(exports);
        editor.openLocalFile(photo);
        QTRY_VERIFY(editor.ready());
        editor.setParameter("exposure", 0.3);
        editor.commitEdit();
        QSignalSpy saved(&editor, &ImageEditor::saved);
        editor.save("Harrow Point", "Late returns", "neon, night", QVariantMap{{"privacy", privacy}, {"format", format}});
        QTRY_VERIFY2_WITH_TIMEOUT(saved.count() == 1, qPrintable(editor.status()), 20000);
        const QString out = saved.first().first().toString();

        // Camera metadata, checked with exiftool.
        const QString tags = exiftool({"-s", "-GPSLatitude", "-Make", "-ImageDescription", "-Orientation", out});
        QCOMPARE(tags.contains("GPSLatitude"), gps);
        if (make.isEmpty()) QVERIFY2(!tags.contains("Make"), qPrintable(tags));
        else QVERIFY2(tags.contains(": " + make), qPrintable(tags));
        QVERIFY2(tags.contains("Late returns"), qPrintable(tags));  // caption → EXIF ImageDescription, in every mode

        // Embedded XMP (read back by cybermeta), and the sidecar.
        QProcess read;
        read.start(Cybermeta::executable(), {"xmp", "read", out});
        QVERIFY(read.waitForFinished(20000));
        const QJsonObject embedded = QJsonDocument::fromJson(read.readAllStandardOutput()).object();
        QCOMPARE(embedded.value("title").toString(), QString("Harrow Point"));
        QCOMPARE(embedded.value("keywords").toArray().size(), 2);
        QVERIFY(embedded.value("recipe").toObject().contains("ops"));
        // Source identity only travels inside the file when keeping everything.
        QCOMPARE(embedded.value("fields").toObject().contains("sourceFolder"), privacy == "keep");
        QVERIFY(QFileInfo::exists(out + ".xmp"));
        QVERIFY(QFileInfo::exists(out + ".xmp") && QFile(out + ".xmp").size() > 0);
        read.start(Cybermeta::executable(), {"xmp", "read", out + ".xmp"});
        QVERIFY(read.waitForFinished(20000));
        const QJsonObject side = QJsonDocument::fromJson(read.readAllStandardOutput()).object();
        QCOMPARE(side.value("fields").toObject().value("privacy").toString(), privacy);
        QVERIFY(side.value("fields").toObject().contains("sourceFilename"));
        QVERIFY(saved.first().at(1).toString().contains(Cybermeta::privacyLabel(Cybermeta::privacyFromString(privacy))));
        editor.cancel();
    }

    void exportKeepsOrientationRight() {
        if (!Cybermeta::available()) QSKIP("cybermeta 0.2+ not installed");
        if (!haveExiftool()) QSKIP("exiftool not installed");
        const QString photo = writeCameraJpeg(m_dir.path(), "rotated.jpg", 6);  // shown rotated 90° clockwise
        QCOMPARE(ImageEngine::sourceSize(photo), QSize(200, 300));
        for (const char *privacy : {"keep", "gps"}) {
            ImageEditor editor;
            editor.setRecipeStore(RecipeStore(m_dir.path() + "/p3o-recipes"));
            editor.setExportDirectory(m_dir.path() + "/p3-orientation-" + privacy);
            editor.openLocalFile(photo);
            QTRY_VERIFY(editor.ready());
            QSignalSpy saved(&editor, &ImageEditor::saved);
            editor.save("", "", "", QVariantMap{{"privacy", privacy}});
            QTRY_VERIFY2_WITH_TIMEOUT(saved.count() == 1, qPrintable(editor.status()), 20000);
            const QString out = saved.first().first().toString();
            // Pixels are upright and nothing asks viewers to rotate them again.
            const VImage raw = VImage::new_from_file(out.toUtf8().constData());
            QCOMPARE(QSize(raw.width(), raw.height()), QSize(200, 300));
            QCOMPARE(ImageEngine::sourceSize(out), QSize(200, 300));
            QVERIFY(pixel(raw, 190, 5)[0] > 200);  // the red marker is now top-right
            editor.cancel();
        }
    }

    void exportWithoutCybermetaNeverLeaksLocation() {
        if (!haveExiftool()) QSKIP("exiftool not installed");
        const QString photo = writeCameraJpeg(m_dir.path(), "nocm.jpg");
        qputenv("GHOSTFRAME_CYBERMETA", "/nonexistent/cybermeta");
        QVERIFY(!Cybermeta::available());
        ImageEditor editor;
        editor.setRecipeStore(RecipeStore(m_dir.path() + "/p3n-recipes"));
        editor.setExportDirectory(m_dir.path() + "/p3-nocm");
        editor.openLocalFile(photo);
        QTRY_VERIFY(editor.ready());
        QTRY_VERIFY(!editor.metadataStatus().isEmpty());  // explains why there's no metadata panel
        QSignalSpy saved(&editor, &ImageEditor::saved);
        editor.save("t", "", "", QVariantMap{{"privacy", "gps"}});
        QTRY_VERIFY2_WITH_TIMEOUT(saved.count() == 1, qPrintable(editor.status()), 20000);
        const QString out = saved.first().first().toString();
        qunsetenv("GHOSTFRAME_CYBERMETA");
        QVERIFY(!exiftool({"-s", "-GPSLatitude", "-Make", out}).contains("GPS"));
        QVERIFY(saved.first().at(1).toString().contains("install cybermeta"));
        QVERIFY(!QFileInfo::exists(out + ".xmp"));
        editor.cancel();
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
    // Keep every standard location (recipes, settings) out of the user's
    // real directories.
    QStandardPaths::setTestModeEnabled(true);
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
