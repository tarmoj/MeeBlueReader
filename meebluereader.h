#ifndef MEEBLUEREADER_H
#define MEEBLUEREADER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <algorithm>
#include "ibeaconscanner.h"

// Forward declaration
class Station;

class MeeBlueReader : public QObject
{
    Q_OBJECT

public:
    explicit MeeBlueReader(QObject *parent = nullptr);
    ~MeeBlueReader();

public slots:
    // Called by IBeaconScanner when new beacon data arrives
    void update(const QList<BeaconInfo> &beacons);

signals:
    // Emitted when station info is updated (id, rssi, proximity, major1:minor1, major2:minor2)
    void newStationInfo(int stationId, int rssi, QString proximity, QString beaconIds);

private:
    // List of all beacon information received from scanner
    QList<BeaconInfo> m_beaconInfo;
    
    // List of stations (each station tracks 2 beacons)
    QList<Station*> m_stations;
};

// Station class - represents a pair of beacons for reliability
class Station : public QObject
{
    Q_OBJECT
    
public:
    Station(int id, int major1, int minor1, int major2, int minor2, MeeBlueReader *parent);
    
    // Update station with current beacon data
    void update(const QList<BeaconInfo> &beacons);
    
    int id() const { return m_id; }
    
signals:
    // Emitted when station data is processed
    void stationUpdated(int stationId, int rssi, QString proximity, QString beaconIds);
    
private:
    // Smooth RSSI readings using median filter
    static int smoothReadings(const QList<int> &values);
    
    // Convert Proximity enum to string
    static QString proximityToString(Proximity prox);
    
    // Convert proximity enum to numeric value for comparison
    static int proximityToInt(Proximity prox);
    
    // Convert numeric proximity back to enum
    static Proximity intToProximity(int val);
    
    int m_id;
    int m_major1, m_minor1;
    int m_major2, m_minor2;
    
    // RSSI history for smoothing (last 4 readings per beacon)
    QMap<int, QList<int>> m_rssiHistory; // Key: minor value
    
    // Previous average for filtering
    double m_previousAverage;
    double m_filterThreshold; // ±5 dB threshold
};

#endif // MEEBLUEREADER_H
