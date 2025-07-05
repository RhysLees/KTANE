#include <Arduino.h>
#include <lcd1602.h>
#include "debug.h"

// --- Encoder Pins ---
#define ENCODER_SW 2
#define ENCODER_CLK 3
#define ENCODER_DT 4

// --- Simple UI States ---
enum DebugMode
{
    MODE_DASHBOARD,     // Always-on info display
    MODE_MENU,          // Simple action menu
    MODE_DISCOVERY,     // Module discovery mode (primary mode)
    MODE_MODULE_DETECT, // Module detection mode
    MODE_EDGEWORK,      // Edgework viewer
    MODE_GAME_CONTROL   // Game control actions
};

static DebugMode currentMode = MODE_DISCOVERY;
static uint8_t menuIndex = 0;
static uint8_t viewIndex = 0;
static bool needsRefresh = true;

// Input handling
static int lastEncoderState = HIGH;
static bool lastButtonState = HIGH;
static unsigned long lastDebounceTime = 0;
static unsigned long buttonHoldStart = 0;
static bool longPressDetected = false;

// Module detection
static unsigned long lastModuleDetectTime = 0;
static uint8_t lastModuleCount = 0;
static bool moduleDetectActive = false;

// Simple menu options  
const char *menuOptions[] = {
    "Game Control",
    "Discovery Mode",
    "Module Detect", 
    "View Edgework",
    "Dashboard"
};
const int menuCount = sizeof(menuOptions) / sizeof(menuOptions[0]);

const char *gameControlOptions[] = {
    "Start Game",
    "Pause/Resume",
    "Reset Game",
    "Add Strike",
    "Clear Strikes",
    "Solve Module",
    "Back to Menu"
};
const int gameControlCount = sizeof(gameControlOptions) / sizeof(gameControlOptions[0]);

// --- Status Display Functions ---
void displayStatusInfo(GameStateManager& gameState)
{
    // Always show key info on line 2
    String statusLine = "";
    
    // Time
    unsigned long timeMs = gameState.getRemainingTime();
    int minutes = timeMs / 60000;
    int seconds = (timeMs % 60000) / 1000;
    statusLine += String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
    
    // Strikes
    statusLine += " S:" + String(gameState.getStrikes()) + "/" + String(gameState.getMaxStrikes());
    
    // Modules
    statusLine += " M:" + String(gameState.getSolvedModules()) + "/" + String(gameState.getTotalModules());
    
    lcd1602PrintLine(1, statusLine);
}

void displayGameState(GameStateManager& gameState)
{
    String stateLine = "";
    
    // Set color and text based on game state
    switch (gameState.getState()) {
        case GameState::DISCOVERY:
            lcd1602SetColor(LCD_COLOR_BLUE);
            stateLine = "DISCOVERY MODE";
            break;
        case GameState::IDLE:
            lcd1602SetColor(LCD_COLOR_GREEN);
            stateLine = "READY - Start";
            break;
        case GameState::RUNNING:
            // Color based on strikes
            if (gameState.getStrikes() == 0) {
                lcd1602SetColor(LCD_COLOR_GREEN);
            } else if (gameState.getStrikes() == 1) {
                lcd1602SetColor(LCD_COLOR_ORANGE);
            } else {
                lcd1602SetColor(LCD_COLOR_RED);
            }
            stateLine = "GAME RUNNING";
            break;
        case GameState::PAUSED:
            lcd1602SetColor(LCD_COLOR_ORANGE);
            stateLine = "PAUSED";
            break;
        case GameState::EXPLODED:
            lcd1602SetColor(LCD_COLOR_RED);
            stateLine = "EXPLODED!";
            break;
        case GameState::DEFUSED:
            lcd1602SetColor(LCD_COLOR_GREEN);
            stateLine = "BOMB DEFUSED!";
            break;
        case GameState::VICTORY:
            lcd1602SetColor(LCD_COLOR_CYAN);
            stateLine = "VICTORY!";
            break;
        default:
            lcd1602SetColor(LCD_COLOR_BLUE);
            stateLine = "Unknown State";
            break;
    }
    lcd1602PrintLine(0, stateLine);
}

