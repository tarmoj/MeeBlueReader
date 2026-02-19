#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#include "ibeaconscanner.h"
#include "meebluehelper.h"
#include <QDebug>
#include <QMetaObject>
#include <cmath>

// Objective-C delegate class for CLLocationManager
@interface IBeaconScannerDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, assign) IBeaconScanner *qtScanner;
@property (nonatomic, strong) CLLocationManager *locationManager;
@property (nonatomic, strong) NSMutableArray<CLBeaconRegion *> *monitoredRegions;
@property (nonatomic, strong) NSMutableArray<CLBeaconIdentityConstraint *> *constraints;
@property (nonatomic, strong) NSMutableArray<NSString *> *beaconUUIDs;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSMutableArray<NSNumber *> *> *rssiHistory;

- (instancetype)initWithQtScanner:(IBeaconScanner *)scanner;
- (void)setBeaconUUIDs:(const QStringList &)uuids;

@end

@implementation IBeaconScannerDelegate

- (instancetype)initWithQtScanner:(IBeaconScanner *)scanner {
    self = [super init];
    if (self) {
        _qtScanner = scanner;
        _locationManager = [[CLLocationManager alloc] init];
        _locationManager.delegate = self;
        _monitoredRegions = [[NSMutableArray alloc] init];
        _constraints = [[NSMutableArray alloc] init];
        
        // Default beacon UUID (common MeeBlue beacon UUID)
        _beaconUUIDs = [[NSMutableArray alloc] initWithArray:@[
            @"D35B76E2-E01C-9FAC-BA8D-7CE20BDBA0C6"
        ]];
        
        // Initialize RSSI history dictionary
        _rssiHistory = [[NSMutableDictionary alloc] init];
        
        // Request authorization for location services
        if ([_locationManager respondsToSelector:@selector(requestWhenInUseAuthorization)]) {
            [_locationManager requestWhenInUseAuthorization];
        }
    }
    return self;
}

- (void)setBeaconUUIDs:(const QStringList &)uuids {
    [_beaconUUIDs removeAllObjects];
    
    for (const QString &uuid : uuids) {
        NSString *nsUuid = uuid.toNSString();
        [_beaconUUIDs addObject:nsUuid];
    }
    
    NSLog(@"Beacon UUIDs updated: %@", _beaconUUIDs);
}

- (void)startScanning {
    NSLog(@"Starting iBeacon scanning with CLLocationManager");
    
    // Define the iBeacon regions to monitor
    // Using common MeeBlue beacon UUIDs - adjust as needed
    // df4f904b-fcb3-4dad-2454-4f06a4eb35cd
    NSArray *beaconUUIDs = @[
        [[NSUUID alloc] initWithUUIDString:@"D35B76E2-E01C-9FAC-BA8D-7CE20BDBA0C6"],
        // Add more UUIDs as needed for different beacon types
    ];
    
    for (NSUUID *uuid in beaconUUIDs) {
        // Use modern API for iOS 13+
        CLBeaconIdentityConstraint *constraint = [[CLBeaconIdentityConstraint alloc] initWithUUID:uuid];
        CLBeaconRegion *region = [[CLBeaconRegion alloc] initWithBeaconIdentityConstraint:constraint
                                                                                identifier:[uuid UUIDString]];
        region.notifyEntryStateOnDisplay = YES;
        region.notifyOnEntry = YES;
        region.notifyOnExit = YES;
        
        [_monitoredRegions addObject:region];
        [_constraints addObject:constraint];
        
        // Start monitoring and ranging
        [_locationManager startMonitoringForRegion:region];
        [_locationManager startRangingBeaconsSatisfyingConstraint:constraint];
        
        NSLog(@"Started monitoring region: %@", [uuid UUIDString]);
    }
}

- (void)stopScanning {
    NSLog(@"Stopping iBeacon scanning");
    
    // Stop ranging for all constraints
    for (CLBeaconIdentityConstraint *constraint in _constraints) {
        [_locationManager stopRangingBeaconsSatisfyingConstraint:constraint];
    }
    
    // Stop monitoring for all regions
    for (CLBeaconRegion *region in _monitoredRegions) {
        [_locationManager stopMonitoringForRegion:region];
    }
    
    [_monitoredRegions removeAllObjects];
    [_constraints removeAllObjects];
}

