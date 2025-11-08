#pragma once

#include <stdint.h>
#include <can_bus.h>

// Simplified heartbeat intervals - only two modes
#define HEARTBEAT_INTERVAL_DISCOVERY 1000   // 1 second for fast module discovery
#define HEARTBEAT_INTERVAL_GAME 5000        // 5 seconds during game play

// Game state for timing control
enum GameRunningState : uint8_t {
    GAME_NOT_RUNNING = 0,
    GAME_RUNNING = 1
};

// Heartbeat manager class
class HeartbeatManager {
private:
    unsigned long lastHeartbeat;
    bool enabled;
    ModuleStatus currentStatus;
    uint8_t progress;
    GameRunningState gameState;
    
public:
    // Constructor
    HeartbeatManager();
    
    // Initialize the heartbeat system
    void begin();
    
    // Update function - call this in loop()
    void update();
    
    // Game state management
    void setGameRunning(bool running);
    bool isGameRunning() const;
    
    // Get current interval based on game state
    unsigned long getCurrentInterval() const;
    
    // Configuration
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
};

// Global convenience functions for simple usage
void initHeartbeat();
void updateHeartbeat();
void setHeartbeatGameRunning(bool running);
void setHeartbeatStatus(ModuleStatus status);
void setHeartbeatProgress(uint8_t progress);
void setHeartbeatSolved(bool solved = true);
void sendHeartbeatNow(); 