// --- Input Handling ---
void handleEncoder()
{
    int clkState = digitalRead(ENCODER_CLK);
    if (clkState != lastEncoderState && clkState == LOW)
    {
        if (digitalRead(ENCODER_DT) != clkState)
        {
            // Clockwise
            switch (currentMode) {
                case MODE_MENU:
                    menuIndex = (menuIndex + 1) % menuCount;
                    break;
                case MODE_GAME_CONTROL:
                    menuIndex = (menuIndex + 1) % gameControlCount;
                    break;
                case MODE_MODULE_DETECT:
                case MODE_EDGEWORK:
                    viewIndex++;
                    break;
                default:
                    break;
            }
        }
        else
        {
            // Counter-clockwise
            switch (currentMode) {
                case MODE_MENU:
                    menuIndex = (menuIndex - 1 + menuCount) % menuCount;
                    break;
                case MODE_GAME_CONTROL:
                    menuIndex = (menuIndex - 1 + gameControlCount) % gameControlCount;
                    break;
                case MODE_MODULE_DETECT:
                case MODE_EDGEWORK:
                    if (viewIndex > 0) viewIndex--;
                    break;
                default:
                    break;
            }
        }
        needsRefresh = true;
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
            // Long press always goes to menu
            if (currentMode != MODE_MENU) {
                currentMode = MODE_MENU;
                menuIndex = 0;
                needsRefresh = true;
            }
        }
    }
    else if (lastButtonState == LOW)
    {
        if (!longPressDetected)
        {
            // Short press - select current option
            needsRefresh = true;
            switch (currentMode) {
                case MODE_DASHBOARD:
                    currentMode = MODE_MENU;
                    menuIndex = 0;
                    break;
                case MODE_MENU:
                    // Menu selection handled in performMenuAction
                    break;
                case MODE_GAME_CONTROL:
                    // Game control handled in performGameControlAction
                    break;
                default:
                    currentMode = MODE_DASHBOARD;
                    break;
            }
        }
        longPressDetected = false;
    }
    lastButtonState = reading;
}

// --- Action Handlers ---
void performMenuAction(uint8_t index, GameStateManager& gameState)
{
    switch (index) {
        case 0: // Game Control
            currentMode = MODE_GAME_CONTROL;
            menuIndex = 0;
            break;
        case 1: // Discovery Mode
            if (gameState.getState() == GameState::IDLE || gameState.getState() == GameState::EXPLODED || 
                gameState.getState() == GameState::DEFUSED || gameState.getState() == GameState::VICTORY) {
                gameState.enterDiscoveryMode();
                currentMode = MODE_DISCOVERY;
                viewIndex = 0;
            }
            break;
        case 2: // Module Detect
            currentMode = MODE_MODULE_DETECT;
            viewIndex = 0;
            moduleDetectActive = true;
            lastModuleDetectTime = millis();
            break;
        case 3: // View Edgework
            currentMode = MODE_EDGEWORK;
            viewIndex = 0;
            break;
        case 4: // Dashboard
            currentMode = MODE_DASHBOARD;
            break;
    }
    needsRefresh = true;
}

void performGameControlAction(uint8_t index, GameStateManager& gameState)
{
    switch (index) {
        case 0: // Start Game
            if (gameState.getState() == GameState::IDLE) {
                gameState.startGame();
            }
            break;
        case 1: // Pause/Resume
            if (gameState.getState() == GameState::RUNNING) {
                gameState.pauseTimer();
            } else if (gameState.getState() == GameState::PAUSED) {
                gameState.resumeTimer();
            }
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
            // Find first unsolved module and solve it
            for (uint8_t i = 0x10; i <= 0x6F; i++) {
                if (gameState.getModule(i) && !gameState.isModuleSolved(i)) {
                    gameState.setModuleSolved(i);
                    break;
                }
            }
            break;
        case 6: // Back to Menu
            currentMode = MODE_MENU;
            menuIndex = 0;
            break;
    }
    needsRefresh = true;
}

// --- Display Functions ---
void drawDashboard(GameStateManager& gameState)
{
    lcd1602Clear();
    displayGameState(gameState);
    displayStatusInfo(gameState);
}

void drawMenu(GameStateManager& gameState)
{
    lcd1602Clear();
    lcd1602PrintLine(0, "> " + String(menuOptions[menuIndex]));
    displayStatusInfo(gameState);
}

void drawGameControl(GameStateManager& gameState)
{
    lcd1602Clear();
    lcd1602PrintLine(0, "> " + String(gameControlOptions[menuIndex]));
    displayStatusInfo(gameState);
}

