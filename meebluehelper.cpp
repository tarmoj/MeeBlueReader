#include "meebluehelper.h"
#include <algorithm>
#include <cmath>
#include <QDebug>

int MeeBlueHelper::calculateMedianRSSI(const QList<int> &readings)
{
    if (readings.isEmpty()) {
        return 0;
    }
    
    // Filter out zero and invalid values (0 means out of range)
    QList<int> validReadings;
    for (int val : readings) {
        if (val != 0) {
            validReadings.append(val);
        }
    }
    
    // If no valid readings remain, return 0 (out of range)
    if (validReadings.isEmpty()) {
        return 0;
    }
    
    // Create a sorted copy of the valid readings
    QList<int> sortedReadings = validReadings;
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

double MeeBlueHelper::smoothReadings(const QList<double> &values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    
    // Filter out zero and invalid values (0 means out of range)
    QList<double> validValues;
    for (double val : values) {
        if (val != 0.0) {
            validValues.append(val);
        }
    }
    
    // If no valid values remain, return 0 (out of range)
    if (validValues.isEmpty()) {
        return 0.0;
    }
    
    // Create a sorted copy of the valid values
    QList<double> sortedValues = validValues;
    std::sort(sortedValues.begin(), sortedValues.end());
    
    qDebug() << "Readings: " << values << " valid: " << validValues << " sorted: " << sortedValues;
    
    int size = sortedValues.size();
    if ( size % 2 == 0) {
        // Even number of values: average of two middle values
        return (sortedValues[size/2 - 1] + sortedValues[size/2]) / 2.0;
    } else {
        // Odd number of values: middle value
        return sortedValues[size/2];
    }
}

double MeeBlueHelper::estimateDistance(int rssi, int txPower, double n)
{
    if (rssi == 0) {
        return INVALID_DISTANCE;
    }
    
    // Log-distance path loss model
    // distance = 10 ^ ((txPower - rssi) / (10 * n))
    double ratio = static_cast<double>(txPower - rssi) / (10.0 * n);
    double distance = std::pow(10.0, ratio);
    
    return distance;
}
