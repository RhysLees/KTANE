#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <vector>

// ============================================================================
// SIMON SAYS CONSTANTS
// ============================================================================

// Hardware pin definitions
#define SIMON_LED_RED     6
#define SIMON_LED_YELLOW  9
#define SIMON_LED_GREEN   7
#define SIMON_LED_BLUE    8

#define SIMON_BTN_RED     2
#define SIMON_BTN_YELLOW  5
#define SIMON_BTN_GREEN   3
#define SIMON_BTN_BLUE    4

#define SIMON_STATUS_LED  11

// CAN bus configuration
#define SIMON_CAN_ID      CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00)

// Game configuration
#define SIMON_MAX_SEQUENCE_LENGTH 5
#define SIMON_DISPLAY_TIME_MS     500
#define SIMON_PAUSE_TIME_MS       250
#define SIMON_INPUT_TIMEOUT_MS    5000
// Note: Strike flash duration is now from module_state (1000ms)
// Use MODULE_STATE_STRIKE_FLASH_DURATION or ModuleState::getStrikeFlashDuration()

// Note: CAN message types are defined in can_bus.h (MODULE_REGISTER, MODULE_SOLVED, etc.)

// ============================================================================
// SIMON SAYS ENUMS
// ============================================================================

enum class SimonColor : uint8_t {
    RED = 0,
    YELLOW = 1,
    GREEN = 2,
    BLUE = 3,
    NONE = 255
};

enum class SimonState : uint8_t {
    IDLE = 0,           // Waiting for game to start
    GENERATING,         // Generating sequence
    DISPLAYING,         // Showing sequence to player
    WAITING_INPUT,      // Waiting for player input
    CHECKING_INPUT,     // Validating player input
    CORRECT_SEQUENCE,   // Player entered correct sequence
    WRONG_INPUT,        // Player made an error
    SOLVED,             // Module completely solved
    STRIKE              // Strike occurred
};

// ============================================================================
// SIMON SAYS CLASS
// ============================================================================

class SimonSays {
private:
    // Game state
    SimonState currentState;
    bool isModuleSolved;
    bool gameStarted;
    bool initializationComplete;
    
    // Sequence management
    std::vector<SimonColor> sequence;
    std::vector<SimonColor> playerInput;
    uint8_t currentSequenceLength;
    uint8_t targetSequenceLength;  // Total stages to complete (3-5)
    uint8_t displayIndex;
    uint8_t inputIndex;
    
    // Timing
    unsigned long lastUpdateTime;
    unsigned long stateStartTime;
    unsigned long lastInputTime;  // For timeout tracking
    
    // Audio control
    bool audioPlayedForCurrentColor;
    
    // Hardware state
    bool ledStates[4];
    bool buttonStates[4];
    bool lastButtonStates[4];
    
    // KTANE rule variables
    bool hasVowelInSerial;
    
    // Hardware methods
    void initHardware();
    void updateButtons();
    void updateLEDs();
    void playAudioForColor(SimonColor color);
    void setLED(SimonColor color, bool state);
    
    // Game logic methods
    void generateSequence();
    void displaySequence();
    void checkInput();
    void nextStage();
    void handleStrike();
    void solvePuzzle();
    void resetModule();
    
    // KTANE rule methods
    bool shouldFlashColor(SimonColor color) const;
    SimonColor getFlashColor(SimonColor color) const;
    
    // Utility methods
    uint8_t getColorPin(SimonColor color, bool isLED) const;
    const char* getColorName(SimonColor color) const;
    const char* getStateName(SimonState state) const;
    
public:
    // Constructor
    SimonSays();
    
    // Main interface
    void begin();
    void update();
    void reset();
    
    // Game state interface
    void startGame();
    void stopGame();
    void onGameStateChange(bool gameRunning);
    void setSerialNumber(const String& serial);
    void setInitializationComplete(bool complete);
    
    // Status interface
    bool isSolved() const { return isModuleSolved; }
    SimonState getState() const { return currentState; }
    uint8_t getSequenceLength() const { return currentSequenceLength; }
    
    // Debug interface removed
    
    // Note: CAN message handling is now done by module_state
    // Module-specific messages can be handled in main.cpp's onCanMessage
    // This method is kept for potential future module-specific messages
    void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
}; 