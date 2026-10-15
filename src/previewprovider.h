#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// Serves the editor's rendered frames to QML with no temporary files, as
// image://ghostframe-edit/<name>/<revision> where name is preview, before,
// histogram or zoom. The revision only defeats QML's cache; the newest frame
// for a name is always returned.
class PreviewProvider final : public QQuickImageProvider {
public:
    PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    void setImage(const QString &name, const QImage &image) {
        QMutexLocker lock(&m_mutex);
        if (image.isNull()) m_images.remove(name);
        else m_images.insert(name, image);
    }
    void clear() {
        QMutexLocker lock(&m_mutex);
        m_images.clear();
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        QImage image;
        {
            QMutexLocker lock(&m_mutex);
            image = m_images.value(id.section('/', 0, 0));
        }
        if (size) *size = image.size();
        if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return image;
    }

private:
    QMutex m_mutex;
    QHash<QString, QImage> m_images;
};