// CLLocationManagerDelegate methods

// Modern API for iOS 13+
- (void)locationManager:(CLLocationManager *)manager
        didRangeBeacons:(NSArray<CLBeacon *> *)beacons
    satisfyingConstraint:(CLBeaconIdentityConstraint *)constraint {
    
    if (beacons.count == 0) {
        return;
    }
    
    // Convert CLBeacon array to QVariantList
    QVariantList qtBeacons;
    
    for (CLBeacon *beacon in beacons) {
        QVariantMap beaconMap;
        
        // UUID as string
        NSString *uuidString = [beacon.UUID UUIDString];
        beaconMap["uuid"] = QString::fromNSString(uuidString);
        
        // Major and minor values
        beaconMap["major"] = [beacon.major intValue];
        beaconMap["minor"] = [beacon.minor intValue];
        
        // RSSI value with smoothing
        NSInteger rssi = beacon.rssi;
        
        // Create unique identifier for this beacon
        NSString *beaconId = [NSString stringWithFormat:@"%@-%d-%d",
                             uuidString, [beacon.major intValue], [beacon.minor intValue]];
        
        // Get or create RSSI history for this beacon
        NSMutableArray<NSNumber *> *history = _rssiHistory[beaconId];
        if (!history) {
            history = [[NSMutableArray alloc] init];
            _rssiHistory[beaconId] = history;
        }
        
        // Add current RSSI to history
        [history addObject:@(rssi)];
        
        // Keep only last 4 readings
        const int MAX_HISTORY = 4;
        if (history.count > MAX_HISTORY) {
            [history removeObjectAtIndex:0];
        }
        
        // Calculate smoothed RSSI using MeeBlueHelper
        QList<double> rssiValues;
        for (NSNumber *value in history) {
            rssiValues.append([value doubleValue]);
        }
        
        //double smoothedRSSI =   MeeBlueHelper::smoothReadings(rssiValues);
        // for testing, report only  about the first beacon
        double smoothedRSSI = beaconMap["minor"]==1 ?  MeeBlueHelper::smoothReadings(rssiValues) : rssi ;
        beaconMap["rssi"] = (int)smoothedRSSI;
        
        // Accuracy (estimated distance in meters)
        double accuracy = beacon.accuracy;
        if (accuracy < 0) {
            // Negative accuracy means unknown distance, calculate from RSSI
            accuracy = [self calculateDistanceFromRSSI:rssi];
        }
        beaconMap["distance"] = accuracy;
        
        // Proximity as string for debugging
        NSString *proximityStr;
        switch (beacon.proximity) {
            case CLProximityImmediate:
                proximityStr = @"Immediate";
                break;
            case CLProximityNear:
                proximityStr = @"Near";
                break;
            case CLProximityFar:
                proximityStr = @"Far";
                break;
            default:
                proximityStr = @"Unknown";
                break;
        }
        beaconMap["proximity"] = QString::fromNSString(proximityStr);
        
        qtBeacons.append(beaconMap);
        
        NSLog(@"Beacon: %@ Major:%@ Minor:%@ RSSI:%ld Distance:%.2fm Proximity:%@",
              uuidString, beacon.major, beacon.minor, (long)rssi, accuracy, proximityStr);
    }
    
    // Call updateBeacons on the Qt scanner object
    if (_qtScanner) {
        QMetaObject::invokeMethod(_qtScanner, "updateBeacons",
                                  Qt::QueuedConnection,
                                  Q_ARG(QVariantList, qtBeacons));
    }
}

- (void)locationManager:(CLLocationManager *)manager
         didEnterRegion:(CLBeaconRegion *)region {
    NSLog(@"Entered beacon region: %@", region.identifier);
}

- (void)locationManager:(CLLocationManager *)manager
          didExitRegion:(CLBeaconRegion *)region {
    NSLog(@"Exited beacon region: %@", region.identifier);
}

