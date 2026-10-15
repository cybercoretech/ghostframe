#include "cybermetaclient.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <vips/vips8>

namespace {

constexpr int kTimeoutMs = 20000;
const QVersionNumber kMinimum(0, 2, 0);

bool run(const QStringList &args, QByteArray *out, QString *error) {
    const QString exe = Cybermeta::executable();
    if (exe.isEmpty()) {
        if (error) *error = "cybermeta is not installed";
        return false;
    }
    QProcess process;
    process.start(exe, args);
    if (!process.waitForStarted(5000)) {
        if (error) *error = "could not start cybermeta";
        return false;
    }
    if (!process.waitForFinished(kTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        if (error) *error = "cybermeta timed out";
        return false;
    }
    if (out) *out = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (error->startsWith("cybermeta: ")) error->remove(0, 11);
            if (error->isEmpty()) *error = "cybermeta failed";
        }
        return false;
    }
    return true;
}

QStringList xmpArgs(const Cybermeta::XmpContent &c) {
    QStringList args{"xmp", "write", "--creator-tool", "Ghostframe"};
    if (!c.title.isEmpty()) args << "--title" << c.title;
    if (!c.description.isEmpty()) args << "--description" << c.description;
    for (const QString &k : c.keywords)
        if (!k.trimmed().isEmpty()) args << "--keyword" << k.trimmed();
    for (auto it = c.fields.cbegin(); it != c.fields.cend(); ++it) args << "--field" << it.key() + "=" + it.value();
    if (!c.recipe.isEmpty()) args << "--recipe" << QString::fromUtf8(QJsonDocument(c.recipe).toJson(QJsonDocument::Compact));
    return args;
}

}  // namespace

namespace Cybermeta {

Privacy privacyFromString(const QString &value) {
    if (value == "keep") return Privacy::Keep;
    if (value == "anonymous") return Privacy::Anonymous;
    if (value == "all") return Privacy::StripAll;
    return Privacy::StripGps;
}

QString privacyLabel(Privacy privacy) {
    switch (privacy) {
    case Privacy::Keep: return "metadata kept";
    case Privacy::StripGps: return "location removed";
    case Privacy::Anonymous: return "location and camera identity removed";
    case Privacy::StripAll: return "camera metadata removed";
    }
    return {};
}

QString executable() {
    const QString override = qEnvironmentVariable("GHOSTFRAME_CYBERMETA");
    if (!override.isEmpty()) return QFileInfo(override).isExecutable() ? override : QString();
    QString found = QStandardPaths::findExecutable("cybermeta");
    if (found.isEmpty())
        found = QStandardPaths::findExecutable("cybermeta", {QDir::homePath() + "/.local/bin", QDir::homePath() + "/.cargo/bin"});
    return found;
}

QVersionNumber version() {
    static QMutex mutex;
    static QString cachedFor;
    static QVersionNumber cached;
    const QString exe = executable();
    QMutexLocker lock(&mutex);
    if (exe == cachedFor && !exe.isEmpty()) return cached;
    cachedFor = exe;
    cached = {};
    QByteArray out;
    if (!exe.isEmpty() && run({"--version"}, &out, nullptr))  // "cybermeta 0.2.0"
        cached = QVersionNumber::fromString(QString::fromUtf8(out).trimmed().section(' ', -1));
    return cached;
}

bool available() {
    const QVersionNumber v = version();
    return !v.isNull() && v >= kMinimum;
}

QVariantMap summary(const QString &imagePath, QString *error) {
    if (!available()) { if (error) *error = "cybermeta 0.2.0 or newer is required"; return {}; }
    QByteArray out;
    if (!run({"show", imagePath, "--summary"}, &out, error)) return {};
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(out, &parse);
    if (!doc.isObject()) { if (error) *error = "unreadable cybermeta output"; return {}; }
    return doc.object().toVariantMap();
}

QByteArray xmpPacket(const XmpContent &content, QString *error) {
    if (!available()) { if (error) *error = "cybermeta 0.2.0 or newer is required"; return {}; }
    QByteArray out;
    if (!run(xmpArgs(content) << "-o" << "-", &out, error)) return {};
    return out;
}

bool writeSidecar(const QString &path, const XmpContent &content, QString *error) {
    if (!available()) { if (error) *error = "cybermeta 0.2.0 or newer is required"; return false; }
    return run(xmpArgs(content) << "-o" << path, nullptr, error);
}

QByteArray cleanExif(const QByteArray &exif, Privacy privacy, const QString &description, QString *error) {
    const bool describe = !description.trimmed().isEmpty();
    if (privacy == Privacy::Keep && !describe) return exif;
    if (privacy == Privacy::StripAll && !describe) return {};
    if (!available()) { if (error) *error = "cybermeta 0.2.0 or newer is required"; return {}; }

    QTemporaryDir dir;
    if (!dir.isValid()) { if (error) *error = "no temporary directory"; return {}; }
    const QString carrier = dir.filePath("exif.jpg");
    try {
        // A tiny JPEG holding just this EXIF block.
        vips::VImage image = vips::VImage::black(8, 8, vips::VImage::option()->set("bands", 3)).cast(VIPS_FORMAT_UCHAR).copy();
        if (!exif.isEmpty()) vips_image_set_blob_copy(image.get_image(), "exif-data", exif.constData(), size_t(exif.size()));
        image.jpegsave(carrier.toUtf8().constData());
    } catch (const vips::VError &e) {
        if (error) *error = QString::fromUtf8(e.what()).trimmed();
        return {};
    }
    switch (privacy) {
    case Privacy::Keep: break;
    case Privacy::StripGps:
        if (!run({"strip", carrier, "--gps", "--in-place"}, nullptr, error)) return {};
        break;
    case Privacy::Anonymous:
        if (!run({"spoof", carrier, "--preset", "anonymous-camera", "--in-place"}, nullptr, error)) return {};
        break;
    case Privacy::StripAll:
        if (!run({"strip", carrier, "--all", "--in-place"}, nullptr, error)) return {};
        break;
    }
    if (describe && !run({"set", carrier, "ImageDescription=" + description.trimmed(), "--in-place"}, nullptr, error))
        return {};
    try {
        const vips::VImage cleaned = vips::VImage::new_from_file(carrier.toUtf8().constData());
        if (cleaned.get_typeof("exif-data") == 0) return {};
        size_t length = 0;
        const void *data = cleaned.get_blob("exif-data", &length);
        return QByteArray(static_cast<const char *>(data), qsizetype(length));
    } catch (const vips::VError &e) {
        if (error) *error = QString::fromUtf8(e.what()).trimmed();
        return {};
    }
}

}  // namespace Cybermeta
