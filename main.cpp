#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "meebluereader.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

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
