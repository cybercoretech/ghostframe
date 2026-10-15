#include "cubelut.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

bool CubeLut::parse(const QByteArray &text, CubeLut *out, QString *error) {
    auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    CubeLut lut;
    std::vector<float> values;
    for (QByteArray line : text.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith("TITLE")) continue;
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts[0] == "LUT_1D_SIZE") return fail("1D LUTs are not supported; use a 3D .cube file.");
        if (parts[0] == "LUT_3D_SIZE") {
            lut.size = parts.value(1).toInt();
            if (lut.size < 2 || lut.size > 129) return fail("LUT_3D_SIZE must be between 2 and 129.");
            values.reserve(size_t(lut.size) * lut.size * lut.size * 3);
            continue;
        }
        if (parts[0] == "DOMAIN_MIN" || parts[0] == "DOMAIN_MAX") {
            float *target = parts[0] == "DOMAIN_MIN" ? lut.domainMin : lut.domainMax;
            for (int i = 0; i < 3; ++i) target[i] = parts.value(i + 1).toFloat();
            continue;
        }
        if (parts.size() != 3) continue;  // other keywords
        bool ok[3];
        for (int i = 0; i < 3; ++i) {
            values.push_back(parts[i].toFloat(&ok[i]));
            if (!ok[i]) return fail("Unreadable value: " + QString::fromUtf8(line.left(60)));
        }
    }
    if (lut.size == 0) return fail("Not a 3D .cube file (no LUT_3D_SIZE).");
    if (values.size() != size_t(lut.size) * lut.size * lut.size * 3)
        return fail(QString("Expected %1 entries, found %2.").arg(lut.size * lut.size * lut.size).arg(values.size() / 3));
    for (int i = 0; i < 3; ++i)
        if (lut.domainMax[i] <= lut.domainMin[i]) return fail("DOMAIN_MAX must be greater than DOMAIN_MIN.");
    lut.table = std::move(values);
    *out = std::move(lut);
    return true;
}

void CubeLut::apply(float *rgb, size_t pixels) const {
    const int n = size, n1 = size - 1;
    const auto at = [this, n](int r, int g, int b) { return &table[(size_t(b) * n * n + size_t(g) * n + size_t(r)) * 3]; };
    for (size_t p = 0; p < pixels; ++p, rgb += 3) {
        float f[3];
        int i0[3], i1[3];
        for (int c = 0; c < 3; ++c) {
            const float v = std::clamp((rgb[c] - domainMin[c]) / (domainMax[c] - domainMin[c]), 0.0f, 1.0f) * n1;
            i0[c] = std::min(int(v), n1 - 1);
            i1[c] = i0[c] + 1;
            f[c] = v - i0[c];
        }
        float out[3];
        for (int c = 0; c < 3; ++c) {
            const float c000 = at(i0[0], i0[1], i0[2])[c], c100 = at(i1[0], i0[1], i0[2])[c];
            const float c010 = at(i0[0], i1[1], i0[2])[c], c110 = at(i1[0], i1[1], i0[2])[c];
            const float c001 = at(i0[0], i0[1], i1[2])[c], c101 = at(i1[0], i0[1], i1[2])[c];
            const float c011 = at(i0[0], i1[1], i1[2])[c], c111 = at(i1[0], i1[1], i1[2])[c];
            const float c00 = c000 + (c100 - c000) * f[0], c10 = c010 + (c110 - c010) * f[0];
            const float c01 = c001 + (c101 - c001) * f[0], c11 = c011 + (c111 - c011) * f[0];
            const float c0 = c00 + (c10 - c00) * f[1], c1 = c01 + (c11 - c01) * f[1];
            out[c] = c0 + (c1 - c0) * f[2];
        }
        for (int c = 0; c < 3; ++c) rgb[c] = std::clamp(out[c], 0.0f, 1.0f);
    }
}

namespace {
QMutex g_mutex;
QString g_directory;
struct Cached { QDateTime modified; std::shared_ptr<const CubeLut> lut; };
QHash<QString, Cached> g_cache;
}  // namespace

namespace LutLibrary {

void setDirectory(const QString &path) {
    QMutexLocker lock(&g_mutex);
    g_directory = path;
    g_cache.clear();
}

QString directory() {
    QMutexLocker lock(&g_mutex);
    if (g_directory.isEmpty())
        g_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/luts";
    return g_directory;
}

QStringList names() {
    QStringList list = QDir(directory()).entryList({"*.cube", "*.CUBE"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    return list;
}

std::shared_ptr<const CubeLut> load(const QString &name, QString *error) {
    const QString safe = QFileInfo(name).fileName();  // never a path
    const QString path = directory() + "/" + safe;
    const QFileInfo info(path);
    if (safe.isEmpty() || !info.isFile()) {
        if (error) *error = "LUT not found: " + safe;
        return nullptr;
    }
    {
        QMutexLocker lock(&g_mutex);
        const auto it = g_cache.constFind(safe);
        if (it != g_cache.constEnd() && it->modified == info.lastModified()) return it->lut;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) {
        if (error) *error = "Could not read " + safe;
        return nullptr;
    }
    auto lut = std::make_shared<CubeLut>();
    if (!CubeLut::parse(file.readAll(), lut.get(), error)) return nullptr;
    QMutexLocker lock(&g_mutex);
    g_cache.insert(safe, {info.lastModified(), lut});
    return lut;
}

QString import(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { if (error) *error = "Could not open " + path; return {}; }
    CubeLut check;
    if (!CubeLut::parse(file.readAll(), &check, error)) return {};
    if (!QDir().mkpath(directory())) { if (error) *error = "Could not create " + directory(); return {}; }
    QString name = QFileInfo(path).fileName();
    if (!name.endsWith(".cube", Qt::CaseInsensitive)) name += ".cube";
    const QString target = directory() + "/" + name;
    if (QFileInfo(target).canonicalFilePath() == QFileInfo(path).canonicalFilePath()) return name;
    QFile::remove(target);
    if (!QFile::copy(path, target)) { if (error) *error = "Could not copy into " + directory(); return {}; }
    return name;
}

}  // namespace LutLibrary
