#include "simon_says.h"
#include <module_state.h>

// Use module_state's strike flash duration
#define SIMON_STRIKE_FLASH_MS MODULE_STATE_STRIKE_FLASH_DURATION

// ============================================================================
// CONSTRUCTOR
// ============================================================================

SimonSays::SimonSays() {
    currentState = SimonState::IDLE;
    isModuleSolved = false;
    gameStarted = false;
    initializationComplete = false;
    
    currentSequenceLength = 0;
    targetSequenceLength = 0;
    displayIndex = 0;
    inputIndex = 0;
    
    lastUpdateTime = 0;
    stateStartTime = 0;
    
    audioPlayedForCurrentColor = false;
    
    hasVowelInSerial = false;
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
        buttonStates[i] = false;
        lastButtonStates[i] = false;
    }
    
    sequence.clear();
    playerInput.clear();
}

// ============================================================================
// MAIN INTERFACE
// ============================================================================

void SimonSays::begin() {
    Serial.println("Simon Says: Initializing...");
    
    initHardware();
    
    // Module registration will be handled by explicit registration
    // No need to send manual registration message
    
    currentState = SimonState::IDLE;
    stateStartTime = millis();
    
    // Welcome flash - flash all LEDs briefly
    for (int i = 0; i < 4; i++) {
        ledStates[i] = true;
    }
    delay(200);
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    Serial.println("Simon Says: Ready!");
}

void SimonSays::update() {
    unsigned long currentTime = millis();
    lastUpdateTime = currentTime;
    
    updateButtons();
    updateLEDs();
    
    // Status updates are now handled by module_state library in main.cpp
    // Module_state will automatically send heartbeats with status/progress
    
    switch (currentState) {
        case SimonState::IDLE:
            break;
            
        case SimonState::GENERATING:
            generateSequence();
            break;
            
        case SimonState::DISPLAYING:
            // Check for button presses during display (can interrupt sequence)
            for (int i = 0; i < 4; i++) {
                if (buttonStates[i] && !lastButtonStates[i]) {
                    // Button pressed during display - interrupt and start input
                    Serial.println("Simon Says: Button pressed during display - interrupting sequence");
                    
                    // Turn off all LEDs
                    for (int j = 0; j < 4; j++) {
                        ledStates[j] = false;
                    }
                    
                    // Start collecting input
                    playerInput.clear();
                    inputIndex = 0;
                    SimonColor pressedColor = static_cast<SimonColor>(i);
                    playerInput.push_back(pressedColor);
                    
                    Serial.print("Simon Says: First input - ");
                    Serial.println(getColorName(pressedColor));
                    
                    setLED(pressedColor, true);
                    playAudioForColor(pressedColor);
                    
                    currentState = SimonState::CHECKING_INPUT;
                    stateStartTime = millis();
                    break;
                }
            }
            
            // Only continue display if no button was pressed
            if (currentState == SimonState::DISPLAYING) {
                displaySequence();
            }
            break;
            
        case SimonState::WAITING_INPUT:
            processInput();
            // Timeout after 5 seconds - replay sequence (no strike)
            if (currentTime - stateStartTime > SIMON_INPUT_TIMEOUT_MS) {
                Serial.println("Simon Says: Input timeout - replaying sequence");
                
                // Show what the player should have pressed
                Serial.print("Simon Says: Reminder - you should press: ");
                for (size_t i = 0; i < sequence.size(); i++) {
                    if (i > 0) Serial.print(" -> ");
                    SimonColor expectedColor = getFlashColor(sequence[i]);
                    Serial.print(getColorName(expectedColor));
                }
                Serial.println();
                
                displayIndex = 0;
                currentState = SimonState::DISPLAYING;
                stateStartTime = millis();
                audioPlayedForCurrentColor = false;
                
                // Clear any partial input
                playerInput.clear();
                inputIndex = 0;
                
                // Turn off all LEDs
                for (int i = 0; i < 4; i++) {
                    ledStates[i] = false;
                }
            }
            break;
            
        case SimonState::CHECKING_INPUT:
            checkInput();
            break;
            
        case SimonState::CORRECT_SEQUENCE:
            nextStage();
            break;
            
        case SimonState::WRONG_INPUT:
            // Wait 5 seconds before replaying sequence after wrong input
            if (currentTime - stateStartTime > 5000) {
                handleStrike();
            }
            break;
            
        case SimonState::SOLVED:
            break;
            
        case SimonState::STRIKE:
            // Use module_state's strike flash duration
            if (currentTime - stateStartTime > MODULE_STATE_STRIKE_FLASH_DURATION) {
                // Turn off flashing LEDs
                for (int i = 0; i < 4; i++) {
                    ledStates[i] = false;
                }
                resetModule();
            }
            break;
    }
}

