#pragma once

#include <Arduino.h>
#include "game_state_v2.h"

// Initialize web server (WiFi and HTTP)
void initWebServer(GameStateManager* gsm);

// Update web server (handle requests)
void updateWebServer();

// Get WiFi IP address
String getWiFiIP();

