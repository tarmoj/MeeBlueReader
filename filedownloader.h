#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>
#include <QStringList>
#include <functional>

// FileDownloader: downloads rules JSON and referenced media files from a base URL.
// Files are stored under QStandardPaths::AppDataLocation/{rules,sounds,images}/.
// Uses HTTP ETag / Last-Modified conditional requests to skip unchanged files.
class FileDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localDataPath READ localDataPath CONSTANT)

public:
    explicit FileDownloader(QObject *parent = nullptr);

    QString localDataPath() const;

    // Download rules file and all referenced media.  Only fetches when server content changed.
    // baseUrl: e.g. "https://tarmo.uuu.ee/meeblue"
    // rulesName: e.g. "rules1"  (file fetched: <baseUrl>/rules/<rulesName>.json)
    Q_INVOKABLE void checkAndDownload(const QString &baseUrl, const QString &rulesName);

    // Check and download rules1..rules4 (and their media) one after another.
    // Emits allRulesChecked() once every file has been processed.
    Q_INVOKABLE void checkAndDownloadAll(const QString &baseUrl);

    // Read the already-downloaded rules file and emit rulesLoaded immediately.
    Q_INVOKABLE void loadLocalRules(const QString &rulesName);

    // List locally available sound/image files (base names, sorted).
    Q_INVOKABLE QStringList listSoundFiles() const;
    Q_INVOKABLE QStringList listImageFiles() const;

    // Read the raw content of a local rules{N}.json file (empty string if missing).
    Q_INVOKABLE QString readRulesFileContent(const QString &rulesName) const;

    // Resolve a rule's "sound" value to a concrete local filename.
    // If pattern has no '*'/'?' wildcard it is returned unchanged.
    // Otherwise a random match among local sounds/ files is returned
    // (empty string if nothing matches).
    Q_INVOKABLE QString pickSoundFile(const QString &pattern) const;

signals:
    // Emitted once the rules JSON has been parsed (from local or freshly downloaded).
    void rulesLoaded(const QVariantList &rules);

    // Progress / status text intended for display in UI.
    void statusMessage(const QString &message);

    // Emitted on network or file error.
    void downloadError(const QString &message);

    // Emitted once checkAndDownloadAll() has processed every rules file.
    void allRulesChecked();

private:
    void checkAndDownloadInternal(const QString &baseUrl, const QString &rulesName,
                                  std::function<void()> onDone);
    void downloadNextQueuedRulesFile();
    void downloadMediaFiles(const QString &baseUrl, const QStringList &soundFiles,
                            const QStringList &imageFiles);
    void downloadNextMediaFile();
    QVariantList parseRulesJson(const QByteArray &data);
    void collectMediaFilenames(const QVariantList &rules,
                               QStringList &soundFiles, QStringList &imageFiles,
                               QStringList &soundPatterns, QStringList &imagePatterns);
    void resolveWildcardPatterns();
    void beginMediaDownloads();
    static QStringList parseDirectoryListing(const QString &html);
    QString etagKey(const QString &rulesName) const;
    QString lastModKey(const QString &rulesName) const;
    QString mediaEtagKey(const QString &filename) const;

    QNetworkAccessManager m_nam;
    QString m_localDataPath;

    // State for sequential media download
    QString m_currentBaseUrl;
    QStringList m_pendingSounds;
    QStringList m_pendingImages;
    QStringList m_pendingSoundPatterns;
    QStringList m_pendingImagePatterns;
    int m_mediaTotal{0};
    int m_mediaDone{0};
    std::function<void()> m_onRulesFileDone;

    // State for checkAndDownloadAll()
    QStringList m_rulesQueue;
    QString m_queueBaseUrl;
};

#endif // FILEDOWNLOADER_H