void SimonSays::reset() {
    Serial.println("Simon Says: Resetting module...");
    
    currentState = SimonState::IDLE;
    isModuleSolved = false;
    gameStarted = false;
    
    currentSequenceLength = 0;
    targetSequenceLength = 0;
    displayIndex = 0;
    inputIndex = 0;
    
    sequence.clear();
    playerInput.clear();
    
    audioPlayedForCurrentColor = false;
    
    // Discovery state is now handled by module_state
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    // Status LED is now handled by module_state library
    // Note: Game LED flashing is now handled via strike callback
    
    // Note: Status update will be sent automatically by update() method
    // when it detects the state change
    
    Serial.println("Simon Says: Reset complete.");
}

// ============================================================================
// GAME STATE INTERFACE
// ============================================================================

void SimonSays::startGame() {
    if (currentState == SimonState::IDLE) {
        if (initializationComplete) {
            Serial.println("Simon Says: Game starting...");
            gameStarted = true;
            currentState = SimonState::GENERATING;
            stateStartTime = millis();
            
            // Status LED is now handled by module_state library
        } else {
            Serial.println("Simon Says: Cannot start game - initialization not complete");
        }
    }
}

void SimonSays::stopGame() {
    Serial.println("Simon Says: Game stopped.");
    gameStarted = false;
    reset();
}

void SimonSays::onGameStateChange(bool gameRunning) {
    if (gameRunning) {
        startGame();
    } else {
        stopGame();
    }
}

// Note: Strike count is now managed by module_state library
// Use globalModuleState->getStrikeCount() to get current strike count

void SimonSays::setSerialNumber(const String& serial) {
    hasVowelInSerial = false;
    for (int i = 0; i < serial.length(); i++) {
        char c = serial.charAt(i);
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
            hasVowelInSerial = true;
            break;
        }
    }
    
    Serial.print("Simon Says: Serial number ");
    Serial.print(serial);
    Serial.print(" has vowel: ");
    Serial.println(hasVowelInSerial ? "YES" : "NO");
}

void SimonSays::setInitializationComplete(bool complete) {
    initializationComplete = complete;
    Serial.print("Simon Says: Initialization complete: ");
    Serial.println(complete ? "YES" : "NO");
}

// Note: Discovery state is now handled by module_state
// No need for setDiscoveredByTimer() anymore

// ============================================================================
// HARDWARE METHODS
// ============================================================================

void SimonSays::initHardware() {
    pinMode(SIMON_LED_RED, OUTPUT);
    pinMode(SIMON_LED_YELLOW, OUTPUT);
    pinMode(SIMON_LED_GREEN, OUTPUT);
    pinMode(SIMON_LED_BLUE, OUTPUT);
    // Status LED is now handled by module_state library
    
    pinMode(SIMON_BTN_RED, INPUT_PULLUP);
    pinMode(SIMON_BTN_YELLOW, INPUT_PULLUP);
    pinMode(SIMON_BTN_GREEN, INPUT_PULLUP);
    pinMode(SIMON_BTN_BLUE, INPUT_PULLUP);
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
}

void SimonSays::updateButtons() {
    // Save previous button states BEFORE updating current states
    for (int i = 0; i < 4; i++) {
        lastButtonStates[i] = buttonStates[i];
    }
    
    // Read current button states
    buttonStates[0] = !digitalRead(SIMON_BTN_RED);
    buttonStates[1] = !digitalRead(SIMON_BTN_YELLOW);
    buttonStates[2] = !digitalRead(SIMON_BTN_GREEN);
    buttonStates[3] = !digitalRead(SIMON_BTN_BLUE);
}

void SimonSays::updateLEDs() {
    // Status LED is now handled by module_state library
    // No need to control it here
    
    // Update game LEDs (no flash state needed - handled by strike state)
    digitalWrite(SIMON_LED_RED, ledStates[0]);
    digitalWrite(SIMON_LED_YELLOW, ledStates[1]);
    digitalWrite(SIMON_LED_GREEN, ledStates[2]);
    digitalWrite(SIMON_LED_BLUE, ledStates[3]);
}