void drawDiscovery(GameStateManager& gameState)
{
    lcd1602Clear();
    
    // Set color for discovery mode
    lcd1602SetColor(LCD_COLOR_BLUE);
    
    // Show different information based on view index
    uint8_t totalModules = gameState.getTotalModules();
    unsigned long discoveryTime = gameState.getDiscoveryDuration();
    
    // Cycle through different views every 3 seconds or when encoder is turned
    static unsigned long lastAutoSwitch = 0;
    static uint8_t autoViewIndex = 0;
    
    if (millis() - lastAutoSwitch > 3000) {
        autoViewIndex = (autoViewIndex + 1) % 4;
        lastAutoSwitch = millis();
    }
    
    // Use manual view index if encoder was turned recently
    static unsigned long lastEncoderTime = 0;
    if (needsRefresh) {
        lastEncoderTime = millis();
    }
    
    uint8_t displayIndex = (millis() - lastEncoderTime < 5000) ? viewIndex % 4 : autoViewIndex;
    
    switch (displayIndex) {
        case 0: // Serial number and discovery status
            lcd1602PrintLine(0, "SN: " + gameState.getSerialNumber());
            lcd1602PrintLine(1, "DISCOVERY " + String(discoveryTime / 1000) + "s");
            break;
            
        case 1: // Module count
            lcd1602PrintLine(0, "Modules: " + String(totalModules));
            if (totalModules == 0) {
                lcd1602PrintLine(1, "Power on modules");
            } else {
                lcd1602PrintLine(1, "Press to continue");
            }
            break;
            
        case 2: // Edgework - Indicators
            {
                const Edgework &edge = gameState.getEdgework();
                lcd1602PrintLine(0, "Indicators: " + String(edge.indicators.size()));
                if (edge.indicators.size() > 0) {
                    String indLine = "";
                    for (uint8_t i = 0; i < min(3, (int)edge.indicators.size()); i++) {
                        if (i > 0) indLine += " ";
                        indLine += edge.indicators[i].label;
                        if (edge.indicators[i].lit) indLine += "*";
                    }
                    lcd1602PrintLine(1, indLine);
                } else {
                    lcd1602PrintLine(1, "None");
                }
            }
            break;
            
        case 3: // Edgework - Ports and Batteries
            {
                const Edgework &edge = gameState.getEdgework();
                lcd1602PrintLine(0, "Batt:" + String(edge.batteryCount) + " Ports:" + String(edge.ports.size()));
                if (edge.ports.size() > 0) {
                    String portLine = "";
                    for (uint8_t i = 0; i < min(2, (int)edge.ports.size()); i++) {
                        if (i > 0) portLine += " ";
                        portLine += edge.ports[i].label;
                    }
                    lcd1602PrintLine(1, portLine);
                } else {
                    lcd1602PrintLine(1, "No ports");
                }
            }
            break;
    }
}

void drawModuleDetect(GameStateManager& gameState)
{
    lcd1602Clear();
    
    // Show module detection info
    uint8_t totalModules = gameState.getTotalModules();
    uint8_t solvedModules = gameState.getSolvedModules();
    bool inDiscoveryMode = gameState.isInDiscoveryMode();
    
    // Check if modules changed
    if (totalModules != lastModuleCount) {
        lastModuleCount = totalModules;
        lastModuleDetectTime = millis();
    }
    
    // Show discovery status if in discovery mode
    if (inDiscoveryMode) {
        lcd1602PrintLine(0, "Discovery mode");
        if (totalModules == 0) {
            lcd1602PrintLine(1, "Awaiting modules");
        } else {
            lcd1602PrintLine(1, "Found:" + String(totalModules) + " Waiting");
        }
        return;
    }
    
    // System is ready - show normal detection status
    if (totalModules == 0) {
        lcd1602PrintLine(0, "No modules found");
        lcd1602PrintLine(1, "Type REDISCOVER");
        return;
    }
    
    // Show detection status
    if (millis() - lastModuleDetectTime < 2000) {
        lcd1602PrintLine(0, "Module detected!");
        lcd1602PrintLine(1, "Tot:" + String(totalModules) + " Sol:" + String(solvedModules));
    } else {
        lcd1602PrintLine(0, "Modules: " + String(totalModules));
        lcd1602PrintLine(1, "Solved:" + String(solvedModules) + " Left:" + String(totalModules - solvedModules));
    }
}

