#include "filedownloader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

FileDownloader::FileDownloader(QObject *parent)
    : QObject(parent)
{
    m_localDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    qDebug() << "FileDownloader: local data path is" << m_localDataPath;

    // Ensure base directories exist
    QDir dir;
    dir.mkpath(m_localDataPath + "/rules");
    dir.mkpath(m_localDataPath + "/sounds");
    dir.mkpath(m_localDataPath + "/images");
}

QString FileDownloader::localDataPath() const
{
    return m_localDataPath;
}

// ---------------------------------------------------------------------------
// Public invokables
// ---------------------------------------------------------------------------

void FileDownloader::checkAndDownload(const QString &baseUrl, const QString &rulesName)
{
    checkAndDownloadInternal(baseUrl, rulesName, nullptr);
}

void FileDownloader::checkAndDownloadAll(const QString &baseUrl)
{
    m_queueBaseUrl = baseUrl;
    m_rulesQueue = {"rules1", "rules2", "rules3", "rules4"};
    downloadNextQueuedRulesFile();
}

void FileDownloader::downloadNextQueuedRulesFile()
{
    if (m_rulesQueue.isEmpty()) {
        emit statusMessage(tr("All rules files checked."));
        emit allRulesChecked();
        return;
    }
    const QString rulesName = m_rulesQueue.takeFirst();
    checkAndDownloadInternal(m_queueBaseUrl, rulesName, [this]() {
        downloadNextQueuedRulesFile();
    });
}

void FileDownloader::checkAndDownloadInternal(const QString &baseUrl, const QString &rulesName,
                                              std::function<void()> onDone)
{
    const QString url = baseUrl.trimmed() + "/rules/" + rulesName + ".json";
    qDebug() << "FileDownloader: checking" << url;
    emit statusMessage(tr("Checking %1…").arg(rulesName + ".json"));

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    // Conditional GET headers
    QSettings s;
    const QString etag = s.value(etagKey(rulesName)).toString();
    const QString lastMod = s.value(lastModKey(rulesName)).toString();
    if (!etag.isEmpty())
        req.setRawHeader("If-None-Match", etag.toUtf8());
    else if (!lastMod.isEmpty())
        req.setRawHeader("If-Modified-Since", lastMod.toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, rulesName, baseUrl, onDone]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            // 304 is not an error in Qt – treat as "not modified"
            if (status == 304) {
                qDebug() << "FileDownloader: rules up to date (304)";
                emit statusMessage(tr("Up to date"));
                loadLocalRules(rulesName);
                if (onDone) onDone();
                return;
            }
            emit downloadError(tr("Network error: %1").arg(reply->errorString()));
            emit statusMessage(tr("Error: %1").arg(reply->errorString()));
            if (onDone) onDone();
            return;
        }

        if (status == 304) {
            qDebug() << "FileDownloader: rules up to date (304)";
            emit statusMessage(tr("Up to date"));
            loadLocalRules(rulesName);
            if (onDone) onDone();
            return;
        }

        // 200 – save file
        const QByteArray data = reply->readAll();
        const QString localFile = m_localDataPath + "/rules/" + rulesName + ".json";
        QFile f(localFile);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit downloadError(tr("Cannot write %1").arg(localFile));
            if (onDone) onDone();
            return;
        }
        f.write(data);
        f.close();

        // Persist ETag / Last-Modified for next conditional request
        QSettings s2;
        const QString newEtag =
            QString::fromUtf8(reply->rawHeader("ETag"));
        const QString newLastMod =
            QString::fromUtf8(reply->rawHeader("Last-Modified"));
        if (!newEtag.isEmpty())
            s2.setValue(etagKey(rulesName), newEtag);
        else if (!newLastMod.isEmpty())
            s2.setValue(lastModKey(rulesName), newLastMod);

        // Parse rules and kick off media downloads
        const QVariantList rules = parseRulesJson(data);
        QStringList sounds, images;
        collectMediaFilenames(rules, sounds, images);

        qDebug() << "FileDownloader: rules downloaded," << sounds.size()
                 << "sounds," << images.size() << "images to fetch";

        // Reload rules right away so the app picks them up immediately;
        // media referenced by the rules keeps downloading in the background.
        emit rulesLoaded(rules);

        if (sounds.isEmpty() && images.isEmpty()) {
            emit statusMessage(tr("Done"));
            if (onDone) onDone();
            return;
        }

        m_currentBaseUrl = baseUrl;
        m_pendingSounds   = sounds;
        m_pendingImages   = images;
        m_mediaTotal = sounds.size() + images.size();
        m_mediaDone  = 0;
        m_onRulesFileDone = onDone;
        emit statusMessage(tr("Downloading media (0/%1)…").arg(m_mediaTotal));
        downloadNextMediaFile();
    });
}

