#include "meebluereader.h"
#include <QDebug>

MeeBlueReader::MeeBlueReader(QObject *parent)
    : QObject(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_scanTimer(new QTimer(this))
    , m_beaconInfo("Waiting for beacons...")
{
    // Add the specified device addresses to the device list
    m_deviceList << "DD:2B:7C:C0:A0:84" << "EB:3B:E8:48:F4:90";

    // Configure the discovery agent
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    // Connect signals
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MeeBlueReader::deviceDiscovered);
    connect(m_discoveryAgent, 
            QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(&QBluetoothDeviceDiscoveryAgent::error),
            this, &MeeBlueReader::scanError);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MeeBlueReader::scanFinished);

    // Configure the timer for 500ms interval
    m_scanTimer->setInterval(500);
    connect(m_scanTimer, &QTimer::timeout, this, &MeeBlueReader::restartScan);

    // Start scanning automatically
    startScanning();
}

MeeBlueReader::~MeeBlueReader()
{
    stopScanning();
}

QString MeeBlueReader::beaconInfo() const
{
    return m_beaconInfo;
}

void MeeBlueReader::startScanning()
{
    qDebug() << "Starting MeeBlue beacon scanning...";
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    m_scanTimer->start();
}

void MeeBlueReader::stopScanning()
{
    qDebug() << "Stopping MeeBlue beacon scanning...";
    m_scanTimer->stop();
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
}

void MeeBlueReader::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (isTargetDevice(device)) {
        int rssi = device.rssi();
        double distance = estimateDistance(rssi);
        
        QString info = QString("%1 - %2 dB - %3 m")
                        .arg(device.address().toString())
                        .arg(rssi)
                        .arg(distance, 0, 'f', 2);
        
        qDebug() << "Beacon found:" << info;
        
        m_beaconInfo = info;
        emit beaconInfoChanged();
    }
}

void MeeBlueReader::scanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QString errorString = m_discoveryAgent->errorString();
    qWarning() << "Bluetooth scan error:" << error << errorString;
    
    m_beaconInfo = QString("Error: %1").arg(errorString);
    emit beaconInfoChanged();
}

void MeeBlueReader::scanFinished()
{
    qDebug() << "Scan finished, waiting for next interval...";
}

void MeeBlueReader::restartScan()
{
    // Stop current scan if still active
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
    
    // Start a new scan
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

double MeeBlueReader::estimateDistance(int rssi) const
{
    if (rssi == 0) {
        return -1.0;
    }
    
    // Log-distance path loss model
    // distance = 10 ^ ((txPower - rssi) / (10 * n))
    double ratio = static_cast<double>(TX_POWER - rssi) / (10.0 * N);
    double distance = std::pow(10.0, ratio);
    
    return distance;
}

bool MeeBlueReader::isTargetDevice(const QBluetoothDeviceInfo &device) const
{
    // Check if device name contains "meeblue" (case-insensitive)
    QString deviceName = device.name().toLower();
    if (deviceName.contains("meeblue")) {
        return true;
    }
    
    // Check if device address is in the device list
    QString address = device.address().toString();
    if (m_deviceList.contains(address, Qt::CaseInsensitive)) {
        return true;
    }
    
    return false;
}
