#include <Arduino.h>
#include <lcd1602.h>
#include "debug.h"

// --- Encoder Pins ---
#define ENCODER_SW 2
#define ENCODER_CLK 3
#define ENCODER_DT 4

// --- UI States ---
enum DebugScreen
{
    SCREEN_MENU,
    SCREEN_MODULE_LIST,
    SCREEN_EDGEWORK,
    SCREEN_LIVE_VIEW
};

static DebugScreen screenState = SCREEN_MENU;
static uint8_t menuIndex = 0;
static uint8_t submenuIndex = 0;

static int lastEncoderState = HIGH;
static bool lastButtonState = HIGH;
static unsigned long lastDebounceTime = 0;
static bool buttonPressed = false;
static bool longPressDetected = false;
static unsigned long buttonHoldStart = 0;

const char *menuOptions[] = {
    "Start Game",
    "Pause Timer",
    "Reset Game",
    "Add Strike",
    "Clear Strikes",
    "Solve Module",
    "Reset Timer",
    "Show Serial",
    "View Modules",
    "View Edgework",
    "Live View"};

const int menuCount = sizeof(menuOptions) / sizeof(menuOptions[0]);

// --- Encoder Input ---
void handleEncoder()
{
    int clkState = digitalRead(ENCODER_CLK);
    if (clkState != lastEncoderState && clkState == LOW)
    {
        if (digitalRead(ENCODER_DT) != clkState)
        {
            if (screenState == SCREEN_MENU)
                menuIndex = (menuIndex + 1) % menuCount;
            else
                submenuIndex++;
        }
        else
        {
            if (screenState == SCREEN_MENU)
                menuIndex = (menuIndex - 1 + menuCount) % menuCount;
            else
                submenuIndex--;
        }
    }
    lastEncoderState = clkState;
}

void handleButton()
{
    bool reading = digitalRead(ENCODER_SW);
    if (reading == LOW)
    {
        if (lastButtonState == HIGH)
        {
            buttonHoldStart = millis();
        }
        else if (!longPressDetected && millis() - buttonHoldStart > 1500)
        {
            longPressDetected = true;
        }
    }
    else if (lastButtonState == LOW)
    {
        if (longPressDetected)
        {
            screenState = SCREEN_MENU;
            menuIndex = 0;
            submenuIndex = 0;
            longPressDetected = false;
        }
        else
        {
            buttonPressed = true;
        }
        longPressDetected = false;
    }
    lastButtonState = reading;
}

// --- Menu Actions ---
void performMenuAction(uint8_t index, GameStateManager& gameState)
{
    switch (index)
    {
    case 0: // Start Game
        gameState.setState(GameState::RUNNING);
        gameState.startTimer();
        break;
    case 1: // Pause Timer
        gameState.pauseTimer();
        break;
    case 2: // Reset Game
        gameState.reset();
        break;
    case 3: // Add Strike
        gameState.addStrike();
        break;
    case 4: // Clear Strikes
        gameState.clearStrikes();
        break;
    case 5: // Solve Module
        for (uint8_t i = 0; i < gameState.getTotalModules(); ++i)
        {
            // Find first unsolved module and solve it
            // This is a simplified implementation
            break;
        }
        break;
    case 6: // Reset Timer
        gameState.resetTimer();
        break;
    case 7: // Show Serial
        lcd1602Clear();
        lcd1602PrintLine(0, "Serial:");
        lcd1602PrintLine(1, gameState.getSerialNumber());
        delay(1500);
        break;
    case 8: // View Modules
        screenState = SCREEN_MODULE_LIST;
        submenuIndex = 0;
        break;
    case 9: // View Edgework
        screenState = SCREEN_EDGEWORK;
        submenuIndex = 0;
        break;
    case 10: // Live View
        screenState = SCREEN_LIVE_VIEW;
        break;
    }
}

// --- UI Rendering ---
void drawMenu(GameStateManager& gameState)
{
    lcd1602Clear();
    String label = String(menuOptions[menuIndex]);
    lcd1602PrintLine(0, "> " + label.substring(0, min(15, label.length())));
    lcd1602PrintLine(1, "S:" + String(gameState.getStrikes()) +
                            " T:" + String(gameState.getRemainingTime() / 1000) + "s");
}

