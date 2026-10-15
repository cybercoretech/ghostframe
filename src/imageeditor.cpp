#include "imageeditor.h"

#include "cubelut.h"
#include "imageengine.h"
#include "previewprovider.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QXmlStreamWriter>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr QSize kProxySize{1600, 1200};  // full-quality preview
constexpr int kDraftWidth = 800;         // while a slider is being dragged
}  // namespace

ImageEditor::ImageEditor(QObject *parent)
    : QObject(parent),
      m_exportDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/Ghostframe/Exports") {
    // Coalesce bursts of slider events; rendering itself is off-thread.
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(16);
    connect(&m_previewTimer, &QTimer::timeout, this, &ImageEditor::startRender);
    m_zoomTimer.setSingleShot(true);
    m_zoomTimer.setInterval(30);
    connect(&m_zoomTimer, &QTimer::timeout, this, &ImageEditor::startZoomRender);

    connect(&m_openWatcher, &QFutureWatcher<OpenResult>::finished, this, [this] {
        const OpenResult result = m_openWatcher.result();
        if (result.session != m_session) return;
        setBusy(false);
        if (!result.ok) { setStatus("Could not open this image: " + result.error); return; }
        m_proxy = result.proxy;
        m_draft = result.draft;
        m_sourceSize = result.sourceSize;
        m_ready = true;
        emit readyChanged();
        emit recipeChanged();  // frame size is known now
        setStatus(QString("Original: %1 × %2 px").arg(m_sourceSize.width()).arg(m_sourceSize.height()));
        startRender();
    });

    connect(&m_renderWatcher, &QFutureWatcher<RenderResult>::finished, this, [this] {
        const RenderResult result = m_renderWatcher.result();
        if (result.session == m_session && m_ready) {
            if (!result.ok) {
                setStatus("Could not render the edit preview: " + result.error);
            } else {
                ++m_revision;
                if (m_provider) {
                    m_provider->setImage("preview", result.preview);
                    m_provider->setImage("histogram", result.histogram);
                    m_provider->setImage("before", result.before);
                }
                const QString rev = QString::number(m_revision);
                m_previewUrl = QUrl("image://ghostframe-edit/preview/" + rev);
                m_histogramUrl = QUrl("image://ghostframe-edit/histogram/" + rev);
                m_beforeUrl = result.before.isNull() ? QUrl() : QUrl("image://ghostframe-edit/before/" + rev);
                m_displaySize = result.size;
                m_lastRenderMs = result.milliseconds;
                emit previewChanged();
                setStatus(QString("%1 %2 ms · %3 × %4 px output")
                              .arg(result.draft ? "Draft" : "Preview").arg(result.milliseconds)
                              .arg(m_displaySize.width()).arg(m_displaySize.height()));
            }
        }
        if (m_renderPending) { m_renderPending = false; startRender(); }
    });

    connect(&m_zoomWatcher, &QFutureWatcher<ZoomResult>::finished, this, [this] {
        const ZoomResult result = m_zoomWatcher.result();
        if (result.session == m_session && m_ready) {
            if (m_original.is_null() && !result.original.is_null()) m_original = result.original;
            if (!result.ok) {
                setStatus("Could not render 1:1 zoom: " + result.error);
            } else if (m_zoomed) {
                if (m_provider) m_provider->setImage("zoom", result.image);
                m_zoomUrl = QUrl("image://ghostframe-edit/zoom/" + QString::number(++m_revision));
                m_zoomRegion = result.region;
                emit zoomChanged();
            }
        }
        if (m_zoomPending) { m_zoomPending = false; startZoomRender(); }
    });

    connect(&m_exportWatcher, &QFutureWatcher<ExportResult>::finished, this, [this] {
        const ExportResult result = m_exportWatcher.result();
        setBusy(false);
        if (result.session != m_session) { QFile::remove(result.stagingPath); return; }
        if (!result.ok) {
            QFile::remove(result.stagingPath);
            setStatus(result.error.isEmpty() ? "Could not render this export." : result.error.left(220));
            return;
        }
        if (!QFile::rename(result.stagingPath, result.path)) {
            QFile::remove(result.stagingPath);
            setStatus("Could not finalize the export in the Ghostframe Exports folder.");
            return;
        }
        if (!writeCybermeta(result.path, m_pendingTitle, m_pendingCaption, m_pendingTags)) {
            setStatus("The image exported, but its Cybermeta XMP sidecar could not be written.");
            emit saved(result.path);
            return;
        }
        setStatus("Export and Cybermeta saved to " + QFileInfo(result.path).absolutePath() + ".");
        emit saved(result.path);
    });
}

