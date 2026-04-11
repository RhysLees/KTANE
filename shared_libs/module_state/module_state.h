#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <stdint.h>
#include <vector>

// Status LED pin (must be set by module)
// Use this value to disable LED
#define MODULE_STATE_NO_LED -1

// Default status LED pin (can be overridden before including this header)
#ifndef MODULE_STATE_DEFAULT_LED_PIN
#define MODULE_STATE_DEFAULT_LED_PIN MODULE_STATE_NO_LED
#endif

// Heartbeat timing constants
#define MODULE_STATE_HEARTBEAT_INTERVAL_DISCOVERY 3000   // 3 second for discovery
#define MODULE_STATE_HEARTBEAT_INTERVAL_GAME 1000        // 1 seconds during game
#define MODULE_STATE_DISCOVERY_LED_INTERVAL 500          // 500ms on/off for discovery
#define MODULE_STATE_STRIKE_FLASH_DURATION 1000         // 1000ms (1 second) flash on strike
#define MODULE_STATE_REGISTER_INTERVAL 1000              // 1 second between registration attempts

// Edgework data structures (matching game_state.h)
enum class IndicatorType : uint8_t {
    SND = 0, CLR = 1, CAR = 2, IND = 3, FRQ = 4,
    SIG = 5, NSA = 6, MSA = 7, TRN = 8, BOB = 9, FRK = 10
};

enum class PortType : uint8_t {
    PARALLEL = 0, SERIAL_PORT = 1, PS2 = 2, RJ45 = 3,
    RCA = 4, DVI = 5, STEREO_RCA = 6
};

struct Indicator {
    IndicatorType type;
    bool lit;
    String label;
    
    Indicator() : type(IndicatorType::SND), lit(false), label("") {}
    Indicator(IndicatorType t, bool l, const String& lbl) 
        : type(t), lit(l), label(lbl) {}
};

struct Port {
    PortType type;
    String label;
    
    Port() : type(PortType::PARALLEL), label("") {}
    Port(PortType t, const String& lbl) : type(t), label(lbl) {}
};

struct Edgework {
    std::vector<Indicator> indicators;
    std::vector<Port> ports;
    uint8_t batteryCount = 0;
    
    // Helper methods
    bool hasIndicator(IndicatorType type) const {
        for (const auto& ind : indicators) {
            if (ind.type == type) return true;
        }
        return false;
    }
    
    bool hasLitIndicator(IndicatorType type) const {
        for (const auto& ind : indicators) {
            if (ind.type == type && ind.lit) return true;
        }
        return false;
    }
    
    bool hasUnlitIndicator(IndicatorType type) const {
        for (const auto& ind : indicators) {
            if (ind.type == type && !ind.lit) return true;
        }
        return false;
    }
    
    bool hasPort(PortType type) const {
        for (const auto& port : ports) {
            if (port.type == type) return true;
        }
        return false;
    }
    
    uint8_t getLitIndicatorCount() const {
        uint8_t count = 0;
        for (const auto& ind : indicators) {
            if (ind.lit) count++;
        }
        return count;
    }
    
    uint8_t getUnlitIndicatorCount() const {
        uint8_t count = 0;
        for (const auto& ind : indicators) {
            if (!ind.lit) count++;
        }
        return count;
    }
    
    uint8_t getPortCount() const {
        return ports.size();
    }
    
    void clear() {
        indicators.clear();
        ports.clear();
        batteryCount = 0;
    }
};

// Callback types
typedef void (*GameStateCallback)(bool gameRunning);
typedef void (*StrikeCallback)(uint8_t strikeCount);
typedef void (*SerialNumberCallback)(const String& serial);
typedef void (*EdgeworkCallback)(const Edgework& edgework);
typedef void (*DiscoveredCallback)();  // Called when module is discovered

// Main class
class ModuleState {
private:
    static const uint8_t MAX_CAN_PAYLOAD = 6;

    // Discovery state
    bool isDiscovered;
    bool isRegistered;
    unsigned long lastRegisterAttempt;
    unsigned long lastDiscoveryFlashTime;
    
    // Heartbeat state
    unsigned long lastHeartbeat;
    bool gameRunning;
    ModuleStatus currentStatus;
    uint8_t progress;
    bool solved;
    uint16_t moduleId;
    bool enabled;
    bool communicationsEnabled;
    bool statusDirty;
    
    // Status LED
    int statusLedPin;
    bool discoveryLedState;
    bool manualLedState;
    bool manualLedOverride;
    
    // Strike handling
    uint8_t currentStrikes;
    bool strikeFlashActive;
    unsigned long strikeFlashStart;
    uint8_t lastStrikeCount;
    
