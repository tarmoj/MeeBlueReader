#include "meebluereader.h"
#include <QDebug>
#include <algorithm>

// ============================================================================
// MeeBlueReader Implementation
// ============================================================================

MeeBlueReader::MeeBlueReader(QObject *parent)
    : QObject(parent)
{
    // Create stations - example configuration
    // Station(id, major1, minor1, major2, minor2)
    // These should be configured based on actual beacon deployment
    m_stations.append(new Station(1, 1, 1, 1, 2, this));
    // Add more stations as needed
    m_stations.append(new Station(2, 1, 3, 1, 4, this));
    m_stations.append(new Station(3, 1, 5, 1, 6, this));
    
    // Connect station signals to our signal
    for (Station *station : m_stations) {
        connect(station, &Station::stationUpdated, this, &MeeBlueReader::newStationInfo);
    }
    
    qDebug() << "MeeBlueReader created with" << m_stations.size() << "stations";
}

MeeBlueReader::~MeeBlueReader()
{
    qDeleteAll(m_stations);
    m_stations.clear();
}

void MeeBlueReader::update(const QList<BeaconInfo> &beacons)
{
    // Store the latest beacon information
    m_beaconInfo = beacons;
    
    qDebug() << "MeeBlueReader::update() called with" << beacons.size() << "beacons";
    
    // Update all stations with the new beacon data
    for (Station *station : m_stations) {
        station->update(m_beaconInfo);
    }
}

// ============================================================================
// Station Implementation
// ============================================================================

Station::Station(int id, int major1, int minor1, int major2, int minor2, MeeBlueReader *parent)
    : QObject(parent)
    , m_id(id)
    , m_major1(major1)
    , m_minor1(minor1)
    , m_major2(major2)
    , m_minor2(minor2)
    , m_previousAverage(0.0)
    , m_filterThreshold(5.0)
{
    qDebug() << "Station" << m_id << "created for beacons" 
             << m_major1 << ":" << m_minor1 << "and" << m_major2 << ":" << m_minor2;
}

