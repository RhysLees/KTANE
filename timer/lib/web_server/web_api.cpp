#include "web_api.h"
#include "web_utils.h"
#include <ArduinoJson.h>
#include <module_tracker.h>
#include <can_bus.h>

// Global game state pointer (set by web_server.cpp)
GameStateManager* gameStatePtr = nullptr;

// Handle status API
void handleStatus(WiFiClient& client) {
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(1024);
    doc["success"] = true;
    doc["state"] = formatGameState(gameStatePtr->getState());
    doc["timeRemaining"] = gameStatePtr->getRemainingTime();
    doc["strikes"] = gameStatePtr->getStrikes();
    doc["maxStrikes"] = gameStatePtr->getMaxStrikes();
    doc["totalModules"] = gameStatePtr->getTotalModules();
    doc["solvedModules"] = gameStatePtr->getSolvedModules();
    doc["remainingModules"] = gameStatePtr->getTotalModules() - gameStatePtr->getSolvedModules();
    
    doc["serialNumber"] = gameStatePtr->getSerialNumber();
    doc["batteries"] = gameStatePtr->getBatteryCount();
    
    const Edgework& edge = gameStatePtr->getEdgework();
    doc["indicators"] = edge.indicators.size();
    doc["ports"] = edge.ports.size();

    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle command API
void handleCommand(WiFiClient& client, String body) {
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error || !doc.containsKey("command")) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
        return;
    }

    String cmd = doc["command"].as<String>();
    bool success = true;
    String errorMsg = "";

    if (cmd == "start") {
        if (gameStatePtr->getState() == GameState::IDLE) {
            gameStatePtr->startGame();
        } else if (gameStatePtr->getState() == GameState::PAUSED) {
            gameStatePtr->resumeTimer();
        } else {
            success = false;
            errorMsg = "Cannot start from current state";
        }
    } else if (cmd == "pause") {
        if (gameStatePtr->getState() == GameState::RUNNING) {
            gameStatePtr->pauseTimer();
        } else {
            success = false;
            errorMsg = "Game is not running";
        }
    } else if (cmd == "reset") {
        gameStatePtr->reset();
    } else if (cmd == "addStrike") {
        gameStatePtr->addStrike();
    } else if (cmd == "clearStrikes") {
        gameStatePtr->clearStrikes();
    } else if (cmd == "solveModule") {
        // Find first unsolved module and solve it
        for (uint16_t i = 0x10; i <= 0x6F; i++) {
            if (gameStatePtr->getModule(i) && !gameStatePtr->isModuleSolved(i)) {
                gameStatePtr->setModuleSolved(i);
                break;
            }
        }
    } else if (cmd.startsWith("setTime:")) {
        String timeStr = cmd.substring(8);
        int colonIndex = timeStr.indexOf(':');
        if (colonIndex != -1) {
            int mins = timeStr.substring(0, colonIndex).toInt();
            int secs = timeStr.substring(colonIndex + 1).toInt();
            unsigned long customCountdownMillis = (mins * 60UL + secs) * 1000UL;
            gameStatePtr->setTimeLimit(customCountdownMillis);
            gameStatePtr->resetTimer();
        } else {
            success = false;
            errorMsg = "Invalid time format";
        }
    } else {
        success = false;
        errorMsg = "Unknown command";
    }

    DynamicJsonDocument response(200);
    response["success"] = success;
    if (!success) response["error"] = errorMsg;
    String responseStr;
    serializeJson(response, responseStr);
    sendResponse(client, success ? 200 : 400, "application/json", responseStr);
}

