#pragma once

#include "editrecipe.h"

#include <QString>

// Remembers each photo's current edit, so reopening a photo (or moving to
// the next one with [ / ]) never loses work. One JSON recipe per photo in
// <AppData>/recipes/<key>.json, where the key is the PhotoPrism hash (or a
// hash of the path for local files). Writes are atomic.
class RecipeStore {
public:
    explicit RecipeStore(QString directory = {});

    static QString keyForHash(const QString &photoHash);
    static QString keyForPath(const QString &absolutePath);

    bool load(const QString &key, EditRecipe *out) const;
    // An identity recipe removes the file (nothing to remember).
    bool save(const QString &key, const EditRecipe &recipe) const;
    QString directory() const { return m_directory; }

private:
    QString pathFor(const QString &key) const;
    QString m_directory;
};
