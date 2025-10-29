#ifndef MEEBLUEREADER_H
#define MEEBLUEREADER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QList>

class MeeBlueReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString beaconInfo READ beaconInfo NOTIFY beaconInfoChanged)

public:
    explicit MeeBlueReader(QObject *parent = nullptr);
    ~MeeBlueReader();

    QString beaconInfo() const;

public slots:
    void startScanning();
    void stopScanning();

signals:
    void beaconInfoChanged();
    void newBeaconInfo(QString address, int rssi, double distance);

private slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void scanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void scanFinished();
    void restartScan();
    void emitSmoothedReadings();

private:
    double estimateDistance(int rssi) const;
    bool isTargetDevice(const QBluetoothDeviceInfo &device) const;
    int calculateMedianRSSI(const QList<int> &readings) const;

    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
    QTimer *m_scanTimer;
    QStringList m_deviceList;
    QString m_beaconInfo;
    
    // Store last 4 RSSI readings per beacon address
    QMap<QString, QList<int>> m_rssiHistory;
    
    // Distance calculation parameters
    static constexpr int TX_POWER = -40;  // Measured power at 1 meter
    static constexpr double N = 2.0;      // Environmental factor
    static constexpr double INVALID_DISTANCE = -1.0;  // Invalid distance indicator
    
    // Timing parameters
    static constexpr int SCAN_INTERVAL_MS =  5000; //250;       // Scan interval in milliseconds
    static constexpr int DISCOVERY_TIMEOUT_MS = 25000;  // Discovery timeout in milliseconds
    static constexpr int MAX_RSSI_HISTORY = 4;         // Number of readings for median calculation
};

#endif // MEEBLUEREADER_H
