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

    // Configure the timer for 250ms interval
    // m_scanTimer->setInterval(SCAN_INTERVAL_MS);
    // connect(m_scanTimer, &QTimer::timeout, this, &MeeBlueReader::restartScan);
    // connect(m_scanTimer, &QTimer::timeout, this, &MeeBlueReader::emitSmoothedReadings);

    // Start scanning automatically
    //startScanning();
}

MeeBlueReader::~MeeBlueReader()
{
    stopScanning();
    
    // Clean up controllers
    for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it) {
        QLowEnergyController *controller = it.value();
        if (controller) {
            controller->disconnectFromDevice();
            controller->deleteLater();
        }
    }
    m_controllers.clear();
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
                    qDebug() << "Raw data:" << data.toHex(' ');

                    // If it's an iBeacon (Apple's company ID 0x004C)
                    if (manufacturerId == 0x004C && data.size() >= 25) {
                        // Parse iBeacon payload
                        QByteArray uuidBytes = data.mid(4, 16); // bytes 4–19
                        QString uuid;
                        uuid += QString(uuidBytes.mid(0,4).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(4,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(6,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(8,2).toHex()) + "-";
                        uuid += QString(uuidBytes.mid(10,6).toHex());
                        uuid = uuid.toLower();

                        quint16 major = (quint8(data[20]) << 8) | quint8(data[21]);
                        quint16 minor = (quint8(data[22]) << 8) | quint8(data[23]);
                        qint8 txPower = qint8(data[24]);

                        qDebug() << "iBeacon UUID:" << uuid;
                        qDebug() << "Major:" << major << "Minor:" << minor << "TxPower:" << txPower;
                    }
                }

            
            
            
            m_rssiHistory[address] = QList<int>();

            QLowEnergyController *controller = QLowEnergyController::createCentral(device, this);
            m_controllers[address] = controller;

            // Connect to device to read iBeacon UUID from GATT service
            connect(controller, &QLowEnergyController::connected, this, [this, controller, address]() {
                qDebug() << "Connected to MeeBlue device:" << address;
                readBeaconUuidFromDevice(controller);
            });

            connect(controller, &QLowEnergyController::errorOccurred, this, 
                [address](QLowEnergyController::Error error) {
                qDebug() << "LE Controller error for" << address << ":" << error;
            });

            connect(controller, &QLowEnergyController::disconnected, this, [address]() {
                qDebug() << "Disconnected from" << address;
            });

            controller->connectToDevice();
        }
/*
        QList<int> &history = m_rssiHistory[address];
        history.append(rssi);
        
        // Keep only last 4 readings
        if (history.size() > MAX_RSSI_HISTORY) {
            history.removeFirst();
        }
        
        //emit newBeaconInfo(address, rssi, estimateDistance(rssi)); // temporary, no smoothing

        double distance = estimateDistance(rssi);

        // Always emit to QML on the main thread
        QMetaObject::invokeMethod(this, [=]() {
                emit newBeaconInfo(address, rssi, distance);
            }, Qt::QueuedConnection);
*/

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

