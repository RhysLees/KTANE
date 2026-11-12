#pragma once
#include <Arduino.h>
#include <AudioTools.h>

// Initialize Bluetooth A2DP audio system
bool initBluetoothAudio();

// Scan for available Bluetooth devices
bool scanBluetoothDevices();

// Connect to a Bluetooth device by name
bool connectBluetoothDevice(const char* deviceName);

// Check if Bluetooth is connected
bool isBluetoothConnected();

// Disconnect Bluetooth
void disconnectBluetooth();

// Get the A2DPSource stream pointer (for audio output)
Stream* getBluetoothAudioStream();

