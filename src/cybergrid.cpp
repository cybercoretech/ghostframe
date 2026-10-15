#include "cybergrid.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace {
const QStringList kRoles{"bg", "white", "acid_green", "hot_pink", "purple", "cyan", "orange", "red", "panel", "line", "muted"};

struct Style { const char *id; const char *label; QStringList roles; };
const QList<Style> &styleTable() {
    static const QList<Style> table{
        {"neon", "NEON", {"bg", "purple", "hot_pink", "cyan", "white"}},
        {"sunset", "SUNSET", {"bg", "purple", "red", "orange", "white"}},
        {"acid", "ACID", {"bg", "panel", "acid_green", "white"}},
        {"duo-pink", "DUO PINK", {"bg", "hot_pink"}},
        {"duo-cyan", "DUO CYAN", {"bg", "cyan"}},
        {"mono", "MONO", {"bg", "muted", "white"}},
    };
    return table;
}

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
}  // namespace

QString CyberGrid::configDirectory() {
    const QString override = qEnvironmentVariable("CYBERCORE_CONFIG_DIR");
    if (!override.isEmpty()) return override;
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    return (xdg.isEmpty() ? QDir::homePath() + "/.config" : xdg) + "/cybercore";
}

CyberGrid::CyberGrid(QObject *parent) : QObject(parent) {
    QDirIterator bundled(":/cybergrid", {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (bundled.hasNext()) {
        const QFileInfo info(bundled.next());
        addTheme(info.completeBaseName(), info.dir().dirName(), readJson(info.filePath()));
    }
    // Custom themes: flat palettes or theme documents ({"id", "palette": {...}}).
    const QDir custom(configDirectory() + "/themes");
    for (const QFileInfo &info : custom.entryInfoList({"*.json"}, QDir::Files)) {
        const QJsonObject json = readJson(info.filePath());
        const QString id = json.value("id").toString(info.completeBaseName());
        addTheme(id, "custom", json.contains("palette") ? json.value("palette").toObject() : json);
    }
    std::sort(m_themes.begin(), m_themes.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap(), y = b.toMap();
        return std::pair(x["family"].toString(), x["id"].toString()) < std::pair(y["family"].toString(), y["id"].toString());
    });
    m_activeId = readJson(configDirectory() + "/active-theme.json").value("id").toString();
}

void CyberGrid::addTheme(const QString &id, const QString &family, const QJsonObject &json) {
    static const QRegularExpression hex("^#?[0-9a-fA-F]{6}$");
    QVariantMap colours;
    for (const QString &role : kRoles) {
        const QString value = json.value(role).toString();
        if (!hex.match(value).hasMatch()) return;  // incomplete palette: skip
        colours.insert(role, value.right(6).toLower());
    }
    for (auto &existing : m_themes)
        if (existing.toMap().value("id") == id) return;  // first one wins (bundled before custom)
    QString name = id;
    name.replace('-', ' ');
    m_themes.append(QVariantMap{{"id", id}, {"family", family}, {"name", name.toUpper()}, {"colors", colours}});
}

QVariantList CyberGrid::styles() const {
    QVariantList list;
    for (const auto &s : styleTable()) list.append(QVariantMap{{"id", s.id}, {"label", s.label}});
    return list;
}

QStringList CyberGrid::gradient(const QString &themeId, const QString &style) const {
    const auto styleIt = std::find_if(styleTable().begin(), styleTable().end(), [&](const Style &s) { return style == QLatin1String(s.id); });
    if (styleIt == styleTable().end()) return {};
    for (const auto &theme : m_themes) {
        const QVariantMap map = theme.toMap();
        if (map.value("id") != themeId) continue;
        const QVariantMap colours = map.value("colors").toMap();
        QStringList stops;
        for (const QString &role : styleIt->roles) stops.append(colours.value(role).toString());
        return stops;
    }
    return {};
}
