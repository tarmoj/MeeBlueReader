#include "meebluereader.h"
#include <QDebug>
#include <algorithm>

MeeBlueReader::MeeBlueReader(QObject *parent)
    : QObject(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_nativeScanner(new NativeBleScanner(this))
    , m_scanTimer(new QTimer(this))
    , m_beaconInfo("Waiting for beacons...")
    , m_useNativeScanner(false)
{
    // Add the specified device addresses to the device list
    m_deviceList << "DD:2B:7C:C0:A0:84" << "EB:3B:E8:48:F4:90";

    // Configure the discovery agent for continuous BLE scanning
    // Setting timeout to DISCOVERY_TIMEOUT_MS allows proper scan cycles
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(DISCOVERY_TIMEOUT_MS);

    // Connect Qt BLE scanner signals
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MeeBlueReader::deviceDiscovered);
    connect(m_discoveryAgent,
            &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this,
            &MeeBlueReader::scanError);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MeeBlueReader::scanFinished);

    // Connect native scanner signals
    connect(m_nativeScanner, &NativeBleScanner::deviceDiscovered,
            this, &MeeBlueReader::nativeDeviceDiscovered);

    // Configure the timer for periodic scan restart and data emission
    m_scanTimer->setInterval(SCAN_INTERVAL_MS);
    connect(m_scanTimer, &QTimer::timeout, this, &MeeBlueReader::restartScan);
    connect(m_scanTimer, &QTimer::timeout, this, &MeeBlueReader::emitSmoothedReadings);

    // Try to use native scanner on Android if available
#ifdef Q_OS_ANDROID
    if (m_nativeScanner->isAvailable()) {
        qDebug() << "Native BLE scanner is available, using it by default";
        m_useNativeScanner = true;
    } else {
        qDebug() << "Native BLE scanner not available, using Qt scanner";
        m_useNativeScanner = false;
    }
#endif

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

bool MeeBlueReader::useNativeScanner() const
{
    return m_useNativeScanner;
}

void MeeBlueReader::setUseNativeScanner(bool use)
{
    if (m_useNativeScanner != use) {
        bool wasScanning = m_scanTimer->isActive();
        
        if (wasScanning) {
            stopScanning();
        }
        
        m_useNativeScanner = use;
        emit useNativeScannerChanged();
        
        qDebug() << "Scanner mode changed to:" << (use ? "Native" : "Qt");
        
        if (wasScanning) {
            startScanning();
        }
    }
}

void MeeBlueReader::startScanning()
{
    qDebug() << "Starting MeeBlue beacon scanning..." << (m_useNativeScanner ? "(Native)" : "(Qt)");
    
    if (m_useNativeScanner && m_nativeScanner->isAvailable()) {
        m_nativeScanner->startScan();
    } else {
        m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    }
    
    m_scanTimer->start();
}

void MeeBlueReader::stopScanning()
{
    qDebug() << "Stopping MeeBlue beacon scanning...";
    m_scanTimer->stop();
    
    if (m_useNativeScanner && m_nativeScanner->isAvailable()) {
        m_nativeScanner->stopScan();
    }
    
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
}

void MeeBlueReader::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (!device.isValid()) return;

    const QString address = device.address().toString();
    const QString name = device.name();
    const qint16 rssi = device.rssi();

    // Every new advertisement will trigger this again with updated RSSI
    qDebug() << "Qt scanner - Beacon" << address << name << "RSSI" << rssi;

    if (isTargetDevice(device)) {
        processDeviceData(address, name, rssi);
    }
}

void MeeBlueReader::nativeDeviceDiscovered(const QString &address, const QString &name, int rssi)
{
    qDebug() << "Native scanner - Beacon" << address << name << "RSSI" << rssi;
    
    if (isTargetDevice(address, name)) {
        processDeviceData(address, name, rssi);
    }
}

void MeeBlueReader::processDeviceData(const QString &address, const QString &name, int rssi)
{
    // Store RSSI reading in history
    if (!m_rssiHistory.contains(address)) {
        m_rssiHistory[address] = QList<int>();
    }
    
    QList<int> &history = m_rssiHistory[address];
    history.append(rssi);
    
    // Keep only last 4 readings
    if (history.size() > MAX_RSSI_HISTORY) {
        history.removeFirst();
    }
    
    qDebug() << "Device" << address << "(" << name << ") RSSI:" << rssi << "History size:" << history.size();
}

void MeeBlueReader::emitSmoothedReadings()
{
    // Emit smoothed readings for all beacons in history
    for (auto it = m_rssiHistory.begin(); it != m_rssiHistory.end(); ++it) {
        QString address = it.key();
        QList<int> readings = it.value();
        
        if (!readings.isEmpty()) {
            // Calculate median RSSI
            int smoothedRSSI = calculateMedianRSSI(readings);
            double distance = estimateDistance(smoothedRSSI);
            
            QString info = QString("%1 | %2 dB | %3 m")
                            .arg(address)
                            .arg(smoothedRSSI)
                            .arg(distance, 0, 'f', 2);
            
            qDebug() << "Emitting smoothed:" << info << "(from" << readings.size() << "readings)";
            
            m_beaconInfo = info;
            emit beaconInfoChanged();
            emit newBeaconInfo(address, smoothedRSSI, distance);
        }
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
    // For native scanner, we don't need to restart - it runs continuously
    if (m_useNativeScanner && m_nativeScanner->isAvailable()) {
        if (!m_nativeScanner->isScanning()) {
            qDebug() << "Native scanner not running, restarting...";
            m_nativeScanner->startScan();
        }
        return;
    }
    
    // For Qt scanner, restart periodically
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
    
    // Start a new scan
    qDebug() << "Restarting Qt BLE scan...";
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

double MeeBlueReader::estimateDistance(int rssi) const
{
    if (rssi == 0) {
        return INVALID_DISTANCE;
    }
    
    // Log-distance path loss model
    // distance = 10 ^ ((txPower - rssi) / (10 * n))
    double ratio = static_cast<double>(TX_POWER - rssi) / (10.0 * N);
    double distance = std::pow(10.0, ratio);
    
    return distance;
}

int MeeBlueReader::calculateMedianRSSI(const QList<int> &readings) const
{
    if (readings.isEmpty()) {
        return 0;
    }
    
    // Create a sorted copy of the readings
    QList<int> sortedReadings = readings;
    std::sort(sortedReadings.begin(), sortedReadings.end());
    
    int size = sortedReadings.size();
    if (size % 2 == 0) {
        // Even number of readings: average of two middle values
        return (sortedReadings[size/2 - 1] + sortedReadings[size/2]) / 2;
    } else {
        // Odd number of readings: middle value
        return sortedReadings[size/2];
    }
}

bool MeeBlueReader::isTargetDevice(const QBluetoothDeviceInfo &device) const
{
    return isTargetDevice(device.address().toString(), device.name());
}

bool MeeBlueReader::isTargetDevice(const QString &address, const QString &name) const
{
    // Check if device name contains "meeblue" (case-insensitive)
    if (!name.isEmpty() && name.toLower().contains("meeblue")) {
        return true;
    }
    
    // Check if device address is in the device list
    if (m_deviceList.contains(address, Qt::CaseInsensitive)) {
        return true;
    }
    
    return false;
}