ImageEditor::~ImageEditor() {
    // Workers hold no pointer to this object, but libvips must outlive them.
    m_openWatcher.waitForFinished();
    m_renderWatcher.waitForFinished();
    m_zoomWatcher.waitForFinished();
    m_exportWatcher.waitForFinished();
}

void ImageEditor::setBusy(bool value) { if (m_busy != value) { m_busy = value; emit busyChanged(); } }
void ImageEditor::setStatus(const QString &value) { if (m_status != value) { m_status = value; emit statusChanged(); } }

QString ImageEditor::labelFor(const QString &key) {
    static const QHash<QString, QString> labels{
        {"straighten", "Straighten"}, {"perspectiveVertical", "Vertical perspective"},
        {"perspectiveHorizontal", "Horizontal perspective"}, {"temperature", "Temperature"}, {"tint", "Tint"},
        {"exposure", "Exposure"}, {"highlights", "Highlights"}, {"shadows", "Shadows"}, {"whites", "Whites"},
        {"blacks", "Blacks"}, {"brightness", "Light"}, {"contrast", "Contrast"}, {"levelsBlack", "Levels black"},
        {"levelsWhite", "Levels white"}, {"levelsGamma", "Levels gamma"}, {"vibrance", "Vibrance"},
        {"saturation", "Saturation"}, {"sharpenAmount", "Sharpen"}, {"sharpenRadius", "Sharpen radius"},
        {"sharpenThreshold", "Sharpen threshold"}, {"noiseLuminance", "Luminance noise"}, {"noiseColor", "Colour noise"},
        {"gradientAmount", "Gradient map"}, {"lutAmount", "LUT strength"}, {"bloomAmount", "Bloom"},
        {"bloomThreshold", "Bloom threshold"}, {"bloomRadius", "Bloom radius"}, {"aberrationAmount", "Chromatic aberration"},
        {"glitchAmount", "Glitch"}, {"glitchSeed", "Glitch seed"}, {"pixelSortAmount", "Pixel sort"},
        {"pixelSortVertical", "Pixel sort direction"}, {"halftoneAmount", "Halftone"}, {"halftoneSize", "Halftone size"},
        {"scanlineAmount", "Scanlines"}, {"scanlineSpacing", "Scanline spacing"}, {"grainAmount", "Grain"},
        {"grainSize", "Grain size"}, {"vignetteAmount", "Vignette"}, {"vignetteMidpoint", "Vignette midpoint"},
        {"lutName", "LUT"}, {"gradientColors", "Gradient map"}, {"textLayers", "Text"},
        {"curve.master", "Curve"}, {"curve.red", "Red curve"}, {"curve.green", "Green curve"}, {"curve.blue", "Blue curve"},
    };
    if (labels.contains(key)) return labels.value(key);
    if (key.startsWith("hsl.")) {
        const QStringList parts = key.split('.');
        if (parts.size() == 3 && !parts[1].isEmpty()) {
            QString band = parts[1];
            band[0] = band[0].toUpper();
            return band + " " + parts[2];
        }
    }
    return "Edit";
}

int ImageEditor::frameWidth() const {
    return m_sourceSize.isValid() ? ImageEngine::geometryFrame(m_sourceSize, m_recipe).width() : 0;
}
int ImageEditor::frameHeight() const {
    return m_sourceSize.isValid() ? ImageEngine::geometryFrame(m_sourceSize, m_recipe).height() : 0;
}

QSize ImageEditor::outputSize(const EditRecipe &r) const {
    QSize size = ImageEngine::geometryFrame(m_sourceSize, r);
    if (r.hasCrop) size = QSize(qMax(1, qRound(size.width() * r.cropWidth)), qMax(1, qRound(size.height() * r.cropHeight)));
    return size;
}

