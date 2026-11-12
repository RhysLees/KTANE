#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>

// ============================================================================
// CONFIGURABLE TIMING CONSTANTS
// ============================================================================

// Module heartbeat timeouts (milliseconds)
#ifndef GAME_STATE_HEARTBEAT_DISCOVERY_TIMEOUT_MS
#define GAME_STATE_HEARTBEAT_DISCOVERY_TIMEOUT_MS 6000UL   // 2 missed 3s heartbeats
#endif

#ifndef GAME_STATE_HEARTBEAT_GAME_TIMEOUT_MS
#define GAME_STATE_HEARTBEAT_GAME_TIMEOUT_MS 2000UL       // 2 missed 1s heartbeats
#endif

// Module inactivity threshold (milliseconds)
#ifndef GAME_STATE_MODULE_INACTIVE_TIMEOUT_MS
#define GAME_STATE_MODULE_INACTIVE_TIMEOUT_MS 5000UL
#endif

// ============================================================================
// GAME CONSTANTS & ENUMS
// ============================================================================

// Game States
enum class GameState : uint8_t
{
    IDLE = 0,           // Ready to start
    RUNNING = 1,        // Game in progress
    PAUSED = 2,         // Game paused
    EXPLODED = 3,       // Bomb exploded (timeout or too many strikes)
    DEFUSED = 4,        // All modules solved successfully
    VICTORY = 5         // Game completed with success
};

// Module Categories
enum class ModuleCategory : uint8_t
{
    REGULAR = 0,        // Standard modules that must be solved
    NEEDY = 1,          // Modules that activate periodically
    IGNORED = 2         // Modules that don't count toward victory
};

// Module Types (matching CAN bus definitions)
enum class ModuleType : uint8_t
{
    // Regular Modules
    WIRES = 0x10,
    BUTTON = 0x11,
    KEYPAD = 0x12,
    SIMON_SAYS = 0x13,
    WHOS_ON_FIRST = 0x14,
    MEMORY = 0x15,
    MORSE_CODE = 0x16,
    COMPLICATED_WIRES = 0x17,
    WIRE_SEQUENCES = 0x18,
    MAZE = 0x19,
    PASSWORD = 0x1A,
    
    // Needy Modules
    VENTING_GAS = 0x30,
    CAPACITOR_DISCHARGE = 0x31,
    KNOB = 0x32,
    
    // Side Modules (Edgework)
    SERIAL_DISPLAY = 0x40,
    INDICATOR_PANEL = 0x41,
    BATTERY_HOLDER = 0x42,
    PORT_PANEL = 0x43,
    
    // Special Modules
    TIMER = 0x00,
    AUDIO = 0x01
};

// Indicator Types
enum class IndicatorType : uint8_t
{
    SND = 0,    // Sound
    CLR = 1,    // Color
    CAR = 2,    // Car
    IND = 3,    // Indicator
    FRQ = 4,    // Frequency
    SIG = 5,    // Signal
    NSA = 6,    // NSA
    MSA = 7,    // MSA
    TRN = 8,    // Train
    BOB = 9,    // Bob
    FRK = 10    // FRK
};

// Port Types
enum class PortType : uint8_t
{
    PARALLEL = 0,
    SERIAL_PORT = 1,
    PS2 = 2,
    RJ45 = 3,
    RCA = 4,
    DVI = 5,
    STEREO_RCA = 6
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Indicator
{
    IndicatorType type;
    bool lit;
    String label;
    
    Indicator(IndicatorType t, bool l, const String& lbl) 
        : type(t), lit(l), label(lbl) {}
};

struct Port
{
    PortType type;
    String label;
    
    Port(PortType t, const String& lbl) : type(t), label(lbl) {}
};

struct Edgework
{
    std::vector<Indicator> indicators;
    std::vector<Port> ports;
    uint8_t batteryCount = 0;
    
