#include "nativeblescanner.h"
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>

// Static instance pointer for JNI callback routing
NativeBleScanner *NativeBleScanner::s_instance = nullptr;

// JNI callback function
extern "C" JNIEXPORT void JNICALL
Java_org_tarmoj_meeblue_NativeBleScanner_onDeviceDiscovered(
    JNIEnv *env, jobject thiz, jstring address, jstring name, jint rssi)
{
    if (NativeBleScanner::s_instance) {
        const char *addressStr = env->GetStringUTFChars(address, nullptr);
        const char *nameStr = env->GetStringUTFChars(name, nullptr);
        
        NativeBleScanner::s_instance->handleDeviceDiscovered(
            QString::fromUtf8(addressStr),
            QString::fromUtf8(nameStr),
            rssi
        );
        
        env->ReleaseStringUTFChars(address, addressStr);
        env->ReleaseStringUTFChars(name, nameStr);
    }
}
#endif

NativeBleScanner::NativeBleScanner(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
    s_instance = this;
    
    // Get Android context
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    
    if (context.isValid()) {
        // Create NativeBleScanner Java object
        m_javaScanner = QJniObject("org/tarmoj/meeblue/NativeBleScanner",
                                   "(Landroid/content/Context;)V",
                                   context.object());
        
        if (m_javaScanner.isValid()) {
            qDebug() << "Native BLE Scanner initialized successfully";
        } else {
            qWarning() << "Failed to create NativeBleScanner Java object";
        }
    } else {
        qWarning() << "Failed to get Android context";
    }
#else
    qDebug() << "Native BLE Scanner is only available on Android";
#endif
}

NativeBleScanner::~NativeBleScanner()
{
#ifdef Q_OS_ANDROID
    stopScan();
    if (s_instance == this) {
        s_instance = nullptr;
    }
#endif
}

bool NativeBleScanner::isAvailable() const
{
#ifdef Q_OS_ANDROID
    return m_javaScanner.isValid();
#else
    return false;
#endif
}

bool NativeBleScanner::startScan()
{
#ifdef Q_OS_ANDROID
    if (!m_javaScanner.isValid()) {
        qWarning() << "Cannot start scan: Java scanner not initialized";
        return false;
    }
    
    bool result = m_javaScanner.callMethod<jboolean>("startScan", "()Z");
    qDebug() << "Native BLE scan start result:" << result;
    return result;
#else
    qWarning() << "Native BLE scanning not available on this platform";
    return false;
#endif
}

void NativeBleScanner::stopScan()
{
#ifdef Q_OS_ANDROID
    if (m_javaScanner.isValid()) {
        m_javaScanner.callMethod<void>("stopScan", "()V");
        qDebug() << "Native BLE scan stopped";
    }
#endif
}

bool NativeBleScanner::isScanning() const
{
#ifdef Q_OS_ANDROID
    if (m_javaScanner.isValid()) {
        return m_javaScanner.callMethod<jboolean>("isScanning", "()Z");
    }
#endif
    return false;
}

#ifdef Q_OS_ANDROID
void NativeBleScanner::handleDeviceDiscovered(const QString &address, const QString &name, int rssi)
{
    qDebug() << "Native scanner discovered device:" << address << name << "RSSI:" << rssi;
    emit deviceDiscovered(address, name, rssi);
}
#endif