void Station::update(const QList<BeaconInfo> &beacons)
{
    // Find our two beacons in the list
    const BeaconInfo *beacon1 = nullptr;
    const BeaconInfo *beacon2 = nullptr;
    
    for (const BeaconInfo &beacon : beacons) {
        if (beacon.major == m_major1 && beacon.minor == m_minor1) {
            beacon1 = &beacon;
        } else if (beacon.major == m_major2 && beacon.minor == m_minor2) {
            beacon2 = &beacon;
        }
    }
    
    if (!beacon1 && !beacon2) {
        qDebug() << "Station" << m_id << ": No beacons found";
        return;
    }
    
    // Process RSSI values with smoothing and filtering
    QList<int> validRssiValues;
    QList<int> rejectedRssiValues;
    QList<Proximity> validProximityValues;
    
    // Process beacon1
    if (beacon1) {
        // Add to history
        QList<int> &history1 = m_rssiHistory[m_minor1];
        history1.append(beacon1->rssi);
        if (history1.size() > 4) {
            history1.removeFirst();
        }
        
        // Smooth the readings
        int smoothedRssi = smoothReadings(history1);
        
        // Filter based on previous average
        if (m_previousAverage != 0.0) {
            double difference = qAbs(smoothedRssi - m_previousAverage);
            if (difference > m_filterThreshold) {
                rejectedRssiValues.append(smoothedRssi);
                qDebug() << "Station" << m_id << "Beacon1 RSSI" << smoothedRssi 
                         << "rejected (diff" << difference << "dB)";
            } else {
                validRssiValues.append(smoothedRssi);
                validProximityValues.append(beacon1->proximity);
                qDebug() << "Station" << m_id << "Beacon1 RSSI" << smoothedRssi << "accepted";
            }
        } else {
            validRssiValues.append(smoothedRssi);
            validProximityValues.append(beacon1->proximity);
            qDebug() << "Station" << m_id << "Beacon1 RSSI" << smoothedRssi << "accepted (initial)";
        }
    }
    
    // Process beacon2
    if (beacon2) {
        // Add to history
        QList<int> &history2 = m_rssiHistory[m_minor2];
        history2.append(beacon2->rssi);
        if (history2.size() > 4) {
            history2.removeFirst();
        }
        
        // Smooth the readings
        int smoothedRssi = smoothReadings(history2);
        
        // Filter based on previous average
        if (m_previousAverage != 0.0) {
            double difference = qAbs(smoothedRssi - m_previousAverage);
            if (difference > m_filterThreshold) {
                rejectedRssiValues.append(smoothedRssi);
                qDebug() << "Station" << m_id << "Beacon2 RSSI" << smoothedRssi 
                         << "rejected (diff" << difference << "dB)";
            } else {
                validRssiValues.append(smoothedRssi);
                validProximityValues.append(beacon2->proximity);
                qDebug() << "Station" << m_id << "Beacon2 RSSI" << smoothedRssi << "accepted";
            }
        } else {
            validRssiValues.append(smoothedRssi);
            validProximityValues.append(beacon2->proximity);
            qDebug() << "Station" << m_id << "Beacon2 RSSI" << smoothedRssi << "accepted (initial)";
        }
    }
    
    // Special rule: if both readings were rejected but are close to each other, accept their average
    if (validRssiValues.isEmpty() && rejectedRssiValues.count() == 2) {
        double differenceBetweenReadings = qAbs(rejectedRssiValues[0] - rejectedRssiValues[1]);
        if (differenceBetweenReadings <= m_filterThreshold) {
            qDebug() << "Station" << m_id << "Both rejected but within" << m_filterThreshold 
                     << "dB of each other - accepting";
            validRssiValues = rejectedRssiValues;
            if (beacon1) validProximityValues.append(beacon1->proximity);
            if (beacon2) validProximityValues.append(beacon2->proximity);
        }
    }
    
    // Calculate average from valid readings
    if (validRssiValues.isEmpty()) {
        qDebug() << "Station" << m_id << "All readings rejected, keeping previous average";
        return;
    }
    
    // Calculate average RSSI
    double sum = 0.0;
    for (int rssi : validRssiValues) {
        sum += rssi;
    }
    int averageRssi = static_cast<int>(sum / validRssiValues.count());
    
    // Update previous average
    m_previousAverage = averageRssi;
    
    // Calculate average proximity (take floor = closer proximity)
    Proximity averageProximity = Proximity::Unknown;
    if (!validProximityValues.isEmpty()) {
        int minProx = proximityToInt(validProximityValues[0]);
        for (Proximity prox : validProximityValues) {
            int proxInt = proximityToInt(prox);
            if (proxInt > 0) { // Ignore Unknown when finding minimum
                if (minProx == 0 || proxInt < minProx) {
                    minProx = proxInt;
                }
            }
        }
        averageProximity = intToProximity(minProx);
    }
    
    QString proximityStr = proximityToString(averageProximity);
    QString beaconIds = QString("%1:%2, %3:%4")
                            .arg(m_major1).arg(m_minor1)
                            .arg(m_major2).arg(m_minor2);
    
    qDebug() << "========================================";
    qDebug() << "Station" << m_id << "Average RSSI:" << averageRssi 
             << "(from" << validRssiValues.count() << "readings)";
    qDebug() << "Station" << m_id << "Proximity:" << proximityStr;
    qDebug() << "========================================";
    
    // Emit signal
    emit stationUpdated(m_id, averageRssi, proximityStr, beaconIds);
}

// Static helper methods for Station

int Station::smoothReadings(const QList<int> &values)
{
    if (values.isEmpty()) {
        return 0;
    }
    
    // Filter out zero and invalid values
    QList<int> validValues;
    for (int val : values) {
        if (val != 0) {
            validValues.append(val);
        }
    }
    
    if (validValues.isEmpty()) {
        return 0;
    }
    
    // Create a sorted copy
    QList<int> sortedValues = validValues;
    std::sort(sortedValues.begin(), sortedValues.end());
    
    int size = sortedValues.size();
    if (size % 2 == 0) {
        // Even number: average of two middle values
        return (sortedValues[size/2 - 1] + sortedValues[size/2]) / 2;
    } else {
        // Odd number: middle value
        return sortedValues[size/2];
    }
}

QString Station::proximityToString(Proximity prox)
{
    switch (prox) {
        case Proximity::Immediate: return "Immediate";
        case Proximity::Near: return "Near";
        case Proximity::Far: return "Far";
        default: return "Unknown";
    }
}

int Station::proximityToInt(Proximity prox)
{
    return static_cast<int>(prox);
}

Proximity Station::intToProximity(int val)
{
    if (val == 1) return Proximity::Immediate;
    if (val == 2) return Proximity::Near;
    if (val == 3) return Proximity::Far;
    return Proximity::Unknown;
}