    // Helper methods
    bool hasIndicator(IndicatorType type) const;
    bool hasLitIndicator(IndicatorType type) const;
    bool hasUnlitIndicator(IndicatorType type) const;
    bool hasPort(PortType type) const;
    uint8_t getLitIndicatorCount() const;
    uint8_t getUnlitIndicatorCount() const;
    uint8_t getPortCount() const;
};

struct AudioModule
{
    uint16_t canId;
    bool connected = false;
    unsigned long lastSeen = 0;
    
    // ModuleState-like tracking
    uint8_t status = 0;  // ModuleStatus (MODULE_STATUS_IDLE, etc.)
    uint8_t progress = 0;
    bool solved = false;
    
    AudioModule(uint16_t id) : canId(id) {}
    
    // Helper methods
    void sendSound(uint8_t soundType);
    void setVolume(uint8_t volumePercent);
    void markSeen();
    void markDisconnected();
    void updateStatus(uint8_t moduleStatus, uint8_t moduleProgress, bool moduleSolved);
    bool isConnected() const { return connected; }
    unsigned long getTimeSinceLastSeen() const;
    uint8_t getStatus() const { return status; }
    uint8_t getProgress() const { return progress; }
    bool isSolved() const { return solved; }
};

struct SerialModule
{
    uint16_t canId;
    bool connected = false;
    unsigned long lastSeen = 0;
    String serialNumber = "";
    
    // ModuleState-like tracking
    uint8_t status = 0;  // ModuleStatus (MODULE_STATUS_IDLE, etc.)
    uint8_t progress = 0;
    bool solved = false;
    
    SerialModule(uint16_t id) : canId(id) {}
    
    // Helper methods
    void setSerialNumber(const String& serial);
    void sendSerialNumber();
    void clear();
    void showCredit();
    void markSeen();
    void markDisconnected();
    void updateStatus(uint8_t moduleStatus, uint8_t moduleProgress, bool moduleSolved);
    bool isConnected() const { return connected; }
    unsigned long getTimeSinceLastSeen() const;
    String getSerialNumber() const { return serialNumber; }
    uint8_t getStatus() const { return status; }
    uint8_t getProgress() const { return progress; }
    bool isSolved() const { return solved; }
};

struct Module
{
    uint16_t canId;
    ModuleType type;
    ModuleCategory category;
    ModuleStatus status = MODULE_STATUS_IDLE;
    uint8_t progress = 0;
    bool isSolved = false;
    bool isActive = false;  // Indicates module has been seen recently
    unsigned long lastSeen = 0;
    unsigned long lastStatusUpdate = 0;
    unsigned long activationTime = 0;
    unsigned long intervalMs = 0;  // For needy modules
    bool hasTelemetry = false;
    uint8_t telemetryType = 0;
    uint8_t telemetryData[4] = {0};
    uint8_t telemetryLen = 0;
    unsigned long lastTelemetryUpdate = 0;
    
    Module(uint16_t id, ModuleType t, ModuleCategory c) 
        : canId(id), type(t), category(c) {}
};

struct GameConfig
{
    unsigned long timeLimitMs = 300000;  // 5 minutes default
    uint8_t maxStrikes = 3;
    bool enableStrikeAcceleration = true;
    float strikeAccelerationFactor = 0.25f;  // 25% faster per strike
    bool enableEmergencyAlarm = true;
    unsigned long emergencyAlarmThreshold = 60000;  // 1 minute
    bool enableNeedyModules = true;
    bool enableEdgework = true;
};

// ============================================================================
// MAIN GAME STATE MANAGER
// ============================================================================

class GameStateManager
{
private:
    // Core State
    GameState currentState = GameState::IDLE;
    unsigned long stateChangeTime = 0;
    unsigned long gameStartTime = 0;
    
    // Timer System
    unsigned long timeLimitMs = 300000;
    unsigned long remainingMs = 300000;
    unsigned long lastUpdateTime = 0;
    bool timerRunning = false;
    
    // Strike System
    uint8_t strikeCount = 0;
    uint8_t maxStrikes = 3;
    
    // Module Management
    std::vector<Module> modules;
    std::map<uint16_t, Module*> moduleMap;
    
