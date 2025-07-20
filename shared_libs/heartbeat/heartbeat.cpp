#include "heartbeat.h"
#include <Arduino.h>
#include <can_bus.h>

// Global instance for convenience functions
static HeartbeatManager* globalHeartbeat = nullptr;

// HeartbeatManager Implementation
HeartbeatManager::HeartbeatManager(unsigned long interval) 
    : lastHeartbeat(0), heartbeatInterval(interval), enabled(false), 
      currentStatus(MODULE_STATUS_IDLE), progress(0) {
}

void HeartbeatManager::begin() {
    enabled = true;
    lastHeartbeat = millis();
    Serial.print("Heartbeat: Initialized with ");
    Serial.print(heartbeatInterval);
    Serial.println("ms interval");
}

void HeartbeatManager::update() {
    if (!enabled) return;
    
    unsigned long now = millis();
    if (now - lastHeartbeat >= heartbeatInterval) {
        sendNow();
    }
}

void HeartbeatManager::setInterval(unsigned long intervalMs) {
    heartbeatInterval = intervalMs;
    Serial.print("Heartbeat: Interval set to ");
    Serial.print(intervalMs);
    Serial.println("ms");
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
    if (!enabled) return;
    
    // Enhanced heartbeat format: [MODULE_HEARTBEAT, status, solved_flag, progress]
    uint8_t heartbeatData[4];
    heartbeatData[0] = MODULE_HEARTBEAT;
    heartbeatData[1] = currentStatus;
    heartbeatData[2] = (currentStatus == MODULE_STATUS_SOLVED) ? 1 : 0;
    heartbeatData[3] = progress;
    
    sendCanMessage(CAN_ID_TIMER, heartbeatData, 4);
    lastHeartbeat = millis();
    
    // Optional debug output (uncomment for troubleshooting)
    // Serial.print("Heartbeat sent - Status: ");
    // Serial.print(currentStatus);
    // Serial.print(" Progress: ");
    // Serial.println(progress);
}

bool HeartbeatManager::isEnabled() const {
    return enabled;
}

ModuleStatus HeartbeatManager::getStatus() const {
    return currentStatus;
}

unsigned long HeartbeatManager::getInterval() const {
    return heartbeatInterval;
}

// Global convenience functions
void initHeartbeat(unsigned long intervalMs) {
    static HeartbeatManager instance(intervalMs);
    globalHeartbeat = &instance;
    globalHeartbeat->begin();
}

void updateHeartbeat() {
    if (globalHeartbeat) {
        globalHeartbeat->update();
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