    // Bomb state
    String serialNumber;
    char serialNumberFirstHalf[4];  // Buffer for first 3 chars of serial number
    bool hasSerialFirstHalf;        // Whether we've received the first half
    Edgework edgework;
    bool edgeworkReceived;
    
    // Callbacks
    GameStateCallback gameStateCallback;
    StrikeCallback strikeCallback;
    SerialNumberCallback serialNumberCallback;
    EdgeworkCallback edgeworkCallback;
    DiscoveredCallback discoveredCallback;
    
    // Internal methods
    void handleTimerMessage(uint8_t msgType, const uint8_t* data, uint8_t len);
    void updateDiscoveryLed();
    void updateStrikeLed();
    void sendRegister();
    void sendHeartbeat();
    unsigned long getCurrentHeartbeatInterval() const;
    void refreshModuleId();
    bool sendCanFrame(uint16_t receiverId, const uint8_t* data, uint8_t len);
    bool sendStatusUpdate(bool force = false);
    
public:
    ModuleState();
    
    // Initialization
    void begin(int statusLedPin = MODULE_STATE_DEFAULT_LED_PIN);
    void update();  // Call in loop()
    
    // CAN message handler (must be called from module's onCanMessage)
    void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
    
    // Status management
    void setStatus(ModuleStatus status);
    void setProgress(uint8_t progressPercent);
    void setSolved(bool solved = true);
    ModuleStatus getStatus() const { return currentStatus; }
    uint8_t getProgress() const { return progress; }
    bool isSolved() const { return solved; }
    
    // Game state
    bool isGameRunning() const { return gameRunning; }
    bool isDiscoveredByTimer() const { return isDiscovered; }
    
    // Bomb state accessors
    String getSerialNumber() const { return serialNumber; }
    const Edgework& getEdgework() const { return edgework; }
    uint8_t getStrikeCount() const { return currentStrikes; }
    uint8_t getBatteryCount() const { return edgework.batteryCount; }
    
    // Callbacks
    void setGameStateCallback(GameStateCallback cb) { gameStateCallback = cb; }
    void setStrikeCallback(StrikeCallback cb) { strikeCallback = cb; }
    void setSerialNumberCallback(SerialNumberCallback cb) { serialNumberCallback = cb; }
    void setEdgeworkCallback(EdgeworkCallback cb) { edgeworkCallback = cb; }
    void setDiscoveredCallback(DiscoveredCallback cb) { discoveredCallback = cb; }
    
    // Manual control
    void sendRegisterNow();
    void sendHeartbeatNow();
    void triggerStrike();  // For module-initiated strikes
    
    // Constants access
    static unsigned long getStrikeFlashDuration() { return MODULE_STATE_STRIKE_FLASH_DURATION; }
    
    // LED control
    void setLedPin(int pin);
    void setLedState(bool state);  // Manual override (for solved state, etc.)
    void clearLedOverride();  // Return to automatic control
    void disableLed();  // Disable status LED handling entirely
    
    // Communication control
    void setCommunicationEnabled(bool enabled);
    bool isCommunicationEnabled() const { return communicationsEnabled; }
    
    // Manual state overrides (for modules without CAN connectivity)
    void setDiscovered(bool discovered = true);
    void setGameRunning(bool running);
    void setStrikeCount(uint8_t strikeCount);
    void setSerialNumber(const String& serial);
    void setEdgework(const Edgework& edgeworkData);

    // CAN integration helpers
    uint16_t getModuleId() const;
    uint8_t getModuleInstanceId() const;
    bool sendMessage(uint16_t receiverId, const uint8_t* data, uint8_t len);
    bool sendTimerMessage(const uint8_t* data, uint8_t len);
    bool sendBroadcastMessage(const uint8_t* data, uint8_t len);
    bool playAudio(AudioMessage sound);
    bool playAudio(const uint8_t* soundCodes, uint8_t len);
    bool sendTelemetry(uint8_t telemetryType, const uint8_t* payload, uint8_t len);
};

// Global convenience functions (singleton pattern)
extern ModuleState* globalModuleState;

void initModuleState(int statusLedPin = MODULE_STATE_DEFAULT_LED_PIN);
void updateModuleState();
void setModuleStateStatus(ModuleStatus status);
void setModuleStateProgress(uint8_t progress);
void setModuleStateSolved(bool solved);
void moduleStateHandleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);

// Status LED control
void setModuleStateLedPin(int pin);
void setModuleStateLedState(bool state);
void clearModuleStateLedOverride();
void disableModuleStateLed();

// Communication and manual state control
void setModuleStateCommunicationEnabled(bool enabled);
void setModuleStateDiscovered(bool discovered = true);
void setModuleStateGameRunning(bool running);
void setModuleStateStrikeCount(uint8_t strikeCount);
void setModuleStateSerialNumber(const String& serial);
void setModuleStateEdgework(const Edgework& edgeworkData);

