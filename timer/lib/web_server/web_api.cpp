#include "web_api.h"
#include "web_utils.h"
#include <ArduinoJson.h>
#include <module_tracker.h>
#include <can_bus.h>

// Global game state pointer (set by web_server.cpp)
GameStateManager* gameStatePtr = nullptr;

// External CAN bus variables
extern bool canBusInitialized;
extern uint16_t thisModuleId;
extern volatile uint32_t canInterruptCount;
extern bool audioModuleConnected;
extern bool serialDisplayConnected;
extern unsigned long lastAudioPing;
extern unsigned long lastSerialDisplayPing;
extern bool idConflictDetected;

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

// Generate CAN log output
String getCanLogOutput() {
    String output = "";
    
    // CAN Bus Status
    output += "=== CAN BUS STATUS ===\n";
    output += "Initialized: " + String(canBusInitialized ? "YES" : "NO") + "\n";
    output += "Module ID: 0x" + String(thisModuleId, HEX) + " (" + String(thisModuleId) + ")\n";
    
    uint8_t moduleType = (thisModuleId >> 5) & 0x7F;
    uint8_t instanceId = thisModuleId & 0x1F;
    output += "Type: 0x" + String(moduleType, HEX) + " (" + String(getModuleTypeName(moduleType)) + ")\n";
    output += "Instance: 0x" + String(instanceId, HEX) + " (" + String(instanceId) + ")\n";
    output += "ID Conflict: " + String(idConflictDetected ? "YES" : "NO") + "\n\n";
    
    output += "--- MODULE CONNECTIONS ---\n";
    output += "Audio: ";
    if (audioModuleConnected) {
        output += "CONNECTED (last ping: " + String((millis() - lastAudioPing) / 1000) + "s ago)\n";
    } else {
        output += "DISCONNECTED\n";
    }
    
    output += "Serial Display: ";
    if (serialDisplayConnected) {
        output += "CONNECTED (last ping: " + String((millis() - lastSerialDisplayPing) / 1000) + "s ago)\n";
    } else {
        output += "DISCONNECTED\n";
    }
    
    output += "\n--- STATISTICS ---\n";
    output += "CAN Interrupts: " + String(canInterruptCount) + "\n\n";
    
    output += "--- CAN ID REFERENCE ---\n";
    output += "TIMER: 0x" + String(CAN_ID_TIMER, HEX) + " (" + String(CAN_ID_TIMER) + ")\n";
    output += "AUDIO: 0x" + String(CAN_ID_AUDIO, HEX) + " (" + String(CAN_ID_AUDIO) + ")\n";
    output += "SERIAL_DISPLAY: 0x" + String(CAN_ID_SERIAL_DISPLAY, HEX) + " (" + String(CAN_ID_SERIAL_DISPLAY) + ")\n";
    output += "BROADCAST: 0x" + String(CAN_ID_BROADCAST, HEX) + " (" + String(CAN_ID_BROADCAST) + ")\n\n";
    
    output += "No message logging yet - coming soon!\n";
    
    return output;
}

// Handle CAN log API
void handleCanLog(WiFiClient& client) {
    String canLogOutput = getCanLogOutput();
    sendResponse(client, 200, "text/plain", canLogOutput);
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

