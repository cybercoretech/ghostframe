#include "looklibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
struct Look { QString name; QJsonObject recipe; };

QList<Look> readDir(const QString &dir) {
    QList<Look> looks;
    for (const QFileInfo &info : QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024) continue;
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        const QString name = json.value("name").toString().trimmed();
        if (!name.isEmpty() && json.value("recipe").isObject()) looks.append({name, json.value("recipe").toObject()});
    }
    return looks;
}
}  // namespace

LookLibrary::LookLibrary(QString directory) : m_directory(std::move(directory)) {
    if (m_directory.isEmpty())
        m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/looks";
}

QString LookLibrary::userPath(const QString &name) const {
    QString file = name.trimmed().toLower();
    file.replace(QRegularExpression("[^a-z0-9]+"), "-");
    file.remove(QRegularExpression("^-+|-+$"));
    return m_directory + "/" + (file.isEmpty() ? QStringLiteral("look") : file.left(60)) + ".json";
}

QVariantList LookLibrary::list() const {
    QVariantList out;
    const QList<Look> mine = readDir(m_directory);
    for (const Look &look : readDir(":/looks")) {
        const bool shadowed = std::any_of(mine.begin(), mine.end(), [&](const Look &m) { return m.name == look.name; });
        if (!shadowed) out.append(QVariantMap{{"name", look.name}, {"builtin", true}});
    }
    for (const Look &look : mine) out.append(QVariantMap{{"name", look.name}, {"builtin", false}});
    return out;
}

QJsonObject LookLibrary::recipe(const QString &name) const {
    for (const QString &dir : {m_directory, QStringLiteral(":/looks")})
        for (const Look &look : readDir(dir))
            if (look.name == name) return look.recipe;
    return {};
}

bool LookLibrary::save(const QString &name, const QJsonObject &recipe, QString *error) const {
    if (name.trimmed().isEmpty()) { if (error) *error = "Give the look a name."; return false; }
    if (!QDir().mkpath(m_directory)) { if (error) *error = "Could not create " + m_directory; return false; }
    QSaveFile file(userPath(name));
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = "Could not write the look."; return false; }
    file.write(QJsonDocument(QJsonObject{{"name", name.trimmed().left(60)}, {"recipe", recipe}}).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool LookLibrary::remove(const QString &name) const {
    for (const QFileInfo &info : QDir(m_directory).entryInfoList({"*.json"}, QDir::Files)) {
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        if (QJsonDocument::fromJson(file.readAll()).object().value("name").toString() == name) {
            file.close();
            return QFile::remove(info.filePath());
        }
    }
    return false;
}
