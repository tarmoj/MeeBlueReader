#include "filedownloader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
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
    dir.mkpath(m_localDataPath + "/pdf");
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

// Parses rules JSON (freshly downloaded or re-read from disk after a 304), emits
// rulesLoaded(), and kicks off media downloads for anything referenced that isn't
// local yet. Must run even when the rules file itself is unchanged, since media
// can still be missing (e.g. added to the server without touching rules{N}.json).
void FileDownloader::processRulesDataAndFetchMedia(const QByteArray &data, const QString &baseUrl,
                                                    std::function<void()> onDone)
{
    const QVariantList rules = parseRulesJson(data);
    QStringList sounds, images, soundPatterns, imagePatterns, pdfFiles, pdfPatterns;
    collectMediaFilenames(rules, sounds, images, soundPatterns, imagePatterns, pdfFiles, pdfPatterns);

    QStringList otherLiteral, otherPatterns;
    collectOtherFiles(data, otherLiteral, otherPatterns);

    // Merge rule-referenced PDFs (rule.pdf) in with top-level other_files, deduped,
    // so a rule can reference a PDF without also needing a redundant other_files entry.
    QSet<QString> literalSet(otherLiteral.begin(), otherLiteral.end());
    for (const QString &pdf : pdfFiles)
        literalSet.insert(pdf);
    otherLiteral = QStringList(literalSet.begin(), literalSet.end());

    QSet<QString> patternSet(otherPatterns.begin(), otherPatterns.end());
    for (const QString &pattern : pdfPatterns)
        patternSet.insert(pattern);
    otherPatterns = QStringList(patternSet.begin(), patternSet.end());

    QList<QPair<QString, QString>> pendingOtherFiles;
    for (const QString &filename : otherLiteral) {
        const QString subdir = otherFileSubdir(filename);
        if (subdir.isEmpty()) {
            qWarning() << "FileDownloader: other_files entry has unrecognized extension, skipping:" << filename;
            continue;
        }
        if (subdir == "images" && images.contains(filename)) continue;
        if (subdir == "sounds" && sounds.contains(filename)) continue;
        pendingOtherFiles.append({filename, subdir});
    }

    QList<QPair<QString, QString>> pendingOtherFilePatterns;
    for (const QString &pattern : otherPatterns) {
        const QString subdir = otherFileSubdir(pattern);
        if (subdir.isEmpty()) {
            qWarning() << "FileDownloader: other_files pattern has unrecognized extension, skipping:" << pattern;
            continue;
        }
        pendingOtherFilePatterns.append({pattern, subdir});
    }

    qDebug() << "FileDownloader: rules have" << sounds.size()
             << "sounds," << images.size() << "images," << soundPatterns.size()
             << "sound wildcards," << imagePatterns.size() << "image wildcards,"
             << pendingOtherFiles.size() << "other_files,"
             << pendingOtherFilePatterns.size() << "other_files wildcards to check";

    // Reload rules right away so the app picks them up immediately;
    // media referenced by the rules keeps downloading in the background.
    emit rulesLoaded(rules);

    if (sounds.isEmpty() && images.isEmpty() && pendingOtherFiles.isEmpty()
        && soundPatterns.isEmpty() && imagePatterns.isEmpty() && pendingOtherFilePatterns.isEmpty()) {
        emit statusMessage(tr("Done"));
        if (onDone) onDone();
        return;
    }

    m_currentBaseUrl = baseUrl;
    m_pendingSounds   = sounds;
    m_pendingImages   = images;
    m_pendingSoundPatterns = soundPatterns;
    m_pendingImagePatterns = imagePatterns;
    m_pendingOtherFiles = pendingOtherFiles;
    m_pendingOtherFilePatterns = pendingOtherFilePatterns;
    m_onRulesFileDone = onDone;
    emit statusMessage(tr("Resolving media…"));
    resolveWildcardPatterns();
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
                const QString localFile = m_localDataPath + "/rules/" + rulesName + ".json";
                QFile f(localFile);
                const QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
                processRulesDataAndFetchMedia(data, baseUrl, onDone);
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
            const QString localFile = m_localDataPath + "/rules/" + rulesName + ".json";
            QFile f(localFile);
            const QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            processRulesDataAndFetchMedia(data, baseUrl, onDone);
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

        processRulesDataAndFetchMedia(data, baseUrl, onDone);
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

QString FileDownloader::pickSoundFile(const QString &pattern) const
{
    if (!pattern.contains('*') && !pattern.contains('?'))
        return pattern;

    QDir dir(m_localDataPath + "/sounds");
    dir.setNameFilters({pattern});
    const QStringList matches = dir.entryList(QDir::Files, QDir::Name);
    if (matches.isEmpty())
        return QString();
    if (matches.size() == 1)
        return matches.first();
    return matches.at(QRandomGenerator::global()->bounded(matches.size()));
}

QStringList FileDownloader::listImageFiles() const
{
    QDir dir(m_localDataPath + "/images");
    return dir.entryList(QDir::Files, QDir::Name);
}

QStringList FileDownloader::listPdfFiles() const
{
    QDir dir(m_localDataPath + "/pdf");
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

QStringList FileDownloader::parseDirectoryListing(const QString &html)
{
    QStringList names;
    static const QRegularExpression hrefRe(
        "href=\"([^\"]+)\"", QRegularExpression::CaseInsensitiveOption);
    auto it = hrefRe.globalMatch(html);
    while (it.hasNext()) {
        const QString href = it.next().captured(1);
        if (href.isEmpty() || href.startsWith('?') || href.startsWith('/')
            || href.contains("://") || href == "../" || href == "./")
            continue;
        QString name = QUrl(href).path();
        if (name.startsWith("./"))
            name.remove(0, 2);
        if (!name.isEmpty() && !name.endsWith('/'))
            names << name;
    }
    return names;
}

void FileDownloader::resolveWildcardPatterns()
{
    if (!m_pendingSoundPatterns.isEmpty()) {
        const QString url = m_currentBaseUrl + "/sounds/";
        QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                const QStringList names = parseDirectoryListing(QString::fromUtf8(reply->readAll()));
                for (const QString &pattern : std::as_const(m_pendingSoundPatterns)) {
                    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern));
                    for (const QString &name : names) {
                        if (re.match(name).hasMatch() && !m_pendingSounds.contains(name))
                            m_pendingSounds << name;
                    }
                }
            } else {
                emit statusMessage(tr("Could not list sounds for wildcard rules (%1)")
                                        .arg(reply->errorString()));
            }
            m_pendingSoundPatterns.clear();
            resolveWildcardPatterns();
        });
        return;
    }

    if (!m_pendingImagePatterns.isEmpty()) {
        const QString url = m_currentBaseUrl + "/images/";
        QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                const QStringList names = parseDirectoryListing(QString::fromUtf8(reply->readAll()));
                for (const QString &pattern : std::as_const(m_pendingImagePatterns)) {
                    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern));
                    for (const QString &name : names) {
                        if (re.match(name).hasMatch() && !m_pendingImages.contains(name))
                            m_pendingImages << name;
                    }
                }
            } else {
                emit statusMessage(tr("Could not list images for wildcard rules (%1)")
                                        .arg(reply->errorString()));
            }
            m_pendingImagePatterns.clear();
            resolveWildcardPatterns();
        });
        return;
    }

    if (!m_pendingOtherFilePatterns.isEmpty()) {
        // Resolve one subdir's worth of patterns per round-trip (images/sounds/pdf
        // can each appear among other_files patterns).
        const QString subdir = m_pendingOtherFilePatterns.first().second;
        QStringList patternsForSubdir;
        QList<QPair<QString, QString>> remaining;
        for (const auto &entry : std::as_const(m_pendingOtherFilePatterns)) {
            if (entry.second == subdir)
                patternsForSubdir << entry.first;
            else
                remaining << entry;
        }
        m_pendingOtherFilePatterns = remaining;

        const QString url = m_currentBaseUrl + "/" + subdir + "/";
        QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply, subdir, patternsForSubdir]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                const QStringList names = parseDirectoryListing(QString::fromUtf8(reply->readAll()));
                for (const QString &pattern : patternsForSubdir) {
                    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern));
                    for (const QString &name : names) {
                        if (re.match(name).hasMatch() && !isAlreadyPending(name, subdir))
                            m_pendingOtherFiles.append({name, subdir});
                    }
                }
            } else {
                emit statusMessage(tr("Could not list %1 for wildcard other_files (%2)")
                                        .arg(subdir, reply->errorString()));
            }
            resolveWildcardPatterns();
        });
        return;
    }

    beginMediaDownloads();
}