void SimonSays::playAudioForColor(SimonColor color) {
    // Send audio message to audio module directly via CAN bus library
    uint8_t audioType;
    switch (color) {
        case SimonColor::RED:
            audioType = AUDIO_SIMON_RED;
            break;
        case SimonColor::YELLOW:
            audioType = AUDIO_SIMON_YELLOW;
            break;
        case SimonColor::GREEN:
            audioType = AUDIO_SIMON_GREEN;
            break;
        case SimonColor::BLUE:
            audioType = AUDIO_SIMON_BLUE;
            break;
    }
    
    uint8_t audioData[1];
    audioData[0] = audioType;
    sendCanMessage(CAN_ID_AUDIO, audioData, 1);
}

// Note: Audio is now handled by module_state library
// playStrikeSound() and playSolvedSound() are no longer needed

// Note: Game LED flashing is now handled in handleStrike() via module_state callback

void SimonSays::setLED(SimonColor color, bool state) {
    if (color != SimonColor::NONE) {
        ledStates[static_cast<uint8_t>(color)] = state;
    }
}

// ============================================================================
// GAME LOGIC METHODS
// ============================================================================

void SimonSays::generateSequence() {
    Serial.println("Simon Says: Generating sequence...");
    
    if (currentSequenceLength == 0) {
        // First time - generate random target sequence length (3-5 stages)
        targetSequenceLength = random(3, 6); // 3 to 5 inclusive
        currentSequenceLength = 1;
        sequence.clear();
        
        // Start with just one color
        SimonColor color = static_cast<SimonColor>(random(4));
        sequence.push_back(color);
        
        Serial.print("Simon Says: Target sequence length: ");
        Serial.println(targetSequenceLength);
    } else {
        // Add one more color to the sequence
        SimonColor color = static_cast<SimonColor>(random(4));
        sequence.push_back(color);
        currentSequenceLength++;
    }
    
    // Debug: Show current sequence
    Serial.print("Simon Says: Current sequence: ");
    for (size_t i = 0; i < sequence.size(); i++) {
        if (i > 0) Serial.print(" -> ");
        Serial.print(getColorName(sequence[i]));
    }
    Serial.println();
    
    displayIndex = 0;
    currentState = SimonState::DISPLAYING;
    stateStartTime = millis();
    
    // Reset audio flag for new sequence display
    audioPlayedForCurrentColor = false;
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
}

void SimonSays::displaySequence() {
    unsigned long elapsed = millis() - stateStartTime;
    
    if (displayIndex < sequence.size()) {
        unsigned long colorStartTime = displayIndex * (SIMON_DISPLAY_TIME_MS + SIMON_PAUSE_TIME_MS);
        unsigned long colorEndTime = colorStartTime + SIMON_DISPLAY_TIME_MS;
        
        if (elapsed >= colorStartTime && elapsed < colorEndTime) {
            // Turn on LED for 500ms and play audio once
            // Show the actual sequence color (not the mapped color)
            SimonColor displayColor = sequence[displayIndex];
            
            setLED(displayColor, true);
            
            // Only play audio once per color display
            if (!audioPlayedForCurrentColor) {
                playAudioForColor(displayColor);
                audioPlayedForCurrentColor = true;
            }
            
        } else if (elapsed >= colorEndTime) {
            // Turn off the LED after 500ms
            SimonColor displayColor = sequence[displayIndex];
            setLED(displayColor, false);
            
            if (elapsed >= colorStartTime + SIMON_DISPLAY_TIME_MS + SIMON_PAUSE_TIME_MS) {
                displayIndex++;
                audioPlayedForCurrentColor = false; // Reset for next color
            }
        }
    } else {
        // Sequence complete - ensure all LEDs are off and transition to input mode
        for (int i = 0; i < 4; i++) {
            ledStates[i] = false;
        }
        
        playerInput.clear();
        inputIndex = 0;
        currentState = SimonState::WAITING_INPUT;
        stateStartTime = millis();
        
        // Show what the player should input
        Serial.println("Simon Says: Sequence complete - waiting for input...");
        Serial.print("Simon Says: Please press: ");
        for (size_t i = 0; i < sequence.size(); i++) {
            if (i > 0) Serial.print(" -> ");
            SimonColor expectedColor = getFlashColor(sequence[i]);
            Serial.print(getColorName(expectedColor));
        }
        Serial.println();
    }
}

