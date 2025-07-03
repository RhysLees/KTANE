#include "game_state_v2.h"
#include <can_bus.h>
#include <lcd1602.h>
#include <countdown.h>
#include <strikes.h>

// Global game state manager
GameStateManager gameState;

// ============================================================================
// CALLBACK FUNCTIONS
// ============================================================================

void onStateChange(GameState oldState, GameState newState) {
    Serial.print("Game state changed: ");
    Serial.print(static_cast<int>(oldState));
    Serial.print(" -> ");
    Serial.println(static_cast<int>(newState));
    
    switch (newState) {
        case GameState::RUNNING:
            // Start timer display
            lcd1602SetColor(LCD_COLOR_GREEN);
            lcd1602PrintLine(0, "GAME RUNNING");
            lcd1602PrintLine(1, "GOOD LUCK!");
            break;
            
        case GameState::EXPLODED:
            // Show explosion
            lcd1602SetColor(LCD_COLOR_RED);
            lcd1602PrintLine(0, "BOOM!");
            lcd1602PrintLine(1, "GAME OVER");
            
            // Send explosion sound
            uint8_t explosionSound = AUDIO_EXPLODED;
            sendCanMessage(CAN_ID_AUDIO, &explosionSound, 1);
            break;
            
        case GameState::DEFUSED:
            // Show victory
            lcd1602SetColor(LCD_COLOR_GREEN);
            lcd1602PrintLine(0, "BOMB DEFUSED!");
            lcd1602PrintLine(1, "VICTORY!");
            
            // Send victory sound
            uint8_t victorySound = AUDIO_DEFUSED;
            sendCanMessage(CAN_ID_AUDIO, &victorySound, 1);
            break;
            
        case GameState::PAUSED:
            lcd1602SetColor(LCD_COLOR_ORANGE);
            lcd1602PrintLine(0, "GAME PAUSED");
            lcd1602PrintLine(1, "Press to resume");
            break;
            
        default:
            break;
    }
}

void onStrikeChange(uint8_t strikes) {
    Serial.print("Strikes: ");
    Serial.println(strikes);
    
    // Update strike display (handled by existing strike system)
    // The existing updateStrikeCount() function will handle this
    
    // Send strike sound
    if (strikes > 0) {
        uint8_t strikeSound = AUDIO_STRIKE;
        sendCanMessage(CAN_ID_AUDIO, &strikeSound, 1);
    }
}

void onModuleSolved(uint8_t solved, uint8_t total) {
    Serial.print("Module solved! ");
    Serial.print(solved);
    Serial.print("/");
    Serial.println(total);
    
    // Update LCD with module progress
    lcd1602PrintLine(1, "Mods: " + String(solved) + "/" + String(total));
    
    // Send correct sound
    uint8_t correctSound = AUDIO_CORRECT_TIME;
    sendCanMessage(CAN_ID_AUDIO, &correctSound, 1);
}

void onTimeUpdate(unsigned long remainingMs) {
    // Update countdown display (handled by existing countdown system)
    // The existing updateCountdownDisplay() function will handle this
    
    // Check for emergency time
    if (gameState.isEmergencyTime()) {
        // Emergency alarm is handled by existing countdown system
    }
}

// ============================================================================
// CAN MESSAGE HANDLING
// ============================================================================

void handleCanMessages() {
    // Handle incoming CAN messages for module registration and solving
    // This would integrate with your existing CAN message handling
    
    // Example: Module registration message
    // if (canId >= MODULE_ID_START && canId <= MODULE_ID_END) {
    //     gameState.registerModule(canId, static_cast<ModuleType>(moduleType));
    // }
    
    // Example: Module solved message
    // if (messageType == MODULE_SOLVED) {
    //     gameState.setModuleSolved(canId);
    // }
}

// ============================================================================
// SERIAL COMMAND HANDLING
// ============================================================================