void drawEdgework(GameStateManager& gameState)
{
    lcd1602Clear();
    
    const Edgework &edge = gameState.getEdgework();
    uint8_t totalItems = 1 + edge.indicators.size() + edge.ports.size();
    
    if (totalItems == 0) {
        lcd1602PrintLine(0, "No edgework");
        lcd1602PrintLine(1, "Generated");
        return;
    }
    
    viewIndex %= totalItems;
    
    if (viewIndex == 0) {
        lcd1602PrintLine(0, "Batteries: " + String(edge.batteryCount));
        lcd1602PrintLine(1, "Items: " + String(totalItems));
    } else if (viewIndex - 1 < edge.indicators.size()) {
        uint8_t indIndex = viewIndex - 1;
        lcd1602PrintLine(0, "IND: " + edge.indicators[indIndex].label);
        lcd1602PrintLine(1, edge.indicators[indIndex].lit ? "LIT" : "UNLIT");
    } else {
        uint8_t portIndex = viewIndex - 1 - edge.indicators.size();
        if (portIndex < edge.ports.size()) {
            lcd1602PrintLine(0, "PORT:");
            lcd1602PrintLine(1, edge.ports[portIndex].label);
        }
    }
}

// --- Main Interface Functions ---
void updateDebugInterface(GameStateManager& gameState)
{
    handleEncoder();
    handleButton();
    
    // Handle button press actions
    static bool lastButtonPressed = false;
    if (needsRefresh || (!lastButtonPressed && !digitalRead(ENCODER_SW))) {
        if (!lastButtonPressed && !digitalRead(ENCODER_SW)) {
            // Button just pressed
            switch (currentMode) {
                case MODE_MENU:
                    performMenuAction(menuIndex, gameState);
                    break;
                case MODE_GAME_CONTROL:
                    performGameControlAction(menuIndex, gameState);
                    break;
                case MODE_DISCOVERY:
                    // Exit discovery mode and create new game
                    gameState.exitDiscoveryMode();
                    currentMode = MODE_DASHBOARD;
                    viewIndex = 0;
                    break;
                default:
                    break;
            }
        }
        
        // Update display
        switch (currentMode) {
            case MODE_DASHBOARD:
                drawDashboard(gameState);
                break;
            case MODE_MENU:
                drawMenu(gameState);
                break;
            case MODE_DISCOVERY:
                drawDiscovery(gameState);
                break;
            case MODE_GAME_CONTROL:
                drawGameControl(gameState);
                break;
            case MODE_MODULE_DETECT:
                drawModuleDetect(gameState);
                break;
            case MODE_EDGEWORK:
                drawEdgework(gameState);
                break;
        }
        
        needsRefresh = false;
    }
    lastButtonPressed = !digitalRead(ENCODER_SW);
    
    // Auto-refresh dashboard every 500ms
    static unsigned long lastAutoRefresh = 0;
    if (currentMode == MODE_DASHBOARD && millis() - lastAutoRefresh > 500) {
        needsRefresh = true;
        lastAutoRefresh = millis();
    }
    
    // Auto-refresh module detect every 1000ms
    if (currentMode == MODE_MODULE_DETECT && millis() - lastAutoRefresh > 1000) {
        needsRefresh = true;
        lastAutoRefresh = millis();
    }
    
    // Auto-refresh discovery mode every 1000ms
    if (currentMode == MODE_DISCOVERY && millis() - lastAutoRefresh > 1000) {
        needsRefresh = true;
        lastAutoRefresh = millis();
    }
}

void refreshDebugDisplay(GameStateManager& gameState)
{
    needsRefresh = true;
    updateDebugInterface(gameState);
}

void initDebugInterface()
{
    pinMode(ENCODER_SW, INPUT_PULLUP);
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    
    // LCD is already initialized in main.cpp
    // Start in discovery mode
    currentMode = MODE_DISCOVERY;
    needsRefresh = true;
    
    Serial.println("Debug interface initialized");
    Serial.println("Controls:");
    Serial.println("- Rotate: Navigate menus");
    Serial.println("- Short press: Select/Enter");
    Serial.println("- Long press: Main menu");
}