void SimonSays::processInput() {
    // Check for button presses
    for (int i = 0; i < 4; i++) {
        if (buttonStates[i] && !lastButtonStates[i]) {
            // Button pressed - add to input
            SimonColor pressedColor = static_cast<SimonColor>(i);
            playerInput.push_back(pressedColor);
            
            Serial.print("Simon Says: Button pressed - ");
            Serial.print(getColorName(pressedColor));
            Serial.print(" (input ");
            Serial.print(playerInput.size());
            Serial.print("/");
            Serial.print(sequence.size());
            Serial.println(")");
            
            setLED(pressedColor, true);
            playAudioForColor(pressedColor);
            
            // Transition to checking state
            currentState = SimonState::CHECKING_INPUT;
            stateStartTime = millis();
            
            break;
        }
    }
    
    // Turn off LEDs when buttons are released
    for (int i = 0; i < 4; i++) {
        if (!buttonStates[i] && lastButtonStates[i]) {
            setLED(static_cast<SimonColor>(i), false);
        }
    }
}

void SimonSays::checkInput() {
    if (millis() - stateStartTime < 200) {
        return;
    }
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    if (playerInput.size() > 0) {
        // Player should press the mapped color, not the sequence color
        SimonColor expectedColor = getFlashColor(sequence[inputIndex]);
        
        if (playerInput[inputIndex] == expectedColor) {
            Serial.print("Simon Says: Correct! Pressed ");
            Serial.print(getColorName(playerInput[inputIndex]));
            Serial.print(" (expected ");
            Serial.print(getColorName(expectedColor));
            Serial.print(") - Progress: ");
            Serial.print(inputIndex + 1);
            Serial.print("/");
            Serial.println(sequence.size());
            
            inputIndex++;
            
            if (inputIndex >= sequence.size()) {
                Serial.println("Simon Says: Sequence completed correctly!");
                currentState = SimonState::CORRECT_SEQUENCE;
                stateStartTime = millis();
            } else {
                Serial.println("Simon Says: Waiting for next input (timeout reset)...");
                currentState = SimonState::WAITING_INPUT;
                stateStartTime = millis(); // Reset timeout for next input
            }
        } else {
            Serial.print("Simon Says: WRONG! Pressed ");
            Serial.print(getColorName(playerInput[inputIndex]));
            Serial.print(" but expected ");
            Serial.println(getColorName(expectedColor));
            
            currentState = SimonState::WRONG_INPUT;
            stateStartTime = millis();
        }
    }
}

void SimonSays::nextStage() {
    if (currentSequenceLength >= targetSequenceLength) {
        solvePuzzle();
    } else {
        generateSequence();
    }
}

void SimonSays::handleStrike() {
    Serial.println("Simon Says: Strike! Flashing LEDs and transitioning to STRIKE state");
    
    // Note: Strike notification to timer and audio are handled by module_state
    // via triggerStrike() in main.cpp when it detects STRIKE state change
    
    // Flash all game LEDs as visual feedback (using module_state timing)
    for (int i = 0; i < 4; i++) {
        ledStates[i] = true;
    }
    
    // Transition to STRIKE state (will reset and replay sequence after flash)
    currentState = SimonState::STRIKE;
    stateStartTime = millis();
}

void SimonSays::solvePuzzle() {
    Serial.println("Simon Says: Module solved!");
    
    isModuleSolved = true;
    currentState = SimonState::SOLVED;
    stateStartTime = millis();
    
    // Status LED is now handled by module_state library
    // MODULE_SOLVED message and solved sound are sent automatically by module_state when setSolved(true) is called
}

void SimonSays::resetModule() {
    Serial.println("Simon Says: Resetting after strike - replaying same sequence...");
    Serial.println("Simon Says: NEW color mappings due to strike:");
    
    // Show the new color mappings
    for (int i = 0; i < 4; i++) {
        SimonColor color = static_cast<SimonColor>(i);
        SimonColor mapped = getFlashColor(color);
        Serial.print("  ");
        Serial.print(getColorName(color));
        Serial.print(" flash -> press ");
        Serial.println(getColorName(mapped));
    }
    
    // Don't change currentSequenceLength or sequence - replay the same sequence
    // Don't clear sequence - keep the same colors
    playerInput.clear();
    
    displayIndex = 0;
    inputIndex = 0;
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    // Go directly to displaying the same sequence
    currentState = SimonState::DISPLAYING;
    stateStartTime = millis();
    audioPlayedForCurrentColor = false;
}

