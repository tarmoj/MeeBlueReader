#ifndef IBEACONSCANNER_H
#define IBEACONSCANNER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class IBeaconScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList beaconList READ beaconList NOTIFY beaconListChanged)

public:
    explicit IBeaconScanner(QObject *parent = nullptr);
    ~IBeaconScanner();
    
    QVariantList beaconList() const { return m_beaconList; }
    
    // Start scanning for iBeacons
    void startScanning();
    
    // Stop scanning for iBeacons
    void stopScanning();
    
    // Set beacon UUIDs to monitor (should be called before startScanning)
    // Pass empty list to use default UUID
    void setBeaconUUIDs(const QStringList &uuids);
    
    // Add a UUID to the monitoring list (can be called while scanning)
    void addBeaconUUID(const QString &uuid);

signals:
    void beaconListChanged();
    void newBeaconInfo(QString uuid, int rssi, double distance, int major, int minor);

public slots:
    // Called by native Objective-C code to update beacon list
    void updateBeacons(const QVariantList &beacons);

private:
    QVariantList m_beaconList;
    void *m_nativeScanner; // Opaque pointer to Objective-C implementation
};

#endif // IBEACONSCANNER_H
