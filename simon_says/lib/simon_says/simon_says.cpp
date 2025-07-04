#include "simon_says.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

SimonSays::SimonSays() {
    currentState = SimonState::IDLE;
    isModuleSolved = false;
    gameStarted = false;
    
    currentSequenceLength = 0;
    displayIndex = 0;
    inputIndex = 0;
    
    lastUpdateTime = 0;
    stateStartTime = 0;
    
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
    
    // Send registration directly via CAN bus library
    uint8_t registrationData[3];
    registrationData[0] = static_cast<uint8_t>(SIMON_MSG_REGISTER);
    registrationData[1] = static_cast<uint8_t>(CAN_TYPE_SIMON);
    registrationData[2] = 0x00; // Instance ID
    sendCanMessage(CAN_ID_TIMER, registrationData, 3);
    Serial.println("Simon Says: Registration sent");
    
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
    
    // Send realtime status on state changes
    static SimonState lastState = SimonState::IDLE;
    static bool lastSolved = false;
    static uint8_t lastSeqLength = 0;
    
    if (currentState != lastState || isModuleSolved != lastSolved || 
        currentSequenceLength != lastSeqLength) {
        
        // Send realtime status directly via CAN bus library
        uint8_t statusData[5];
        statusData[0] = static_cast<uint8_t>(SIMON_MSG_STATUS);
        statusData[1] = static_cast<uint8_t>(currentState);
        statusData[2] = isModuleSolved ? 1 : 0;
        statusData[3] = currentSequenceLength;
        statusData[4] = strikeCount;
        sendCanMessage(CAN_ID_TIMER, statusData, 5);
        
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
            if (currentTime - stateStartTime > SIMON_INPUT_TIMEOUT_MS) {
                handleStrike();
            }
            break;
            
        case SimonState::CHECKING_INPUT:
            checkInput();
            break;
            
        case SimonState::CORRECT_SEQUENCE:
            nextStage();
            break;
            
        case SimonState::WRONG_INPUT:
            handleStrike();
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
    displayIndex = 0;
    inputIndex = 0;
    
    isFlashing = false;
    
    sequence.clear();
    playerInput.clear();
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    digitalWrite(SIMON_STATUS_LED, LOW);
    
    // Send realtime status update directly via CAN bus library
    uint8_t statusData[5];
    statusData[0] = static_cast<uint8_t>(SIMON_MSG_STATUS);
    statusData[1] = static_cast<uint8_t>(currentState);
    statusData[2] = isModuleSolved ? 1 : 0;
    statusData[3] = currentSequenceLength;
    statusData[4] = strikeCount;
    sendCanMessage(CAN_ID_TIMER, statusData, 5);
    
    Serial.println("Simon Says: Reset complete.");
}

// ============================================================================
// GAME STATE INTERFACE
// ============================================================================

void SimonSays::startGame() {
    if (currentState == SimonState::IDLE) {
        Serial.println("Simon Says: Game starting...");
        gameStarted = true;
        currentState = SimonState::GENERATING;
        stateStartTime = millis();
        
        digitalWrite(SIMON_STATUS_LED, HIGH);
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
    buttonStates[0] = !digitalRead(SIMON_BTN_RED);
    buttonStates[1] = !digitalRead(SIMON_BTN_YELLOW);
    buttonStates[2] = !digitalRead(SIMON_BTN_GREEN);
    buttonStates[3] = !digitalRead(SIMON_BTN_BLUE);
    
    for (int i = 0; i < 4; i++) {
        lastButtonStates[i] = buttonStates[i];
    }
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
        currentSequenceLength = 3;
        sequence.clear();
        
        for (int i = 0; i < 3; i++) {
            SimonColor color = static_cast<SimonColor>(random(4));
            sequence.push_back(color);
        }
    } else {
        SimonColor color = static_cast<SimonColor>(random(4));
        sequence.push_back(color);
        currentSequenceLength++;
    }
    
    printSequence();
    
    displayIndex = 0;
    currentState = SimonState::DISPLAYING;
    stateStartTime = millis();
    
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
            SimonColor displayColor = shouldFlashColor(sequence[displayIndex]) ? 
                                     getFlashColor(sequence[displayIndex]) : 
                                     sequence[displayIndex];
            
            setLED(displayColor, true);
            playAudioForColor(displayColor);
            
        } else if (elapsed >= colorEndTime) {
            setLED(sequence[displayIndex], false);
            
            if (elapsed >= colorStartTime + SIMON_DISPLAY_TIME_MS + SIMON_PAUSE_TIME_MS) {
                displayIndex++;
            }
        }
    } else {
        playerInput.clear();
        inputIndex = 0;
        currentState = SimonState::WAITING_INPUT;
        stateStartTime = millis();
    }
}

