#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>
#include "ibeaconscanner.h"
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

- (instancetype)initWithQtScanner:(IBeaconScanner *)scanner;
- (void)setBeaconUUIDs:(const QStringList &)uuids;
- (void)addBeaconUUID:(const QString &)uuid;

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

- (void)addBeaconUUID:(const QString &)uuid {
    NSString *nsUuid = uuid.toNSString();
    
    // Check if UUID already exists
    if ([_beaconUUIDs containsObject:nsUuid]) {
        NSLog(@"UUID %@ already in monitoring list", nsUuid);
        return;
    }
    
    [_beaconUUIDs addObject:nsUuid];
    NSLog(@"Added UUID to monitoring list: %@", nsUuid);
    
    // If already scanning, add this UUID to the ranging immediately
    NSUUID *nsuuid = [[NSUUID alloc] initWithUUIDString:nsUuid];
    if (!nsuuid) {
        NSLog(@"Invalid UUID format: %@", nsUuid);
        return;
    }
    
    // Create constraint and region for this UUID
    CLBeaconIdentityConstraint *constraint = [[CLBeaconIdentityConstraint alloc] initWithUUID:nsuuid];
    CLBeaconRegion *region = [[CLBeaconRegion alloc] initWithBeaconIdentityConstraint:constraint
                                                                            identifier:[nsuuid UUIDString]];
    region.notifyEntryStateOnDisplay = YES;
    region.notifyOnEntry = YES;
    region.notifyOnExit = YES;
    
    [_monitoredRegions addObject:region];
    [_constraints addObject:constraint];
    
    // Start monitoring and ranging for this new UUID
    [_locationManager startMonitoringForRegion:region];
    [_locationManager startRangingBeaconsSatisfyingConstraint:constraint];
    
    NSLog(@"Started monitoring dynamically added UUID: %@", [nsuuid UUIDString]);
}

- (void)startScanning {
    NSLog(@"Starting iBeacon scanning with CLLocationManager");
    
    // Use configured beacon UUIDs
    for (NSString *uuidString in _beaconUUIDs) {
        NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
        if (!uuid) {
            NSLog(@"Invalid UUID: %@", uuidString);
            continue;
        }
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
        
        // RSSI value
        NSInteger rssi = beacon.rssi;
        beaconMap["rssi"] = (int)rssi;
        
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
// Using log-distance path loss model
- (double)calculateDistanceFromRSSI:(NSInteger)rssi {
    if (rssi == 0) {
        return -1.0; // Invalid
    }
    
    // Constants for distance calculation
    const int TX_POWER = -59; // Measured power at 1 meter (typical for iBeacons)
    const double N = 2.0;     // Environmental factor
    
    double ratio = (double)(TX_POWER - rssi) / (10.0 * N);
    double distance = pow(10.0, ratio);
    
    return distance;
}

@end

// C++ Implementation

IBeaconScanner::IBeaconScanner(QObject *parent)
    : QObject(parent)
    , m_nativeScanner(nullptr)
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

void IBeaconScanner::addBeaconUUID(const QString &uuid)
{
    qDebug() << "IBeaconScanner::addBeaconUUID() called with UUID:" << uuid;
    
    if (m_nativeScanner) {
        IBeaconScannerDelegate *delegate = (__bridge IBeaconScannerDelegate *)m_nativeScanner;
        [delegate addBeaconUUID:uuid];
    }
}

void IBeaconScanner::updateBeacons(const QVariantList &beacons)
{
    qDebug() << "IBeaconScanner::updateBeacons() called with" << beacons.size() << "beacons";
    
    // Update the internal beacon list
    m_beaconList = beacons;
    emit beaconListChanged();
    
    // Emit individual beacon signals for each beacon
    for (const QVariant &beaconVariant : beacons) {
        QVariantMap beaconMap = beaconVariant.toMap();
        
        QString uuid = beaconMap["uuid"].toString();
        int rssi = beaconMap["rssi"].toInt();
        double distance = beaconMap["distance"].toDouble();
        int major = beaconMap["major"].toInt();
        int minor = beaconMap["minor"].toInt();
        
        emit newBeaconInfo(uuid, rssi, distance, major, minor);
    }
}
