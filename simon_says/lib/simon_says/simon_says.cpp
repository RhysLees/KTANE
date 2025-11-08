#include "simon_says.h"

#include <algorithm>

// -----------------------------------------------------------------------------
// Static Helpers
// -----------------------------------------------------------------------------

uint8_t SimonSays::colorToIndex(SimonColor color) {
    for (uint8_t i = 0; i < COLOR_COUNT; ++i) {
        if (COLOR_ORDER[i] == color) {
            return i;
        }
    }
    return 0xFF;
}

SimonColor SimonSays::indexToColor(uint8_t index) {
    if (index < COLOR_COUNT) {
        return COLOR_ORDER[index];
    }
    return SimonColor::None;
}

bool SimonSays::serialNumberHasVowel(const String& serial) {
    for (uint16_t i = 0; i < serial.length(); ++i) {
        const char c = serial.charAt(i);
        switch (c) {
            case 'A':
            case 'E':
            case 'I':
            case 'O':
            case 'U':
            case 'a':
            case 'e':
            case 'i':
            case 'o':
            case 'u':
                return true;
            default:
                break;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

SimonSays::SimonSays()
    : hardwareInitialized(false),
      gameRunning(false),
      solved(false),
      displayLedOn(false),
      playerFeedbackActive(false),
      playerFeedbackColor(SimonColor::None),
      playerFeedbackEndTime(0),
      serialNumberValue(),
      serialHasVowelFlag(false),
      strikeCountValue(0),
      strikePending(false),
      sequence{},
      stageLength(SIMON_INITIAL_SEQUENCE),
      inputIndex(0),
      displayIndex(0),
      lastPhaseTransition(0),
      lastDisplayToggle(0),
      lastButtonActivity(0),
      currentPhase(Phase::Idle),
      currentProgress(0),
      buttons{} {
    sequence.fill(SimonColor::Red);
}

void SimonSays::begin() {
    if (!hardwareInitialized) {
        for (uint8_t i = 0; i < COLOR_COUNT; ++i) {
            pinMode(LED_PINS[i], OUTPUT);
            digitalWrite(LED_PINS[i], HIGH);
            pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        }
        hardwareInitialized = true;
    }

    randomSeed(rp2040.hwrand32());
    reset();
}

void SimonSays::reset() {
    generateFullSequence();
    stageLength = SIMON_INITIAL_SEQUENCE;
    inputIndex = 0;
    displayIndex = 0;
    displayLedOn = false;
    playerFeedbackActive = false;
    playerFeedbackColor = SimonColor::None;
    playerFeedbackEndTime = 0;
    solved = false;
    strikePending = false;
    currentPhase = Phase::Idle;

    for (auto& button : buttons) {
        button.held = false;
        button.lastChange = millis();
    }

    lastPhaseTransition = millis();
    lastDisplayToggle = lastPhaseTransition;
    lastButtonActivity = lastPhaseTransition;
    updateProgress();
    enterIdlePhase();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void SimonSays::update() {
    handlePlayerFeedback();

    if (!gameRunning) {
        if (currentPhase != Phase::Idle) {
            enterIdlePhase();
        }
        return;
    }

    switch (currentPhase) {
        case Phase::Idle:
            enterDisplayPhase();
            break;
        case Phase::Displaying:
            handleDisplayPhase();
            break;
        case Phase::Input:
            handleInputPhase();
            break;
        case Phase::StrikeFlash:
            handleStrikeFlashPhase();
            break;
        case Phase::Solved:
        default:
            break;
    }
}

void SimonSays::setGameRunning(bool running) {
    if (gameRunning == running) {
        return;
    }
    gameRunning = running;
    if (!gameRunning) {
        enterIdlePhase();
    } else {
        reset();
    }
}

void SimonSays::setSerialNumber(const String& serial) {
    serialNumberValue = serial;
    serialHasVowelFlag = serialNumberHasVowel(serialNumberValue);
}

void SimonSays::setStrikeCount(uint8_t strikes) {
    strikeCountValue = strikes;
}

bool SimonSays::serialHasVowel() const {
    return serialHasVowelFlag;
}

uint8_t SimonSays::strikeCount() const {
    return strikeCountValue;
}

SimonSays::Phase SimonSays::phase() const {
    return currentPhase;
}

uint8_t SimonSays::progressPercent() const {
    return currentProgress;
}

bool SimonSays::isSolved() const {
    return solved;
}

bool SimonSays::consumeStrikeTriggered() {
    bool pending = strikePending;
    strikePending = false;
    return pending;
}

SimonColor SimonSays::mapFlashToPress(SimonColor flash) const {
    const uint8_t flashIndex = colorToIndex(flash);
    if (flashIndex >= COLOR_COUNT) {
        return SimonColor::None;
    }

    const uint8_t strikeIndex = std::min<uint8_t>(strikeCountValue, MAX_STRIKE_LEVEL);
    if (serialHasVowelFlag) {
        return VOWEL_MAPPING[strikeIndex][flashIndex];
    }
    return CONSONANT_MAPPING[strikeIndex][flashIndex];
}

const __FlashStringHelper* SimonSays::colorToString(SimonColor color) {
    switch (color) {
        case SimonColor::Red:
            return F("Red");
        case SimonColor::Blue:
            return F("Blue");
        case SimonColor::Green:
            return F("Green");
        case SimonColor::Yellow:
            return F("Yellow");
        default:
            return F("None");
    }
}

// -----------------------------------------------------------------------------
// Phase Control
// -----------------------------------------------------------------------------

void SimonSays::generateFullSequence() {
    for (uint8_t i = 0; i < SIMON_MAX_SEQUENCE_LENGTH; ++i) {
        sequence[i] = indexToColor(static_cast<uint8_t>(random(COLOR_COUNT)));
    }
}

void SimonSays::enterIdlePhase() {
    setAllLeds(false);
    displayLedOn = false;
    playerFeedbackActive = false;
    playerFeedbackColor = SimonColor::None;
    playerFeedbackEndTime = 0;
    currentPhase = Phase::Idle;
}

void SimonSays::enterDisplayPhase() {
    currentPhase = Phase::Displaying;
    displayIndex = 0;
    displayLedOn = false;
    lightColor(playerFeedbackColor, false);
    playerFeedbackActive = false;
    playerFeedbackColor = SimonColor::None;
    lastDisplayToggle = millis();
    lastPhaseTransition = lastDisplayToggle;
    setAllLeds(false);
}

void SimonSays::enterInputPhase() {
    currentPhase = Phase::Input;
    inputIndex = 0;
    lastButtonActivity = millis();
    playerFeedbackActive = false;
    playerFeedbackColor = SimonColor::None;
    setAllLeds(false);
}

void SimonSays::enterStrikeFlashPhase() {
    currentPhase = Phase::StrikeFlash;
    lastPhaseTransition = millis();
    setAllLeds(true);
}

void SimonSays::enterSolvedPhase() {
    solved = true;
    currentPhase = Phase::Solved;
    setAllLeds(false);
    currentProgress = 100;
    sendAudio(AUDIO_DEFUSED);
}

void SimonSays::handleDisplayPhase() {
    const unsigned long now = millis();

    SimonColor pressed = pollButtonPress();
    if (pressed != SimonColor::None) {
        setAllLeds(false);
        displayLedOn = false;
        displayIndex = stageLength;
        enterInputPhase();
        processPlayerInput(pressed);
        return;
    }

    if (displayIndex >= stageLength) {
        enterInputPhase();
        return;
    }

    if (!displayLedOn) {
        if (now - lastDisplayToggle >= SIMON_PAUSE_TIME_MS) {
            SimonColor current = sequence[displayIndex];
            Serial.print('[');
            Serial.print(millis());
            Serial.print(F("] [Simon] Flashing "));
            Serial.println(colorToString(current));
            SimonColor expected = mapFlashToPress(current);
            Serial.print('[');
            Serial.print(millis());
            Serial.print(F("] [Simon] Expect Press "));
            Serial.println(colorToString(expected));
            lightColor(current, true);
            playDisplaySound(current);
            displayLedOn = true;
            lastDisplayToggle = now;
        }
    } else if (now - lastDisplayToggle >= SIMON_DISPLAY_TIME_MS) {
        lightColor(sequence[displayIndex], false);
        displayLedOn = false;
        lastDisplayToggle = now;
        ++displayIndex;
    }
}

void SimonSays::handleInputPhase() {
    const unsigned long now = millis();

    if (now - lastButtonActivity >= SIMON_INPUT_TIMEOUT_MS) {
        Serial.print('[');
        Serial.print(millis());
        Serial.println(F("] [Simon] Input timeout -> replay sequence"));
        enterDisplayPhase();
        return;
    }

    SimonColor pressed = pollButtonPress();
    if (pressed != SimonColor::None) {
        processPlayerInput(pressed);
    }
}

void SimonSays::handleStrikeFlashPhase() {
    const unsigned long now = millis();
    if (now - lastPhaseTransition >= SIMON_STRIKE_FLASH_MS) {
        setAllLeds(false);
        enterDisplayPhase();
    }
}

void SimonSays::handlePlayerFeedback() {
    if (!playerFeedbackActive) {
        return;
    }
    const unsigned long now = millis();
    if (now >= playerFeedbackEndTime) {
        if (playerFeedbackColor != SimonColor::None) {
            lightColor(playerFeedbackColor, false);
        }
        playerFeedbackActive = false;
        playerFeedbackColor = SimonColor::None;
    }
}

void SimonSays::processPlayerInput(SimonColor pressed) {
    const unsigned long now = millis();
    lastButtonActivity = now;

    if (pressed != SimonColor::None) {
        Serial.print('[');
        Serial.print(now);
        Serial.print(F("] [Simon] Button "));
        Serial.print(colorToString(pressed));
        if (playerFeedbackActive && playerFeedbackColor != SimonColor::None) {
            lightColor(playerFeedbackColor, false);
        }
        playerFeedbackColor = pressed;
        playerFeedbackActive = true;
        playerFeedbackEndTime = now + SIMON_DISPLAY_TIME_MS;
        lightColor(pressed, true);
        playPressSound(pressed);
    }

    if (inputIndex >= stageLength) {
        inputIndex = (stageLength == 0) ? 0 : stageLength - 1;
    }

    SimonColor expected = mapFlashToPress(sequence[inputIndex]);
    if (pressed == expected) {
        Serial.print('[');
        Serial.print(now);
        Serial.println(F("] [Simon] -> Correct"));
        ++inputIndex;
        if (inputIndex >= stageLength) {
            if (stageLength >= SIMON_MAX_SEQUENCE_LENGTH) {
                Serial.print('[');
                Serial.print(now);
                Serial.println(F("] [Simon] Stage complete -> solved"));
                enterSolvedPhase();
            } else {
                Serial.print('[');
                Serial.print(now);
                Serial.println(F("] [Simon] Stage complete -> extending sequence"));
                ++stageLength;
                lastButtonActivity = now;
                updateProgress();
                enterDisplayPhase();
            }
        }
    } else {
        Serial.print('[');
        Serial.print(now);
        Serial.print(F("] [Simon] -> Wrong (expected "));
        Serial.print(colorToString(expected));
        Serial.println(F(")"));
        registerStrike();
        enterStrikeFlashPhase();
    }
}

SimonColor SimonSays::pollButtonPress() {
    const unsigned long now = millis();
    for (uint8_t i = 0; i < COLOR_COUNT; ++i) {
        const bool pressed = readButton(i);
        ButtonState& state = buttons[i];

        if (pressed != state.held) {
            if (now - state.lastChange >= BUTTON_DEBOUNCE_MS) {
                state.lastChange = now;
                state.held = pressed;
                if (pressed) {
                    lastButtonActivity = now;
                    SimonColor color = indexToColor(i);
                    Serial.print('[');
                    Serial.print(now);
                    Serial.print(F("] [Simon] Detected press "));
                    Serial.println(colorToString(color));
                    return color;
                }
            }
        }
    }
    return SimonColor::None;
}

bool SimonSays::readButton(uint8_t index) const {
    if (index >= COLOR_COUNT) {
        return false;
    }
    return digitalRead(BUTTON_PINS[index]) == LOW;
}

void SimonSays::setAllLeds(bool on) {
    for (uint8_t i = 0; i < COLOR_COUNT; ++i) {
        lightColor(indexToColor(i), on);
    }
}

void SimonSays::lightColor(SimonColor color, bool on) {
    const uint8_t index = colorToIndex(color);
    if (index >= COLOR_COUNT) {
        return;
    }
    pinMode(LED_PINS[index], OUTPUT);
    digitalWrite(LED_PINS[index], on ? HIGH : LOW);
}

void SimonSays::playDisplaySound(SimonColor color) {
    const uint8_t index = colorToIndex(color);
    if (index >= COLOR_COUNT) {
        return;
    }
    sendAudio(DISPLAY_SOUNDS[index]);
}

void SimonSays::playPressSound(SimonColor color) {
    const uint8_t index = colorToIndex(color);
    if (index >= COLOR_COUNT) {
        return;
    }
    sendAudio(PRESS_SOUNDS[index]);
}

void SimonSays::sendAudio(CanAudioSound sound) {
    uint8_t payload[1] = {static_cast<uint8_t>(sound)};
    sendCanMessage(CAN_ID_AUDIO, payload, 1);
}

void SimonSays::registerStrike() {
    strikePending = true;
    sendAudio(AUDIO_STRIKE);
    strikeCountValue = std::min<uint8_t>(static_cast<uint8_t>(strikeCountValue + 1), static_cast<uint8_t>(255));
}

void SimonSays::updateProgress() {
    uint8_t completed = 0;
    if (stageLength > SIMON_INITIAL_SEQUENCE) {
        completed = static_cast<uint8_t>(stageLength - SIMON_INITIAL_SEQUENCE);
    }

    const uint8_t totalStages = static_cast<uint8_t>((SIMON_MAX_SEQUENCE_LENGTH - SIMON_INITIAL_SEQUENCE) + 1);
    uint8_t progress = static_cast<uint8_t>((completed * 100) / totalStages);
    if (progress > 100) {
        progress = 100;
    }
    currentProgress = progress;
}


