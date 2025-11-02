#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "game_state_v2.h"

// Global game state pointer (defined in web_api.cpp)
extern GameStateManager* gameStatePtr;

// API handlers
void handleStatus(WiFiClient& client);
void handleCommand(WiFiClient& client, String body);
void handleGetConfig(WiFiClient& client);
void handleSetConfig(WiFiClient& client, String body);
void handleCanLog(WiFiClient& client);
void handleModules(WiFiClient& client);
String getCanLogOutput();

