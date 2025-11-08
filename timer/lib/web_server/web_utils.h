#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "game_state.h"

// Helper function to send HTTP response
void sendResponse(WiFiClient& client, int code, const String& contentType, const String& body);

// Helper function to format game state
String formatGameState(GameState state);
String formatModuleStatus(ModuleStatus status);

// Parse HTTP request
bool parseRequest(WiFiClient& client, String& method, String& path);

