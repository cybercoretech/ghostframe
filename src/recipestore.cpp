#include "recipestore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

RecipeStore::RecipeStore(QString directory) : m_directory(std::move(directory)) {
    if (m_directory.isEmpty())
        m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recipes";
}

QString RecipeStore::keyForHash(const QString &photoHash) {
    // PhotoPrism hashes are hex; anything else is reduced to safe characters.
    QString key = photoHash.toLower();
    key.remove(QRegularExpression("[^a-z0-9]"));
    return key.isEmpty() ? QString() : "pp-" + key.left(64);
}

QString RecipeStore::keyForPath(const QString &absolutePath) {
    return "local-" + QString::fromLatin1(QCryptographicHash::hash(absolutePath.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QString RecipeStore::pathFor(const QString &key) const { return m_directory + "/" + key + ".json"; }

bool RecipeStore::load(const QString &key, EditRecipe *out) const {
    if (key.isEmpty()) return false;
    QFile file(pathFor(key));
    if (!file.open(QIODevice::ReadOnly)) return false;
    return EditRecipe::fromJson(QJsonDocument::fromJson(file.readAll()).object(), out);
}

bool RecipeStore::save(const QString &key, const EditRecipe &recipe) const {
    if (key.isEmpty()) return false;
    if (recipe.isIdentity()) {
        QFile::remove(pathFor(key));
        return true;
    }
    if (!QDir().mkpath(m_directory)) return false;
    QSaveFile file(pathFor(key));
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(recipe.toJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}
