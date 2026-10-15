#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

// The CYBERGRID palette catalog for gradient maps: the palettes bundled from
// the cybercore repo (:/cybergrid) plus custom themes in
// $CYBERCORE_CONFIG_DIR or ~/.config/cybercore/themes. Each palette has the
// 11 CYBERGRID roles (bg, white, acid_green, hot_pink, purple, cyan, orange,
// red, panel, line, muted).
class CyberGrid final : public QObject {
    Q_OBJECT
    // [{id, family, name, colors: {role: hex}}], sorted by family then id.
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    // The desktop's active theme (from active-theme.json), if known.
    Q_PROPERTY(QString activeId READ activeId CONSTANT)
    // [{id, label}] of the gradient styles below.
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)
public:
    explicit CyberGrid(QObject *parent = nullptr);

    QVariantList themes() const { return m_themes; }
    QString activeId() const { return m_activeId; }
    QVariantList styles() const;

    // Gradient stops (hex, dark → light) for a theme and style, e.g.
    // "neon" = bg, purple, hot_pink, cyan, white. Empty if unknown.
    Q_INVOKABLE QStringList gradient(const QString &themeId, const QString &style) const;

    static QString configDirectory();

private:
    void addTheme(const QString &id, const QString &family, const QJsonObject &json);
    QVariantList m_themes;
    QString m_activeId;
};
