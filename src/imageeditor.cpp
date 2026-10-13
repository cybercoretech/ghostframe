#include "imageeditor.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QUrl>
#include <QVariant>

ImageEditor::ImageEditor(QObject *parent) : QObject(parent) {
    m_magick = QStandardPaths::findExecutable("magick");
    if (m_magick.isEmpty()) m_magick = QStandardPaths::findExecutable("convert");
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(180);
    connect(&m_previewTimer, &QTimer::timeout, this, &ImageEditor::updatePreview);
}

void ImageEditor::setBusy(bool value) { if (m_busy != value) { m_busy = value; emit busyChanged(); } }
void ImageEditor::setStatus(const QString &value) { if (m_status != value) { m_status = value; emit statusChanged(); } }

void ImageEditor::openOriginal(const QString &path, const QString &title, const QString &hash, const QString &folder, const QString &originalName) {
    cancel();
    m_sourcePath = path; m_title = title; m_hash = hash; m_folder = folder; m_originalName = originalName;
    m_rotation = 0; m_brightness = 0; m_contrast = 0; m_saturation = 100; m_hasCrop = false;
    emit sourceChanged(); emit adjustmentsChanged(); emit cropChanged();
    if (m_magick.isEmpty()) { setStatus("ImageMagick is required. Install the imagemagick package and reopen Ghostframe."); return; }
    if (!QFileInfo::exists(path)) { setStatus("The temporary full-resolution working copy is missing."); return; }
    setBusy(true); setStatus("Preparing full-resolution image…");
    auto *probe = new QProcess(this);
    connect(probe, &QProcess::finished, this, [this, probe](int code, QProcess::ExitStatus exitStatus) {
        const QByteArray output = probe->readAllStandardOutput().trimmed();
        probe->deleteLater();
        const auto parts = output.split(' ');
        if (code != 0 || exitStatus != QProcess::NormalExit || parts.size() != 2) {
            setBusy(false); setStatus("ImageMagick could not read this image format."); return;
        }
        bool wOk = false, hOk = false;
        m_sourceWidth = parts.at(0).toInt(&wOk); m_sourceHeight = parts.at(1).toInt(&hOk);
        if (!wOk || !hOk || m_sourceWidth < 1 || m_sourceHeight < 1) { setBusy(false); setStatus("Image dimensions could not be read."); return; }
        m_ready = true; emit readyChanged(); setBusy(false); setStatus(QString("Original: %1 × %2 px").arg(m_sourceWidth).arg(m_sourceHeight));
        updatePreview();
    });
    probe->start(m_magick, {path + "[0]", "-auto-orient", "-format", "%w %h", "info:"});
}

QStringList ImageEditor::editArguments(bool preview) const {
    QStringList args{m_sourcePath + "[0]", "-auto-orient"};
    if (m_rotation) args << "-rotate" << QString::number(m_rotation);
    if (m_brightness != 0 || m_contrast != 0)
        args << "-brightness-contrast" << QString("%1x%2").arg(m_brightness, 0, 'f', 1).arg(m_contrast, 0, 'f', 1);
    if (m_saturation != 100) args << "-modulate" << QString("100,%1,100").arg(m_saturation, 0, 'f', 1);
    int width = (m_rotation == 90 || m_rotation == 270) ? m_sourceHeight : m_sourceWidth;
    int height = (m_rotation == 90 || m_rotation == 270) ? m_sourceWidth : m_sourceHeight;
    if (m_hasCrop) {
        const int x = qBound(0, qRound(m_cropX * width), width - 1);
        const int y = qBound(0, qRound(m_cropY * height), height - 1);
        const int w = qBound(1, qRound(m_cropWidth * width), width - x);
        const int h = qBound(1, qRound(m_cropHeight * height), height - y);
        args << "-crop" << QString("%1x%2+%3+%4").arg(w).arg(h).arg(x).arg(y) << "+repage";
    }
    if (preview) args << "-resize" << "1600x1200>";
    return args;
}

void ImageEditor::updatePreview() {
    if (!m_ready || m_sourcePath.isEmpty()) return;
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) { m_previewProcess->kill(); m_previewProcess->waitForFinished(1000); }
    const QString output = m_tempDir.filePath(QString("preview-%1.jpg").arg(++m_previewRevision));
    auto *process = new QProcess(this); m_previewProcess = process;
    auto args = editArguments(true); args << "-quality" << "88" << output;
    connect(process, &QProcess::finished, this, [this, process, output](int code, QProcess::ExitStatus) {
        if (process != m_previewProcess) { process->deleteLater(); return; }
        process->deleteLater(); m_previewProcess = nullptr;
        if (code != 0 || !QFileInfo::exists(output)) { setStatus("Could not render edit preview."); return; }
        m_previewUrl = QUrl::fromLocalFile(output);
        m_displayWidth = (m_rotation == 90 || m_rotation == 270) ? m_sourceHeight : m_sourceWidth;
        m_displayHeight = (m_rotation == 90 || m_rotation == 270) ? m_sourceWidth : m_sourceHeight;
        if (m_hasCrop) { m_displayWidth = qRound(m_displayWidth * m_cropWidth); m_displayHeight = qRound(m_displayHeight * m_cropHeight); }
        emit previewChanged();
        setStatus(QString("Preview ready · %1 × %2 px source").arg(m_displayWidth).arg(m_displayHeight));
    });
    process->start(m_magick, args);
}

