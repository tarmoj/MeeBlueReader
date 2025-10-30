#ifndef NATIVEBLESCANNER_H
#define NATIVEBLESCANNER_H

#include <QObject>
#include <QString>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

class NativeBleScanner : public QObject
{
    Q_OBJECT

public:
    explicit NativeBleScanner(QObject *parent = nullptr);
    ~NativeBleScanner();

    bool isAvailable() const;
    bool startScan();
    void stopScan();
    bool isScanning() const;

#ifdef Q_OS_ANDROID
    // Instance method to handle the callback - public so JNI function can access
    void handleDeviceDiscovered(const QString &address, const QString &name, int rssi);
    
    // Static pointer to the current instance for callback routing - public for JNI access
    static NativeBleScanner *s_instance;
#endif

signals:
    void deviceDiscovered(const QString &address, const QString &name, int rssi);

private:
#ifdef Q_OS_ANDROID
    QJniObject m_javaScanner;
#endif
};

#endif // NATIVEBLESCANNER_H