void ImageEditor::openOriginal(const QString &path, const QString &title, const QString &hash, const QString &folder, const QString &originalName) {
    cancel();
    m_sourcePath = path; m_title = title; m_hash = hash; m_folder = folder; m_originalName = originalName;
    m_ownsSource = true;
    m_storeKey = RecipeStore::keyForHash(hash);
    EditRecipe stored;
    if (m_store.load(m_storeKey, &stored) && !stored.isIdentity()) {
        m_recipe = stored;
        m_history.reset(stored, "Opened saved edit");
    } else {
        m_recipe = {};
        m_history.reset(m_recipe);
    }
    emit sourceChanged(); emit recipeChanged(); emit historyChanged();
    if (!QFileInfo::exists(path)) { setStatus("The temporary full-resolution working copy is missing."); return; }
    setBusy(true);
    setStatus("Preparing image…");
    const int session = m_session;
    m_openWatcher.setFuture(QtConcurrent::run([session, path] {
        OpenResult result;
        result.session = session;
        try {
            result.sourceSize = ImageEngine::sourceSize(path);
            result.proxy = ImageEngine::loadProxy(path, kProxySize);
            result.draft = result.proxy.width() > kDraftWidth
                               ? result.proxy.thumbnail_image(kDraftWidth).copy_memory()
                               : result.proxy;
            result.ok = true;
        } catch (const vips::VError &e) {
            result.error = QString::fromUtf8(e.what()).trimmed();
        }
        return result;
    }));
}

void ImageEditor::openLocalFile(const QString &path) {
    const QFileInfo info(path);
    openOriginal(info.absoluteFilePath(), info.completeBaseName(), QString(), info.absolutePath(), info.fileName());
    m_ownsSource = false;
    m_storeKey = RecipeStore::keyForPath(info.absoluteFilePath());
    EditRecipe stored;
    if (m_store.load(m_storeKey, &stored) && !stored.isIdentity()) {
        m_recipe = stored;
        m_history.reset(stored, "Opened saved edit");
        emit recipeChanged(); emit historyChanged();
    }
}

void ImageEditor::schedulePreview() {
    if (!m_ready) return;
    m_previewTimer.start();
    if (m_zoomed) m_zoomTimer.start();
}

void ImageEditor::startRender() {
    if (!m_ready) return;
    if (m_renderWatcher.isRunning()) { m_renderPending = true; return; }
    if (!m_recipe.lutName.isEmpty()) {
        QString error;
        if (!LutLibrary::load(m_recipe.lutName, &error)) setStatus(error + " (rendering without it)");
    }
    const int session = m_session;
    // An uncommitted change means a slider is moving: render the draft.
    const bool draft = m_recipe != m_history.current() && !m_draft.is_null();
    const vips::VImage image = draft ? m_draft : m_proxy;
    EditRecipe recipe = m_recipe;
    if (m_cropMode) recipe.hasCrop = false;  // show the whole frame while cropping
    const QSize size = outputSize(recipe);
    const double scale = double(image.width()) / qMax(1, m_sourceSize.width());
    const bool clipping = m_showClipping, before = m_showBefore;
    m_renderWatcher.setFuture(QtConcurrent::run([=] {
        RenderResult result;
        result.session = session;
        result.draft = draft;
        result.size = size;
        QElapsedTimer timer;
        timer.start();
        try {
            ImageEngine::ApplyOptions options;
            options.scale = scale;
            const vips::VImage out = ImageEngine::apply(image, recipe, options).copy_memory();
            result.histogram = ImageEngine::histogramImage(out);
            result.preview = ImageEngine::toQImage(clipping ? ImageEngine::clippingOverlay(out) : out);
            if (before) result.before = ImageEngine::toQImage(ImageEngine::apply(image, recipe.geometryOnly(), options));
            result.ok = true;
        } catch (const vips::VError &e) {
            result.error = QString::fromUtf8(e.what()).trimmed();
        }
        result.milliseconds = int(timer.elapsed());
        return result;
    }));
}