    // System Management (Audio, Serial Display)
    std::vector<AudioModule> audioModules;
    std::map<uint16_t, AudioModule*> audioModuleMap;
    std::vector<SerialModule> serialModules;
    std::map<uint16_t, SerialModule*> serialModuleMap;
    
    // Game Configuration
    GameConfig config;
    uint8_t audioVolume = 100;
    
    // Edgework
    Edgework edgework;
    
    // Serial Number
    String serialNumber = "";
    
    // Callbacks
    std::function<void(GameState, GameState)> onStateChange;
    std::function<void(uint8_t)> onStrikeChange;
    std::function<void(uint8_t, uint8_t)> onModuleSolved;
    std::function<void(unsigned long)> onTimeUpdate;
    
    // Internal Methods
    void updateTimer();
    void updateNeedyModules();
    void checkGameEndConditions();
    void handleModuleTimeout();
    ModuleCategory getModuleCategory(ModuleType type) const;
    bool isNeedyModule(ModuleType type) const;
    bool isIgnoredModule(ModuleType type) const;
    unsigned long getNeedyModuleInterval(ModuleType type);
    void ingestModuleStatus(uint16_t canId, const uint8_t* data, uint8_t len, bool sendDiscoveryAck);
    
public:
    // ========================================================================
    // CONSTRUCTOR & INITIALIZATION
    // ========================================================================
    GameStateManager();
    GameStateManager(const GameConfig& cfg);
    
    void initialize();
    void reset();
    
    // ========================================================================
    // CORE GAME LOOP
    // ========================================================================
    void tick();
    void update();
    
    // ========================================================================
    // STATE MANAGEMENT
    // ========================================================================
    void setState(GameState newState);
    GameState getState() const { return currentState; }
    bool is(GameState state) const { return currentState == state; }
    bool isRunning() const { return currentState == GameState::RUNNING; }
    bool isPaused() const { return currentState == GameState::PAUSED; }
    bool isGameOver() const { return currentState == GameState::EXPLODED || currentState == GameState::DEFUSED; }
    unsigned long getStateDuration() const { return millis() - stateChangeTime; }
    unsigned long getGameDuration() const { return millis() - gameStartTime; }
    
    // ========================================================================
    // TIMER MANAGEMENT
    // ========================================================================
    void setTimeLimit(unsigned long ms);
    void startTimer();
    void stopTimer();
    void pauseTimer();
    void resumeTimer();
    void resetTimer();
    void startGame();  // New method: broadcast to modules then start timer
    unsigned long getRemainingTime() const { return remainingMs; }
    unsigned long getElapsedTime() const { return timeLimitMs - remainingMs; }
    bool isTimerRunning() const { return timerRunning; }
    float getTimerProgress() const { return (float)(timeLimitMs - remainingMs) / timeLimitMs; }
    
    // ========================================================================
    // STRIKE MANAGEMENT
    // ========================================================================
    void setStrikes(uint8_t strikes);
    void addStrike();
    void clearStrikes();
    uint8_t getStrikes() const { return strikeCount; }
    uint8_t getMaxStrikes() const { return maxStrikes; }
    bool hasStrikes() const { return strikeCount > 0; }
    bool isOnLastStrike() const { return strikeCount >= maxStrikes; }
    
    // ========================================================================
    // MODULE MANAGEMENT
    // ========================================================================
    void registerModule(uint16_t canId, ModuleType type);
    void unregisterModule(uint16_t canId);
    void setModuleSolved(uint16_t canId);
    void setModuleActive(uint16_t canId, bool active);
    void updateModuleSeen(uint16_t canId);
    
    bool isModuleSolved(uint16_t canId) const;
    bool isModuleActive(uint16_t canId) const;
    Module* getModule(uint16_t canId);
    const Module* getModule(uint16_t canId) const;
    
    uint8_t getTotalModules() const;
    uint8_t getSolvedModules() const;
    uint8_t getActiveModules() const;
    uint8_t getNeedyModules() const;
    uint8_t getRegularModules() const;
    
    bool allModulesSolved() const;
    bool hasActiveNeedyModules() const;
    
