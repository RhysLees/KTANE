#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <module_state.h>

void setup() {
    Serial.begin(115200); // USB Serial
    Serial1.begin(115200); // UART Serial
    delay(5000);
    randomSeed(rp2040.hwrand32());
    
    // Initialize CAN bus and negotiate unique ID
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    registerCanCallback(onCanMessage);
    assignUniqueId(CAN_TYPE_SIMON);
    
    // Initialize module_state with status LED on pin 11
    initModuleState();
}

void loop() {
    handleCanMessages();
    handleSerialCommands();
    updateModuleState();
} 