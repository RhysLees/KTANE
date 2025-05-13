#include <Arduino.h>
#include <lcd1602.h>
#include <game_state.h>
#include "debug.h"

// --- Encoder Pins ---
#define ENCODER_SW 2
#define ENCODER_CLK 3
#define ENCODER_DT 4

extern GameStateManager gameState;

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

            gameState.resetTimer();
            gameState.setStrikes(0);
            lcd1602Clear();
            lcd1602PrintLine(0, "HARD RESET");
            delay(1500);
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
void performMenuAction(uint8_t index)
{
    switch (index)
    {
    case 0:
        gameState.setState(GAME_RUNNING);
        gameState.startTimer();
        break;
    case 1:
        gameState.stopTimer();
        break;
    case 2:
        gameState.resetTimer();
        gameState.setStrikes(0);
        break;
    case 3:
        gameState.incrementStrikes();
        break;
    case 4:
        gameState.setStrikes(0);
        break;
    case 5:
        for (uint8_t i = 0; i < gameState.getTotalModules(); ++i)
        {
            if (!gameState.isModuleSolved(i))
            {
                gameState.setModuleSolved(i);
                break;
            }
        }
        break;
    case 6:
        gameState.resetTimer();
        break;
    case 7:
        lcd1602Clear();
        lcd1602PrintLine(0, "Serial:");
        lcd1602PrintLine(1, gameState.getSerial());
        delay(1500);
        break;
    case 8:
        screenState = SCREEN_MODULE_LIST;
        submenuIndex = 0;
        break;
    case 9:
        screenState = SCREEN_EDGEWORK;
        submenuIndex = 0;
        break;
    case 10:
        screenState = SCREEN_LIVE_VIEW;
        break;
    }
}

// --- UI Rendering ---
void drawMenu()
{
    lcd1602Clear();
    String label = String(menuOptions[menuIndex]);
    lcd1602PrintLine(0, "> " + label.substring(0, min(15, label.length())));
    lcd1602PrintLine(1, "S:" + String(gameState.getStrikes()) +
                            " T:" + String(gameState.getRemainingMillis() / 1000) + "s");
}

void drawModuleList()
{
    uint8_t total = gameState.getTotalModules();
    if (total == 0)
    {
        lcd1602PrintLine(0, "No modules");
        lcd1602PrintLine(1, " ");
        return;
    }
    submenuIndex %= total;

    const ModuleState &mod = gameState.getModule(submenuIndex);
    lcd1602Clear();
    lcd1602PrintLine(0, "Mod #" + String(submenuIndex));
    lcd1602PrintLine(1, "Type: 0x" + String(mod.type, HEX) + (mod.isSolved ? " OK" : " ---"));
}

void drawEdgeworkView()
{
    const Edgework &edge = gameState.getEdgework();
    uint8_t total = 1 + edge.indicators.size() + edge.ports.size();
    submenuIndex %= total;

    lcd1602Clear();
    if (submenuIndex == 0)
    {
        lcd1602PrintLine(0, "Batteries:");
        lcd1602PrintLine(1, String(edge.batteryCount));
    }
    else if (submenuIndex - 1 < edge.indicators.size())
    {
        lcd1602PrintLine(0, "IND:");
        lcd1602PrintLine(1, edge.indicators[submenuIndex - 1]);
    }
    else
    {
        uint8_t portIndex = submenuIndex - 1 - edge.indicators.size();
        if (portIndex < edge.ports.size())
        {
            lcd1602PrintLine(0, "PORT:");
            lcd1602PrintLine(1, edge.ports[portIndex]);
        }
    }
}

void drawLiveView()
{
    lcd1602Clear();
    lcd1602PrintLine(0, "T:" + String(gameState.getRemainingMillis() / 1000) + "s  S:" + String(gameState.getStrikes()));
    lcd1602PrintLine(1, "Mods " + String(gameState.getSolvedModules()) + "/" + String(gameState.getTotalModules()));
}

void updateDebugInterface()
{
    handleEncoder();
    handleButton();

    if (buttonPressed)
    {
        if (screenState == SCREEN_MENU)
        {
            performMenuAction(menuIndex);
        }
        else
        {
            screenState = SCREEN_MENU;
        }
        buttonPressed = false;
    }

    static unsigned long lastDraw = 0;
    if (millis() - lastDraw > 200)
    {
        switch (screenState)
        {
        case SCREEN_MENU:
            drawMenu();
            break;
        case SCREEN_MODULE_LIST:
            drawModuleList();
            break;
        case SCREEN_EDGEWORK:
            drawEdgeworkView();
            break;
        case SCREEN_LIVE_VIEW:
            drawLiveView();
            break;
        }
        lastDraw = millis();
    }
}

void initDebugInterface()
{
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);

    initLcd1602(16, 2, Wire1);
    lcd1602SetColor(LCD_COLOR_GREEN);

    drawMenu();
}