void drawModuleList(GameStateManager& gameState)
{
    uint8_t total = gameState.getTotalModules();
    if (total == 0)
    {
        lcd1602PrintLine(0, "No modules");
        lcd1602PrintLine(1, " ");
        return;
    }
    submenuIndex %= total;

    // This is simplified - would need access to module details
    lcd1602Clear();
    lcd1602PrintLine(0, "Mod #" + String(submenuIndex));
    lcd1602PrintLine(1, "Count: " + String(total));
}

void drawEdgeworkView(GameStateManager& gameState)
{
    const Edgework &edge = gameState.getEdgework();
    uint8_t total = 1 + edge.indicators.size() + edge.ports.size();
    
    if (total == 0) {
        lcd1602PrintLine(0, "No edgework");
        lcd1602PrintLine(1, " ");
        return;
    }
    
    submenuIndex %= total;

    lcd1602Clear();
    if (submenuIndex == 0)
    {
        lcd1602PrintLine(0, "Batteries:");
        lcd1602PrintLine(1, String(edge.batteryCount));
    }
    else if (submenuIndex - 1 < edge.indicators.size())
    {
        uint8_t indIndex = submenuIndex - 1;
        lcd1602PrintLine(0, "IND:");
        lcd1602PrintLine(1, edge.indicators[indIndex].label + 
                             (edge.indicators[indIndex].lit ? " LIT" : " OFF"));
    }
    else
    {
        uint8_t portIndex = submenuIndex - 1 - edge.indicators.size();
        if (portIndex < edge.ports.size())
        {
            lcd1602PrintLine(0, "PORT:");
            lcd1602PrintLine(1, edge.ports[portIndex].label);
        }
    }
}

void drawLiveView(GameStateManager& gameState)
{
    lcd1602Clear();
    
    // Show state
    String stateStr;
    switch (gameState.getState()) {
        case GameState::IDLE: stateStr = "IDLE"; break;
        case GameState::RUNNING: stateStr = "RUN"; break;
        case GameState::PAUSED: stateStr = "PAUSE"; break;
        case GameState::EXPLODED: stateStr = "BOOM"; break;
        case GameState::DEFUSED: stateStr = "SAFE"; break;
        case GameState::VICTORY: stateStr = "WIN"; break;
        default: stateStr = "???"; break;
    }
    
    lcd1602PrintLine(0, stateStr + " T:" + String(gameState.getRemainingTime() / 1000) + 
                         "s S:" + String(gameState.getStrikes()));
    lcd1602PrintLine(1, "M:" + String(gameState.getSolvedModules()) + 
                         "/" + String(gameState.getTotalModules()) + 
                         " " + gameState.getSerialNumber());
}

void updateDebugInterface(GameStateManager& gameState)
{
    handleEncoder();
    handleButton();

    if (buttonPressed)
    {
        if (screenState == SCREEN_MENU)
        {
            performMenuAction(menuIndex, gameState);
        }
        else
        {
            screenState = SCREEN_MENU;
        }
        buttonPressed = false;
    }

    // Handle long press for hard reset
    if (longPressDetected && lastButtonState == LOW)
    {
        screenState = SCREEN_MENU;
        menuIndex = 0;
        submenuIndex = 0;

        gameState.reset();
        lcd1602Clear();
        lcd1602PrintLine(0, "HARD RESET");
        delay(1500);
        longPressDetected = false;
    }

    static unsigned long lastDraw = 0;
    if (millis() - lastDraw > 200)
    {
        switch (screenState)
        {
        case SCREEN_MENU:
            drawMenu(gameState);
            break;
        case SCREEN_MODULE_LIST:
            drawModuleList(gameState);
            break;
        case SCREEN_EDGEWORK:
            drawEdgeworkView(gameState);
            break;
        case SCREEN_LIVE_VIEW:
            drawLiveView(gameState);
            break;
        }
        lastDraw = millis();
    }
}

void initDebugInterface()
{
    pinMode(ENCODER_SW, INPUT_PULLUP);
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    
    screenState = SCREEN_MENU;
    menuIndex = 0;
    submenuIndex = 0;
}
