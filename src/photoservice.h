#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QUrl>

class PhotoService final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(int photoCount READ rowCount NOTIFY photoCountChanged)
public:
    enum Roles { TitleRole = Qt::UserRole + 1, SubtitleRole, ThumbnailRole, FullRole, HashRole };
    explicit PhotoService(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString status() const { return m_status; }
    bool connected() const { return m_connected; }
    QString serverUrl() const { return m_serverUrl; }

    Q_INVOKABLE void connectToServer(const QString &url, const QString &token);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void disconnectFromServer();

signals:
    void statusChanged();
    void connectedChanged();
    void serverUrlChanged();
    void photoCountChanged();

private:
    struct Photo {
        QString title, subtitle, hash;
        QUrl thumbnail, full;
    };
    void setStatus(const QString &value);
    void fetchPhotos(bool append);
    QNetworkRequest request(const QUrl &url) const;
    QUrl apiUrl(const QString &path) const;
    static QString stringField(const QJsonObject &obj, const QStringList &names);

    QNetworkAccessManager m_network;
    QVector<Photo> m_photos;
    QString m_status = QStringLiteral("Enter your PhotoPrism server details to connect.");
    QString m_serverUrl, m_token, m_previewToken, m_query;
    int m_offset = 0;
    bool m_connected = false, m_loading = false, m_hasMore = true;
};
