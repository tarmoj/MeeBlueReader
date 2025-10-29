package org.tarmoj.meeblue;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.util.Log;

import java.util.List;

/**
 * Native Android BLE Scanner that uses the Android BluetoothLeScanner API directly.
 * This provides better compatibility with various BLE beacon types including MeeBlue beacons.
 */
public class NativeBleScanner {
    private static final String TAG = "NativeBleScanner";
    
    private Context mContext;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothLeScanner mBluetoothLeScanner;
    private ScanCallback mScanCallback;
    private boolean mIsScanning = false;
    
    // Native callback method to be called from C++
    private native void onDeviceDiscovered(String address, String name, int rssi);
    
    public NativeBleScanner(Context context) {
        mContext = context;
        
        BluetoothManager bluetoothManager = 
            (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (bluetoothManager != null) {
            mBluetoothAdapter = bluetoothManager.getAdapter();
            if (mBluetoothAdapter != null) {
                mBluetoothLeScanner = mBluetoothAdapter.getBluetoothLeScanner();
            }
        }
        
        mScanCallback = new ScanCallback() {
            @Override
            public void onScanResult(int callbackType, ScanResult result) {
                if (result != null && result.getDevice() != null) {
                    String address = result.getDevice().getAddress();
                    String name = result.getDevice().getName();
                    int rssi = result.getRssi();
                    
                    Log.d(TAG, "Device found: " + address + " (" + name + ") RSSI: " + rssi);
                    
                    // Call native C++ callback
                    onDeviceDiscovered(address, name != null ? name : "", rssi);
                }
            }
            
            @Override
            public void onBatchScanResults(List<ScanResult> results) {
                for (ScanResult result : results) {
                    onScanResult(ScanCallback.SCAN_FAILED_ALREADY_STARTED, result);
                }
            }
            
            @Override
            public void onScanFailed(int errorCode) {
                Log.e(TAG, "BLE scan failed with error code: " + errorCode);
                mIsScanning = false;
            }
        };
    }
    
    /**
     * Start BLE scanning with low latency settings for better beacon detection
     */
    public boolean startScan() {
        if (mBluetoothLeScanner == null) {
            Log.e(TAG, "BluetoothLeScanner is not available");
            return false;
        }
        
        if (mIsScanning) {
            Log.w(TAG, "Scan already in progress");
            return true;
        }
        
        try {
            // Configure scan settings for low latency and maximum beacon detection
            ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setReportDelay(0)  // Report immediately
                .build();
            
            mBluetoothLeScanner.startScan(null, settings, mScanCallback);
            mIsScanning = true;
            Log.i(TAG, "BLE scan started successfully");
            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "Security exception when starting scan: " + e.getMessage());
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error starting scan: " + e.getMessage());
            return false;
        }
    }
    
    /**
     * Stop BLE scanning
     */
    public void stopScan() {
        if (mBluetoothLeScanner == null || !mIsScanning) {
            return;
        }
        
        try {
            mBluetoothLeScanner.stopScan(mScanCallback);
            mIsScanning = false;
            Log.i(TAG, "BLE scan stopped");
        } catch (SecurityException e) {
            Log.e(TAG, "Security exception when stopping scan: " + e.getMessage());
        } catch (Exception e) {
            Log.e(TAG, "Error stopping scan: " + e.getMessage());
        }
    }
    
    /**
     * Check if currently scanning
     */
    public boolean isScanning() {
        return mIsScanning;
    }
}