void handleSerialCommands() {
    if (!Serial.available()) return;
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    
    if (input == "START") {
        gameState.setState(GameState::RUNNING);
        gameState.startTimer();
        Serial.println("Game started!");
    }
    else if (input == "STOP") {
        gameState.pauseTimer();
        Serial.println("Game paused!");
    }
    else if (input == "RESET") {
        gameState.reset();
        Serial.println("Game reset!");
    }
    else if (input.startsWith("TIME ")) {
        String timeStr = input.substring(5);
        int colonIndex = timeStr.indexOf(':');
        if (colonIndex != -1) {
            int mins = timeStr.substring(0, colonIndex).toInt();
            int secs = timeStr.substring(colonIndex + 1).toInt();
            unsigned long ms = (mins * 60UL + secs) * 1000UL;
            gameState.setTimeLimit(ms);
            Serial.print("Time set to ");
            Serial.println(timeStr);
        }
    }
    else if (input.startsWith("STRIKE ")) {
        String strikeStr = input.substring(7);
        int strikes = strikeStr.toInt();
        gameState.setStrikes(strikes);
        Serial.print("Strikes set to ");
        Serial.println(strikes);
    }
    else if (input.startsWith("MODULE ")) {
        // Format: MODULE <id> <type>
        // Example: MODULE 16 10 (register Wires module with ID 0x10)
        String args = input.substring(7);
        int spaceIndex = args.indexOf(' ');
        if (spaceIndex != -1) {
            uint16_t canId = args.substring(0, spaceIndex).toInt();
            uint8_t moduleType = args.substring(spaceIndex + 1).toInt();
            gameState.registerModule(canId, static_cast<ModuleType>(moduleType));
            Serial.print("Module registered: ID=0x");
            Serial.print(canId, HEX);
            Serial.print(" Type=0x");
            Serial.println(moduleType, HEX);
        }
    }
    else if (input.startsWith("SOLVE ")) {
        String idStr = input.substring(6);
        uint16_t canId = idStr.toInt();
        gameState.setModuleSolved(canId);
        Serial.print("Module solved: ID=0x");
        Serial.println(canId, HEX);
    }
    else if (input == "STATUS") {
        gameState.printStatus();
    }
    else if (input == "MODULES") {
        gameState.printModules();
    }
    else if (input == "EDGEWORK") {
        gameState.printEdgework();
    }
    else if (input == "HELP") {
        Serial.println("Available commands:");
        Serial.println("  START         - Start the game");
        Serial.println("  STOP          - Pause the game");
        Serial.println("  RESET         - Reset the game");
        Serial.println("  TIME mm:ss    - Set time limit");
        Serial.println("  STRIKE x      - Set strike count");
        Serial.println("  MODULE id type - Register a module");
        Serial.println("  SOLVE id      - Mark module as solved");
        Serial.println("  STATUS        - Show game status");
        Serial.println("  MODULES       - Show module list");
        Serial.println("  EDGEWORK      - Show edgework info");
        Serial.println("  HELP          - Show this help");
    }
}

// ============================================================================
// SETUP AND LOOP
// ============================================================================

void setup() {
    Serial.begin(115200);
    
    // Initialize hardware
    initCanBus(CAN_ID_TIMER);
    initLcd1602(16, 2, Wire1);
    initStrikeDisplay();
    initCountdownDisplay();
    
    // Configure game state
    GameConfig config;
    config.timeLimitMs = 300000;  // 5 minutes
    config.maxStrikes = 3;
    config.enableStrikeAcceleration = true;
    config.strikeAccelerationFactor = 0.25f;
    config.enableEmergencyAlarm = true;
    config.emergencyAlarmThreshold = 60000;
    config.enableNeedyModules = true;
    config.enableEdgework = true;
    
    gameState.setConfig(config);
    
    // Set up callbacks
    gameState.setStateChangeCallback(onStateChange);
    gameState.setStrikeChangeCallback(onStrikeChange);
    gameState.setModuleSolvedCallback(onModuleSolved);
    gameState.setTimeUpdateCallback(onTimeUpdate);
    
    // Initialize game state
    gameState.initialize();
    
    // Show startup message
    lcd1602SetColor(LCD_COLOR_BLUE);
    lcd1602PrintLine(0, "KTANE v2.0");
    lcd1602PrintLine(1, "Ready!");
    
    Serial.println("KTANE Game State v2.0 initialized");
    Serial.print("Serial Number: ");
    Serial.println(gameState.getSerialNumber());
    Serial.println("Type HELP for commands");
}

