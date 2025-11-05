#include "web_api.h"
#include "web_utils.h"
#include <ArduinoJson.h>
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
    
    // Include detailed edgework
    JsonArray indicatorsArray = doc.createNestedArray("indicatorDetails");
    for (const auto& indicator : edge.indicators) {
        JsonObject indObj = indicatorsArray.createNestedObject();
        indObj["label"] = indicator.label;
        indObj["lit"] = indicator.lit;
        indObj["type"] = static_cast<uint8_t>(indicator.type);
    }
    
    JsonArray portsArray = doc.createNestedArray("portDetails");
    for (const auto& port : edge.ports) {
        JsonObject portObj = portsArray.createNestedObject();
        portObj["label"] = port.label;
        portObj["type"] = static_cast<uint8_t>(port.type);
    }

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
    if (!gameStatePtr) {
        sendResponse(client, 500, "application/json", "{\"success\":false,\"error\":\"Game state not initialized\"}");
        return;
    }
    
    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    
    // Get all modules from game state
    const std::vector<Module>& allModules = gameStatePtr->getAllModules();
    JsonArray modulesArray = doc.createNestedArray("modules");
    
    for (const auto& module : allModules) {
        JsonObject moduleObj = modulesArray.createNestedObject();
        moduleObj["id"] = "0x" + String(module.canId, HEX);
        moduleObj["canId"] = module.canId;
        
        uint8_t moduleType = static_cast<uint8_t>(module.type);
        moduleObj["type"] = getModuleTypeName(moduleType);
        moduleObj["typeId"] = moduleType;
        moduleObj["category"] = static_cast<uint8_t>(module.category);
        
        moduleObj["isRegistered"] = true;  // All modules in game_state are registered
        moduleObj["isActive"] = module.isActive;
        moduleObj["isSolved"] = module.isSolved;
        // If lastSeen is 0, send 0 (never seen). Otherwise send seconds ago
        moduleObj["lastSeen"] = (module.lastSeen == 0) ? 0 : (millis() - module.lastSeen) / 1000;
        // For activationTime: if it's a needy module with interval, send time until next activation (or 0 if passed)
        // For regular modules, activationTime is 0
        if (module.intervalMs > 0 && module.activationTime > 0) {
            unsigned long now = millis();
            if (module.activationTime > now) {
                moduleObj["activationTime"] = (module.activationTime - now) / 1000; // seconds until activation
            } else {
                moduleObj["activationTime"] = 0; // activation time has passed
            }
        } else {
            moduleObj["activationTime"] = 0; // Not a needy module or not scheduled
        }
        moduleObj["intervalMs"] = module.intervalMs;
    }
    
    doc["totalModules"] = allModules.size();
    doc["registeredModules"] = allModules.size();  // All are registered
    doc["solvedModules"] = gameStatePtr->getSolvedModules();
    
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
    
    // Include detailed edgework
    JsonArray indicatorsArray = doc["status"].createNestedArray("indicatorDetails");
    for (const auto& indicator : edge.indicators) {
        JsonObject indObj = indicatorsArray.createNestedObject();
        indObj["label"] = indicator.label;
        indObj["lit"] = indicator.lit;
        indObj["type"] = static_cast<uint8_t>(indicator.type);
    }
    
    JsonArray portsArray = doc["status"].createNestedArray("portDetails");
    for (const auto& port : edge.ports) {
        JsonObject portObj = portsArray.createNestedObject();
        portObj["label"] = port.label;
        portObj["type"] = static_cast<uint8_t>(port.type);
    }
    
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
    const std::vector<Module>& allModules = gameStatePtr->getAllModules();
    JsonArray modulesArray = doc.createNestedArray("modules");
    
    for (const auto& module : allModules) {
        JsonObject moduleObj = modulesArray.createNestedObject();
        moduleObj["id"] = "0x" + String(module.canId, HEX);
        moduleObj["canId"] = module.canId;
        
        uint8_t moduleType = static_cast<uint8_t>(module.type);
        moduleObj["type"] = getModuleTypeName(moduleType);
        moduleObj["typeId"] = moduleType;
        moduleObj["category"] = static_cast<uint8_t>(module.category);
        
        moduleObj["isRegistered"] = true;  // All modules in game_state are registered
        moduleObj["isActive"] = module.isActive;
        moduleObj["isSolved"] = module.isSolved;
        // If lastSeen is 0, send 0 (never seen). Otherwise send seconds ago
        moduleObj["lastSeen"] = (module.lastSeen == 0) ? 0 : (millis() - module.lastSeen) / 1000;
        // For activationTime: if it's a needy module with interval, send time until next activation (or 0 if passed)
        // For regular modules, activationTime is 0
        if (module.intervalMs > 0 && module.activationTime > 0) {
            unsigned long now = millis();
            if (module.activationTime > now) {
                moduleObj["activationTime"] = (module.activationTime - now) / 1000; // seconds until activation
            } else {
                moduleObj["activationTime"] = 0; // activation time has passed
            }
        } else {
            moduleObj["activationTime"] = 0; // Not a needy module or not scheduled
        }
        moduleObj["intervalMs"] = module.intervalMs;
    }
    
    doc["modulesMeta"]["totalModules"] = allModules.size();
    doc["modulesMeta"]["registeredModules"] = allModules.size();  // All are registered
    doc["modulesMeta"]["solvedModules"] = gameStatePtr->getSolvedModules();
    
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