void MeeBlueReader::readBeaconUuidFromDevice(QLowEnergyController *controller)
{
    if (!controller) {
        qWarning() << "readBeaconUuidFromDevice: null controller";
        return;
    }

    // Discover services
    connect(controller, &QLowEnergyController::serviceDiscovered, this,
        [](const QBluetoothUuid &serviceUuid) {
            qDebug() << "Service discovered:" << serviceUuid.toString();
            if (serviceUuid.toString().contains("d35b2000")) {
                qDebug() << "Service found !";
            }
        });

    connect(controller, &QLowEnergyController::discoveryFinished, this,
        [this, controller]() {
            qDebug() << "Service discovery finished";
            
            // Look for service 0x2000
            QBluetoothUuid targetServiceUuid("d35b2000-e01c-9fac-ba8d-7ce20bdba0c6");
            //Check:
            qDebug() << targetServiceUuid.toString();
            
            if (!controller->services().contains(targetServiceUuid)) {
                qWarning() << "Service 0x2000 not found";
                controller->disconnectFromDevice();
                return;
            }
            
            QLowEnergyService *service = controller->createServiceObject(targetServiceUuid, this);
            if (!service) {
                qWarning() << "Failed to create service object for 0x2000";
                controller->disconnectFromDevice();
                return;
            }
        
        
        
            
            // Discover characteristics
            connect(service, &QLowEnergyService::stateChanged, this,
                [this, service, controller](QLowEnergyService::ServiceState newState) {
                    if (newState == QLowEnergyService::RemoteServiceDiscovered) {
                        qDebug() << "Characteristics discovered for service 0x2000";
                        
                        
//                            const QList<QLowEnergyCharacteristic> chars = service->characteristics();
//                            qDebug() << "Characterstc found: " << chars.count();
//                            for (const QLowEnergyCharacteristic &ch : chars) {
//                                qDebug() << "--------------------------------";
//                                qDebug() << "Characteristic UUID:" << ch.uuid().toString();
//                                qDebug() << "Name:" << ch.name();
//
//                                // Print properties
//                                QStringList props;
//                                if (ch.properties() & QLowEnergyCharacteristic::Read) props << "Read";
//                                if (ch.properties() & QLowEnergyCharacteristic::Write) props << "Write";
//                                if (ch.properties() & QLowEnergyCharacteristic::WriteNoResponse) props << "WriteNoResponse";
//                                if (ch.properties() & QLowEnergyCharacteristic::Notify) props << "Notify";
//                                if (ch.properties() & QLowEnergyCharacteristic::Indicate) props << "Indicate";
//                                if (ch.properties() & QLowEnergyCharacteristic::Broadcasting) props << "Broadcast";
//                                qDebug() << "Properties:" << props.join(", ");
//
//                                // Print descriptors (if any)
//                                const QList<QLowEnergyDescriptor> descs = ch.descriptors();
//                                for (const QLowEnergyDescriptor &desc : descs) {
//                                    qDebug() << "  Descriptor UUID:" << desc.uuid().toString()
//                                             << "Name:" << desc.name();
//                                }
//                            }
//
//                            qDebug() << "--------------------------------";
                        
                        // Look for characteristic 0x2001 (first 20 bytes of iBeacon data)
                        QBluetoothUuid char0x2001Uuid("d35b2001-e01c-9fac-ba8d-7ce20bdba0c6");
                        
                        QLowEnergyCharacteristic characteristic = service->characteristic(char0x2001Uuid);
                        qDebug() << "Characteristic: " << characteristic.uuid().toString();
                        if (!characteristic.isValid()) {
                            qWarning() << "Characteristic 0x2001 not found";
                            service->deleteLater();
                            controller->disconnectFromDevice();
                            return;
                        }
                        
                        // Read the characteristic
                        connect(service, &QLowEnergyService::characteristicRead, this,
                            [this, service, controller](const QLowEnergyCharacteristic &info, const QByteArray &value) {
                                qDebug() << "Characteristic 0x2001 read, size:" << value.size();
                                qDebug() << "Data:" << value.toHex(' ');
                                
                                // Extract UUID from the data
                                QString uuid = extractUuidFromBeaconData(value);
                                if (!uuid.isEmpty()) {
                                    qDebug() << "Extracted iBeacon UUID:" << uuid;
                                    emit beaconUuidDiscovered(uuid);
                                } else {
                                    qWarning() << "Failed to extract UUID from beacon data";
                                }
                                
                                // Clean up
                                service->deleteLater();
                                controller->disconnectFromDevice();
                            });
                        
                        service->readCharacteristic(characteristic);
                    }
                });
            
            service->discoverDetails();
        });

    controller->discoverServices();
}

QString MeeBlueReader::extractUuidFromBeaconData(const QByteArray &data) const
{
    // According to the spec:
    // The first byte is the effective length
    // iBeacon data format (30 bytes total):
    // 0x{01 06 1A FF 4C 00 02 15 [16 bytes UUID] [2 bytes Major] [2 bytes Minor] [1 byte Power]}
    // The UUID starts at byte 9 (after the header: 01 06 1A FF 4C 00 02 15)
    
    if (data.size() < 20) {
        qWarning() << "Data too short for iBeacon format:" << data.size() << "bytes";
        return QString();
    }
    
    // Check if this looks like iBeacon data
    // Expected header: 01 06 1A FF 4C 00 02 15
    // But the first byte might be the length, so let's check from byte 1
    if (data.size() >= 9 && 
        static_cast<quint8>(data[1]) == 0x06 &&
        static_cast<quint8>(data[2]) == 0x1A &&
        static_cast<quint8>(data[3]) == 0xFF &&
        static_cast<quint8>(data[4]) == 0x4C &&
        static_cast<quint8>(data[5]) == 0x00 &&
        static_cast<quint8>(data[6]) == 0x02 &&
        static_cast<quint8>(data[7]) == 0x15) {
        
        // Extract 16 bytes of UUID starting at byte 8
        if (data.size() >= 24) {
            QByteArray uuidBytes = data.mid(8, 16);
            
            // Format as standard UUID string: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
            QString uuid = QString("%1-%2-%3-%4-%5")
                .arg(QString::fromLatin1(uuidBytes.mid(0, 4).toHex()).toUpper())
                .arg(QString::fromLatin1(uuidBytes.mid(4, 2).toHex()).toUpper())
                .arg(QString::fromLatin1(uuidBytes.mid(6, 2).toHex()).toUpper())
                .arg(QString::fromLatin1(uuidBytes.mid(8, 2).toHex()).toUpper())
                .arg(QString::fromLatin1(uuidBytes.mid(10, 6).toHex()).toUpper());
            
            return uuid;
        }
    }
    
    qWarning() << "Data does not match iBeacon format";
    qWarning() << "Data hex:" << data.toHex(' ');
    return QString();
}
