#include "bluetooth.h"
#include <BluetoothAudio.h>
#include <BluetoothHCI.h>

static A2DPSource* a2dpSource = nullptr;
static BluetoothHCI* bluetoothHCI = nullptr;
static bool bluetoothInitialized = false;
static bool bluetoothConnected = false;
static bool bluetoothHCIInitialized = false;

bool initBluetoothAudio() {
  if (bluetoothInitialized) return true;

  // IMPORTANT: Initialize BluetoothHCI FIRST for scanning
  // A2DPSource will be initialized later when connecting (to avoid conflicts)
  if (!bluetoothHCIInitialized) {
    Serial.println(F("[Bluetooth] Initializing BluetoothHCI for device scanning..."));
    if (!bluetoothHCI) {
      bluetoothHCI = new BluetoothHCI();
    }
    
    if (bluetoothHCI) {
      Serial.println(F("[Bluetooth] Installing BluetoothHCI stack..."));
      bluetoothHCI->install();
      Serial.println(F("[Bluetooth] Starting BluetoothHCI..."));
      bluetoothHCI->begin();
      
      // Wait longer for Bluetooth stack to fully initialize
      Serial.println(F("[Bluetooth] Waiting for Bluetooth stack to initialize (2 seconds)..."));
      delay(2000);
      
      bluetoothHCIInitialized = true;
      Serial.println(F("[Bluetooth] BluetoothHCI initialized. Use 'S' to scan for devices."));
      Serial.println(F("[Bluetooth] NOTE: A2DPSource will be initialized when connecting."));
    } else {
      Serial.println(F("[Bluetooth] ERROR: Failed to create BluetoothHCI"));
      return false;
    }
  }

  // Don't initialize A2DPSource here - it will be initialized when connecting
  // This avoids conflicts with BluetoothHCI during scanning

  bluetoothInitialized = true;
  return true;
}

bool scanBluetoothDevices() {
  if (!bluetoothInitialized) {
    Serial.println(F("[Bluetooth] Bluetooth not initialized. Use initBluetoothAudio() first."));
    return false;
  }

  if (!bluetoothHCI || !bluetoothHCIInitialized) {
    Serial.println(F("[Bluetooth] BluetoothHCI not initialized. Cannot scan devices."));
    return false;
  }

  Serial.println(F("[Bluetooth] === Starting Bluetooth Device Scan ==="));
  Serial.println(F("[Bluetooth] IMPORTANT: Devices must be in pairing/discoverable mode!"));
  Serial.println(F("[Bluetooth] Scan will take 30 seconds..."));
  Serial.flush();
  
  // IMPORTANT: A2DPSource might interfere with scanning
  // Make sure A2DPSource is NOT initialized when scanning
  if (a2dpSource) {
    Serial.println(F("[Bluetooth] WARNING: A2DPSource is initialized. This may interfere with scanning."));
    Serial.println(F("[Bluetooth] Disconnecting any existing A2DP connections..."));
    if (bluetoothConnected) {
      a2dpSource->disconnect();
      bluetoothConnected = false;
      delay(500);
    }
  }
  
  // Give extra time for Bluetooth stack to be ready
  Serial.println(F("[Bluetooth] Preparing for scan..."));
  delay(2000); // Longer delay to ensure stack is ready
  Serial.println(F("[Bluetooth] Beginning scan now..."));
  Serial.flush();

  // Try scanning with mask 0 (which should scan for all device types including Classic Bluetooth)
  // Some implementations use 0 for "all devices" instead of 0xFFFFFFFF
  // Also try a longer scan time (30 seconds) to give more time to find devices
  Serial.println(F("[Bluetooth] Scanning with mask 0 (all devices), 30 second duration..."));
  Serial.println(F("[Bluetooth] This should find both BLE and Classic Bluetooth devices."));
  Serial.flush();
  
  unsigned long scanStartTime = millis();
  // Try mask 0 first (all device types)
  auto devices = bluetoothHCI->scan(0, 30, false);
  unsigned long scanDuration = millis() - scanStartTime;
  
  Serial.print(F("[Bluetooth] First scan completed in "));
  Serial.print(scanDuration / 1000);
  Serial.print(F(" seconds. Found "));
  Serial.print(devices.size());
  Serial.println(F(" device(s)"));
  Serial.flush();
  
  // If no devices found with mask 0, try mask 0xFFFFFFFF
  if (devices.empty()) {
    Serial.println(F("[Bluetooth] No devices found with mask 0. Trying mask 0xFFFFFFFF..."));
    Serial.flush();
    delay(1000);
    
    scanStartTime = millis();
    devices = bluetoothHCI->scan(0xFFFFFFFF, 30, false);
    scanDuration = millis() - scanStartTime;
    
    Serial.print(F("[Bluetooth] Second scan completed in "));
    Serial.print(scanDuration / 1000);
    Serial.print(F(" seconds. Found "));
    Serial.print(devices.size());
    Serial.println(F(" device(s)"));
    Serial.flush();
  }
  
  if (devices.empty()) {
    Serial.println(F("[Bluetooth] No Bluetooth devices found."));
    Serial.println(F("[Bluetooth] Troubleshooting:"));
    Serial.println(F("[Bluetooth]   1. Make sure devices are in pairing/discoverable mode"));
    Serial.println(F("[Bluetooth]   2. Check that devices are within range"));
    Serial.println(F("[Bluetooth]   3. Try power-cycling the Pico W"));
    Serial.println(F("[Bluetooth]   4. Ensure no other Bluetooth connections are active"));
    Serial.println(F("[Bluetooth]   5. NOTE: BluetoothHCI may only find BLE devices"));
    Serial.println(F("[Bluetooth]      Classic Bluetooth (A2DP) speakers may not appear"));
    Serial.println(F("[Bluetooth]      You may need to use device MAC address directly"));
    Serial.println(F("[Bluetooth]      Check your phone's Bluetooth settings for the MAC"));
    return false;
  }

  Serial.print(F("[Bluetooth] Found "));
  Serial.print(devices.size());
  Serial.println(F(" device(s):"));
  Serial.println();
  
  size_t index = 1;
  for (auto& device : devices) {
    Serial.print(F("  "));
    Serial.print(index++);
    Serial.print(F(". Name: "));
    String deviceName = device.name();
    if (deviceName.length() > 0) {
      Serial.print(deviceName);
    } else {
      Serial.print(F("(Unknown)"));
    }
    Serial.print(F(" - MAC: "));
    Serial.println(device.addressString());
  }
  Serial.println();
  Serial.println(F("[Bluetooth] Use connectBluetoothDevice() to connect to a device"));
  
  return true;
}

