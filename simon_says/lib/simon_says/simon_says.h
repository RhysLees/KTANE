#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <vector>

// ============================================================================
// SIMON SAYS CONSTANTS
// ============================================================================

// Hardware pin definitions
#define SIMON_LED_RED     2
#define SIMON_LED_YELLOW  3
#define SIMON_LED_GREEN   4
#define SIMON_LED_BLUE    5

#define SIMON_BTN_RED     6
#define SIMON_BTN_YELLOW  7
#define SIMON_BTN_GREEN   8
#define SIMON_BTN_BLUE    9

#define SIMON_STATUS_LED  11

// CAN bus configuration
#define SIMON_CAN_ID      CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00)

// Game configuration
#define SIMON_MAX_SEQUENCE_LENGTH 5
#define SIMON_DISPLAY_TIME_MS     800
#define SIMON_PAUSE_TIME_MS       200
#define SIMON_INPUT_TIMEOUT_MS    5000
#define SIMON_STRIKE_FLASH_MS     1000

// CAN message types
enum SimonCanMessage : uint8_t {
    SIMON_MSG_REGISTER    = 0x01,  // Register module with game state
    SIMON_MSG_SOLVED      = 0x02,  // Module solved successfully
    SIMON_MSG_STRIKE      = 0x03,  // Strike occurred
    SIMON_MSG_HEARTBEAT   = 0x04,  // Realtime heartbeat
    SIMON_MSG_RESET       = 0x05,  // Reset module
    SIMON_MSG_STATUS      = 0x06   // Realtime status update
};

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
    
    // Sequence management
    std::vector<SimonColor> sequence;
    std::vector<SimonColor> playerInput;
    uint8_t currentSequenceLength;
    uint8_t displayIndex;
    uint8_t inputIndex;
    
    // Timing
    unsigned long lastUpdateTime;
    unsigned long stateStartTime;
    
    // Hardware state
    bool ledStates[4];
    bool buttonStates[4];
    bool lastButtonStates[4];
    
    // Strike counting
    uint8_t strikeCount;
    bool isFlashing;
    unsigned long flashStartTime;
    
    // KTANE rule variables
    bool hasVowelInSerial;
    uint8_t numStrikes;
    
    // Hardware methods
    void initHardware();
    void updateButtons();
    void updateLEDs();
    void playAudioForColor(SimonColor color);
    void playStrikeSound();
    void playSolvedSound();
    void flashAllLEDs(unsigned long duration);
    void setLED(SimonColor color, bool state);
    
    // Game logic methods
    void generateSequence();
    void displaySequence();
    void processInput();
    void checkInput();
    void nextStage();
    void handleStrike();
    void solvePuzzle();
    void resetModule();
    
    // KTANE rule methods
    bool shouldFlashColor(SimonColor color);
    SimonColor getFlashColor(SimonColor color);
    
    // Utility methods
    uint8_t getColorPin(SimonColor color, bool isLED);
    const char* getColorName(SimonColor color);
    const char* getStateName(SimonState state);
    
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
    void setStrikeCount(uint8_t strikes);
    void setSerialNumber(const String& serial);
    
    // Status interface
    bool isSolved() const { return isModuleSolved; }
    SimonState getState() const { return currentState; }
    uint8_t getSequenceLength() const { return currentSequenceLength; }
    uint8_t getStrikeCount() const { return strikeCount; }
    
    // Debug interface
    void printStatus() const;
    void printSequence() const;
    void printRules() const;
    
    // CAN message handler
    void handleCanMessage(uint16_t id, const uint8_t* data, uint8_t len);
}; 