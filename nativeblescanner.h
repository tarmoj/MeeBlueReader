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

signals:
    void deviceDiscovered(const QString &address, const QString &name, int rssi);

private:
#ifdef Q_OS_ANDROID
    QJniObject m_javaScanner;
    
    // Static JNI callback that will be called from Java
    static void onDeviceDiscoveredCallback(JNIEnv *env, jobject thiz, jstring address, jstring name, jint rssi);
    
    // Instance method to handle the callback
    void handleDeviceDiscovered(const QString &address, const QString &name, int rssi);
    
    // Static pointer to the current instance for callback routing
    static NativeBleScanner *s_instance;
#endif
};

#endif // NATIVEBLESCANNER_H
