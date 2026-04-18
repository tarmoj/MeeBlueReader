#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>
#include <QStringList>

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

    // Read the already-downloaded rules file and emit rulesLoaded immediately.
    Q_INVOKABLE void loadLocalRules(const QString &rulesName);

signals:
    // Emitted once the rules JSON has been parsed (from local or freshly downloaded).
    void rulesLoaded(const QVariantList &rules);

    // Progress / status text intended for display in UI.
    void statusMessage(const QString &message);

    // Emitted on network or file error.
    void downloadError(const QString &message);

private:
    void downloadMediaFiles(const QString &baseUrl, const QStringList &soundFiles,
                            const QStringList &imageFiles);
    void downloadNextMediaFile();
    QVariantList parseRulesJson(const QByteArray &data);
    void collectMediaFilenames(const QVariantList &rules,
                               QStringList &soundFiles, QStringList &imageFiles);
    QString etagKey(const QString &rulesName) const;
    QString lastModKey(const QString &rulesName) const;
    QString mediaEtagKey(const QString &filename) const;

    QNetworkAccessManager m_nam;
    QString m_localDataPath;

    // State for sequential media download
    QString m_currentBaseUrl;
    QStringList m_pendingSounds;
    QStringList m_pendingImages;
    QVariantList m_pendingRules;   // rules waiting for media to finish
    int m_mediaTotal{0};
    int m_mediaDone{0};
};

#endif // FILEDOWNLOADER_H
