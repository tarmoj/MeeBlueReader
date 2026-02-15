#ifndef MEEBLUEHELPER_H
#define MEEBLUEHELPER_H

#include <QList>

/**
 * @brief Helper class for MeeBlue beacon signal processing and distance calculations
 * 
 * This class provides utility functions for:
 * - Signal smoothing (median filtering)
 * - Distance estimation based on RSSI values
 * - Can be used by both MeeBlueReader and IBeaconScanner
 */
class MeeBlueHelper
{
public:
    /**
     * @brief Calculate median RSSI from a list of integer readings
     * @param readings List of RSSI values
     * @return Median RSSI value, or 0 if list is empty
     */
    static int calculateMedianRSSI(const QList<int> &readings);
    
    /**
     * @brief Smooth a list of double values using median filtering
     * @param values List of double values to smooth
     * @return Smoothed (median) value, or 0.0 if list is empty
     */
    static double smoothReadings(const QList<double> &values);
    
    /**
     * @brief Estimate distance from RSSI value using log-distance path loss model
     * @param rssi Received Signal Strength Indicator value
     * @param txPower Measured power at 1 meter (default: -53 dBm)
     * @param n Environmental factor (default: 2.0)
     * @return Estimated distance in meters, or -1.0 if RSSI is invalid
     */
    static double estimateDistance(int rssi, int txPower = -53, double n = 2.0);
    
    // Constants
    static constexpr int DEFAULT_TX_POWER = -53;  // Measured power at 1 meter
    static constexpr double DEFAULT_N = 2.0;      // Environmental factor
    static constexpr double INVALID_DISTANCE = -1.0;  // Invalid distance indicator

private:
    MeeBlueHelper() = delete;  // Prevent instantiation - utility class only
    ~MeeBlueHelper() = delete;
};

#endif // MEEBLUEHELPER_H
