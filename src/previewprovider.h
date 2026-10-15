#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// Serves the editor's latest rendered frame to QML as
// image://ghostframe-edit/preview/<revision>, with no temporary files.
// The revision only defeats QML's cache; the newest frame is always returned.
class PreviewProvider final : public QQuickImageProvider {
public:
    PreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    void setImage(const QImage &image) {
        QMutexLocker lock(&m_mutex);
        m_image = image;
    }

    QImage requestImage(const QString &, QSize *size, const QSize &requestedSize) override {
        QImage image;
        {
            QMutexLocker lock(&m_mutex);
            image = m_image;
        }
        if (size) *size = image.size();
        if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty())
            return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return image;
    }

private:
    QMutex m_mutex;
    QImage m_image;
};
