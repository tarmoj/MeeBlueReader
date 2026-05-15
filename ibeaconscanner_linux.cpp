// Linux/Android implementation of IBeaconScanner
// Uses QBluetoothDeviceDiscoveryAgent to scan for BLE advertisements,
// parses iBeacon manufacturer data (Apple company ID 0x004C), and
// emits beaconDataUpdated with smoothed RSSI readings.

#include "ibeaconscanner.h"
#include "meebluehelper.h"
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QMap>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>

// ============================================================================
// Helpers
// ============================================================================

static Proximity proximityFromRSSI(int rssi)
{
    if (rssi == 0)    return Proximity::Unknown;
    if (rssi >= -50)  return Proximity::Immediate;
    if (rssi >= -70)  return Proximity::Near;
    if (rssi >= -90)  return Proximity::Far;
    return Proximity::Unknown;
}

static Proximity proximityFromString(const QString &str)
{
    if (str == "Immediate") return Proximity::Immediate;
    if (str == "Near")      return Proximity::Near;
    if (str == "Far")       return Proximity::Far;
    return Proximity::Unknown;
}

static QString proximityToString(Proximity prox)
{
    switch (prox) {
    case Proximity::Immediate: return "Immediate";
    case Proximity::Near:      return "Near";
    case Proximity::Far:       return "Far";
    default:                   return "Unknown";
    }
}

// ============================================================================
// Private BLE scanner class (lives only in this translation unit)
// ============================================================================

class LinuxBleScanner : public QObject
{
    Q_OBJECT

public:
    explicit LinuxBleScanner(IBeaconScanner *qtScanner, QObject *parent = nullptr)
        : QObject(parent)
        , m_qtScanner(qtScanner)
        , m_agent(new QBluetoothDeviceDiscoveryAgent(this))
        , m_emitTimer(new QTimer(this))
    {
        // Continuous scanning (timeout = 0)
        m_agent->setLowEnergyDiscoveryTimeout(0);

        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                this, &LinuxBleScanner::onDeviceDiscovered);
        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
                this, &LinuxBleScanner::onError);
        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
                this, &LinuxBleScanner::onFinished);

        // Emit buffered beacon data every second
        m_emitTimer->setInterval(1000);
        connect(m_emitTimer, &QTimer::timeout, this, &LinuxBleScanner::emitBeacons);
    }

    void start()
    {
        qDebug() << "LinuxBleScanner: starting BLE scan";
        m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
        m_emitTimer->start();
    }

    void stop()
    {
        qDebug() << "LinuxBleScanner: stopping BLE scan";
        m_emitTimer->stop();
        if (m_agent->isActive()) {
            m_agent->stop();
        }
    }

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device)
    {
        if (!device.isValid()) return;

        const auto manufacturerData = device.manufacturerData();
        for (auto it = manufacturerData.cbegin(); it != manufacturerData.cend(); ++it) {
            if (it.key() != 0x004C) continue; // Only Apple / iBeacon

            const QByteArray &data = it.value();
            // iBeacon payload: 0x02 0x15 <16-byte UUID> <2-byte major> <2-byte minor> <1-byte txPower>
            // Total data length must be at least 23 bytes.
            if (data.size() < 23) continue;
            if ((quint8)data[0] != 0x02 || (quint8)data[1] != 0x15) continue;

            // Parse UUID
            QByteArray uuidBytes = data.mid(2, 16);
            QString uuid = uuidBytes.mid(0, 4).toHex() + "-"
                         + uuidBytes.mid(4, 2).toHex() + "-"
                         + uuidBytes.mid(6, 2).toHex() + "-"
                         + uuidBytes.mid(8, 2).toHex() + "-"
                         + uuidBytes.mid(10, 6).toHex();
            uuid = uuid.toLower();

            quint16 major = ((quint8)data[18] << 8) | (quint8)data[19];
            quint16 minor = ((quint8)data[20] << 8) | (quint8)data[21];
            // qint8 txPower = (qint8)data[22]; // could be used in distance estimation

            int rssi = device.rssi();
            if (rssi == 0) rssi = -100;

            // Key uniquely identifies one beacon
            QString key = uuid + ":" + QString::number(major) + ":" + QString::number(minor);

            // Keep rolling RSSI history (last 4 readings)
            QList<int> &history = m_rssiHistory[key];
            history.append(rssi);
            if (history.size() > 4) {
                history.removeFirst();
            }

            // Update latest snapshot for this beacon
            BeaconInfo info;
            info.uuid      = uuid;
            info.major     = static_cast<int>(major);
            info.minor     = static_cast<int>(minor);
            info.rssi      = rssi;
            info.proximity = proximityFromRSSI(rssi);
            m_latestBeacons[key] = info;

            // qDebug() << "iBeacon:" << uuid
            //          << "major:" << major << "minor:" << minor
            //          << "rssi:" << rssi;
        }
    }

    void onError(QBluetoothDeviceDiscoveryAgent::Error error)
    {
        qWarning() << "LinuxBleScanner: BLE scan error:" << error
                   << m_agent->errorString();
    }

    void onFinished()
    {
        qDebug() << "LinuxBleScanner: scan finished (will not auto-restart with timeout=0)";
    }

    // Called every second: smooth each beacon's RSSI and push a QVariantList to IBeaconScanner
    void emitBeacons()
    {
        if (m_latestBeacons.isEmpty()) return;

        QVariantList qtBeacons;
        for (auto it = m_latestBeacons.cbegin(); it != m_latestBeacons.cend(); ++it) {
            const QString    &key  = it.key();
            const BeaconInfo &info = it.value();

            int smoothedRssi = MeeBlueHelper::calculateMedianRSSI(m_rssiHistory.value(key));
            Proximity prox   = proximityFromRSSI(smoothedRssi);

            QVariantMap map;
            map["uuid"]      = info.uuid;
            map["major"]     = info.major;
            map["minor"]     = info.minor;
            map["rssi"]      = smoothedRssi;
            map["proximity"] = proximityToString(prox);
            qtBeacons.append(map);
        }

        // Deliver on the Qt event loop via IBeaconScanner::updateBeacons
        QMetaObject::invokeMethod(m_qtScanner, "updateBeacons",
                                  Qt::QueuedConnection,
                                  Q_ARG(QVariantList, qtBeacons));
    }

