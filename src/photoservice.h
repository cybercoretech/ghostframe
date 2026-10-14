#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QVariantList>
#include <QUrl>

class PhotoService final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool startupConnecting READ startupConnecting NOTIFY startupConnectingChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(int photoCount READ rowCount NOTIFY photoCountChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QString selectedFolder READ selectedFolder NOTIFY selectedFolderChanged)
public:
    enum Roles { TitleRole = Qt::UserRole + 1, SubtitleRole, ThumbnailRole, FullRole, HashRole, PathRole, OriginalNameRole };
    explicit PhotoService(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString status() const { return m_status; }
    bool connected() const { return m_connected; }
    bool startupConnecting() const { return m_startupConnecting; }
    QString serverUrl() const { return m_serverUrl; }
    QVariantList folders() const { return m_folders; }
    QString selectedFolder() const { return m_selectedFolder; }

    Q_INVOKABLE void connectToServer(const QString &url, const QString &token);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void browseFolder(const QString &path);
    Q_INVOKABLE void refreshFolders();
    Q_INVOKABLE void downloadOriginal(const QString &hash, const QString &title, const QString &folder, const QString &originalName);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void disconnectFromServer();

signals:
    void statusChanged();
    void connectedChanged();
    void startupConnectingChanged();
    void serverUrlChanged();
    void photoCountChanged();
    void foldersChanged();
    void selectedFolderChanged();
    void originalDownloaded(const QString &title, const QString &hash, const QString &folder, const QString &filePath, const QString &originalName);
    void originalDownloadFailed(const QString &message);

private:
    struct Photo {
        QString title, subtitle, hash, folder, originalName;
        QUrl thumbnail, full;
    };
    void setStatus(const QString &value);
    void fetchPhotos(bool append);
    void fetchFolders();
    QString loadLocalCredential() const;
    bool storeLocalCredential() const;
    QString localCredentialPath() const;
    QNetworkRequest request(const QUrl &url) const;
    QUrl apiUrl(const QString &path) const;
    static QString stringField(const QJsonObject &obj, const QStringList &names);

    QNetworkAccessManager m_network;
    QVector<Photo> m_photos;
    QVariantList m_folders;
    QString m_status = QStringLiteral("Enter your PhotoPrism server details to connect.");
    QString m_serverUrl, m_token, m_previewToken, m_downloadToken, m_query, m_selectedFolder, m_credentialStatus;
    QTemporaryDir m_downloadDir;
    int m_offset = 0;
    bool m_connected = false, m_startupConnecting = false, m_loading = false, m_pendingRefresh = false, m_hasMore = true;
};
