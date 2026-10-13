#include "photoservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QUrlQuery>

PhotoService::PhotoService(QObject *parent) : QAbstractListModel(parent) {
    QSettings settings;
    const QString savedUrl = settings.value("photoPrism/serverUrl").toString();
    if (savedUrl.isEmpty()) return;
    m_serverUrl = savedUrl;
    const QString secretTool = QStandardPaths::findExecutable("secret-tool");
    if (secretTool.isEmpty()) {
        setStatus("Saved PhotoPrism address found; install GNOME Keyring/libsecret to restore its password automatically.");
        return;
    }
    QProcess lookup;
    lookup.start(secretTool, {"lookup", "service", "ghostframe", "server", savedUrl});
    if (!lookup.waitForStarted(1500) || !lookup.waitForFinished(3000) || lookup.exitCode() != 0) {
        setStatus("Saved PhotoPrism address found; unlock the desktop keyring to reconnect automatically.");
        return;
    }
    const QString savedToken = QString::fromUtf8(lookup.readAllStandardOutput()).trimmed();
    if (!savedToken.isEmpty()) connectToServer(savedUrl, savedToken);
}

int PhotoService::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_photos.size();
}

QVariant PhotoService::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_photos.size()) return {};
    const auto &photo = m_photos.at(index.row());
    switch (role) {
    case TitleRole: return photo.title;
    case SubtitleRole: return photo.subtitle;
    case ThumbnailRole: return photo.thumbnail;
    case FullRole: return photo.full;
    case HashRole: return photo.hash;
    case PathRole: return photo.folder;
    case OriginalNameRole: return photo.originalName;
    default: return {};
    }
}

QHash<int, QByteArray> PhotoService::roleNames() const {
    return {{TitleRole, "title"}, {SubtitleRole, "subtitle"},
            {ThumbnailRole, "thumbnail"}, {FullRole, "fullImage"}, {HashRole, "hash"},
            {PathRole, "folder"}, {OriginalNameRole, "originalName"}};
}

QString PhotoService::stringField(const QJsonObject &obj, const QStringList &names) {
    for (const auto &name : names) {
        const auto value = obj.value(name);
        if (value.isString() && !value.toString().isEmpty()) return value.toString();
    }
    return {};
}

void PhotoService::setStatus(const QString &value) {
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
}

QUrl PhotoService::apiUrl(const QString &path) const {
    return QUrl(m_serverUrl + path);
}

QNetworkRequest PhotoService::request(const QUrl &url) const {
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QByteArray("Bearer ") + m_token.toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, "Ghostframe/0.1");
    return req;
}

void PhotoService::connectToServer(const QString &url, const QString &token) {
    QUrl parsed(url.trimmed());
    if (!parsed.isValid() || (parsed.scheme() != "http" && parsed.scheme() != "https") || parsed.host().isEmpty()) {
        setStatus("Enter a valid http:// or https:// PhotoPrism address.");
        return;
    }
    m_serverUrl = parsed.toString(QUrl::StripTrailingSlash);
    QSettings().setValue("photoPrism/serverUrl", m_serverUrl);
    m_token = token.trimmed();
    if (m_token.isEmpty()) {
        setStatus("Enter a PhotoPrism app password or scoped API token.");
        return;
    }
    emit serverUrlChanged();
    setStatus("Checking PhotoPrism connection…");
    auto *reply = m_network.get(request(apiUrl("/api/v1/config")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto bytes = reply->readAll();
        const auto error = reply->error();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto previewTokenHeader = reply->rawHeader("X-Preview-Token");
        const auto downloadTokenHeader = reply->rawHeader("X-Download-Token");
        reply->deleteLater();
        if (error != QNetworkReply::NoError || statusCode >= 400) {
            setStatus(statusCode == 401 || statusCode == 403
                          ? "Authentication failed. Check the app password or API token."
                          : "Could not reach PhotoPrism. Check the server address and connection.");
            return;
        }
        const bool credentialStored = storeCredential();
        m_credentialStatus = credentialStored ? " · password saved in desktop keyring" : " · keyring unavailable; password is session-only";
        const auto doc = QJsonDocument::fromJson(bytes);
        const auto obj = doc.object();
        m_previewToken = stringField(obj, {"previewToken", "PreviewToken", "preview_token"});
        if (m_previewToken.isEmpty()) m_previewToken = QString::fromUtf8(previewTokenHeader);
        m_downloadToken = QString::fromUtf8(downloadTokenHeader);
        if (!m_connected) { m_connected = true; emit connectedChanged(); }
        m_offset = 0;
        m_hasMore = true;
        fetchFolders();
        fetchPhotos(false);
    });
}