void ImageEditor::startZoomRender() {
    if (!m_ready || !m_zoomed) return;
    if (m_zoomWatcher.isRunning()) { m_zoomPending = true; return; }
    const QSize out = outputSize(m_recipe);
    const double w = qMin(1.0, double(m_zoomViewport.width()) / out.width());
    const double h = qMin(1.0, double(m_zoomViewport.height()) / out.height());
    m_zoomCenter.setX(qBound(w / 2, m_zoomCenter.x(), 1 - w / 2));
    m_zoomCenter.setY(qBound(h / 2, m_zoomCenter.y(), 1 - h / 2));
    const QRectF region(m_zoomCenter.x() - w / 2, m_zoomCenter.y() - h / 2, w, h);
    const int session = m_session;
    const vips::VImage original = m_original;
    const QString path = m_sourcePath;
    const EditRecipe recipe = m_recipe;
    const bool clipping = m_showClipping;
    if (original.is_null()) setStatus("Loading full resolution for 1:1…");
    m_zoomWatcher.setFuture(QtConcurrent::run([=] {
        ZoomResult result;
        result.session = session;
        result.region = region;
        try {
            // Decoded once per photo and kept, so panning stays quick.
            result.original = !original.is_null() ? original : ImageEngine::loadOriginal(path).copy_memory();
            ImageEngine::ApplyOptions options;
            options.region = region;
            const vips::VImage pixels = ImageEngine::apply(result.original, recipe, options);
            result.image = ImageEngine::toQImage(clipping ? ImageEngine::clippingOverlay(pixels) : pixels);
            result.ok = true;
        } catch (const vips::VError &e) {
            result.error = QString::fromUtf8(e.what()).trimmed();
        }
        return result;
    }));
}

void ImageEditor::setZoom(bool on, double centerX, double centerY, int viewportWidth, int viewportHeight) {
    if (!m_ready || m_cropMode) on = false;
    m_zoomed = on;
    if (on) {
        m_zoomCenter = QPointF(qBound(0.0, centerX, 1.0), qBound(0.0, centerY, 1.0));
        m_zoomViewport = QSize(qMax(16, viewportWidth), qMax(16, viewportHeight));
        startZoomRender();
    } else {
        m_zoomUrl = QUrl();
        m_zoomRegion = {};
        if (m_provider) m_provider->setImage("zoom", {});
    }
    emit zoomChanged();
}

void ImageEditor::panZoom(double dxPixels, double dyPixels) {
    if (!m_zoomed) return;
    const QSize out = outputSize(m_recipe);
    m_zoomCenter -= QPointF(dxPixels / qMax(1, out.width()), dyPixels / qMax(1, out.height()));
    m_zoomTimer.start();
}

void ImageEditor::setCropMode(bool value) {
    if (!m_ready) value = false;
    if (m_cropMode == value) return;
    m_cropMode = value;
    if (value && m_zoomed) setZoom(false, 0, 0, 0, 0);
    emit viewOptionsChanged();
    schedulePreview();
}

QVariantList ImageEditor::curveSamples(const QString &key, int count) const {
    const QVariantMap map = m_recipe.toVariantMap();
    QList<QPointF> points;
    for (const QVariant &item : map.value(key).toList()) {
        const QVariantList pair = item.toList();
        if (pair.size() == 2) points.append(QPointF(pair[0].toDouble(), pair[1].toDouble()));
    }
    QVariantList samples;
    count = qBound(2, count, 1024);
    for (int i = 0; i < count; ++i) samples.append(ImageEngine::curveValue(points, double(i) / (count - 1)));
    return samples;
}

void ImageEditor::setShowClipping(bool value) {
    if (m_showClipping == value) return;
    m_showClipping = value;
    emit viewOptionsChanged();
    schedulePreview();
}

void ImageEditor::setShowBefore(bool value) {
    if (m_showBefore == value) return;
    m_showBefore = value;
    emit viewOptionsChanged();
    schedulePreview();
}

void ImageEditor::persist() {
    // Remember the committed state for this photo (identity removes it).
    m_store.save(m_storeKey, m_history.current());
}

void ImageEditor::setRecipe(const EditRecipe &recipe, bool commit, const QString &label) {
    const EditRecipe before = m_recipe;
    m_recipe = recipe;
    if (commit && m_history.commit(m_recipe, label.isEmpty() ? QStringLiteral("Edit") : label)) persist();
    if (before != recipe) emit recipeChanged();
    emit historyChanged();
    schedulePreview();
}

void ImageEditor::setParameter(const QString &key, const QVariant &value) {
    EditRecipe r = m_recipe;
    if (!r.setParameter(key, value)) return;
    m_lastLabel = labelFor(key);
    setRecipe(r, false);
}