void FileDownloader::beginMediaDownloads()
{
    if (m_pendingSounds.isEmpty() && m_pendingImages.isEmpty() && m_pendingOtherFiles.isEmpty()) {
        emit statusMessage(tr("Done"));
        const std::function<void()> onDone = m_onRulesFileDone;
        m_onRulesFileDone = nullptr;
        if (onDone) onDone();
        return;
    }

    m_mediaTotal = m_pendingSounds.size() + m_pendingImages.size() + m_pendingOtherFiles.size();
    m_mediaDone  = 0;
    emit statusMessage(tr("Downloading media (0/%1)…").arg(m_mediaTotal));
    downloadNextMediaFile();
}

void FileDownloader::downloadNextMediaFile()
{
    // Pick next pending file: sounds first, then images, then other_files
    QString subdir;
    QString filename;
    if (!m_pendingSounds.isEmpty()) {
        filename = m_pendingSounds.takeFirst();
        subdir   = "sounds";
    } else if (!m_pendingImages.isEmpty()) {
        filename = m_pendingImages.takeFirst();
        subdir   = "images";
    } else if (!m_pendingOtherFiles.isEmpty()) {
        const auto entry = m_pendingOtherFiles.takeFirst();
        filename = entry.first;
        subdir   = entry.second;
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

QVariantList FileDownloader::parseRulesText(const QString &jsonText)
{
    return parseRulesJson(jsonText.toUtf8());
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
                                           QStringList &imageFiles,
                                           QStringList &soundPatterns,
                                           QStringList &imagePatterns,
                                           QStringList &pdfFiles,
                                           QStringList &pdfPatterns)
{
    QSet<QString> sounds, images, soundPat, imagePat, pdfs, pdfPat;
    for (const QVariant &v : rules) {
        const QVariantMap ev = v.toMap();
        const QString sound  = ev.value("sound").toString().trimmed();
        const QString image  = ev.value("image").toString().trimmed();
        const QString pdf    = ev.value("pdf").toString().trimmed();
        if (!sound.isEmpty()) {
            if (sound.contains('*') || sound.contains('?'))
                soundPat.insert(sound);
            else
                sounds.insert(sound);
        }
        if (!image.isEmpty() && image.compare("none", Qt::CaseInsensitive) != 0) {
            if (image.contains('*') || image.contains('?'))
                imagePat.insert(image);
            else
                images.insert(image);
        }
        if (!pdf.isEmpty() && pdf.compare("none", Qt::CaseInsensitive) != 0) {
            if (pdf.contains('*') || pdf.contains('?'))
                pdfPat.insert(pdf);
            else
                pdfs.insert(pdf);
        }
    }
    soundFiles    = QStringList(sounds.begin(), sounds.end());
    imageFiles    = QStringList(images.begin(), images.end());
    soundPatterns = QStringList(soundPat.begin(), soundPat.end());
    imagePatterns = QStringList(imagePat.begin(), imagePat.end());
    pdfFiles      = QStringList(pdfs.begin(), pdfs.end());
    pdfPatterns   = QStringList(pdfPat.begin(), pdfPat.end());
}

void FileDownloader::collectOtherFiles(const QByteArray &data, QStringList &literalFiles,
                                       QStringList &patternFiles) const
{
    literalFiles.clear();
    patternFiles.clear();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    if (!obj.contains("other_files") || !obj["other_files"].isArray())
        return;

    for (const QJsonValue &v : obj["other_files"].toArray()) {
        const QString filename = v.toString().trimmed();
        if (filename.isEmpty())
            continue;
        if (filename.contains('*') || filename.contains('?'))
            patternFiles << filename;
        else
            literalFiles << filename;
    }
}

QString FileDownloader::otherFileSubdir(const QString &filename)
{
    const QString ext = filename.section('.', -1).toLower();
    static const QSet<QString> imageExts = {"png", "jpg", "jpeg", "gif", "bmp", "webp"};
    static const QSet<QString> soundExts = {"mp3", "wav", "m4a", "aac", "ogg", "flac"};
    if (imageExts.contains(ext))
        return "images";
    if (soundExts.contains(ext))
        return "sounds";
    if (ext == "pdf")
        return "pdf";
    return QString();
}

bool FileDownloader::isAlreadyPending(const QString &filename, const QString &subdir) const
{
    if (subdir == "images" && m_pendingImages.contains(filename))
        return true;
    if (subdir == "sounds" && m_pendingSounds.contains(filename))
        return true;
    for (const auto &entry : m_pendingOtherFiles) {
        if (entry.first == filename && entry.second == subdir)
            return true;
    }
    return false;
}

void FileDownloader::deleteAllDownloadedFiles()
{
    const QStringList subdirs = {"rules", "sounds", "images", "pdf"};
    for (const QString &subdir : subdirs) {
        QDir dir(m_localDataPath + "/" + subdir);
        for (const QString &entry : dir.entryList(QDir::Files))
            dir.remove(entry);
    }

    // Clear ETag/Last-Modified cache so the next Check && Download refetches
    // everything instead of getting 304 Not Modified for files we just deleted.
    QSettings s;
    const QStringList keys = s.allKeys();
    for (const QString &key : keys) {
        if (key.startsWith("etag/") || key.startsWith("lastmod/"))
            s.remove(key);
    }

    qDebug() << "FileDownloader: deleted all downloaded files";
    emit statusMessage(tr("Deleted all downloaded files."));
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