// Helper function to find device by name and return MAC address string
static bool findDeviceMAC(const char* deviceName, String& macAddressString) {
  if (!bluetoothHCI || !bluetoothHCIInitialized) {
    Serial.println(F("[Bluetooth] BluetoothHCI not initialized. Cannot scan for device."));
    return false;
  }

  Serial.print(F("[Bluetooth] Scanning for device: "));
  Serial.println(deviceName);
  Serial.println(F("[Bluetooth] IMPORTANT: Device must be in pairing/discoverable mode!"));
  Serial.println(F("[Bluetooth] Scan will take up to 30 seconds..."));
  Serial.flush();
  
  // IMPORTANT: A2DPSource might interfere with scanning
  if (a2dpSource && bluetoothConnected) {
    Serial.println(F("[Bluetooth] WARNING: Disconnecting A2DPSource for scanning..."));
    a2dpSource->disconnect();
    bluetoothConnected = false;
    delay(500);
  }
  
  // Ensure BluetoothHCI is ready
  delay(2000);
  Serial.println(F("[Bluetooth] Starting device scan..."));
  Serial.flush();

  // Try mask 0 first (all device types)
  unsigned long scanStartTime = millis();
  auto devices = bluetoothHCI->scan(0, 30, false);
  unsigned long scanDuration = millis() - scanStartTime;
  
  Serial.print(F("[Bluetooth] First scan completed in "));
  Serial.print(scanDuration / 1000);
  Serial.print(F(" seconds. Found "));
  Serial.print(devices.size());
  Serial.println(F(" device(s)"));
  Serial.flush();
  
  // If no devices found, try mask 0xFFFFFFFF
  if (devices.empty()) {
    Serial.println(F("[Bluetooth] No devices found with mask 0. Trying mask 0xFFFFFFFF..."));
    delay(1000);
    scanStartTime = millis();
    devices = bluetoothHCI->scan(0xFFFFFFFF, 30, false);
    scanDuration = millis() - scanStartTime;
    
    Serial.print(F("[Bluetooth] Second scan completed in "));
    Serial.print(scanDuration / 1000);
    Serial.print(F(" seconds. Found "));
    Serial.print(devices.size());
    Serial.println(F(" device(s)"));
    Serial.flush();
  }
  
  if (devices.empty()) {
    Serial.println(F("[Bluetooth] No Bluetooth devices found during scan."));
    return false;
  }

  Serial.print(F("[Bluetooth] Found "));
  Serial.print(devices.size());
  Serial.println(F(" device(s) during scan. Searching for device name..."));
  Serial.flush();

  // Search for device with matching name (case-insensitive)
  String targetName(deviceName);
  targetName.toLowerCase();
  
  for (auto& device : devices) {
    String deviceNameStr = device.name();
    String deviceNameLower = deviceNameStr;
    deviceNameLower.toLowerCase();
    
    Serial.print(F("[Bluetooth] Checking device: \""));
    Serial.print(deviceNameStr);
    Serial.print(F("\" vs \""));
    Serial.print(deviceName);
    Serial.println(F("\""));
    Serial.flush();
    
    // Check if names match (case-insensitive)
    if (deviceNameLower == targetName || deviceNameStr == deviceName) {
      // Found matching device - get MAC address string
      macAddressString = device.addressString();
      Serial.print(F("[Bluetooth] Found device! MAC: "));
      Serial.println(macAddressString);
      return true;
    }
  }

  Serial.println(F("[Bluetooth] Device not found in scan results."));
  Serial.println(F("[Bluetooth] Available devices:"));
  for (auto& device : devices) {
    String devName = device.name();
    Serial.print(F("  - "));
    Serial.print(devName.length() > 0 ? devName : F("(Unknown)"));
    Serial.println();
  }
  
  return false;
}

