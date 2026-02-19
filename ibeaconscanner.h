#ifndef IBEACONSCANNER_H
#define IBEACONSCANNER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QMap>

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
    
    // Calculate average RSSI from two beacons identified by their minor values
    // Filters out readings that differ more than threshold% from previous average
    double averageRssi(int minor1, int minor2);

signals:
    void beaconListChanged();
    void newBeaconInfo(QString uuid, int rssi, QString proximity, int major, int minor);

public slots:
    // Called by native Objective-C code to update beacon list
    void updateBeacons(const QVariantList &beacons);

private:
    QVariantList m_beaconList;
    void *m_nativeScanner; // Opaque pointer to Objective-C implementation
    QElapsedTimer m_updateTimer; // Timer to measure call intervals
    
    // For averageRssi functionality
    QMap<int, int> m_currentRssiValues; // Current RSSI values indexed by minor
    double m_previousAverage; // Previous average RSSI value
    double m_filterThreshold; // Filter threshold (0.10 = 10%)
};

#endif // IBEACONSCANNER_H