- (void)locationManager:(CLLocationManager *)manager
       didFailWithError:(NSError *)error {
    NSLog(@"Location manager failed with error: %@", error.localizedDescription);
}

- (void)locationManager:(CLLocationManager *)manager
didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    NSLog(@"Authorization status changed: %d", status);
    
    if (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
        status == kCLAuthorizationStatusAuthorizedAlways) {
        NSLog(@"Location authorization granted");
    } else if (status == kCLAuthorizationStatusDenied ||
               status == kCLAuthorizationStatusRestricted) {
        NSLog(@"Location authorization denied or restricted");
    }
}

// Helper method to calculate distance from RSSI
// Using MeeBlueHelper for consistent distance calculation
- (double)calculateDistanceFromRSSI:(NSInteger)rssi {
    // Use MeeBlueHelper with iBeacon-typical TX power of -59 dBm
    return MeeBlueHelper::estimateDistance((int)rssi, -59, 2.0);
}

@end

// C++ Implementation

IBeaconScanner::IBeaconScanner(QObject *parent)
    : QObject(parent)
    , m_nativeScanner(nullptr)
    , m_previousAverage(0.0)
    , m_filterThreshold(0.25) // 10% threshold
{
    // Create the Objective-C delegate
    IBeaconScannerDelegate *delegate = [[IBeaconScannerDelegate alloc] initWithQtScanner:this];
    m_nativeScanner = (__bridge_retained void *)delegate;
    
    qDebug() << "IBeaconScanner created";
}

IBeaconScanner::~IBeaconScanner()
{
    if (m_nativeScanner) {
        IBeaconScannerDelegate *delegate = (__bridge_transfer IBeaconScannerDelegate *)m_nativeScanner;
        [delegate stopScanning];
        delegate = nil;
        m_nativeScanner = nullptr;
    }
}

void IBeaconScanner::startScanning()
{
    qDebug() << "IBeaconScanner::startScanning() called";
    
    if (m_nativeScanner) {
        IBeaconScannerDelegate *delegate = (__bridge IBeaconScannerDelegate *)m_nativeScanner;
        [delegate startScanning];
    }
}

void IBeaconScanner::stopScanning()
{
    qDebug() << "IBeaconScanner::stopScanning() called";
    
    if (m_nativeScanner) {
        IBeaconScannerDelegate *delegate = (__bridge IBeaconScannerDelegate *)m_nativeScanner;
        [delegate stopScanning];
    }
}

void IBeaconScanner::setBeaconUUIDs(const QStringList &uuids)
{
    qDebug() << "IBeaconScanner::setBeaconUUIDs() called with" << uuids.size() << "UUIDs";
    
    if (m_nativeScanner) {
        IBeaconScannerDelegate *delegate = (__bridge IBeaconScannerDelegate *)m_nativeScanner;
        [delegate setBeaconUUIDs:uuids];
    }
}

void IBeaconScanner::updateBeacons(const QVariantList &beacons)
{
    // Measure time interval between calls
    if (!m_updateTimer.isValid()) {
        // First call - start the timer
        m_updateTimer.start();
        // qDebug() << "IBeaconScanner::updateBeacons() - First call, timer started";
    } else {
        // Subsequent calls - report elapsed time
        qint64 elapsed = m_updateTimer.restart();
        qDebug() << "IBeaconScanner::updateBeacons() - Time since last call:" << elapsed << "ms (" << (elapsed/1000.0) << "s)";
    }
    
    qDebug() << "IBeaconScanner::updateBeacons() called with" << beacons.size() << "beacons";
    
    // Update the internal beacon list
    m_beaconList = beacons;
    emit beaconListChanged();
    
    // Store current RSSI values by minor for averaging
    m_currentRssiValues.clear();
    
    // Emit individual beacon signals for each beacon
    for (const QVariant &beaconVariant : beacons) {
        QVariantMap beaconMap = beaconVariant.toMap();
        
        QString uuid = beaconMap["uuid"].toString();
        int rssi = beaconMap["rssi"].toInt();
        double distance = beaconMap["distance"].toDouble();
        int major = beaconMap["major"].toInt();
        int minor = beaconMap["minor"].toInt();
        QString proximity = beaconMap["proximity"].toString();
        
        // Store RSSI value for this minor
        m_currentRssiValues[minor] = rssi;
        
        if (minor==2 || minor==1) {
            emit newBeaconInfo(uuid, rssi, proximity, major, minor);
        }
    }
    
    // Test average RSSI calculation with minors 1 and 2
    averageRssi(1, 2);
}