void loop() {
    // Update game state
    gameState.tick();
    
    // Handle game state
    switch (gameState.getState()) {
        case GameState::RUNNING:
            // Update displays (existing functions)
            updateCountdownDisplay();
            updateStrikeCount();
            break;
            
        case GameState::IDLE:
            // Show idle status
            lcd1602PrintLine(0, "Waiting to start");
            lcd1602PrintLine(1, "Type START");
            break;
            
        case GameState::PAUSED:
            // Paused state handled by callback
            break;
            
        case GameState::EXPLODED:
        case GameState::DEFUSED:
            // End states handled by callbacks
            break;
            
        default:
            break;
    }
    
    // Handle input
    handleSerialCommands();
    handleCanMessages();
    
    // Small delay to prevent overwhelming the system
    delay(10);
}

// ============================================================================
// INTEGRATION WITH EXISTING SYSTEMS
// ============================================================================

// Modify existing countdown display to use new game state
void updateCountdownDisplay() {
    if (!gameState.isRunning()) {
        updateCountdownRaw("----");
        return;
    }
    
    unsigned long remainingMs = gameState.getRemainingTime();
    unsigned long seconds = remainingMs / 1000;
    int mins = seconds / 60;
    int secs = seconds % 60;
    int timeValue = mins * 100 + secs;
    
    // Update 7-segment display
    display.print(timeValue);
    
    // Handle colon blinking based on strikes
    unsigned long now = millis();
    unsigned long blinkRate = (gameState.getStrikes() >= 2) ? 125 : 500;
    if (now - lastColonToggle >= blinkRate) {
        colonVisible = !colonVisible;
        lastColonToggle = now;
    }
    
    display.drawColon(colonVisible);
    display.writeDisplay();
    
    // Handle time beeps
    if (seconds != lastSecondSent) {
        lastSecondSent = seconds;
        uint8_t sound = (gameState.getStrikes() == 2) ? AUDIO_BEEP_HIGH
                       : (gameState.getStrikes() == 1) ? AUDIO_BEEP_FAST
                       : AUDIO_BEEP_NORMAL;
        sendCanMessage(CAN_ID_AUDIO, &sound, 1);
    }
    
    // Handle emergency alarm
    if (gameState.isEmergencyTime()) {
        static unsigned long lastEmergencyAlarmSent = 0;
        if (now - lastEmergencyAlarmSent >= 3000) {
            lastEmergencyAlarmSent = now;
            uint8_t emergencySound = AUDIO_ALARM_EMERGENCY;
            sendCanMessage(CAN_ID_AUDIO, &emergencySound, 1);
        }
    }
}

// Modify existing strike display to use new game state
void updateStrikeCount() {
    uint8_t strikes = gameState.getStrikes();
    static uint8_t lastStrikes = 255;
    static char display[3] = "  ";
    
    if (strikes != lastStrikes) {
        lastStrikes = strikes;
        // Strike sound is handled by callback
    }
    
    if (strikes == 0) {
        display[0] = ' ';
        display[1] = ' ';
    }
    else if (strikes == 1) {
        display[0] = 'X';
        display[1] = ' ';
    }
    else {
        display[0] = 'X';
        display[1] = 'X';
    }
    
    if (strikes < 2) {
        updateAlphaDisplay(display);
        return;
    }
    
    // Blink when 2+ strikes
    unsigned long now = millis();
    if (now - lastStrikeBlink >= 125) {
        lastStrikeBlink = now;
        strikeVisible = !strikeVisible;
    }
    
    updateAlphaDisplay(strikeVisible ? display : "  ");
} 