private:
    IBeaconScanner                *m_qtScanner;
    QBluetoothDeviceDiscoveryAgent *m_agent;
    QTimer                        *m_emitTimer;

    QMap<QString, QList<int>>  m_rssiHistory;   // key -> rolling RSSI buffer
    QMap<QString, BeaconInfo>  m_latestBeacons; // key -> last seen BeaconInfo
};

// Pull in the MOC-generated code for the class defined above
#include "ibeaconscanner_linux.moc"

// ============================================================================
// IBeaconScanner – C++ implementation for Linux / Android
// ============================================================================

IBeaconScanner::IBeaconScanner(QObject *parent)
    : QObject(parent)
    , m_nativeScanner(nullptr)
{
    LinuxBleScanner *scanner = new LinuxBleScanner(this, this);
    m_nativeScanner = static_cast<void *>(scanner);
    qDebug() << "IBeaconScanner (Linux/Android) created";
}

IBeaconScanner::~IBeaconScanner()
{
    if (m_nativeScanner) {
        // The scanner is parented to 'this', so Qt will destroy it.
        // We just make sure scanning is stopped cleanly first.
        static_cast<LinuxBleScanner *>(m_nativeScanner)->stop();
        m_nativeScanner = nullptr;
    }
}

void IBeaconScanner::startScanning()
{
    qDebug() << "IBeaconScanner::startScanning() (Linux/Android)";
    if (m_nativeScanner) {
        static_cast<LinuxBleScanner *>(m_nativeScanner)->start();
    }
}

void IBeaconScanner::stopScanning()
{
    qDebug() << "IBeaconScanner::stopScanning() (Linux/Android)";
    if (m_nativeScanner) {
        static_cast<LinuxBleScanner *>(m_nativeScanner)->stop();
    }
}

void IBeaconScanner::updateBeacons(const QVariantList &beacons)
{
    // qDebug() << "IBeaconScanner::updateBeacons() called with" << beacons.size() << "beacons";

    QList<BeaconInfo> beaconList;
    for (const QVariant &v : beacons) {
        QVariantMap m = v.toMap();
        BeaconInfo info;
        info.uuid      = m["uuid"].toString();
        info.major     = m["major"].toInt();
        info.minor     = m["minor"].toInt();
        info.rssi      = m["rssi"].toInt();
        info.proximity = proximityFromString(m["proximity"].toString());
        beaconList.append(info);
    }

    emit beaconDataUpdated(beaconList);
}