double IBeaconScanner::averageRssi(int minor1, int minor2)
{
    QList<int> validRssiValues;
    
    // Check if we have readings for both beacons
    bool hasMinor1 = m_currentRssiValues.contains(minor1);
    bool hasMinor2 = m_currentRssiValues.contains(minor2);
    
    if (!hasMinor1 && !hasMinor2) {
        qDebug() << "averageRssi: No RSSI readings available for minors" << minor1 << "and" << minor2;
        return 0.0;
    }
    
    // Process minor1
    if (hasMinor1) {
        int rssi1 = m_currentRssiValues[minor1];
        
        // If we have a previous average, check if this reading is within threshold
        if (m_previousAverage != 0.0) {
            double deviation = qAbs(rssi1 - m_previousAverage) / qAbs(m_previousAverage);
            if (deviation > m_filterThreshold) {
                qDebug() << "averageRssi: Minor" << minor1 << "RSSI" << rssi1 
                         << "rejected (deviation" << QString::number(deviation * 100, 'f', 1) 
                         << "% exceeds" << QString::number(m_filterThreshold * 100, 'f', 1) << "% threshold)";
            } else {
                validRssiValues.append(rssi1);
                qDebug() << "averageRssi: Minor" << minor1 << "RSSI" << rssi1 << "accepted";
            }
        } else {
            // No previous average, accept all initial readings
            validRssiValues.append(rssi1);
            qDebug() << "averageRssi: Minor" << minor1 << "RSSI" << rssi1 << "accepted (initial)";
        }
    } else {
        qDebug() << "averageRssi: No RSSI reading for minor" << minor1;
    }
    
    // Process minor2
    if (hasMinor2) {
        int rssi2 = m_currentRssiValues[minor2];
        
        // If we have a previous average, check if this reading is within threshold
        if (m_previousAverage != 0.0) {
            double deviation = qAbs(rssi2 - m_previousAverage) / qAbs(m_previousAverage);
            if (deviation > m_filterThreshold) {
                qDebug() << "averageRssi: Minor" << minor2 << "RSSI" << rssi2 
                         << "rejected (deviation" << QString::number(deviation * 100, 'f', 1) 
                         << "% exceeds" << QString::number(m_filterThreshold * 100, 'f', 1) << "% threshold)";
            } else {
                validRssiValues.append(rssi2);
                qDebug() << "averageRssi: Minor" << minor2 << "RSSI" << rssi2 << "accepted";
            }
        } else {
            // No previous average, accept all initial readings
            validRssiValues.append(rssi2);
            qDebug() << "averageRssi: Minor" << minor2 << "RSSI" << rssi2 << "accepted (initial)";
        }
    } else {
        qDebug() << "averageRssi: No RSSI reading for minor" << minor2;
    }
    
    // Calculate average from valid readings
    if (validRssiValues.isEmpty()) {
        // Keep previous average if all readings were rejected
        qDebug() << "averageRssi: All readings rejected, keeping previous average:" << m_previousAverage;
        return m_previousAverage;
    }
    
    double sum = 0.0;
    for (int rssi : validRssiValues) {
        sum += rssi;
    }
    
    double newAverage = sum / validRssiValues.count();
    
    qDebug() << "========================================";
    qDebug() << "averageRssi: Calculated average RSSI:" << newAverage 
             << "(from" << validRssiValues.count() << "readings)";
    qDebug() << "========================================";
    
    // Update previous average for next iteration
    m_previousAverage = newAverage;

    emit newBeaconInfo("average 1-1", newAverage, "unknown", 0, 1);
    
    return newAverage;
}