void ImageEditor::setAdjustments(double brightness, double contrast, double saturation) {
    m_brightness = qBound(-100.0, brightness, 100.0); m_contrast = qBound(-100.0, contrast, 100.0); m_saturation = qBound(0.0, saturation, 200.0);
    emit adjustmentsChanged(); m_previewTimer.start();
}
void ImageEditor::rotateBy(int degrees) {
    m_rotation = (m_rotation + degrees) % 360; if (m_rotation < 0) m_rotation += 360;
    emit adjustmentsChanged(); m_previewTimer.start();
}
void ImageEditor::setCrop(double x, double y, double width, double height) {
    const double x2 = qBound(0.0, x + width, 1.0), y2 = qBound(0.0, y + height, 1.0);
    m_cropX = qBound(0.0, x, 1.0); m_cropY = qBound(0.0, y, 1.0);
    m_cropWidth = x2 - m_cropX; m_cropHeight = y2 - m_cropY;
    m_hasCrop = m_cropWidth > 0.015 && m_cropHeight > 0.015;
    emit cropChanged(); m_previewTimer.start();
}
void ImageEditor::clearCrop() { m_hasCrop = false; m_cropX = m_cropY = 0; m_cropWidth = m_cropHeight = 1; emit cropChanged(); m_previewTimer.start(); }
void ImageEditor::resetEdits() { m_rotation = 0; m_brightness = 0; m_contrast = 0; m_saturation = 100; clearCrop(); emit adjustmentsChanged(); }
void ImageEditor::cancel() {
    m_previewTimer.stop();
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) m_previewProcess->kill();
    if (m_process && m_process->state() != QProcess::NotRunning) m_process->kill();
    m_ready = false; emit readyChanged(); setBusy(false);
    if (!m_sourcePath.isEmpty() && m_sourcePath.startsWith(m_tempDir.path() + QDir::separator())) QFile::remove(m_sourcePath);
    m_sourcePath.clear();
}

bool ImageEditor::writeCybermeta(const QString &path, const QString &title, const QString &caption, const QString &tags) const {
    QSaveFile file(path + ".xmp");
    if (!file.open(QIODevice::WriteOnly)) return false;
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
    xml.writeTextElement("cybermeta:sourceFilename", m_originalName); xml.writeTextElement("cybermeta:rotation", QString::number(m_rotation));
    xml.writeTextElement("cybermeta:brightness", QString::number(m_brightness)); xml.writeTextElement("cybermeta:contrast", QString::number(m_contrast));
    xml.writeTextElement("cybermeta:saturation", QString::number(m_saturation));
    xml.writeTextElement("cybermeta:crop", m_hasCrop ? QString("%1,%2,%3,%4").arg(m_cropX).arg(m_cropY).arg(m_cropWidth).arg(m_cropHeight) : "none");
    xml.writeEndElement(); xml.writeEndElement(); xml.writeEndElement(); xml.writeEndDocument();
    if (xml.hasError()) { file.cancelWriting(); return false; }
    return file.commit();
}

void ImageEditor::save(const QString &title, const QString &caption, const QString &tags) {
    if (!m_ready || m_busy) return;
    const QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/Ghostframe/Exports";
    if (!QDir().mkpath(base)) { setStatus("Could not create ~/Pictures/Ghostframe/Exports."); return; }
    QString stem = QFileInfo(m_originalName).completeBaseName();
    if (stem.isEmpty()) stem = "Ghostframe-Image";
    stem.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "-");
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString extension = QFileInfo(m_originalName).suffix().toLower();
    const QString format = (extension == "png" || extension == "webp" || extension == "tif" || extension == "tiff") ? extension : "jpg";
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString path = base + "/" + stem + "-Ghostframe-" + timestamp + "-" + unique + "." + format;
    const QString stagingPath = base + "/." + stem + "-Ghostframe-" + timestamp + "-" + unique + "-working." + format;
    auto *process = new QProcess(this); m_process = process; setBusy(true); setStatus("Rendering full-resolution export…");
    auto args = editArguments(false);
    if (format == "jpg") args << "-quality" << "95";
    args << stagingPath;
    connect(process, &QProcess::finished, this, [this, process, path, stagingPath, title, caption, tags](int code, QProcess::ExitStatus exitStatus) {
        const QString errorOutput = QString::fromUtf8(process->readAllStandardError()).trimmed();
        process->deleteLater(); m_process = nullptr; setBusy(false);
        if (code != 0 || exitStatus != QProcess::NormalExit || !QFileInfo::exists(stagingPath) || QFileInfo(stagingPath).size() == 0) {
            QFile::remove(stagingPath); setStatus(errorOutput.isEmpty() ? "ImageMagick failed to render this export." : errorOutput.left(220)); return;
        }
        if (!QFile::rename(stagingPath, path)) { QFile::remove(stagingPath); setStatus("Could not finalize the export in the Ghostframe Exports folder."); return; }
        if (!writeCybermeta(path, title, caption, tags)) { setStatus("The image exported, but its Cybermeta XMP sidecar could not be written."); emit saved(path); return; }
        setStatus("Export and Cybermeta saved to ~/Pictures/Ghostframe/Exports."); emit saved(path);
    });
    process->start(m_magick, args);
}
