// Test file to verify integration of updated timer libraries
// This demonstrates the new API usage

#include <Arduino.h>
#include "lib/game_state/game_state_v2.h"
#include "lib/countdown/countdown.h"
#include "lib/strikes/strikes.h"
#include "lib/serial_command/serial_command.h"
#include "lib/debug/debug.h"

void testGameStateV2() {
    Serial.println("=== Testing Game State v2.0 ===");
    
    // Create game state manager
    GameStateManager gameState;
    
    // Test configuration
    GameConfig config;
    config.timeLimitMs = 180000;  // 3 minutes for testing
    config.maxStrikes = 3;
    config.enableStrikeAcceleration = true;
    gameState.setConfig(config);
    
    // Test initialization
    gameState.initialize();
    Serial.print("Initial state: ");
    Serial.println(static_cast<int>(gameState.getState()));
    Serial.print("Serial number: ");
    Serial.println(gameState.getSerialNumber());
    
    // Test module registration
    gameState.registerModule(0x10, ModuleType::WIRES);
    gameState.registerModule(0x11, ModuleType::BUTTON);
    gameState.registerModule(0x30, ModuleType::VENTING_GAS);
    
    Serial.print("Total modules: ");
    Serial.println(gameState.getTotalModules());
    Serial.print("Needy modules: ");
    Serial.println(gameState.getNeedyModules());
    
    // Test strikes
    gameState.addStrike();
    Serial.print("Strikes after add: ");
    Serial.println(gameState.getStrikes());
    
    // Test timer
    gameState.setState(GameState::RUNNING);
    gameState.startTimer();
    Serial.println("Timer started");
    
    // Test module solving
    gameState.setModuleSolved(0x10);
    Serial.print("Solved modules: ");
    Serial.print(gameState.getSolvedModules());
    Serial.print("/");
    Serial.println(gameState.getTotalModules());
    
    // Test edgework
    const Edgework& edgework = gameState.getEdgework();
    Serial.print("Batteries: ");
    Serial.println(edgework.batteryCount);
    Serial.print("Indicators: ");
    Serial.println(edgework.indicators.size());
    Serial.print("Ports: ");
    Serial.println(edgework.ports.size());
    
    Serial.println("Game State v2.0 test completed!\n");
}

void testUpdatedLibraries() {
    Serial.println("=== Testing Updated Libraries ===");
    
    GameStateManager gameState;
    gameState.initialize();
    
    // Test countdown display
    Serial.println("Testing countdown display...");
    updateCountdownDisplay(gameState);
    
    // Test strikes display  
    Serial.println("Testing strikes display...");
    updateStrikeCount(gameState);
    
    // Test serial commands
    Serial.println("Testing serial commands...");
    // handleSerialCommands(gameState); // Would need actual serial input
    
    // Test debug interface
    Serial.println("Testing debug interface...");
    // updateDebugInterface(gameState); // Would need hardware
    
    Serial.println("Updated libraries test completed!\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("KTANE Timer Libraries Integration Test");
    Serial.println("======================================");
    
    testGameStateV2();
    testUpdatedLibraries();
    
    Serial.println("All tests completed!");
    Serial.println("Ready for normal operation.");
}

void loop() {
    // Empty loop for test
    delay(1000);
} 