// CAN Send Example
// Based on: https://github.com/coryjfowler/MCP_CAN_lib/blob/master/examples/CAN_send/CAN_send.ino

#include <mcp_can.h>
#include <SPI.h>

#define CAN0_INT 20                              // Set INT to pin 20
#define CAN0_CS 17                               // Set CS to pin 17

MCP_CAN CAN0(CAN0_CS);                           // Set CS to pin 17

// Function to check and display error status
void checkErrorStatus() {
  byte error = CAN0.getError();
  Serial.print("Error Status: 0x");
  Serial.print(error, HEX);
  
  if(error == 0) {
    Serial.println(" (No errors)");
  } else {
    Serial.print(" (Error detected: ");
    if(error & 0x01) Serial.print("TXWAR ");
    if(error & 0x02) Serial.print("RXWAR ");
    if(error & 0x04) Serial.print("TXBO ");
    if(error & 0x08) Serial.print("RXBO ");
    if(error & 0x10) Serial.print("EPASS ");
    if(error & 0x20) Serial.print("EWARN ");
    Serial.println(")");
  }
  
  Serial.print("TX Error Count: ");
  Serial.println(CAN0.errorCountTX());
  Serial.print("RX Error Count: ");
  Serial.println(CAN0.errorCountRX());
}

// Function to reset the CAN controller and clear error states
void resetCANController() {
  Serial.println("=== Resetting CAN Controller ===");
  
  // Check error status before reset
  Serial.println("Error status before reset:");
  checkErrorStatus();
  Serial.println();
  
  // Method 1: Software reset via SPI command (0xC0)
  Serial.println("Sending software reset command (0xC0)...");
  digitalWrite(CAN0_CS, LOW);
  SPI.transfer(0xC0);  // RESET command
  digitalWrite(CAN0_CS, HIGH);
  delay(10);  // Wait for reset to complete
  
  // Method 2: Re-initialize the controller
  Serial.println("Re-initializing CAN controller...");
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN controller re-initialized successfully!");
  } else {
    Serial.println("ERROR: Re-initialization failed!");
    return;
  }
  
  // Method 3: Reset mode to clear error states
  Serial.println("Resetting mode to clear error states...");
  CAN0.setMode(MCP_NORMAL);
  delay(10);
  
  // Clear any pending interrupts by reading status
  Serial.println("Clearing interrupt status...");
  pinMode(CAN0_INT, INPUT_PULLUP);
  
  // Check error status after reset
  Serial.println("Error status after reset:");
  checkErrorStatus();
  
  Serial.println("CAN controller reset complete!");
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("==========================================");
  Serial.println("CAN Test Send Module");
  Serial.println("Based on MCP_CAN library example");
  Serial.println("==========================================");

  // Initialize SPI before initializing CAN controller
  SPI.begin();

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  // NOTE: Change MCP_8MHZ to MCP_16MHZ if your module uses a 16MHz crystal
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    while(1);  // Stop here if initialization failed
  }

  // Perform a full reset to clear any error states
  resetCANController();
  
  // Check interrupt pin configuration
  pinMode(CAN0_INT, INPUT);
  pinMode(CAN0_INT, INPUT_PULLUP);  // Enable pullup to ensure proper state
  
  Serial.println("CAN bus ready - sending test messages every 100ms");
  Serial.println();
  Serial.println("=== DIAGNOSTICS ===");
  Serial.print("Interrupt pin (");
  Serial.print(CAN0_INT);
  Serial.print(") state: ");
  Serial.println(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
  Serial.println("Expected: HIGH (active low interrupt)");
  Serial.println("Mode: NORMAL (set after initialization)");
  Serial.println();
  Serial.println("=== TROUBLESHOOTING GUIDE ===");
  Serial.println("If you see error frames or high error counts:");
  Serial.println("1. FIRST: Test in LOOPBACK mode (can_test_loopback)");
  Serial.println("   - If loopback works, MCP2515 is OK, issue is bus hardware");
  Serial.println("   - If loopback fails, MCP2515 or SPI connection has issues");
  Serial.println();
  Serial.println("2. Check J1 termination jumper:");
  Serial.println("   - Single node: ENABLE J1 (jumper ON)");
  Serial.println("   - Two nodes: ENABLE J1 on BOTH ends only");
  Serial.println();
  Serial.println("3. Verify CANH/CANL wiring:");
  Serial.println("   - CANH to CANH, CANL to CANL between modules");
  Serial.println("   - Both must be connected (even for single node)");
  Serial.println("   - Use a multimeter to verify continuity");
  Serial.println();
  Serial.println("4. Verify common ground:");
  Serial.println("   - Both modules MUST share the same GND");
  Serial.println("   - Check GND connection between modules");
  Serial.println();
  Serial.println("5. Check both modules are running:");
  Serial.println("   - Receiver module must be powered and running");
  Serial.println("   - Both modules must use same baudrate (500kbps)");
  Serial.println("   - Both modules must use same clock (8MHz or 16MHz)");
  Serial.println();
  Serial.println("6. Check MCP2551 transceiver:");
  Serial.println("   - VCC = 5V, GND connected on both modules");
  Serial.println("   - STB pin LOW/grounded (if present)");
  Serial.println();
  Serial.println("7. Press 'R' in Serial Monitor to manually reset");
  Serial.println("==========================================");
  Serial.println();
}