bool PhotoService::storeCredential() {
    const QString secretTool = QStandardPaths::findExecutable("secret-tool");
    if (secretTool.isEmpty() || m_token.isEmpty()) return false;
    QProcess store;
    store.start(secretTool, {"store", "--label=Ghostframe PhotoPrism", "service", "ghostframe", "server", m_serverUrl});
    if (!store.waitForStarted(1500)) return false;
    if (store.write(m_token.toUtf8() + '\n') < 0) { store.kill(); return false; }
    store.closeWriteChannel();
    return store.waitForFinished(5000) && store.exitStatus() == QProcess::NormalExit && store.exitCode() == 0;
}

void PhotoService::search(const QString &query) {
    if (!m_selectedFolder.isEmpty()) {
        m_selectedFolder.clear();
        emit selectedFolderChanged();
    }
    if (m_query == query) return;
    m_query = query;
    if (m_connected) { m_offset = 0; m_hasMore = true; fetchPhotos(false); }
}

void PhotoService::browseFolder(const QString &path) {
    const QString cleanPath = path.trimmed();
    if (m_selectedFolder != cleanPath) {
        m_selectedFolder = cleanPath;
        emit selectedFolderChanged();
    }
    QString folderQuery;
    if (!cleanPath.isEmpty()) {
        QString escapedPath = cleanPath;
        escapedPath.replace("\\", "\\\\");
        escapedPath.replace("\"", "\\\"");
        folderQuery = QStringLiteral("path:\"%1|%1/*\"").arg(escapedPath);
    }
    if (m_query == folderQuery && !m_photos.isEmpty()) return;
    m_query = folderQuery;
    if (m_connected) { m_offset = 0; m_hasMore = true; fetchPhotos(false); }
}

void PhotoService::refreshFolders() {
    if (m_connected) fetchFolders();
}

void PhotoService::downloadOriginal(const QString &hash, const QString &title, const QString &folder, const QString &originalName) {
    if (!m_connected || hash.isEmpty()) {
        emit originalDownloadFailed("Connect to PhotoPrism and select an indexed photo first.");
        return;
    }
    if (m_downloadToken.isEmpty()) {
        emit originalDownloadFailed("PhotoPrism did not provide a full-resolution download token. Reconnect and try again.");
        return;
    }
    QUrl url = apiUrl("/api/v1/dl/" + hash);
    QUrlQuery query;
    query.addQueryItem("t", m_downloadToken);
    url.setQuery(query);
    auto *reply = m_network.get(request(url));
    const QString suffix = QFileInfo(originalName).suffix().replace(QRegularExpression("[^A-Za-z0-9]"), "");
    const QString path = m_downloadDir.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + (suffix.isEmpty() ? QString() : "." + suffix));
    auto *file = new QFile(path, reply);
    if (!file->open(QIODevice::WriteOnly)) {
        reply->abort();
        file->deleteLater();
        reply->deleteLater();
        emit originalDownloadFailed("Could not create a temporary full-resolution working copy.");
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] {
        if (file->write(reply->readAll()) < 0) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, path, title, hash, folder, originalName] {
        const auto tail = reply->readAll();
        bool writeOk = tail.isEmpty() || file->write(tail) >= 0;
        const auto error = reply->error();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        file->close();
        file->deleteLater();
        reply->deleteLater();
        if (!writeOk || error != QNetworkReply::NoError || code >= 400 || QFileInfo(path).size() == 0) {
            QFile::remove(path);
            emit originalDownloadFailed(code == 401 || code == 403
                ? "PhotoPrism denied original download. Ensure this account has photo read and download access."
                : "Could not download the full-resolution original from PhotoPrism.");
            return;
        }
        emit originalDownloaded(title, hash, folder, path, originalName);
    });
}

void PhotoService::fetchFolders() {
    if (!m_connected) return;
    QUrl url = apiUrl("/api/v1/folders/originals");
    QUrlQuery query;
    query.addQueryItem("recursive", "true");
    url.setQuery(query);
    auto *reply = m_network.get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto bytes = reply->readAll();
        const auto error = reply->error();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (error != QNetworkReply::NoError || statusCode >= 400) {
            m_folders.clear();
            emit foldersChanged();
            return;
        }
        const auto response = QJsonDocument::fromJson(bytes).object();
        const auto folderArray = response.value("folders").toArray();
        QVariantList folders;
        folders.reserve(folderArray.size());
        for (const auto &value : folderArray) {
            const auto folder = value.toObject();
            const QString path = stringField(folder, {"Path", "path"});
            if (path.isEmpty()) continue;
            const QString title = stringField(folder, {"Title", "title"});
            const int depth = path.count('/');
            folders.append(QVariantMap{
                {"path", path},
                {"title", title.isEmpty() ? path.section('/', -1) : title},
                {"depth", depth}
            });
        }
        m_folders = std::move(folders);
        emit foldersChanged();
    });
}

