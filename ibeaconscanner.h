#ifndef IBEACONSCANNER_H
#define IBEACONSCANNER_H

#include <QObject>
#include <QString>

// Proximity levels for beacons
enum class Proximity {
    Unknown = 0,
    Immediate = 1,
    Near = 2,
    Far = 3
};

// Beacon information structure
struct BeaconInfo {
    QString uuid;
    int major;
    int minor;
    int rssi;
    Proximity proximity;
    
    BeaconInfo() : major(0), minor(0), rssi(0), proximity(Proximity::Unknown) {}
    
    BeaconInfo(const QString &u, int maj, int min, int r, Proximity prox)
        : uuid(u), major(maj), minor(min), rssi(r), proximity(prox) {}
};

class IBeaconScanner : public QObject
{
    Q_OBJECT

public:
    explicit IBeaconScanner(QObject *parent = nullptr);
    ~IBeaconScanner();
    
    // Start scanning for iBeacons
    void startScanning();
    
    // Stop scanning for iBeacons
    void stopScanning();

signals:
    // Emitted when beacon data is updated - connects to MeeBlueReader::update()
    void beaconDataUpdated(const QList<BeaconInfo> &beacons);

public slots:
    // Called by native Objective-C code to update beacon list
    void updateBeacons(const QVariantList &beacons);

private:
    void *m_nativeScanner; // Opaque pointer to Objective-C implementation
};

#endif // IBEACONSCANNER_H