// Handle get config
void handleGetConfig(WiFiClient& client) {
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    GameConfig config = gameStatePtr->getConfig();
    DynamicJsonDocument doc(512);
    doc["success"] = true;
    doc["config"]["maxStrikes"] = config.maxStrikes;
    doc["config"]["enableStrikeAcceleration"] = config.enableStrikeAcceleration;
    doc["config"]["strikeAccelerationFactor"] = config.strikeAccelerationFactor;
    doc["config"]["enableEmergencyAlarm"] = config.enableEmergencyAlarm;
    doc["config"]["emergencyAlarmThreshold"] = config.emergencyAlarmThreshold;
    doc["config"]["enableNeedyModules"] = config.enableNeedyModules;
    doc["config"]["enableEdgework"] = config.enableEdgework;

    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle set config
void handleSetConfig(WiFiClient& client, String body) {
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    GameConfig config;
    config.maxStrikes = doc["maxStrikes"] | 3;
    config.enableStrikeAcceleration = doc["enableStrikeAcceleration"] | true;
    config.strikeAccelerationFactor = doc["strikeAccelerationFactor"] | 0.25f;
    config.enableEmergencyAlarm = doc["enableEmergencyAlarm"] | true;
    config.emergencyAlarmThreshold = doc["emergencyAlarmThreshold"] | 60000;
    config.enableNeedyModules = doc["enableNeedyModules"] | true;
    config.enableEdgework = doc["enableEdgework"] | true;
    config.timeLimitMs = 300000; // Keep existing time limit

    gameStatePtr->setConfig(config);

    DynamicJsonDocument response(200);
    response["success"] = true;
    String responseStr;
    serializeJson(response, responseStr);
    sendResponse(client, 200, "application/json", responseStr);
}

// Handle modules API
void handleModules(WiFiClient& client) {
    ModuleTracker* tracker = getModuleTracker();
    
    if (!tracker) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Module tracker not initialized\"}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    
    // Get discovered modules
    std::map<uint16_t, ModuleInfo> discoveredModules = tracker->getDiscoveredModules();
    JsonArray modulesArray = doc.createNestedArray("modules");
    
    for (const auto& pair : discoveredModules) {
        JsonObject moduleObj = modulesArray.createNestedObject();
        moduleObj["id"] = "0x" + String(pair.first, HEX);
        moduleObj["canId"] = pair.first;
        moduleObj["type"] = pair.second.moduleTypeName;
        moduleObj["isRegistered"] = pair.second.isRegistered;
        moduleObj["isActive"] = pair.second.isActive;
        moduleObj["isSolved"] = pair.second.isSolved;
        moduleObj["progress"] = pair.second.progress;
        moduleObj["lastStatus"] = pair.second.lastStatus;
        moduleObj["lastHeartbeat"] = (millis() - pair.second.lastHeartbeat) / 1000;
    }
    
    doc["totalModules"] = discoveredModules.size();
    doc["registeredModules"] = tracker->getRegisteredModuleCount();
    
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle combined "all" API - returns status, modules, and config
void handleAll(WiFiClient& client) {
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }

    // Create a document - reduced size to avoid heap exhaustion on RP2040
    // Start with 8KB, allocate more if needed (but this should be sufficient)
    DynamicJsonDocument doc(8192);  // Reduced from 16KB to avoid memory issues
    doc["success"] = true;
    
    // Include status data
    doc["status"]["state"] = formatGameState(gameStatePtr->getState());
    doc["status"]["timeRemaining"] = gameStatePtr->getRemainingTime();
    doc["status"]["strikes"] = gameStatePtr->getStrikes();
    doc["status"]["maxStrikes"] = gameStatePtr->getMaxStrikes();
    doc["status"]["totalModules"] = gameStatePtr->getTotalModules();
    doc["status"]["solvedModules"] = gameStatePtr->getSolvedModules();
    doc["status"]["remainingModules"] = gameStatePtr->getTotalModules() - gameStatePtr->getSolvedModules();
    doc["status"]["serialNumber"] = gameStatePtr->getSerialNumber();
    doc["status"]["batteries"] = gameStatePtr->getBatteryCount();
    
    const Edgework& edge = gameStatePtr->getEdgework();
    doc["status"]["indicators"] = edge.indicators.size();
    doc["status"]["ports"] = edge.ports.size();
    
    // Include config data
    GameConfig config = gameStatePtr->getConfig();
    doc["config"]["maxStrikes"] = config.maxStrikes;
    doc["config"]["enableStrikeAcceleration"] = config.enableStrikeAcceleration;
    doc["config"]["strikeAccelerationFactor"] = config.strikeAccelerationFactor;
    doc["config"]["enableEmergencyAlarm"] = config.enableEmergencyAlarm;
    doc["config"]["emergencyAlarmThreshold"] = config.emergencyAlarmThreshold;
    doc["config"]["enableNeedyModules"] = config.enableNeedyModules;
    doc["config"]["enableEdgework"] = config.enableEdgework;
    
    // Include modules data
    ModuleTracker* tracker = getModuleTracker();
    if (tracker) {
        std::map<uint16_t, ModuleInfo> discoveredModules = tracker->getDiscoveredModules();
        JsonArray modulesArray = doc.createNestedArray("modules");
        
        for (const auto& pair : discoveredModules) {
            JsonObject moduleObj = modulesArray.createNestedObject();
            moduleObj["id"] = "0x" + String(pair.first, HEX);
            moduleObj["canId"] = pair.first;
            moduleObj["type"] = pair.second.moduleTypeName;
            moduleObj["isRegistered"] = pair.second.isRegistered;
            moduleObj["isActive"] = pair.second.isActive;
            moduleObj["isSolved"] = pair.second.isSolved;
            moduleObj["progress"] = pair.second.progress;
            moduleObj["lastStatus"] = pair.second.lastStatus;
            moduleObj["lastHeartbeat"] = (millis() - pair.second.lastHeartbeat) / 1000;
        }
        
        doc["modulesMeta"]["totalModules"] = discoveredModules.size();
        doc["modulesMeta"]["registeredModules"] = tracker->getRegisteredModuleCount();
    } else {
        doc["modules"] = JsonArray();
        doc["modulesMeta"]["totalModules"] = 0;
        doc["modulesMeta"]["registeredModules"] = 0;
    }
    
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle ping API - informational endpoint
void handlePing(WiFiClient& client) {
    // Note: Discovery now uses heartbeat messages automatically
    // - Modules start sending heartbeats immediately when they get a CAN ID
    // - When game is not running, timer uses heartbeats for discovery
    // - Timer responds to new modules with TIMER_MODULE_DISCOVERED
    // - This endpoint is informational - modules discover automatically via heartbeat
    
    DynamicJsonDocument doc(200);
    doc["success"] = true;
    doc["message"] = "Modules discover automatically via heartbeat when game is not running";
    doc["info"] = "Heartbeat-based discovery is active - modules will be discovered automatically";
    
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
    
    Serial.println("Web server: Ping info requested - heartbeat-based discovery is active");
}

// Forward declarations for WiFi functions
extern bool loadWiFiCredentials(String& ssid, String& password);
extern bool saveWiFiCredentials(const String& ssid, const String& password);
extern bool clearWiFiCredentials();
extern bool connectToWiFi(const String& ssid, const String& password);
extern String getWiFiIP();
extern bool isWiFiConnected();
extern String getWiFiMode();

// Handle get WiFi status
void handleGetWiFi(WiFiClient& client) {
    DynamicJsonDocument doc(512);
    doc["success"] = true;
    doc["mode"] = getWiFiMode();
    doc["ip"] = getWiFiIP();
    doc["connected"] = isWiFiConnected();
    
    // Try to get stored SSID (but don't show password)
    String ssid, password;
    if (loadWiFiCredentials(ssid, password)) {
        doc["ssid"] = ssid;
        doc["hasCredentials"] = true;
    } else {
        doc["hasCredentials"] = false;
    }
    
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// Handle set WiFi credentials
void handleSetWiFi(WiFiClient& client, String body) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Check if this is a clear request
    if (doc.containsKey("clear") && doc["clear"].as<bool>()) {
        if (clearWiFiCredentials()) {
            DynamicJsonDocument response(200);
            response["success"] = true;
            response["message"] = "WiFi credentials cleared";
            String responseStr;
            serializeJson(response, responseStr);
            sendResponse(client, 200, "application/json", responseStr);
            
            Serial.println("WiFi credentials cleared - device will need to restart");
            // Note: In a production system, you might want to restart here
            // For now, the device will reconnect on next boot
        } else {
            sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Failed to clear credentials\"}");
        }
        return;
    }
    
    // Validate SSID and password
    if (!doc.containsKey("ssid")) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"SSID required\"}");
        return;
    }
    
    String ssid = doc["ssid"].as<String>();
    String password = doc.containsKey("password") ? doc["password"].as<String>() : "";
    
    if (ssid.length() == 0) {
        sendResponse(client, 400, "application/json", "{\"success\":false,\"error\":\"SSID cannot be empty\"}");
        return;
    }
    
    // Save credentials
    if (!saveWiFiCredentials(ssid, password)) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Failed to save credentials\"}");
        return;
    }
    
    Serial.print("WiFi credentials saved: ");
    Serial.println(ssid);
    
    // Try to connect to WiFi
    bool connected = connectToWiFi(ssid, password);
    
    DynamicJsonDocument response(300);
    response["success"] = true;
    if (connected) {
        response["message"] = "WiFi credentials saved and connected successfully";
        response["ip"] = getWiFiIP();
    } else {
        response["message"] = "WiFi credentials saved but connection failed. Device will try again on restart.";
        response["warning"] = "Connection failed - device will return to AP mode";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    sendResponse(client, connected ? 200 : 201, "application/json", responseStr);
}