void PhotoService::loadMore() {
    if (m_connected && m_hasMore && !m_loading) fetchPhotos(true);
}

void PhotoService::fetchPhotos(bool append) {
    if (m_loading) {
        if (!append) m_pendingRefresh = true;
        return;
    }
    m_loading = true;
    setStatus(m_query.isEmpty() ? "Loading your library…" : "Searching your library…");
    QUrl url = apiUrl("/api/v1/photos");
    QUrlQuery query;
    query.addQueryItem("count", "80");
    query.addQueryItem("offset", QString::number(append ? m_offset : 0));
    query.addQueryItem("merged", "true");
    query.addQueryItem("primary", "true");
    query.addQueryItem("order", "newest");
    if (!m_query.isEmpty()) query.addQueryItem("q", m_query);
    url.setQuery(query);
    auto *reply = m_network.get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, append] {
        const auto bytes = reply->readAll();
        const auto error = reply->error();
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto previewTokenHeader = reply->rawHeader("X-Preview-Token");
        const auto downloadTokenHeader = reply->rawHeader("X-Download-Token");
        reply->deleteLater();
        m_loading = false;
        if (m_pendingRefresh) {
            m_pendingRefresh = false;
            fetchPhotos(false);
            return;
        }
        if (error != QNetworkReply::NoError || statusCode >= 400) {
            setStatus(statusCode == 401 || statusCode == 403
                          ? "PhotoPrism rejected the token. Generate one with photo read access."
                          : "Photo search failed. Check the PhotoPrism API and try again.");
            return;
        }
        const auto doc = QJsonDocument::fromJson(bytes);
        if (!doc.isArray()) { setStatus("PhotoPrism returned an unexpected response."); return; }
        if (!previewTokenHeader.isEmpty()) m_previewToken = QString::fromUtf8(previewTokenHeader);
        if (!downloadTokenHeader.isEmpty()) m_downloadToken = QString::fromUtf8(downloadTokenHeader);
        const auto array = doc.array();
        QVector<Photo> found;
        found.reserve(array.size());
        for (const auto &value : array) {
            const auto obj = value.toObject();
            const auto hash = stringField(obj, {"Hash", "hash"});
            if (hash.isEmpty()) continue;
            Photo photo;
            photo.hash = hash;
            photo.title = stringField(obj, {"Title", "title", "Name", "name"});
            if (photo.title.isEmpty()) photo.title = "Untitled image";
            photo.subtitle = stringField(obj, {"TakenAtLocal", "TakenAt", "takenAt", "Year"});
            photo.folder = stringField(obj, {"Path", "path"});
            photo.originalName = stringField(obj, {"FileName", "fileName", "Name", "name"});
            const QString token = m_previewToken.isEmpty() ? "public" : m_previewToken;
            const QString base = m_serverUrl + "/api/v1/t/" + hash + "/" + token + "/";
            photo.thumbnail = QUrl(base + "fit_720");
            photo.full = QUrl(base + "fit_2048");
            found.append(photo);
        }
        if (!append) { beginResetModel(); m_photos.clear(); endResetModel(); m_offset = 0; }
        const int first = m_photos.size();
        if (!found.isEmpty()) {
            beginInsertRows({}, first, first + found.size() - 1);
            for (auto &photo : found) m_photos.append(std::move(photo));
            endInsertRows();
        }
        m_offset = append ? m_offset + array.size() : array.size();
        m_hasMore = array.size() >= 80;
        emit photoCountChanged();
        setStatus(m_photos.isEmpty() ? "No images found. Index some photos in PhotoPrism, then refresh." :
                  QString::number(m_photos.size()) + " images · connected to PhotoPrism" + m_credentialStatus);
    });
}

void PhotoService::disconnectFromServer() {
    m_connected = false;
    m_serverUrl.clear();
    m_token.clear();
    m_previewToken.clear();
    m_downloadToken.clear();
    m_selectedFolder.clear();
    m_folders.clear();
    m_offset = 0;
    m_hasMore = true;
    beginResetModel(); m_photos.clear(); endResetModel();
    emit connectedChanged();
    emit serverUrlChanged();
    emit photoCountChanged();
    emit foldersChanged();
    emit selectedFolderChanged();
    setStatus("Enter your PhotoPrism server details to connect.");
}