void ImageEditor::setAdjustments(double brightness, double contrast, double saturation) {
    EditRecipe r = m_recipe;
    r.setParameter("brightness", brightness);
    r.setParameter("contrast", contrast);
    r.setParameter("saturation", saturation);
    m_lastLabel = "Light / contrast / saturation";
    setRecipe(r, false);
}

void ImageEditor::commitEdit(const QString &label) {
    const QString name = !label.isEmpty() ? label : !m_lastLabel.isEmpty() ? m_lastLabel : QStringLiteral("Edit");
    if (m_history.commit(m_recipe, name)) {
        persist();
        emit historyChanged();
    }
    schedulePreview();  // replace the draft with a full-quality render
}

void ImageEditor::applyParameter(const QString &key, const QVariant &value, const QString &label) {
    EditRecipe r = m_recipe;
    if (!r.setParameter(key, value)) return;
    setRecipe(r, true, label.isEmpty() ? labelFor(key) : label);
}

void ImageEditor::resetParameters(const QStringList &keys, const QString &label) {
    const QVariantMap defaults = EditRecipe{}.toVariantMap();
    EditRecipe r = m_recipe;
    for (const QString &key : keys) r.setParameter(key, defaults.value(key));
    setRecipe(r, true, label);
}

void ImageEditor::rotateBy(int degrees) {
    EditRecipe r = m_recipe;
    r.rotation = (((r.rotation + degrees) % 360) + 360) % 360;
    setRecipe(r, true, degrees < 0 ? "Rotate left" : "Rotate right");
}

void ImageEditor::flip(bool horizontal) {
    EditRecipe r = m_recipe;
    if (horizontal) r.flipHorizontal = !r.flipHorizontal;
    else r.flipVertical = !r.flipVertical;
    setRecipe(r, true, horizontal ? "Flip horizontal" : "Flip vertical");
}

void ImageEditor::setCrop(double x, double y, double width, double height) {
    EditRecipe r = m_recipe;
    const double x2 = qBound(0.0, x + width, 1.0), y2 = qBound(0.0, y + height, 1.0);
    r.cropX = qBound(0.0, x, 1.0);
    r.cropY = qBound(0.0, y, 1.0);
    r.cropWidth = x2 - r.cropX;
    r.cropHeight = y2 - r.cropY;
    r.hasCrop = r.cropWidth > 0.015 && r.cropHeight > 0.015;
    if (!r.hasCrop) { r.cropX = r.cropY = 0; r.cropWidth = r.cropHeight = 1; }
    setRecipe(r, true, "Crop");
}

void ImageEditor::clearCrop() {
    EditRecipe r = m_recipe;
    r.hasCrop = false;
    r.cropX = r.cropY = 0;
    r.cropWidth = r.cropHeight = 1;
    setRecipe(r, true, "Clear crop");
}

void ImageEditor::resetEdits() { setRecipe({}, true, "Reset all"); }

void ImageEditor::undo() {
    commitEdit();  // an uncommitted slider change becomes its own step first
    if (!m_history.canUndo()) return;
    setRecipe(m_history.undo(), false);
    persist();
}

void ImageEditor::redo() {
    if (m_recipe != m_history.current() || !m_history.canRedo()) return;
    setRecipe(m_history.redo(), false);
    persist();
}

void ImageEditor::jumpToHistory(int index) {
    commitEdit();
    setRecipe(m_history.jumpTo(index), false);
    persist();
}

void ImageEditor::cancel() {
    ++m_session;  // in-flight worker results are now ignored
    m_previewTimer.stop();
    m_zoomTimer.stop();
    m_renderPending = m_zoomPending = false;
    m_ready = false;
    emit readyChanged();
    setBusy(false);
    m_proxy = m_draft = m_original = vips::VImage();
    m_sourceSize = {};
    if (m_provider) m_provider->clear();
    if (m_ownsSource && !m_sourcePath.isEmpty()) QFile::remove(m_sourcePath);
    m_ownsSource = false;
    m_storeKey.clear();
    m_sourcePath.clear();
    m_title.clear(); m_hash.clear(); m_folder.clear(); m_originalName.clear();
    m_previewUrl = m_beforeUrl = m_histogramUrl = m_zoomUrl = QUrl();
    m_zoomed = false;
    if (m_cropMode) { m_cropMode = false; emit viewOptionsChanged(); }
    m_zoomRegion = {};
    m_displaySize = {};
    emit previewChanged(); emit sourceChanged(); emit zoomChanged();
}

