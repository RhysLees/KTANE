// KTANE Simon Says shared library
// Implements core game logic and hardware interface, driven externally by module_state

#pragma once

#include <Arduino.h>
#include <array>
#include <can_bus.h>

enum class SimonColor : uint8_t {
    Red = 0,
    Blue = 1,
    Green = 2,
    Yellow = 3,
    None = 0xFF
};

class SimonSays {
public:
    enum class Phase : uint8_t {
        Idle,
        Displaying,
        Input,
        StrikeFlash,
        Solved
    };

    SimonSays();

    void begin();
    void reset();
    void update();

    void setGameRunning(bool running);
    void setSerialNumber(const String& serial);
    void setStrikeCount(uint8_t strikes);

    bool serialHasVowel() const;
    uint8_t strikeCount() const;
    Phase phase() const;
    uint8_t progressPercent() const;
    bool isSolved() const;
    bool consumeStrikeTriggered();

    SimonColor mapFlashToPress(SimonColor flash) const;
    const String& serialNumber() const { return serialNumberValue; }

    static const __FlashStringHelper* colorToString(SimonColor color);

    static constexpr uint8_t COLOR_COUNT = 4;
    static constexpr uint8_t MAX_STRIKE_LEVEL = 2;

private:
    // ---------------------------------------------------------------------
    // Configuration
    // ---------------------------------------------------------------------
    struct ButtonState {
        bool held = false;
        unsigned long lastChange = 0;
    };

    static constexpr uint16_t SIMON_DISPLAY_TIME_MS = 500;
    static constexpr uint16_t SIMON_PAUSE_TIME_MS = 200;
    static constexpr uint16_t SIMON_INPUT_TIMEOUT_MS = 5000;
    static constexpr uint16_t SIMON_STRIKE_FLASH_MS = 1000;
    static constexpr uint16_t BUTTON_DEBOUNCE_MS = 30;
    static constexpr uint8_t SIMON_INITIAL_SEQUENCE = 1;
    static constexpr uint8_t SIMON_MAX_SEQUENCE_LENGTH = 5;
    static constexpr uint8_t SIMON_STAGE_COUNT =
        static_cast<uint8_t>((SIMON_MAX_SEQUENCE_LENGTH - SIMON_INITIAL_SEQUENCE) + 1);

    static constexpr uint8_t BUTTON_PIN_RED = 2;
    static constexpr uint8_t BUTTON_PIN_GREEN = 3;
    static constexpr uint8_t BUTTON_PIN_BLUE = 4;
    static constexpr uint8_t BUTTON_PIN_YELLOW = 5;

    static constexpr uint8_t LED_PIN_RED = 6;
    static constexpr uint8_t LED_PIN_GREEN = 7;
    static constexpr uint8_t LED_PIN_BLUE = 8;
    static constexpr uint8_t LED_PIN_YELLOW = 9;

    static constexpr std::array<uint8_t, COLOR_COUNT> LED_PINS = {
        LED_PIN_RED,
        LED_PIN_BLUE,
        LED_PIN_GREEN,
        LED_PIN_YELLOW};

    static constexpr std::array<uint8_t, COLOR_COUNT> BUTTON_PINS = {
        BUTTON_PIN_RED,
        BUTTON_PIN_BLUE,
        BUTTON_PIN_GREEN,
        BUTTON_PIN_YELLOW};

    static constexpr std::array<SimonColor, COLOR_COUNT> COLOR_ORDER = {
        SimonColor::Red,
        SimonColor::Blue,
        SimonColor::Green,
        SimonColor::Yellow
    };

    static constexpr std::array<std::array<SimonColor, COLOR_COUNT>, 3> VOWEL_MAPPING = {{
        {SimonColor::Blue, SimonColor::Red, SimonColor::Yellow, SimonColor::Green},
        {SimonColor::Yellow, SimonColor::Green, SimonColor::Blue, SimonColor::Red},
        {SimonColor::Green, SimonColor::Red, SimonColor::Yellow, SimonColor::Blue},
    }};

    static constexpr std::array<std::array<SimonColor, COLOR_COUNT>, 3> CONSONANT_MAPPING = {{
        {SimonColor::Blue, SimonColor::Yellow, SimonColor::Green, SimonColor::Red},
        {SimonColor::Red, SimonColor::Blue, SimonColor::Yellow, SimonColor::Green},
        {SimonColor::Yellow, SimonColor::Green, SimonColor::Blue, SimonColor::Red},
    }};

    static constexpr std::array<CanAudioSound, COLOR_COUNT> DISPLAY_SOUNDS = {
        AUDIO_SIMON_RED,
        AUDIO_SIMON_BLUE,
        AUDIO_SIMON_GREEN,
        AUDIO_SIMON_YELLOW};

    static constexpr std::array<CanAudioSound, COLOR_COUNT> PRESS_SOUNDS = {
        AUDIO_BEEP_NORMAL,
        AUDIO_CORRECT_TIME,
        AUDIO_BEEP_HIGH,
        AUDIO_BEEP_FAST};

    // ---------------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------------
    static uint8_t colorToIndex(SimonColor color);
    static SimonColor indexToColor(uint8_t index);
    static bool serialNumberHasVowel(const String& serial);

    void initializeHardware();
    void generateFullSequence();
    void enterIdlePhase();
    void enterDisplayPhase();
    void enterInputPhase();
    void enterStrikeFlashPhase();
    void enterSolvedPhase();
    void handleDisplayPhase();
    void handleInputPhase();
    void handleStrikeFlashPhase();
    void handlePlayerFeedback();
    void processPlayerInput(SimonColor pressed);
    SimonColor pollButtonPress();
    void setAllLeds(bool on);
    void lightColor(SimonColor color, bool on);
    bool readButton(uint8_t index) const;
    void playDisplaySound(SimonColor color);
    void playPressSound(SimonColor color);
    void sendAudio(CanAudioSound sound);
    void registerStrike();
    void updateProgress();
    void resetButtonStates();
    void clearPlayerFeedback();
    void activatePlayerFeedback(SimonColor color, unsigned long now);
    void logEvent(const __FlashStringHelper* message) const;
    void logColorEvent(const __FlashStringHelper* prefix, SimonColor color) const;

    bool hardwareInitialized;
    bool gameRunning;
    bool solved;
    bool displayLedOn;
    bool playerFeedbackActive;
    SimonColor playerFeedbackColor;
    unsigned long playerFeedbackEndTime;

    String serialNumberValue;
    bool serialHasVowelFlag;
    uint8_t strikeCountValue;
    bool strikePending;

    std::array<SimonColor, SIMON_MAX_SEQUENCE_LENGTH> sequence;
    uint8_t stageLength;
    uint8_t inputIndex;
    uint8_t displayIndex;
    unsigned long lastPhaseTransition;
    unsigned long lastDisplayToggle;
    unsigned long lastButtonActivity;

    Phase currentPhase;
    uint8_t currentProgress;

    std::array<ButtonState, COLOR_COUNT> buttons;
};
