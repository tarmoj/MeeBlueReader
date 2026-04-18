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
    connect(reply, &QNetworkReply::finished, this, [this, reply, rulesName, baseUrl]() {
        reply->deleteLater();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            // 304 is not an error in Qt – treat as "not modified"
            if (status == 304) {
                qDebug() << "FileDownloader: rules up to date (304)";
                emit statusMessage(tr("Up to date"));
                loadLocalRules(rulesName);
                return;
            }
            emit downloadError(tr("Network error: %1").arg(reply->errorString()));
            emit statusMessage(tr("Error: %1").arg(reply->errorString()));
            return;
        }

        if (status == 304) {
            qDebug() << "FileDownloader: rules up to date (304)";
            emit statusMessage(tr("Up to date"));
            loadLocalRules(rulesName);
            return;
        }

        // 200 – save file
        const QByteArray data = reply->readAll();
        const QString localFile = m_localDataPath + "/rules/" + rulesName + ".json";
        QFile f(localFile);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit downloadError(tr("Cannot write %1").arg(localFile));
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

        if (sounds.isEmpty() && images.isEmpty()) {
            emit statusMessage(tr("Done"));
            emit rulesLoaded(rules);
            return;
        }

        m_currentBaseUrl = baseUrl;
        m_pendingSounds   = sounds;
        m_pendingImages   = images;
        m_pendingRules    = rules;
        m_mediaTotal = sounds.size() + images.size();
        m_mediaDone  = 0;
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
        emit rulesLoaded(m_pendingRules);
        m_pendingRules.clear();
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
