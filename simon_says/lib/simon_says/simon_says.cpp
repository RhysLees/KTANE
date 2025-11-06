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
    lastInputTime = 0;
    
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
                    // Button pressed during display - add to input
                    SimonColor pressedColor = static_cast<SimonColor>(i);
                    
                    // Only add if we don't already have this input pending
                    // We should have exactly inputIndex inputs already processed
                    if (playerInput.size() == inputIndex) {
                        playerInput.push_back(pressedColor);
                        
                        // Update last input time for timeout tracking
                        lastInputTime = millis();
                        
                        setLED(pressedColor, true);
                        playAudioForColor(pressedColor);
                        
                        // Transition to checking state (but display continues in background)
                        currentState = SimonState::CHECKING_INPUT;
                        stateStartTime = millis();
                    }
                    break;
                }
            }
            
            // Turn off LEDs when buttons are released
            for (int i = 0; i < 4; i++) {
                if (!buttonStates[i] && lastButtonStates[i] && currentState == SimonState::DISPLAYING) {
                    setLED(static_cast<SimonColor>(i), false);
                }
            }
            
            // Continue displaying sequence (it loops continuously)
            if (currentState == SimonState::DISPLAYING) {
                displaySequence();
            }
            break;
            
        case SimonState::WAITING_INPUT:
            // WAITING_INPUT state - transition back to DISPLAYING to show sequence
            // The sequence loops continuously while waiting for input
            currentState = SimonState::DISPLAYING;
            displayIndex = 0;
            audioPlayedForCurrentColor = false;
            stateStartTime = millis();
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
}

// ============================================================================
// GAME STATE INTERFACE
// ============================================================================

void SimonSays::startGame() {
    if (currentState == SimonState::IDLE) {
        if (initializationComplete) {
            gameStarted = true;
            currentState = SimonState::GENERATING;
            stateStartTime = millis();
            
            // Status LED is now handled by module_state library
        }
    }
}

void SimonSays::stopGame() {
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
}

void SimonSays::setInitializationComplete(bool complete) {
    initializationComplete = complete;
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
    if (currentSequenceLength == 0) {
        // First time - generate random target sequence length (3-5 stages)
        targetSequenceLength = random(3, 6); // 3 to 5 inclusive
        currentSequenceLength = 1;
        sequence.clear();
        
        // Start with just one color
        SimonColor color = static_cast<SimonColor>(random(4));
        sequence.push_back(color);
    } else {
        // Add one more color to the sequence
        SimonColor color = static_cast<SimonColor>(random(4));
        sequence.push_back(color);
        currentSequenceLength++;
    }
    
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
        // Sequence complete - loop back to start of sequence
        // The sequence will loop continuously until input is complete
        displayIndex = 0;
        audioPlayedForCurrentColor = false;
        
        // Check if we've been waiting too long - reset input if so
        // One full loop is about: sequence.size() * (DISPLAY_TIME + PAUSE_TIME) = ~3-4 seconds
        // If we have partial input and one full loop has passed since last input, reset
        if (playerInput.size() > 0 && playerInput.size() < sequence.size()) {
            // We have partial input - check if timeout has occurred
            // Calculate one full sequence loop duration
            unsigned long sequenceDuration = sequence.size() * (SIMON_DISPLAY_TIME_MS + SIMON_PAUSE_TIME_MS);
            
            // Check if we've waited more than one full loop since last input
            if (lastInputTime > 0 && (millis() - lastInputTime) > sequenceDuration) {
                // Timeout - reset input sequence but don't strike
                playerInput.clear();
                inputIndex = 0;
                lastInputTime = 0;  // Reset timeout tracking
            }
        }
        
        // Continue looping the sequence
        // Don't transition to WAITING_INPUT - keep displaying so player can see the sequence
    }
}

// Note: processInput() is no longer needed - input handling is done directly in state handlers

void SimonSays::checkInput() {
    if (millis() - stateStartTime < 100) {
        return;  // Wait 100ms for button to settle (reduced from 200ms)
    }
    
    // Turn off all LEDs
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    // Check if we have input to validate
    if (playerInput.size() <= inputIndex) {
        // No input yet - transition back to displaying sequence
        currentState = SimonState::DISPLAYING;
        displayIndex = 0;
        audioPlayedForCurrentColor = false;
        stateStartTime = millis();
        return;
    }
    
    // Validate the current input
    SimonColor pressedColor = playerInput[inputIndex];
    SimonColor expectedColor = getFlashColor(sequence[inputIndex]);
    
    if (pressedColor == expectedColor) {
        inputIndex++;
        
        if (inputIndex >= sequence.size()) {
            // Sequence completed correctly!
            playerInput.clear();
            inputIndex = 0;
            currentState = SimonState::CORRECT_SEQUENCE;
            stateStartTime = millis();
        } else {
            // Wait for next input - continue displaying sequence
            // Don't remove elements - inputIndex tracks position in both arrays
            // Continue displaying sequence (it loops)
            currentState = SimonState::DISPLAYING;
            displayIndex = 0;  // Reset display to start
            audioPlayedForCurrentColor = false;
            stateStartTime = millis(); // Reset timer for next loop check
        }
    } else {
        // Wrong input!
        // Clear all input
        playerInput.clear();
        inputIndex = 0;
        
        currentState = SimonState::WRONG_INPUT;
        stateStartTime = millis();
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
    isModuleSolved = true;
    currentState = SimonState::SOLVED;
    stateStartTime = millis();
    
    // Status LED is now handled by module_state library
    // MODULE_SOLVED message and solved sound are sent automatically by module_state when setSolved(true) is called
}

void SimonSays::resetModule() {
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
