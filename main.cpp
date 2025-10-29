#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "meebluereader.h"

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QtCore/private/qandroidextras_p.h>

bool requestPermission(QString permission)
{


    auto r = QtAndroidPrivate::requestPermission(permission).result();
    if (r == QtAndroidPrivate::Denied) {
        qDebug() << permission << " Denied.";
        return false;
    } else
        return true;
}

#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);


#ifdef Q_OS_ANDROID

    // check permissions
    // see: https://stackoverflow.com/questions/71216717/requesting-android-permissions-in-qt-6
    // https://doc-snapshots.qt.io/qt6-dev/qcoreapplication.html#requestPermission

    //requestPermission("android.permission.WRITE_EXTERNAL_STORAGE");

    // List of permissions to consider. Keep both "old" location permission (for scans on <= Android 11) // and Android 12+ specific BLUETOOTH_* permissions.
    const QStringList permissions = QStringList()  << "android.permission.BLUETOOTH_SCAN"
                                    << "android.permission.BLUETOOTH_CONNECT"
                                    << "android.permission.ACCESS_FINE_LOCATION";

    for (const QString &permission : permissions) {
        requestPermission(permission);
    }

    //keep screen on:
    QJniObject activity
        = QNativeInterface::QAndroidApplication::context(); //  QtAndroid::androidActivity();
    if (activity.isValid()) {
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");

        if (window.isValid()) {
            const int FLAG_KEEP_SCREEN_ON = 128;
            window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);

            // test:
            // try setting titlebar color here...
            window.callMethod<void>("addFlags", "(I)V", 0x80000000);
            window.callMethod<void>("clearFlags", "(I)V", 0x04000000);
            window.callMethod<void>(
                "setStatusBarColor",
                "(I)V",
                0x1c1b1f); // hardcoded color for now. later try to get via QML engine Material.background
            QJniObject decorView = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
            decorView.callMethod<void>("setSystemUiVisibility", "(I)V", 0x00002000);
        }
        QJniEnvironment env;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } //Clear any possible pending exceptions.
    }

#endif

    QQmlApplicationEngine engine;
    
    // Create and register MeeBlueReader instance
    MeeBlueReader reader;
    engine.rootContext()->setContextProperty("meeBlueReader", &reader);
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("MeeBlueReader", "Main");

    return app.exec();
}
