#pragma once

#include <Arduino.h>
#include "game_state.h"

// Initialize web server (WiFi and HTTP)
void initWebServer(GameStateManager* gsm);

// Update web server (handle requests)
void updateWebServer();

// Get WiFi IP address
String getWiFiIP();

// Get WiFi connection status
bool isWiFiConnected();

// Get WiFi mode (AP or STA)
String getWiFiMode();

