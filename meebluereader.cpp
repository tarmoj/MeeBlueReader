#include "meebluereader.h"
#include <QDebug>
#include <algorithm>
#include <QLowEnergyController>

MeeBlueReader::MeeBlueReader(QObject *parent)
    : QObject(parent)
    , m_discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this))
    , m_scanTimer(new QTimer(this))
    , m_beaconInfo("Waiting for beacons...")
{
    // Add the specified device addresses to the device list
    m_deviceList << "DD:2B:7C:C0:A0:84" << "EB:3B:E8:48:F4:90";

    // Configure the discovery agent
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(0);
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);

    // Connect signals
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MeeBlueReader::deviceDiscovered);
    connect(m_discoveryAgent,
            &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this,
            &MeeBlueReader::scanError);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MeeBlueReader::scanFinished);
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
    // m_scanTimer->start();
}

void MeeBlueReader::stopScanning()
{
    qDebug() << "Stopping MeeBlue beacon scanning...";
    //m_scanTimer->stop();
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
}

void MeeBlueReader::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if (!device.isValid()) return;

    //test
    // const QString addr = device.address().toString();
    // const qint16 rssi = device.rssi();

    // // Every new advertisement will trigger this again with updated RSSI
    // qDebug() << "Beacon" << addr << "RSSI" << rssi;
    // qDebug() << device.name();

    if (isTargetDevice(device)) {
        int rssi = device.rssi();

        QString address;


#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        address = device.deviceUuid().toString(); // not really address, but should maybe work
#else
        address = device.address().toString();
#endif
        
        
        

        // Store RSSI reading in history
        if (!m_rssiHistory.contains(address)) {
            
            // Manufacturer data is stored as a QHash<quint16, QByteArray>
                const auto manufacturerData = device.manufacturerData();
                for (auto it = manufacturerData.cbegin(); it != manufacturerData.cend(); ++it) {
                    quint16 manufacturerId = it.key();
                    QByteArray data = it.value();
                    
                    // returns:
                    //Manufacturer ID: "0x004c"
                    // Raw data: "02 15 d3 5b 76 e2 e0 1c 9f ac ba 8d 7c e2 0b db a0 c6 90 f4 48 e8 cb"

                    qDebug() << "Manufacturer ID:" << QString("0x%1").arg(manufacturerId, 4, 16, QLatin1Char('0'));
                    qDebug() << "Raw data:" << data.toHex(' ') << " length: " << data.size();

                    // If it's an iBeacon (Apple's company ID 0x004C)
                    if (manufacturerId == 0x004C && data.size() >= 20) {
                        // Parse iBeacon payload
                        QByteArray uuidBytes = data.mid(2, 16); // bytes 4–19
                        QString uuid;
                        uuid += QString(uuidBytes.mid(0,4).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(4,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(6,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(8,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(10,6).toHex());
                        uuid = uuid.toLower();

                        quint16 major = (quint8(data[20]) << 8) | quint8(data[20]);
                        quint16 minor = (quint8(data[21]) << 8) | quint8(data[22]);
                        qint8 txPower = qint8(data[22]);

                        qDebug() << "iBeacon UUID:" << uuid;
                        qDebug() << "Major:" << major << "Minor:" << minor << "TxPower:" << txPower;
                        emit beaconUuidDiscovered(uuid);
                    }
                }

            
            
            
            m_rssiHistory[address] = QList<int>();


        }

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
        QList<int> &history = m_rssiHistory[address];
        history.append(rssi);
        
        // Keep only last 4 readings
        if (history.size() > MAX_RSSI_HISTORY) {
            history.removeFirst();
        }
        
        double distance = estimateDistance(rssi);

        // Always emit to QML on the main thread
        QMetaObject::invokeMethod(this, [=]() {
                emit newBeaconInfo(address, rssi, distance);
        }, Qt::QueuedConnection);

#endif
        qDebug() << "Device" << address << device.name() << "RSSI:" << rssi;
    }
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
            
            //qDebug() << "Emitting smoothed:" << info << "(from" << readings.size() << "readings)";
            
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

// most likely not needed.
void MeeBlueReader::restartScan()
{
    // Stop current scan if still active
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
        qDebug() << "Stop agent";
        QTimer::singleShot(3000, this, &MeeBlueReader::restartScan);
        return;
    }
    
    // Start a new scan
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