void SimonSays::processInput() {
    for (int i = 0; i < 4; i++) {
        if (buttonStates[i] && !lastButtonStates[i]) {
            SimonColor pressedColor = static_cast<SimonColor>(i);
            playerInput.push_back(pressedColor);
            
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
        SimonColor expectedColor = shouldFlashColor(sequence[inputIndex]) ? 
                                  getFlashColor(sequence[inputIndex]) : 
                                  sequence[inputIndex];
        
        if (playerInput[inputIndex] == expectedColor) {
            inputIndex++;
            
            if (inputIndex >= sequence.size()) {
                currentState = SimonState::CORRECT_SEQUENCE;
                stateStartTime = millis();
            } else {
                currentState = SimonState::WAITING_INPUT;
                stateStartTime = millis();
            }
        } else {
            currentState = SimonState::WRONG_INPUT;
            stateStartTime = millis();
        }
    }
}

void SimonSays::nextStage() {
    if (currentSequenceLength >= SIMON_MAX_SEQUENCE_LENGTH) {
        solvePuzzle();
    } else {
        generateSequence();
    }
}

void SimonSays::handleStrike() {
    Serial.println("Simon Says: Strike!");
    
    strikeCount++;
    
    // Send strike message directly to timer module via CAN bus library
    uint8_t strikeData[2];
    strikeData[0] = static_cast<uint8_t>(SIMON_MSG_STRIKE);
    strikeData[1] = strikeCount;
    sendCanMessage(CAN_ID_TIMER, strikeData, 2);
    
    // Also send strike update directly to timer module
    uint8_t timerData[2];
    timerData[0] = 0x12; // STRIKE_UPDATE message type
    timerData[1] = strikeCount;
    sendCanMessage(CAN_ID_TIMER, timerData, 2);
    
    Serial.println("Simon Says: Strike message sent");
    
    flashAllLEDs(SIMON_STRIKE_FLASH_MS);
    playStrikeSound();
    
    currentState = SimonState::STRIKE;
    stateStartTime = millis();
    
    // Send realtime status update directly via CAN bus library
    uint8_t statusData[5];
    statusData[0] = static_cast<uint8_t>(SIMON_MSG_STATUS);
    statusData[1] = static_cast<uint8_t>(currentState);
    statusData[2] = isModuleSolved ? 1 : 0;
    statusData[3] = currentSequenceLength;
    statusData[4] = strikeCount;
    sendCanMessage(CAN_ID_TIMER, statusData, 5);
}

void SimonSays::solvePuzzle() {
    Serial.println("Simon Says: Module solved!");
    
    isModuleSolved = true;
    currentState = SimonState::SOLVED;
    stateStartTime = millis();
    
    digitalWrite(SIMON_STATUS_LED, HIGH);
    playSolvedSound();
    
    // Send solved message directly via CAN bus library
    uint8_t solvedData[1];
    solvedData[0] = static_cast<uint8_t>(SIMON_MSG_SOLVED);
    sendCanMessage(CAN_ID_TIMER, solvedData, 1);
    Serial.println("Simon Says: Solved message sent");
    
    // Send realtime status update directly via CAN bus library
    uint8_t statusData[5];
    statusData[0] = static_cast<uint8_t>(SIMON_MSG_STATUS);
    statusData[1] = static_cast<uint8_t>(currentState);
    statusData[2] = isModuleSolved ? 1 : 0;
    statusData[3] = currentSequenceLength;
    statusData[4] = strikeCount;
    sendCanMessage(CAN_ID_TIMER, statusData, 5);
}

void SimonSays::resetModule() {
    Serial.println("Simon Says: Resetting after strike...");
    
    currentSequenceLength = 0;
    sequence.clear();
    playerInput.clear();
    
    displayIndex = 0;
    inputIndex = 0;
    
    isFlashing = false;
    
    for (int i = 0; i < 4; i++) {
        ledStates[i] = false;
    }
    
    currentState = SimonState::GENERATING;
    stateStartTime = millis();
}

// ============================================================================
// KTANE RULE METHODS
// ============================================================================

bool SimonSays::shouldFlashColor(SimonColor color) const {
    return true;
}

SimonColor SimonSays::getFlashColor(SimonColor color) const {
    if (numStrikes == 0) {
        if (hasVowelInSerial) {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::BLUE: return SimonColor::RED;
                case SimonColor::YELLOW: return SimonColor::YELLOW;
                case SimonColor::GREEN: return SimonColor::GREEN;
                default: return color;
            }
        } else {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
                default: return color;
            }
        }
    } else if (numStrikes == 1) {
        if (hasVowelInSerial) {
            switch (color) {
                case SimonColor::RED: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
                case SimonColor::YELLOW: return SimonColor::YELLOW;
                case SimonColor::GREEN: return SimonColor::GREEN;
                default: return color;
            }
        } else {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
                default: return color;
            }
        }
    } else {
        if (hasVowelInSerial) {
            switch (color) {
                case SimonColor::RED: return SimonColor::YELLOW;
                case SimonColor::GREEN: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::BLUE;
                default: return color;
            }
        } else {
            switch (color) {
                case SimonColor::RED: return SimonColor::BLUE;
                case SimonColor::YELLOW: return SimonColor::RED;
                case SimonColor::GREEN: return SimonColor::YELLOW;
                case SimonColor::BLUE: return SimonColor::GREEN;
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

void SimonSays::printStatus() const {
    Serial.println("=== SIMON SAYS STATUS ===");
    Serial.print("State: "); Serial.println(getStateName(currentState));
    Serial.print("Solved: "); Serial.println(isModuleSolved ? "YES" : "NO");
    Serial.print("Game Started: "); Serial.println(gameStarted ? "YES" : "NO");
    Serial.print("Sequence Length: "); Serial.println(currentSequenceLength);
    Serial.print("Strikes: "); Serial.println(strikeCount);
    Serial.print("Has Vowel in Serial: "); Serial.println(hasVowelInSerial ? "YES" : "NO");
    Serial.print("Current Strikes: "); Serial.println(numStrikes);
}

void SimonSays::printSequence() const {
    Serial.print("Simon Says: Sequence (");
    Serial.print(sequence.size());
    Serial.print("): ");
    
    for (size_t i = 0; i < sequence.size(); i++) {
        if (i > 0) Serial.print(" -> ");
        Serial.print(getColorName(sequence[i]));
    }
    Serial.println();
}

void SimonSays::printRules() const {
    Serial.println("=== SIMON SAYS RULES ===");
    Serial.print("Strikes: "); Serial.println(numStrikes);
    Serial.print("Serial has vowel: "); Serial.println(hasVowelInSerial ? "YES" : "NO");
    Serial.println("Color mappings:");
    
    for (int i = 0; i < 4; i++) {
        SimonColor color = static_cast<SimonColor>(i);
        SimonColor mapped = getFlashColor(color);
        Serial.print("  ");
        Serial.print(getColorName(color));
        Serial.print(" -> ");
        Serial.println(getColorName(mapped));
    }
}



// ============================================================================
// CAN MESSAGE HANDLER
// ============================================================================

void SimonSays::handleCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if (len == 0) return;
    
    uint8_t msgType = data[0];
    
    switch (msgType) {
        case SIMON_MSG_RESET:
            reset();
            break;
            
        default:
            break;
    }
}
