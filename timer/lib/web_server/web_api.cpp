#include "web_api.h"
#include "web_utils.h"
#include <ArduinoJson.h>
#include <module_tracker.h>
#include <can_bus.h>

// Global game state pointer (set by web_server.cpp)
GameStateManager* gameStatePtr = nullptr;

// CAN message log storage
#define MAX_CAN_LOG_ENTRIES 200
struct CanLogEntry {
    uint16_t receiverId;
    uint16_t senderId;
    uint8_t data[8];
    uint8_t len;
    unsigned long timestamp;
};

static CanLogEntry canLog[MAX_CAN_LOG_ENTRIES];
static uint16_t canLogIndex = 0;
static uint16_t canLogCount = 0;

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

// Handle CAN log API
void handleCanLog(WiFiClient& client) {
    DynamicJsonDocument doc(16384); // Large enough for 200 entries
    doc["success"] = true;
    doc["count"] = canLogCount;
    
    JsonArray messages = doc.createNestedArray("messages");
    
    // Start from the oldest entry (circular buffer)
    uint16_t startIndex = (canLogCount < MAX_CAN_LOG_ENTRIES) ? 0 : canLogIndex;
    
    for (uint16_t i = 0; i < canLogCount && i < MAX_CAN_LOG_ENTRIES; i++) {
        uint16_t idx = (startIndex + i) % MAX_CAN_LOG_ENTRIES;
        const CanLogEntry& entry = canLog[idx];
        
        JsonObject msg = messages.createNestedObject();
        
        // Sender info
        uint8_t senderType = (entry.senderId >> 5) & 0x7F;
        msg["senderId"] = entry.senderId;
        msg["senderName"] = String(getModuleTypeName(senderType));
        msg["senderDisplay"] = String(getModuleTypeName(senderType)) + " (0x" + String(entry.senderId, HEX) + ")";
        
        // Receiver info
        uint8_t receiverType = (entry.receiverId >> 5) & 0x7F;
        msg["receiverId"] = entry.receiverId;
        msg["receiverName"] = String(getModuleTypeName(receiverType));
        msg["receiverDisplay"] = String(getModuleTypeName(receiverType)) + " (0x" + String(entry.receiverId, HEX) + ")";
        
        // Decode message data
        String dataDecoded = "";
        if (entry.len >= 3) {
            // Message type is at byte 2
            uint8_t msgType = entry.data[2];
            dataDecoded += String(getMessageTypeName(msgType));
            
            // Additional payload
            if (entry.len > 3) {
                dataDecoded += " [";
                for (uint8_t j = 3; j < entry.len; j++) {
                    if (j > 3) dataDecoded += ", ";
                    dataDecoded += String(entry.data[j]);
                }
                dataDecoded += "]";
            }
        }
        msg["dataDecoded"] = dataDecoded;
        
        // Data (hex)
        JsonArray dataArray = msg.createNestedArray("data");
        for (uint8_t j = 0; j < entry.len; j++) {
            dataArray.add(entry.data[j]);
        }
        
        msg["len"] = entry.len;
        msg["timestamp"] = entry.timestamp;
    }
    
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response);
}

// CAN log callback function
void onRawCanMessage(uint16_t receiverId, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp) {
    CanLogEntry& entry = canLog[canLogIndex];
    entry.receiverId = receiverId;
    entry.senderId = senderId;
    entry.len = len > 8 ? 8 : len; // Cap at 8 bytes
    entry.timestamp = timestamp;
    memcpy(entry.data, data, entry.len);
    
    canLogIndex = (canLogIndex + 1) % MAX_CAN_LOG_ENTRIES;
    if (canLogCount < MAX_CAN_LOG_ENTRIES) {
        canLogCount++;
    }
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