byte data[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

void loop()
{
  static unsigned long lastDiagnostic = 0;
  unsigned long now = millis();
  
  // Check for manual reset command via Serial
  if(Serial.available()) {
    char cmd = Serial.read();
    if(cmd == 'R' || cmd == 'r') {
      resetCANController();
    }
    // Clear any remaining characters
    while(Serial.available()) Serial.read();
  }
  
  // Print diagnostics every 2 seconds
  if(now - lastDiagnostic >= 2000) {
    lastDiagnostic = now;
    Serial.println("=== Diagnostic Info ===");
    Serial.print("INT pin state: ");
    Serial.print(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
    Serial.println(" | Mode: NORMAL");
    
    // Check for errors
    checkErrorStatus();
    Serial.println();
  }
  
  // Check error counts before sending - if too high, don't send
  byte rxErrors = CAN0.errorCountRX();
  byte txErrors = CAN0.errorCountTX();
  
  // Print error counts every time (for debugging)
  static unsigned long lastErrorPrint = 0;
  if(millis() - lastErrorPrint >= 1000) {
    lastErrorPrint = millis();
    Serial.print("TX Errors: ");
    Serial.print(txErrors);
    Serial.print(" | RX Errors: ");
    Serial.println(rxErrors);
  }
  
  if(rxErrors >= 96 || txErrors >= 96) {
    static unsigned long lastErrorWarning = 0;
    if(millis() - lastErrorWarning >= 5000) {  // Warn every 5 seconds
      lastErrorWarning = millis();
      Serial.println("!!! ERROR: Error counts too high - stopping transmission !!!");
      Serial.print("RX Errors: ");
      Serial.print(rxErrors);
      Serial.print(" | TX Errors: ");
      Serial.println(txErrors);
      Serial.println();
      Serial.println("=== DIAGNOSIS BASED ON YOUR PATTERN ===");
      Serial.println("Your pattern suggests:");
      Serial.println("- No jumpers: TX errors (no termination, signals reflect)");
      Serial.println("- Jumper on sender: RX errors (termination on one end only)");
      Serial.println("- Jumper on receiver: RX errors (termination on one end only)");
      Serial.println("- Both jumpers: RX errors (should work but doesn't)");
      Serial.println();
      Serial.println("POSSIBLE ISSUES:");
      Serial.println("1. CANH/CANL not connected between modules");
      Serial.println("2. Modules don't share common ground");
      Serial.println("3. Receiver module not powered or not running");
      Serial.println("4. Baud rate mismatch between modules");
      Serial.println("5. Clock frequency mismatch (8MHz vs 16MHz)");
      Serial.println();
      Serial.println("SOLUTION:");
      Serial.println("1. FIRST: Test loopback mode (can_test_loopback)");
      Serial.println("2. Verify CANH/CANL wires are connected between modules");
      Serial.println("3. Verify both modules share the same GND");
      Serial.println("4. Check both modules are powered (5V)");
      Serial.println("5. Verify both modules use same baudrate (500kbps)");
      Serial.println("6. Verify both modules use same clock (8MHz or 16MHz)");
      Serial.println();
    }
    delay(100);
    return;  // Don't attempt to send if errors are too high
  }
  
  // send data:  ID = 0x100, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(0x100, 0, 8, data);
  if(sndStat == CAN_OK){
    Serial.println("Message Sent Successfully!");
  } else {
    Serial.print("Error Sending Message... (code: ");
    Serial.print(sndStat);
    
    // Decode error codes
    Serial.print(" - ");
    switch(sndStat) {
      case 0: Serial.print("CAN_OK");
        break;
      case 1: Serial.print("CAN_SENDMSGTIMEOUT");
        break;
      case 2: Serial.print("CAN_FAILINIT");
        break;
      case 3: Serial.print("CAN_FAILTX");
        break;
      case 4: Serial.print("CAN_FAILTX");
        break;
      case 7: Serial.print("CAN_SENDMSGTIMEOUT (bus error)");
        break;
      default: Serial.print("UNKNOWN (code: ");
        Serial.print(sndStat);
        Serial.print(")");
        break;
    }
    Serial.println();
    
    // If send fails, check if we should reset
    if(sndStat == 1) {  // CAN_SENDMSGTIMEOUT
      Serial.println("ERROR: Transmission timeout - bus may have errors!");
      Serial.print("RX Error Count: ");
      Serial.println(rxErrors);
      Serial.println("Try loopback mode first to verify MCP2515 works!");
      Serial.println();
    }
    
    // Only reset if error count is reasonable (not too high)
    if(rxErrors < 128 && txErrors < 128) {
      Serial.println("Attempting full reset...");
      resetCANController();
    } else {
      Serial.println("ERROR: Error counts too high - reset required!");
      Serial.println("Press 'R' to manually reset");
    }
  }
  delay(100);   // send data per 100ms
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/

