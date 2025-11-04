#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "game_state_v2.h"

// Global game state pointer (defined in web_api.cpp)
extern GameStateManager* gameStatePtr;

// CAN log callback
void onRawCanMessage(uint16_t receiverId, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp);

// API handlers
void handleStatus(WiFiClient& client);
void handleAll(WiFiClient& client);  // Combined endpoint returning all data
void handleCommand(WiFiClient& client, String body);
void handleGetConfig(WiFiClient& client);
void handleSetConfig(WiFiClient& client, String body);
void handleCanLog(WiFiClient& client);
void handleModules(WiFiClient& client);
void handlePing(WiFiClient& client);