QStringList ImageEditor::lutNames() const { return LutLibrary::names(); }
QString ImageEditor::lutDirectory() const { return LutLibrary::directory(); }

void ImageEditor::applyLook(const QString &name) {
    const QJsonObject look = m_looks.recipe(name);
    if (look.isEmpty()) { setStatus("Unknown look: " + name); return; }
    setRecipe(m_recipe.withLook(look), true, "Look: " + name);
}

bool ImageEditor::saveLook(const QString &name) {
    QString error;
    if (!m_looks.save(name, m_recipe.lookJson(), &error)) { setStatus(error); return false; }
    emit looksChanged();
    setStatus("Saved look “" + name.trimmed() + "”.");
    return true;
}

void ImageEditor::deleteLook(const QString &name) {
    if (m_looks.remove(name)) emit looksChanged();
}

void ImageEditor::importLut(const QUrl &file) {
    QString error;
    const QString name = LutLibrary::import(file.toLocalFile(), &error);
    if (name.isEmpty()) { setStatus("LUT not imported: " + error); return; }
    emit lutsChanged();
    applyParameter("lutName", name, "LUT: " + name);
}

void ImageEditor::addTextLayer(const QString &text) {
    QVariantList layers = m_recipe.toVariantMap().value("textLayers").toList();
    QVariantMap layer{{"text", text.isEmpty() ? QStringLiteral("GHOSTFRAME") : text}};
    if (layers.size() >= 16) return;
    layers.append(layer);
    applyParameter("textLayers", layers, "Add text");
}

void ImageEditor::setTextLayer(int index, const QString &field, const QVariant &value, bool commit) {
    QVariantList layers = m_recipe.toVariantMap().value("textLayers").toList();
    if (index < 0 || index >= layers.size()) return;
    QVariantMap layer = layers[index].toMap();
    layer.insert(field, value);
    layers[index] = layer;
    EditRecipe r = m_recipe;
    r.setParameter("textLayers", layers);
    m_lastLabel = "Text " + field;
    setRecipe(r, commit, m_lastLabel);
}

void ImageEditor::removeTextLayer(int index) {
    QVariantList layers = m_recipe.toVariantMap().value("textLayers").toList();
    if (index < 0 || index >= layers.size()) return;
    layers.removeAt(index);
    applyParameter("textLayers", layers, "Remove text");
}