bool connectBluetoothDevice(const char* deviceName) {
  if (!bluetoothInitialized) {
    Serial.println(F("[Bluetooth] Bluetooth not initialized. Use initBluetoothAudio() first."));
    return false;
  }

  Serial.println(F("[Bluetooth] === Connecting to Bluetooth Device ==="));
  
  // Initialize A2DPSource NOW (when we're ready to connect)
  // This avoids conflicts with BluetoothHCI during scanning
  if (!a2dpSource) {
    Serial.println(F("[Bluetooth] Initializing A2DPSource for connection..."));
    a2dpSource = new A2DPSource();
    if (!a2dpSource) {
      Serial.println(F("[Bluetooth] ERROR: Failed to create A2DPSource"));
      return false;
    }
    
    // Initialize A2DPSource (following example: a2dp.begin())
    Serial.println(F("[Bluetooth] Starting A2DPSource..."));
    a2dpSource->begin();
    
    // Wait for initialization
    delay(500);
    Serial.println(F("[Bluetooth] A2DPSource initialized."));
  }

  // If device name provided, log it (but A2DPSource connects to first available)
  if (deviceName && strlen(deviceName) > 0) {
    Serial.print(F("[Bluetooth] Target device: "));
    Serial.println(deviceName);
    Serial.println(F("[Bluetooth] NOTE: A2DPSource connects to first available A2DP sink."));
    Serial.println(F("[Bluetooth] Make sure this device is the only one in pairing mode."));
    Serial.flush();
  }

  // Disconnect if already connected (following example pattern)
  if (bluetoothConnected) {
    Serial.println(F("[Bluetooth] Disconnecting existing connection..."));
    a2dpSource->disconnect();
    bluetoothConnected = false;
    delay(500);
  }

  // Clear any existing pairing (following example: a2dp.clearPairing())
  Serial.println(F("[Bluetooth] Clearing existing pairing information..."));
  a2dpSource->clearPairing();
  delay(500);

  // Connect to first available A2DP sink device (following example: a2dp.connect())
  // This is how the official example works - it connects to the first available device
  Serial.println(F("[Bluetooth] Scanning for and connecting to first available A2DP sink..."));
  Serial.println(F("[Bluetooth] Make sure target device is in pairing/discoverable mode!"));
  Serial.flush();

  // Use A2DPSource's connect() method (no parameters - connects to first available)
  // This follows the official example pattern
  bool connected = a2dpSource->connect();
  
  if (connected) {
    bluetoothConnected = true;
    
    Serial.println(F("[Bluetooth] Successfully connected!"));
    
    delay(500); // Wait a bit for connection to stabilize
    
    return true;
  } else {
    Serial.println(F("[Bluetooth] Connection failed."));
    Serial.println(F("[Bluetooth] Troubleshooting:"));
    Serial.println(F("[Bluetooth]   1. Make sure device is in pairing/discoverable mode"));
    Serial.println(F("[Bluetooth]   2. Ensure device is within range"));
    Serial.println(F("[Bluetooth]   3. Try disconnecting device from other sources first"));
    Serial.println(F("[Bluetooth]   4. Ensure device supports A2DP sink profile"));
    return false;
  }
}

bool isBluetoothConnected() {
  return bluetoothConnected && bluetoothInitialized;
}

void disconnectBluetooth() {
  if (a2dpSource && bluetoothInitialized) {
    // Use disconnect() instead of end() (following example pattern)
    // end() would shut down A2DPSource completely, disconnect() just disconnects from device
    a2dpSource->disconnect();
    bluetoothConnected = false;
    Serial.println(F("[Bluetooth] Bluetooth disconnected"));
  }
}

Stream* getBluetoothAudioStream() {
  if (bluetoothConnected && a2dpSource) {
    return a2dpSource;
  }
  return nullptr;
}

