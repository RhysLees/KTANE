#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "game_state.h"

// Global game state pointer (defined in web_api.cpp)
extern GameStateManager* gameStatePtr;

// API handlers
void handleStatus(WiFiClient& client);
void handleAll(WiFiClient& client);  // Combined endpoint returning all data
void handleCommand(WiFiClient& client, String body);
void handleGetConfig(WiFiClient& client);
void handleSetConfig(WiFiClient& client, String body);
void handleModules(WiFiClient& client);
void handleGetAudio(WiFiClient& client);
void handleSetAudio(WiFiClient& client, String body);
void handlePing(WiFiClient& client);
void handleGetWiFi(WiFiClient& client);
void handleSetWiFi(WiFiClient& client, String body);