// ============================================================================
// KTANE RULE METHODS
// ============================================================================

bool SimonSays::shouldFlashColor(SimonColor color) const {
    return true; // Always use color mapping
}

SimonColor SimonSays::getFlashColor(SimonColor color) const {
    // Get strike count from module_state
    extern ModuleState* globalModuleState;
    uint8_t strikes = 0;
    if (globalModuleState) {
        strikes = globalModuleState->getStrikeCount();
    }
    
    if (hasVowelInSerial) {
        // Serial number contains a vowel
        if (strikes == 0) {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::BLUE: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::YELLOW: return SimonColor::GREEN;
                default: return color;
            }
        } else if (strikes == 1) {
            switch (color) {
                case SimonColor::RED: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
                case SimonColor::GREEN: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::RED;
                default: return color;
            }
        } else { // 2+ strikes
            switch (color) {
                case SimonColor::RED: return SimonColor::GREEN;
                case SimonColor::BLUE: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::YELLOW: return SimonColor::BLUE;
                default: return color;
            }
        }
    } else {
        // Serial number does NOT contain a vowel
        if (strikes == 0) {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::BLUE: return SimonColor::YELLOW;
                case SimonColor::GREEN: return SimonColor::GREEN;
                case SimonColor::YELLOW: return SimonColor::RED;
                default: return color;
            }
        } else if (strikes == 1) {
            switch (color) {
                case SimonColor::RED: return SimonColor::RED;
                case SimonColor::BLUE: return SimonColor::BLUE;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::YELLOW: return SimonColor::GREEN;
                default: return color;
            }
        } else { // 2+ strikes
            switch (color) {
                case SimonColor::RED: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
                case SimonColor::GREEN: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::RED;
                default: return color;
            }
        }
    }
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

uint8_t SimonSays::getColorPin(SimonColor color, bool isLED) const {
    if (isLED) {
        switch (color) {
            case SimonColor::RED: return SIMON_LED_RED;
            case SimonColor::YELLOW: return SIMON_LED_YELLOW;
            case SimonColor::GREEN: return SIMON_LED_GREEN;
            case SimonColor::BLUE: return SIMON_LED_BLUE;
            default: return 0;
        }
    } else {
        switch (color) {
            case SimonColor::RED: return SIMON_BTN_RED;
            case SimonColor::YELLOW: return SIMON_BTN_YELLOW;
            case SimonColor::GREEN: return SIMON_BTN_GREEN;
            case SimonColor::BLUE: return SIMON_BTN_BLUE;
            default: return 0;
        }
    }
}

const char* SimonSays::getColorName(SimonColor color) const {
    switch (color) {
        case SimonColor::RED: return "RED";
        case SimonColor::YELLOW: return "YELLOW";
        case SimonColor::GREEN: return "GREEN";
        case SimonColor::BLUE: return "BLUE";
        default: return "NONE";
    }
}

const char* SimonSays::getStateName(SimonState state) const {
    switch (state) {
        case SimonState::IDLE: return "IDLE";
        case SimonState::GENERATING: return "GENERATING";
        case SimonState::DISPLAYING: return "DISPLAYING";
        case SimonState::WAITING_INPUT: return "WAITING_INPUT";
        case SimonState::CHECKING_INPUT: return "CHECKING_INPUT";
        case SimonState::CORRECT_SEQUENCE: return "CORRECT_SEQUENCE";
        case SimonState::WRONG_INPUT: return "WRONG_INPUT";
        case SimonState::SOLVED: return "SOLVED";
        case SimonState::STRIKE: return "STRIKE";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// DEBUG INTERFACE
// ============================================================================

// Debug functions removed

// ============================================================================
// CAN MESSAGE HANDLER
// ============================================================================

void SimonSays::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    // Note: TIMER_RESET is handled by module_state, which calls gameStateCallback(false)
    // That triggers onGameStateChange(false) which calls stopGame() -> reset()
    // So no need to handle it here directly
    
    // Module-specific messages can be handled here if needed
    // (Currently none - all handled by module_state or main.cpp)
}
