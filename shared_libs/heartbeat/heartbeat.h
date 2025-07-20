#pragma once

#include <stdint.h>

// Default heartbeat intervals for different module types
#define HEARTBEAT_INTERVAL_AUDIO 2000        // 2 seconds
#define HEARTBEAT_INTERVAL_SERIAL_DISPLAY 3000  // 3 seconds  
#define HEARTBEAT_INTERVAL_MODULE 5000       // 5 seconds for game modules
#define HEARTBEAT_INTERVAL_NEEDY 1000        // 1 second for needy modules

// Module status flags for enhanced heartbeats
enum ModuleStatus : uint8_t {
    MODULE_STATUS_IDLE = 0x00,
    MODULE_STATUS_ACTIVE = 0x01,
    MODULE_STATUS_SOLVED = 0x02,
    MODULE_STATUS_ARMED = 0x03,      // For needy modules
    MODULE_STATUS_ERROR = 0xFF
};

// Heartbeat manager class
class HeartbeatManager {
private:
    unsigned long lastHeartbeat;
    unsigned long heartbeatInterval;
    bool enabled;
    ModuleStatus currentStatus;
    uint8_t progress;
    
public:
    // Constructor with default 5-second interval
    HeartbeatManager(unsigned long interval = HEARTBEAT_INTERVAL_MODULE);
    
    // Initialize the heartbeat system
    void begin();
    
    // Update function - call this in loop()
    void update();
    
    // Configuration
    void setInterval(unsigned long intervalMs);
    void enable(bool enabled = true);
    void disable();
    
    // Status management
    void setStatus(ModuleStatus status);
    void setProgress(uint8_t progressPercent);
    void setSolved(bool solved = true);
    
    // Manual heartbeat (bypasses timing)
    void sendNow();
    
    // Status queries
    bool isEnabled() const;
    ModuleStatus getStatus() const;
    unsigned long getInterval() const;
};

// Global convenience functions for simple usage
void initHeartbeat(unsigned long intervalMs = HEARTBEAT_INTERVAL_MODULE);
void updateHeartbeat();
void setHeartbeatStatus(ModuleStatus status);
void setHeartbeatProgress(uint8_t progress);
void setHeartbeatSolved(bool solved = true);
void sendHeartbeatNow(); 