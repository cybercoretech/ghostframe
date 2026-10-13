#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

class ImageEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int displayWidth READ displayWidth NOTIFY previewChanged)
    Q_PROPERTY(int displayHeight READ displayHeight NOTIFY previewChanged)
    Q_PROPERTY(double brightness READ brightness NOTIFY adjustmentsChanged)
    Q_PROPERTY(double contrast READ contrast NOTIFY adjustmentsChanged)
    Q_PROPERTY(double saturation READ saturation NOTIFY adjustmentsChanged)
    Q_PROPERTY(bool hasCrop READ hasCrop NOTIFY cropChanged)
    Q_PROPERTY(double cropX READ cropX NOTIFY cropChanged)
    Q_PROPERTY(double cropY READ cropY NOTIFY cropChanged)
    Q_PROPERTY(double cropWidth READ cropWidth NOTIFY cropChanged)
    Q_PROPERTY(double cropHeight READ cropHeight NOTIFY cropChanged)
    Q_PROPERTY(QString sourceTitle READ sourceTitle NOTIFY sourceChanged)
public:
    explicit ImageEditor(QObject *parent = nullptr);
    QUrl previewUrl() const { return m_previewUrl; }
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int displayWidth() const { return m_displayWidth; }
    int displayHeight() const { return m_displayHeight; }
    double brightness() const { return m_brightness; }
    double contrast() const { return m_contrast; }
    double saturation() const { return m_saturation; }
    bool hasCrop() const { return m_hasCrop; }
    double cropX() const { return m_cropX; }
    double cropY() const { return m_cropY; }
    double cropWidth() const { return m_cropWidth; }
    double cropHeight() const { return m_cropHeight; }
    QString sourceTitle() const { return m_title; }

    Q_INVOKABLE void openOriginal(const QString &path, const QString &title, const QString &hash,
                                  const QString &folder, const QString &originalName);
    Q_INVOKABLE void setAdjustments(double brightness, double contrast, double saturation);
    Q_INVOKABLE void rotateBy(int degrees);
    Q_INVOKABLE void setCrop(double x, double y, double width, double height);
    Q_INVOKABLE void clearCrop();
    Q_INVOKABLE void resetEdits();
    Q_INVOKABLE void save(const QString &title, const QString &caption, const QString &tags);
    Q_INVOKABLE void cancel();
signals:
    void previewChanged();
    void readyChanged();
    void busyChanged();
    void statusChanged();
    void adjustmentsChanged();
    void cropChanged();
    void sourceChanged();
    void saved(const QString &path);
    void closeRequested();
private:
    QStringList editArguments(bool preview) const;
    void updatePreview();
    void setBusy(bool value);
    void setStatus(const QString &value);
    bool writeCybermeta(const QString &path, const QString &title, const QString &caption, const QString &tags) const;
    QString m_sourcePath, m_title, m_hash, m_folder, m_originalName, m_status;
    QString m_magick;
    QUrl m_previewUrl;
    QTemporaryDir m_tempDir;
    QTimer m_previewTimer;
    QProcess *m_previewProcess = nullptr;
    QProcess *m_process = nullptr;
    int m_sourceWidth = 0, m_sourceHeight = 0, m_displayWidth = 0, m_displayHeight = 0, m_rotation = 0, m_previewRevision = 0;
    double m_brightness = 0, m_contrast = 0, m_saturation = 100;
    double m_cropX = 0, m_cropY = 0, m_cropWidth = 1, m_cropHeight = 1;
    bool m_ready = false, m_busy = false, m_hasCrop = false;
};
