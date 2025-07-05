#include "simon_says.h"

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
    
    strikeCount = 0;
    isFlashing = false;
    flashStartTime = 0;
    
    hasVowelInSerial = false;
    numStrikes = 0;
    
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
    
    // Module registration will be handled by discovery system
    // No need to send manual registration message
    
    currentState = SimonState::IDLE;
    stateStartTime = millis();
    
    flashAllLEDs(500);
    
    Serial.println("Simon Says: Ready!");
}

void SimonSays::update() {
    unsigned long currentTime = millis();
    lastUpdateTime = currentTime;
    
    updateButtons();
    updateLEDs();
    
    // Send periodic heartbeat (every 5 seconds)
    static unsigned long lastHeartbeat = 0;
    if (currentTime - lastHeartbeat > 5000) {
        uint8_t heartbeatData[4];
        heartbeatData[0] = MODULE_HEARTBEAT;
        heartbeatData[1] = static_cast<uint8_t>(currentState);
        heartbeatData[2] = isModuleSolved ? 1 : 0;
        heartbeatData[3] = currentSequenceLength;
        sendCanMessage(CAN_ID_TIMER, heartbeatData, 4);
        lastHeartbeat = currentTime;
    }
    
    // Send realtime status on state changes
    static SimonState lastState = SimonState::IDLE;
    static bool lastSolved = false;
    static uint8_t lastSeqLength = 0;
    
    if (currentState != lastState || isModuleSolved != lastSolved || 
        currentSequenceLength != lastSeqLength) {
        
        // Send status update to timer
        uint8_t statusData[5];
        statusData[0] = MODULE_STATUS;
        statusData[1] = static_cast<uint8_t>(currentState);
        statusData[2] = isModuleSolved ? 1 : 0;
        statusData[3] = currentSequenceLength;
        statusData[4] = strikeCount;
        sendCanMessage(CAN_ID_TIMER, statusData, 5);
        
        // Check for important state changes
        if (isModuleSolved && !lastSolved) {
            // Module just got solved!
            uint8_t solvedData[1];
            solvedData[0] = MODULE_SOLVED;
            sendCanMessage(CAN_ID_TIMER, solvedData, 1);
            Serial.println("Simon Says: Sent MODULE_SOLVED to timer");
        }
        
        lastState = currentState;
        lastSolved = isModuleSolved;
        lastSeqLength = currentSequenceLength;
    }
    
    if (isFlashing && currentTime - flashStartTime > SIMON_STRIKE_FLASH_MS) {
        isFlashing = false;
        for (int i = 0; i < 4; i++) {
            ledStates[i] = false;
        }
    }
    
    switch (currentState) {
        case SimonState::IDLE:
            break;
            
        case SimonState::GENERATING:
            generateSequence();
            break;
            
        case SimonState::DISPLAYING:
            displaySequence();
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
            if (currentTime - stateStartTime > SIMON_STRIKE_FLASH_MS) {
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
    
    isFlashing = false;
    
    sequence.clear();
    playerInput.clear();
    
    audioPlayedForCurrentColor = false;
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    digitalWrite(SIMON_STATUS_LED, LOW);
    
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
            
            // Status LED should only be on when solved, not when game starts
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

void SimonSays::setStrikeCount(uint8_t strikes) {
    numStrikes = strikes;
    strikeCount = strikes;
}

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

// ============================================================================
// HARDWARE METHODS
// ============================================================================

void SimonSays::initHardware() {
    pinMode(SIMON_LED_RED, OUTPUT);
    pinMode(SIMON_LED_YELLOW, OUTPUT);
    pinMode(SIMON_LED_GREEN, OUTPUT);
    pinMode(SIMON_LED_BLUE, OUTPUT);
    pinMode(SIMON_STATUS_LED, OUTPUT);
    
    pinMode(SIMON_BTN_RED, INPUT_PULLUP);
    pinMode(SIMON_BTN_YELLOW, INPUT_PULLUP);
    pinMode(SIMON_BTN_GREEN, INPUT_PULLUP);
    pinMode(SIMON_BTN_BLUE, INPUT_PULLUP);
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    digitalWrite(SIMON_STATUS_LED, LOW);
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
    if (isFlashing) {
        bool flashState = ((millis() - flashStartTime) % 200) < 100;
        digitalWrite(SIMON_LED_RED, flashState);
        digitalWrite(SIMON_LED_YELLOW, flashState);
        digitalWrite(SIMON_LED_GREEN, flashState);
        digitalWrite(SIMON_LED_BLUE, flashState);
    } else {
        digitalWrite(SIMON_LED_RED, ledStates[0]);
        digitalWrite(SIMON_LED_YELLOW, ledStates[1]);
        digitalWrite(SIMON_LED_GREEN, ledStates[2]);
        digitalWrite(SIMON_LED_BLUE, ledStates[3]);
    }
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

void SimonSays::playStrikeSound() {
    uint8_t audioData[1];
    audioData[0] = AUDIO_STRIKE;
    sendCanMessage(CAN_ID_AUDIO, audioData, 1);
}

void SimonSays::playSolvedSound() {
    uint8_t audioData[1];
    audioData[0] = AUDIO_DEFUSED;
    sendCanMessage(CAN_ID_AUDIO, audioData, 1);
}

void SimonSays::flashAllLEDs(unsigned long duration) {
    for (int i = 0; i < 4; i++) {
        ledStates[i] = true;
    }
    
    isFlashing = true;
    flashStartTime = millis();
}

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
    
    printSequence();
    
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
    for (int i = 0; i < 4; i++) {
        if (buttonStates[i] && !lastButtonStates[i]) {
            SimonColor pressedColor = static_cast<SimonColor>(i);
            playerInput.push_back(pressedColor);
            
            Serial.print("Simon Says: Button pressed - ");
            Serial.println(getColorName(pressedColor));
            
            setLED(pressedColor, true);
            playAudioForColor(pressedColor);
            
            currentState = SimonState::CHECKING_INPUT;
            stateStartTime = millis();
            
            break;
        }
    }
    
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
    Serial.println("Simon Says: Strike!");
    
    strikeCount++;
    numStrikes++; // Update the strikes used for color mapping rules
    
    Serial.print("Simon Says: Strike count now ");
    Serial.print(strikeCount);
    Serial.println(" - color mappings will change!");
    
    // Send strike notification to timer
    uint8_t strikeData[1];
    strikeData[0] = MODULE_STRIKE;
    sendCanMessage(CAN_ID_TIMER, strikeData, 1);
    Serial.println("Simon Says: Strike notification sent to timer");
    
    flashAllLEDs(SIMON_STRIKE_FLASH_MS);
    playStrikeSound();
    
    currentState = SimonState::STRIKE;
    stateStartTime = millis();
}

void SimonSays::solvePuzzle() {
    Serial.println("Simon Says: Module solved!");
    
    isModuleSolved = true;
    currentState = SimonState::SOLVED;
    stateStartTime = millis();
    
    digitalWrite(SIMON_STATUS_LED, HIGH);
    playSolvedSound();
    
    // Note: MODULE_SOLVED message is sent automatically by update() method
    // when it detects the state change to solved
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
    
    isFlashing = false;
    
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
    if (hasVowelInSerial) {
        // Serial number contains a vowel
        if (numStrikes == 0) {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::BLUE: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::YELLOW: return SimonColor::GREEN;
                default: return color;
            }
        } else if (numStrikes == 1) {
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
        if (numStrikes == 0) {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::BLUE: return SimonColor::YELLOW;
                case SimonColor::GREEN: return SimonColor::GREEN;
                case SimonColor::YELLOW: return SimonColor::RED;
                default: return color;
            }
        } else if (numStrikes == 1) {
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

void SimonSays::handleCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if (len < 3) return;
    
    // New message format: [senderType, senderInstance, messageType, ...messageData]
    uint8_t senderType = data[0];
    uint8_t senderInstance = data[1];
    uint8_t msgType = data[2];
    
    switch (msgType) {
        case SIMON_MSG_RESET:
            reset();
            break;
            
        default:
            break;
    }
}