    // Get all modules (for iteration)
    const std::vector<Module>& getAllModules() const { return modules; }
    
    // ========================================================================
    // AUDIO MODULE MANAGEMENT
    // ========================================================================
    void registerAudioModule(uint16_t canId);
    void unregisterAudioModule(uint16_t canId);
    void updateAudioModuleSeen(uint16_t canId);
    AudioModule* getAudioModule(uint16_t canId);
    const AudioModule* getAudioModule(uint16_t canId) const;
    AudioModule* getAudioModule();
    const AudioModule* getAudioModule() const;
    
    // Get all audio modules (for iteration)
    const std::vector<AudioModule>& getAllAudioModules() const { return audioModules; }
    void setAudioVolume(uint8_t volumePercent);
    uint8_t getAudioVolume() const { return audioVolume; }
    
    // ========================================================================
    // SERIAL MODULE MANAGEMENT
    // ========================================================================
    void registerSerialModule(uint16_t canId);
    void unregisterSerialModule(uint16_t canId);
    void updateSerialModuleSeen(uint16_t canId);
    SerialModule* getSerialModule(uint16_t canId);
    const SerialModule* getSerialModule(uint16_t canId) const;
    SerialModule* getSerialModule();
    const SerialModule* getSerialModule() const;
    void clearSerialDisplay();
    void showSerialCredit();
    
    // Get all serial modules (for iteration)
    const std::vector<SerialModule>& getAllSerialModules() const { return serialModules; }
    
    // ========================================================================
    // SERIAL NUMBER
    // ========================================================================
    void setSerialNumber(const String& serial);
    void generateSerialNumber();
    String getSerialNumber() const;
    
    // ========================================================================
    // MODULE CONNECTION TRACKING
    // ========================================================================
    void updateModuleConnections();  // Check for timeouts and disconnect modules
    bool isModuleConnected(uint16_t canId) const;
    unsigned long getModuleLastSeen(uint16_t canId) const;
    
    // ========================================================================
    // EDGEWORK MANAGEMENT
    // ========================================================================
    void setupEdgework();
    const Edgework& getEdgework() const { return edgework; }
    
    // Indicator helpers
    bool hasIndicator(IndicatorType type) const;
    bool hasLitIndicator(IndicatorType type) const;
    bool hasUnlitIndicator(IndicatorType type) const;
    uint8_t getLitIndicatorCount() const;
    uint8_t getUnlitIndicatorCount() const;
    
    // Port helpers
    bool hasPort(PortType type) const;
    uint8_t getPortCount() const;
    uint8_t getBatteryCount() const { return edgework.batteryCount; }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    void setConfig(const GameConfig& cfg);
    GameConfig getConfig() const { return config; }
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    void setStateChangeCallback(std::function<void(GameState, GameState)> callback);
    void setStrikeChangeCallback(std::function<void(uint8_t)> callback);
    void setModuleSolvedCallback(std::function<void(uint8_t, uint8_t)> callback);
    void setTimeUpdateCallback(std::function<void(unsigned long)> callback);
    
    // ========================================================================
    // DEBUG & UTILITY
    // ========================================================================
    // Debug functions removed
    
    // ========================================================================
    // CAN COMMUNICATION INTERFACE
    // ========================================================================
    void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
    void broadcastGameState(uint16_t targetId = CAN_ID_BROADCAST); // Default to broadcast ID
    
    // ========================================================================
    // GAME LOGIC HELPERS
    // ========================================================================
    bool shouldExplode() const;
    bool shouldDefuse() const;
    float getStrikeAcceleration() const;
    bool isEmergencyTime() const;
    unsigned long getTimeUntilExplosion() const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    struct GameStats
    {
        unsigned long totalGameTime = 0;
        unsigned long timeInState[6] = {0};  // Indexed by GameState enum
        uint8_t totalStrikes = 0;
        uint8_t modulesSolved = 0;
        uint8_t modulesFailed = 0;
        bool wasVictory = false;
    };
    
    GameStats getStats() const;
    void resetStats();
    
private:
    GameStats stats;
}; 