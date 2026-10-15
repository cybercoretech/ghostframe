#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantList>

// "Looks": saved styles (every adjustment and effect, without geometry or
// text) that can be applied to any photo. Built-in looks ship in :/looks;
// your own are JSON files in <AppData>/looks. File format:
//   {"name": "Neon Noir", "recipe": {"version": 1, "ops": [...]}}
class LookLibrary {
public:
    explicit LookLibrary(QString directory = {});

    // [{name, builtin}], built-ins first, then yours by name.
    QVariantList list() const;
    // The look's recipe JSON, or empty if unknown. Yours shadow built-ins
    // with the same name.
    QJsonObject recipe(const QString &name) const;
    bool save(const QString &name, const QJsonObject &recipe, QString *error = nullptr) const;
    bool remove(const QString &name) const;  // only your own looks
    QString directory() const { return m_directory; }

private:
    QString userPath(const QString &name) const;
    QString m_directory;
};
