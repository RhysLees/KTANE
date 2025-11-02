#include "heartbeat.h"
#include <Arduino.h>
#include <can_bus.h>

// Global instance for convenience functions
static HeartbeatManager* globalHeartbeat = nullptr;

// HeartbeatManager Implementation
HeartbeatManager::HeartbeatManager() 
    : lastHeartbeat(0), enabled(false), 
      currentStatus(MODULE_STATUS_IDLE), progress(0), gameState(GAME_NOT_RUNNING) {
}

void HeartbeatManager::begin() {
    enabled = true;
    lastHeartbeat = millis();
    Serial.print("Heartbeat: Initialized - Discovery mode (");
    Serial.print(getCurrentInterval());
    Serial.println("ms)");
}

void HeartbeatManager::update() {
    if (!enabled) return;
    
    unsigned long now = millis();
    if (now - lastHeartbeat >= getCurrentInterval()) {
        sendNow();
    }
}

void HeartbeatManager::setGameRunning(bool running) {
    GameRunningState newState = running ? GAME_RUNNING : GAME_NOT_RUNNING;
    if (newState != gameState) {
        gameState = newState;
        Serial.print("Heartbeat: Game state changed to ");
        Serial.print(running ? "RUNNING" : "DISCOVERY");
        Serial.print(" (");
        Serial.print(getCurrentInterval());
        Serial.println("ms)");
        
        // Send immediate heartbeat on state change
        sendNow();
    }
}

bool HeartbeatManager::isGameRunning() const {
    return gameState == GAME_RUNNING;
}

unsigned long HeartbeatManager::getCurrentInterval() const {
    return (gameState == GAME_RUNNING) ? HEARTBEAT_INTERVAL_GAME : HEARTBEAT_INTERVAL_DISCOVERY;
}

void HeartbeatManager::enable(bool en) {
    enabled = en;
    if (enabled) {
        lastHeartbeat = millis();
        Serial.println("Heartbeat: Enabled");
    } else {
        Serial.println("Heartbeat: Disabled");
    }
}

void HeartbeatManager::disable() {
    enable(false);
}

void HeartbeatManager::setStatus(ModuleStatus status) {
    currentStatus = status;
}

void HeartbeatManager::setProgress(uint8_t progressPercent) {
    progress = min(progressPercent, 100);
}

void HeartbeatManager::setSolved(bool solved) {
    currentStatus = solved ? MODULE_STATUS_SOLVED : MODULE_STATUS_IDLE;
}

void HeartbeatManager::sendNow() {
    if (!enabled) {
        Serial.println("Heartbeat: Not enabled, skipping send");
        return;
    }
    
    // Simple heartbeat format: [MODULE_HEARTBEAT] 
    // CAN library automatically prepends sender ID
    uint8_t heartbeatData[1];
    heartbeatData[0] = MODULE_HEARTBEAT;
    
    Serial.print("Heartbeat: Sending heartbeat to timer (mode: ");
    Serial.print(gameState == GAME_RUNNING ? "GAME" : "DISCOVERY");
    Serial.println(")");
    
    sendCanMessage(CAN_ID_TIMER, heartbeatData, 1);
    lastHeartbeat = millis();
}

bool HeartbeatManager::isEnabled() const {
    return enabled;
}

ModuleStatus HeartbeatManager::getStatus() const {
    return currentStatus;
}

// Global convenience functions
void initHeartbeat() {
    static HeartbeatManager instance;
    globalHeartbeat = &instance;
    globalHeartbeat->begin();
}

void updateHeartbeat() {
    if (globalHeartbeat) {
        globalHeartbeat->update();
    }
}

void setHeartbeatGameRunning(bool running) {
    if (globalHeartbeat) {
        globalHeartbeat->setGameRunning(running);
    }
}

void setHeartbeatStatus(ModuleStatus status) {
    if (globalHeartbeat) {
        globalHeartbeat->setStatus(status);
    }
}

void setHeartbeatProgress(uint8_t progress) {
    if (globalHeartbeat) {
        globalHeartbeat->setProgress(progress);
    }
}

void setHeartbeatSolved(bool solved) {
    if (globalHeartbeat) {
        globalHeartbeat->setSolved(solved);
    }
}

void sendHeartbeatNow() {
    if (globalHeartbeat) {
        globalHeartbeat->sendNow();
    }
} 