#include "imageeditor.h"

#include "imageengine.h"
#include "previewprovider.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QXmlStreamWriter>
#include <QtConcurrent/QtConcurrentRun>

ImageEditor::ImageEditor(QObject *parent)
    : QObject(parent),
      m_exportDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/Ghostframe/Exports") {
    // Coalesce bursts of slider events; rendering itself is off-thread.
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(16);
    connect(&m_previewTimer, &QTimer::timeout, this, &ImageEditor::startRender);

    connect(&m_openWatcher, &QFutureWatcher<OpenResult>::finished, this, [this] {
        const OpenResult result = m_openWatcher.result();
        if (result.session != m_session) return;
        setBusy(false);
        if (!result.ok) { setStatus("Could not open this image: " + result.error); return; }
        m_proxy = result.proxy;
        m_sourceSize = result.sourceSize;
        m_ready = true;
        emit readyChanged();
        setStatus(QString("Original: %1 × %2 px").arg(m_sourceSize.width()).arg(m_sourceSize.height()));
        startRender();
    });

    connect(&m_renderWatcher, &QFutureWatcher<RenderResult>::finished, this, [this] {
        const RenderResult result = m_renderWatcher.result();
        if (result.session == m_session && m_ready) {
            if (!result.ok) {
                setStatus("Could not render the edit preview: " + result.error);
            } else {
                if (m_provider) m_provider->setImage(result.image);
                m_previewUrl = QUrl(QString("image://ghostframe-edit/preview/%1").arg(++m_previewRevision));
                m_displaySize = result.size;
                m_lastRenderMs = result.milliseconds;
                emit previewChanged();
                setStatus(QString("Preview %1 ms · %2 × %3 px output")
                              .arg(result.milliseconds).arg(m_displaySize.width()).arg(m_displaySize.height()));
            }
        }
        if (m_renderPending) { m_renderPending = false; startRender(); }
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
    m_exportWatcher.waitForFinished();
}

void ImageEditor::setBusy(bool value) { if (m_busy != value) { m_busy = value; emit busyChanged(); } }
void ImageEditor::setStatus(const QString &value) { if (m_status != value) { m_status = value; emit statusChanged(); } }

void ImageEditor::openOriginal(const QString &path, const QString &title, const QString &hash, const QString &folder, const QString &originalName) {
    cancel();
    m_sourcePath = path; m_title = title; m_hash = hash; m_folder = folder; m_originalName = originalName;
    m_ownsSource = true;
    m_recipe = {};
    m_history.reset(m_recipe);
    emit sourceChanged(); emit adjustmentsChanged(); emit cropChanged(); emit historyChanged(); emit recipeChanged();
    if (!QFileInfo::exists(path)) { setStatus("The temporary full-resolution working copy is missing."); return; }
    setBusy(true);
    setStatus("Preparing image…");
    const int session = m_session;
    m_openWatcher.setFuture(QtConcurrent::run([session, path] {
        OpenResult result;
        result.session = session;
        try {
            result.sourceSize = ImageEngine::sourceSize(path);
            result.proxy = ImageEngine::loadProxy(path);
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
}

void ImageEditor::schedulePreview() { if (m_ready) m_previewTimer.start(); }

void ImageEditor::startRender() {
    if (!m_ready) return;
    if (m_renderWatcher.isRunning()) { m_renderPending = true; return; }
    const int session = m_session;
    const vips::VImage proxy = m_proxy;
    const EditRecipe recipe = m_recipe;
    const QSize sourceSize = m_sourceSize;
    m_renderWatcher.setFuture(QtConcurrent::run([session, proxy, recipe, sourceSize] {
        RenderResult result;
        result.session = session;
        QElapsedTimer timer;
        timer.start();
        try {
            result.image = ImageEngine::toQImage(ImageEngine::apply(proxy, recipe));
            // Report the full-resolution output size, not the proxy's.
            const bool quarterTurn = recipe.rotation == 90 || recipe.rotation == 270;
            QSize size = quarterTurn ? sourceSize.transposed() : sourceSize;
            if (recipe.hasCrop) size = QSize(qRound(size.width() * recipe.cropWidth), qRound(size.height() * recipe.cropHeight));
            result.size = size;
            result.ok = true;
        } catch (const vips::VError &e) {
            result.error = QString::fromUtf8(e.what()).trimmed();
        }
        result.milliseconds = int(timer.elapsed());
        return result;
    }));
}

void ImageEditor::setRecipe(const EditRecipe &recipe, bool commit) {
    const EditRecipe before = m_recipe;
    m_recipe = recipe;
    if (before.brightness != recipe.brightness || before.contrast != recipe.contrast
        || before.saturation != recipe.saturation || before.rotation != recipe.rotation)
        emit adjustmentsChanged();
    if (before.hasCrop != recipe.hasCrop || before.cropX != recipe.cropX || before.cropY != recipe.cropY
        || before.cropWidth != recipe.cropWidth || before.cropHeight != recipe.cropHeight)
        emit cropChanged();
    if (commit) m_history.commit(m_recipe);
    if (before != recipe) emit recipeChanged();
    emit historyChanged();
    schedulePreview();
}

void ImageEditor::setAdjustments(double brightness, double contrast, double saturation) {
    EditRecipe r = m_recipe;
    r.brightness = qBound(-100.0, brightness, 100.0);
    r.contrast = qBound(-100.0, contrast, 100.0);
    r.saturation = qBound(0.0, saturation, 200.0);
    setRecipe(r, false);
}

void ImageEditor::commitEdit() {
    if (m_history.commit(m_recipe)) emit historyChanged();
}

void ImageEditor::rotateBy(int degrees) {
    EditRecipe r = m_recipe;
    r.rotation = (((r.rotation + degrees) % 360) + 360) % 360;
    setRecipe(r, true);
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
    setRecipe(r, true);
}

void ImageEditor::clearCrop() {
    EditRecipe r = m_recipe;
    r.hasCrop = false;
    r.cropX = r.cropY = 0;
    r.cropWidth = r.cropHeight = 1;
    setRecipe(r, true);
}

void ImageEditor::resetEdits() { setRecipe({}, true); }

void ImageEditor::undo() {
    commitEdit();  // an uncommitted slider change becomes its own step first
    if (!m_history.canUndo()) return;
    setRecipe(m_history.undo(), false);
}

void ImageEditor::redo() {
    if (m_recipe != m_history.current() || !m_history.canRedo()) return;
    setRecipe(m_history.redo(), false);
}

void ImageEditor::cancel() {
    ++m_session;  // in-flight worker results are now ignored
    m_previewTimer.stop();
    m_renderPending = false;
    m_ready = false;
    emit readyChanged();
    setBusy(false);
    m_proxy = vips::VImage();
    if (m_provider) m_provider->setImage({});
    if (m_ownsSource && !m_sourcePath.isEmpty()) QFile::remove(m_sourcePath);
    m_ownsSource = false;
    m_sourcePath.clear();
    m_title.clear(); m_hash.clear(); m_folder.clear(); m_originalName.clear();
    m_previewUrl = QUrl();
    m_displaySize = {};
    emit previewChanged(); emit sourceChanged();
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

void ImageEditor::save(const QString &title, const QString &caption, const QString &tags) {
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
    const QString format = (extension == "png" || extension == "webp" || extension == "tif" || extension == "tiff") ? extension : "jpg";
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString name = stem + "-Ghostframe-" + timestamp + "-" + unique;
    const QString path = base + "/" + name + "." + format;
    const QString stagingPath = base + "/." + name + "-working." + format;

    m_pendingTitle = title; m_pendingCaption = caption; m_pendingTags = tags;
    m_exportRecipe = m_recipe;
    setBusy(true);
    setStatus("Rendering full-resolution export…");
    const int session = m_session;
    const QString source = m_sourcePath;
    const EditRecipe recipe = m_recipe;
    m_exportWatcher.setFuture(QtConcurrent::run([session, source, recipe, stagingPath, path] {
        ExportResult result;
        result.session = session;
        result.stagingPath = stagingPath;
        result.path = path;
        result.ok = ImageEngine::exportImage(source, recipe, stagingPath, &result.error)
                    && QFileInfo(stagingPath).size() > 0;
        return result;
    }));
}