void FileDownloader::loadLocalRules(const QString &rulesName)
{
    const QString localFile = m_localDataPath + "/rules/" + rulesName + ".json";
    QFile f(localFile);
    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "FileDownloader: no local rules file" << localFile;
        emit statusMessage(tr("Loaded 0 rules."));
        emit rulesLoaded({});
        return;
    }
    const QByteArray data = f.readAll();
    f.close();
    const QVariantList rules = parseRulesJson(data);
    qDebug() << "FileDownloader: loaded" << rules.size() << "rules from" << localFile;
    emit statusMessage(tr("Loaded %1 rules.").arg(rules.size()));
    emit rulesLoaded(rules);
}

QStringList FileDownloader::listSoundFiles() const
{
    QDir dir(m_localDataPath + "/sounds");
    return dir.entryList(QDir::Files, QDir::Name);
}

QStringList FileDownloader::listImageFiles() const
{
    QDir dir(m_localDataPath + "/images");
    return dir.entryList(QDir::Files, QDir::Name);
}

QString FileDownloader::readRulesFileContent(const QString &rulesName) const
{
    QFile f(m_localDataPath + "/rules/" + rulesName + ".json");
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FileDownloader::downloadNextMediaFile()
{
    // Pick next pending file: sounds first, then images
    QString subdir;
    QString filename;
    if (!m_pendingSounds.isEmpty()) {
        filename = m_pendingSounds.takeFirst();
        subdir   = "sounds";
    } else if (!m_pendingImages.isEmpty()) {
        filename = m_pendingImages.takeFirst();
        subdir   = "images";
    } else {
        // All done
        emit statusMessage(tr("Done"));
        const std::function<void()> onDone = m_onRulesFileDone;
        m_onRulesFileDone = nullptr;
        if (onDone) onDone();
        return;
    }

    const QString url    = m_currentBaseUrl + "/" + subdir + "/" + filename;
    const QString dest   = m_localDataPath  + "/" + subdir + "/" + filename;
    const QString ekey   = mediaEtagKey(subdir + "/" + filename);

    qDebug() << "FileDownloader: media GET" << url;

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QSettings s;
    const QString etag = s.value(ekey).toString();
    if (!etag.isEmpty())
        req.setRawHeader("If-None-Match", etag.toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dest, ekey, filename, subdir]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError || status == 304) {
            if (status != 304 && reply->error() == QNetworkReply::NoError) {
                // Save file
                QFile f(dest);
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    f.write(reply->readAll());
                    f.close();
                }
                // Persist ETag
                const QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
                if (!newEtag.isEmpty()) {
                    QSettings s2;
                    s2.setValue(ekey, newEtag);
                }
            }
        } else {
            qWarning() << "FileDownloader: failed to download" << filename
                       << reply->errorString();
        }

        ++m_mediaDone;
        emit statusMessage(
            tr("Downloading media (%1/%2)…").arg(m_mediaDone).arg(m_mediaTotal));
        downloadNextMediaFile();
    });
}

QVariantList FileDownloader::parseRulesJson(const QByteArray &data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "FileDownloader: JSON parse error:" << err.errorString();
        return {};
    }
    QVariantList result;
    // Accept either an array of events or an object with an "events" array
    if (doc.isArray()) {
        result = doc.array().toVariantList();
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("events") && obj["events"].isArray())
            result = obj["events"].toArray().toVariantList();
        else
            result = QVariantList{doc.object().toVariantMap()};
    }
    return result;
}

void FileDownloader::collectMediaFilenames(const QVariantList &rules,
                                           QStringList &soundFiles,
                                           QStringList &imageFiles)
{
    QSet<QString> sounds, images;
    for (const QVariant &v : rules) {
        const QVariantMap ev = v.toMap();
        const QString sound  = ev.value("sound").toString().trimmed();
        const QString image  = ev.value("image").toString().trimmed();
        if (!sound.isEmpty())
            sounds.insert(sound);
        if (!image.isEmpty())
            images.insert(image);
    }
    soundFiles = QStringList(sounds.begin(), sounds.end());
    imageFiles = QStringList(images.begin(), images.end());
}

QString FileDownloader::etagKey(const QString &rulesName) const
{
    return "etag/rules/" + rulesName;
}

QString FileDownloader::lastModKey(const QString &rulesName) const
{
    return "lastmod/rules/" + rulesName;
}

QString FileDownloader::mediaEtagKey(const QString &filename) const
{
    return "etag/media/" + filename;
}