bool ImageEditor::writeCybermeta(const QString &path, const QString &title, const QString &caption, const QString &tags) const {
    QSaveFile file(path + ".xmp");
    if (!file.open(QIODevice::WriteOnly)) return false;
    const EditRecipe &r = m_exportRecipe;
    QXmlStreamWriter xml(&file); xml.setAutoFormatting(true); xml.writeStartDocument();
    xml.writeStartElement("x:xmpmeta"); xml.writeAttribute("xmlns:x", "adobe:ns:meta/");
    xml.writeStartElement("rdf:RDF"); xml.writeAttribute("xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
    xml.writeStartElement("rdf:Description");
    xml.writeAttribute("xmlns:dc", "http://purl.org/dc/elements/1.1/");
    xml.writeAttribute("xmlns:xmp", "http://ns.adobe.com/xap/1.0/");
    xml.writeAttribute("xmlns:cybermeta", "https://cybercore.tech/ns/cybermeta/1.0/");
    auto localized = [&xml](const QString &property, const QString &value) {
        xml.writeStartElement(property); xml.writeStartElement("rdf:Alt"); xml.writeStartElement("rdf:li"); xml.writeAttribute("xml:lang", "x-default"); xml.writeCharacters(value); xml.writeEndElement(); xml.writeEndElement(); xml.writeEndElement();
    };
    localized("dc:title", title); localized("dc:description", caption);
    const QStringList tagList = tags.split(',', Qt::SkipEmptyParts);
    if (!tagList.isEmpty()) {
        xml.writeStartElement("dc:subject"); xml.writeStartElement("rdf:Bag");
        for (const auto &tag : tagList) { xml.writeStartElement("rdf:li"); xml.writeCharacters(tag.trimmed()); xml.writeEndElement(); }
        xml.writeEndElement(); xml.writeEndElement();
    }
    xml.writeTextElement("xmp:CreatorTool", "Ghostframe Cybermeta Editor");
    xml.writeTextElement("xmp:MetadataDate", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    xml.writeTextElement("cybermeta:version", "1.0");
    xml.writeTextElement("cybermeta:sourceHash", m_hash); xml.writeTextElement("cybermeta:sourceFolder", m_folder);
    xml.writeTextElement("cybermeta:sourceFilename", m_originalName);
    // The full recipe (reopenable); the flat fields below stay for tools that
    // read sidecars from earlier Ghostframe versions.
    xml.writeTextElement("cybermeta:recipe", r.toJsonString());
    xml.writeTextElement("cybermeta:rotation", QString::number(r.rotation));
    xml.writeTextElement("cybermeta:brightness", QString::number(r.brightness)); xml.writeTextElement("cybermeta:contrast", QString::number(r.contrast));
    xml.writeTextElement("cybermeta:saturation", QString::number(r.saturation));
    xml.writeTextElement("cybermeta:crop", r.hasCrop ? QString("%1,%2,%3,%4").arg(r.cropX).arg(r.cropY).arg(r.cropWidth).arg(r.cropHeight) : "none");
    xml.writeEndElement(); xml.writeEndElement(); xml.writeEndElement(); xml.writeEndDocument();
    if (xml.hasError()) { file.cancelWriting(); return false; }
    return file.commit();
}

void ImageEditor::save(const QString &title, const QString &caption, const QString &tags, const QVariantMap &exportOptions) {
    // A cancelled export may still be finishing; let it clean up first.
    if (!m_ready || m_busy || m_exportWatcher.isRunning()) return;
    commitEdit();
    const QString base = m_exportDir;
    if (!QDir().mkpath(base)) { setStatus("Could not create the export folder " + base + "."); return; }
    QString stem = QFileInfo(m_originalName).completeBaseName();
    if (stem.isEmpty()) stem = "Ghostframe-Image";
    stem.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "-");
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString extension = QFileInfo(m_originalName).suffix().toLower();
    QString format = (extension == "png" || extension == "webp" || extension == "tif" || extension == "tiff") ? extension : "jpg";
    const QString requested = exportOptions.value("format").toString().toLower();
    if (QStringList{"jpg", "png", "webp", "avif", "tif"}.contains(requested)) format = requested;
    ImageEngine::ExportOptions options;
    const int width = exportOptions.value("width").toInt(), height = exportOptions.value("height").toInt();
    if (width > 0 && height > 0) options.size = QSize(qMin(width, 16384), qMin(height, 16384));
    const QString fit = exportOptions.value("fit").toString();
    options.fit = fit == "blur" ? ImageEngine::ExportOptions::FitBlur : fit == "pad" ? ImageEngine::ExportOptions::FitPad
                                                                                    : ImageEngine::ExportOptions::Fill;
    options.quality = qBound(0, exportOptions.value("quality").toInt(), 100);
    QString preset = exportOptions.value("preset").toString().toLower();
    preset.replace(QRegularExpression("[^a-z0-9]+"), "-");
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString name = stem + "-Ghostframe-" + (preset.isEmpty() ? QString() : preset + "-") + timestamp + "-" + unique;
    const QString path = base + "/" + name + "." + format;
    const QString stagingPath = base + "/." + name + "-working." + format;

    m_pendingTitle = title; m_pendingCaption = caption; m_pendingTags = tags;
    m_exportRecipe = m_recipe;
    setBusy(true);
    setStatus("Rendering full-resolution export…");
    const int session = m_session;
    const QString source = m_sourcePath;
    const EditRecipe recipe = m_recipe;
    m_exportWatcher.setFuture(QtConcurrent::run([session, source, recipe, stagingPath, path, options] {
        ExportResult result;
        result.session = session;
        result.stagingPath = stagingPath;
        result.path = path;
        result.ok = ImageEngine::exportImage(source, recipe, stagingPath, options, &result.error)
                    && QFileInfo(stagingPath).size() > 0;
        return result;
    }));
}
