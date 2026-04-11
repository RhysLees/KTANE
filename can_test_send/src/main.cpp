// CAN Send Example
// Based on: https://github.com/coryjfowler/MCP_CAN_lib/blob/master/examples/CAN_send/CAN_send.ino

#include <Arduino.h>
#include <ktane_console.h>
#include <mcp_can.h>
#include <SPI.h>

#define CAN0_INT 20                              // Set INT to pin 20
#define CAN0_CS 17                               // Set CS to pin 17

MCP_CAN CAN0(CAN0_CS);                           // Set CS to pin 17

// Function to check and display error status
void checkErrorStatus() {
  byte error = CAN0.getError();
  KTANE_CONSOLE_OUT.print("Error Status: 0x");
  KTANE_CONSOLE_OUT.print(error, HEX);
  
  if(error == 0) {
    KTANE_CONSOLE_OUT.println(" (No errors)");
  } else {
    KTANE_CONSOLE_OUT.print(" (Error detected: ");
    if(error & 0x01) KTANE_CONSOLE_OUT.print("TXWAR ");
    if(error & 0x02) KTANE_CONSOLE_OUT.print("RXWAR ");
    if(error & 0x04) KTANE_CONSOLE_OUT.print("TXBO ");
    if(error & 0x08) KTANE_CONSOLE_OUT.print("RXBO ");
    if(error & 0x10) KTANE_CONSOLE_OUT.print("EPASS ");
    if(error & 0x20) KTANE_CONSOLE_OUT.print("EWARN ");
    KTANE_CONSOLE_OUT.println(")");
  }
  
  KTANE_CONSOLE_OUT.print("TX Error Count: ");
  KTANE_CONSOLE_OUT.println(CAN0.errorCountTX());
  KTANE_CONSOLE_OUT.print("RX Error Count: ");
  KTANE_CONSOLE_OUT.println(CAN0.errorCountRX());
}

// Function to reset the CAN controller and clear error states
void resetCANController() {
  KTANE_CONSOLE_OUT.println("=== Resetting CAN Controller ===");
  
  // Check error status before reset
  KTANE_CONSOLE_OUT.println("Error status before reset:");
  checkErrorStatus();
  KTANE_CONSOLE_OUT.println();
  
  // Method 1: Software reset via SPI command (0xC0)
  KTANE_CONSOLE_OUT.println("Sending software reset command (0xC0)...");
  digitalWrite(CAN0_CS, LOW);
  SPI.transfer(0xC0);  // RESET command
  digitalWrite(CAN0_CS, HIGH);
  delay(10);  // Wait for reset to complete
  
  // Method 2: Re-initialize the controller
  KTANE_CONSOLE_OUT.println("Re-initializing CAN controller...");
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    KTANE_CONSOLE_OUT.println("CAN controller re-initialized successfully!");
  } else {
    KTANE_CONSOLE_OUT.println("ERROR: Re-initialization failed!");
    return;
  }
  
  // Method 3: Reset mode to clear error states
  KTANE_CONSOLE_OUT.println("Resetting mode to clear error states...");
  CAN0.setMode(MCP_NORMAL);
  delay(10);
  
  // Clear any pending interrupts by reading status
  KTANE_CONSOLE_OUT.println("Clearing interrupt status...");
  pinMode(CAN0_INT, INPUT_PULLUP);
  
  // Check error status after reset
  KTANE_CONSOLE_OUT.println("Error status after reset:");
  checkErrorStatus();
  
  KTANE_CONSOLE_OUT.println("CAN controller reset complete!");
  KTANE_CONSOLE_OUT.println();
}

void setup()
{
  ktaneConsoleInit(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println("CAN Test Send Module");
  KTANE_CONSOLE_OUT.println("Based on MCP_CAN library example");
  KTANE_CONSOLE_OUT.println("==========================================");

  // Initialize SPI before initializing CAN controller
  SPI.begin();

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  // NOTE: Change MCP_8MHZ to MCP_16MHZ if your module uses a 16MHz crystal
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    KTANE_CONSOLE_OUT.println("MCP2515 Initialized Successfully!");
  } else {
    KTANE_CONSOLE_OUT.println("Error Initializing MCP2515...");
    while(1);  // Stop here if initialization failed
  }

  // Perform a full reset to clear any error states
  resetCANController();
  
  // Check interrupt pin configuration
  pinMode(CAN0_INT, INPUT);
  pinMode(CAN0_INT, INPUT_PULLUP);  // Enable pullup to ensure proper state
  
  KTANE_CONSOLE_OUT.println("CAN bus ready - sending test messages every 100ms");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("=== DIAGNOSTICS ===");
  KTANE_CONSOLE_OUT.print("Interrupt pin (");
  KTANE_CONSOLE_OUT.print(CAN0_INT);
  KTANE_CONSOLE_OUT.print(") state: ");
  KTANE_CONSOLE_OUT.println(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
  KTANE_CONSOLE_OUT.println("Expected: HIGH (active low interrupt)");
  KTANE_CONSOLE_OUT.println("Mode: NORMAL (set after initialization)");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("=== TROUBLESHOOTING GUIDE ===");
  KTANE_CONSOLE_OUT.println("If you see error frames or high error counts:");
  KTANE_CONSOLE_OUT.println("1. FIRST: Test in LOOPBACK mode (can_test_loopback)");
  KTANE_CONSOLE_OUT.println("   - If loopback works, MCP2515 is OK, issue is bus hardware");
  KTANE_CONSOLE_OUT.println("   - If loopback fails, MCP2515 or SPI connection has issues");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("2. Check J1 termination jumper:");
  KTANE_CONSOLE_OUT.println("   - Single node: ENABLE J1 (jumper ON)");
  KTANE_CONSOLE_OUT.println("   - Two nodes: ENABLE J1 on BOTH ends only");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("3. Verify CANH/CANL wiring:");
  KTANE_CONSOLE_OUT.println("   - CANH to CANH, CANL to CANL between modules");
  KTANE_CONSOLE_OUT.println("   - Both must be connected (even for single node)");
  KTANE_CONSOLE_OUT.println("   - Use a multimeter to verify continuity");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("4. Verify common ground:");
  KTANE_CONSOLE_OUT.println("   - Both modules MUST share the same GND");
  KTANE_CONSOLE_OUT.println("   - Check GND connection between modules");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("5. Check both modules are running:");
  KTANE_CONSOLE_OUT.println("   - Receiver module must be powered and running");
  KTANE_CONSOLE_OUT.println("   - Both modules must use same baudrate (500kbps)");
  KTANE_CONSOLE_OUT.println("   - Both modules must use same clock (8MHz or 16MHz)");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("6. Check MCP2551 transceiver:");
  KTANE_CONSOLE_OUT.println("   - VCC = 5V, GND connected on both modules");
  KTANE_CONSOLE_OUT.println("   - STB pin LOW/grounded (if present)");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("7. Press 'R' in Serial Monitor to manually reset");
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println();
}

byte data[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

void loop()
{
  static unsigned long lastDiagnostic = 0;
  unsigned long now = millis();
  
  // Check for manual reset command via Serial
  if(KTANE_CONSOLE_OUT.available()) {
    char cmd = KTANE_CONSOLE_OUT.read();
    if(cmd == 'R' || cmd == 'r') {
      resetCANController();
    }
    // Clear any remaining characters
    while(KTANE_CONSOLE_OUT.available()) KTANE_CONSOLE_OUT.read();
  }
  
  // Print diagnostics every 2 seconds
  if(now - lastDiagnostic >= 2000) {
    lastDiagnostic = now;
    KTANE_CONSOLE_OUT.println("=== Diagnostic Info ===");
    KTANE_CONSOLE_OUT.print("INT pin state: ");
    KTANE_CONSOLE_OUT.print(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
    KTANE_CONSOLE_OUT.println(" | Mode: NORMAL");
    
    // Check for errors
    checkErrorStatus();
    KTANE_CONSOLE_OUT.println();
  }
  
  // Check error counts before sending - if too high, don't send
  byte rxErrors = CAN0.errorCountRX();
  byte txErrors = CAN0.errorCountTX();
  
  // Print error counts every time (for debugging)
  static unsigned long lastErrorPrint = 0;
  if(millis() - lastErrorPrint >= 1000) {
    lastErrorPrint = millis();
    KTANE_CONSOLE_OUT.print("TX Errors: ");
    KTANE_CONSOLE_OUT.print(txErrors);
    KTANE_CONSOLE_OUT.print(" | RX Errors: ");
    KTANE_CONSOLE_OUT.println(rxErrors);
  }
  
  if(rxErrors >= 96 || txErrors >= 96) {
    static unsigned long lastErrorWarning = 0;
    if(millis() - lastErrorWarning >= 5000) {  // Warn every 5 seconds
      lastErrorWarning = millis();
      KTANE_CONSOLE_OUT.println("!!! ERROR: Error counts too high - stopping transmission !!!");
      KTANE_CONSOLE_OUT.print("RX Errors: ");
      KTANE_CONSOLE_OUT.print(rxErrors);
      KTANE_CONSOLE_OUT.print(" | TX Errors: ");
      KTANE_CONSOLE_OUT.println(txErrors);
      KTANE_CONSOLE_OUT.println();
      KTANE_CONSOLE_OUT.println("=== DIAGNOSIS BASED ON YOUR PATTERN ===");
      KTANE_CONSOLE_OUT.println("Your pattern suggests:");
      KTANE_CONSOLE_OUT.println("- No jumpers: TX errors (no termination, signals reflect)");
      KTANE_CONSOLE_OUT.println("- Jumper on sender: RX errors (termination on one end only)");
      KTANE_CONSOLE_OUT.println("- Jumper on receiver: RX errors (termination on one end only)");
      KTANE_CONSOLE_OUT.println("- Both jumpers: RX errors (should work but doesn't)");
      KTANE_CONSOLE_OUT.println();
      KTANE_CONSOLE_OUT.println("POSSIBLE ISSUES:");
      KTANE_CONSOLE_OUT.println("1. CANH/CANL not connected between modules");
      KTANE_CONSOLE_OUT.println("2. Modules don't share common ground");
      KTANE_CONSOLE_OUT.println("3. Receiver module not powered or not running");
      KTANE_CONSOLE_OUT.println("4. Baud rate mismatch between modules");
      KTANE_CONSOLE_OUT.println("5. Clock frequency mismatch (8MHz vs 16MHz)");
      KTANE_CONSOLE_OUT.println();
      KTANE_CONSOLE_OUT.println("SOLUTION:");
      KTANE_CONSOLE_OUT.println("1. FIRST: Test loopback mode (can_test_loopback)");
      KTANE_CONSOLE_OUT.println("2. Verify CANH/CANL wires are connected between modules");
      KTANE_CONSOLE_OUT.println("3. Verify both modules share the same GND");
      KTANE_CONSOLE_OUT.println("4. Check both modules are powered (5V)");
      KTANE_CONSOLE_OUT.println("5. Verify both modules use same baudrate (500kbps)");
      KTANE_CONSOLE_OUT.println("6. Verify both modules use same clock (8MHz or 16MHz)");
      KTANE_CONSOLE_OUT.println();
    }
    delay(100);
    return;  // Don't attempt to send if errors are too high
  }
  
  // send data:  ID = 0x100, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(0x100, 0, 8, data);
  if(sndStat == CAN_OK){
    KTANE_CONSOLE_OUT.println("Message Sent Successfully!");
  } else {
    KTANE_CONSOLE_OUT.print("Error Sending Message... (code: ");
    KTANE_CONSOLE_OUT.print(sndStat);
    
    // Decode error codes
    KTANE_CONSOLE_OUT.print(" - ");
    switch(sndStat) {
      case 0: KTANE_CONSOLE_OUT.print("CAN_OK");
        break;
      case 1: KTANE_CONSOLE_OUT.print("CAN_SENDMSGTIMEOUT");
        break;
      case 2: KTANE_CONSOLE_OUT.print("CAN_FAILINIT");
        break;
      case 3: KTANE_CONSOLE_OUT.print("CAN_FAILTX");
        break;
      case 4: KTANE_CONSOLE_OUT.print("CAN_FAILTX");
        break;
      case 7: KTANE_CONSOLE_OUT.print("CAN_SENDMSGTIMEOUT (bus error)");
        break;
      default: KTANE_CONSOLE_OUT.print("UNKNOWN (code: ");
        KTANE_CONSOLE_OUT.print(sndStat);
        KTANE_CONSOLE_OUT.print(")");
        break;
    }
    KTANE_CONSOLE_OUT.println();
    
    // If send fails, check if we should reset
    if(sndStat == 1) {  // CAN_SENDMSGTIMEOUT
      KTANE_CONSOLE_OUT.println("ERROR: Transmission timeout - bus may have errors!");
      KTANE_CONSOLE_OUT.print("RX Error Count: ");
      KTANE_CONSOLE_OUT.println(rxErrors);
      KTANE_CONSOLE_OUT.println("Try loopback mode first to verify MCP2515 works!");
      KTANE_CONSOLE_OUT.println();
    }
    
    // Only reset if error count is reasonable (not too high)
    if(rxErrors < 128 && txErrors < 128) {
      KTANE_CONSOLE_OUT.println("Attempting full reset...");
      resetCANController();
    } else {
      KTANE_CONSOLE_OUT.println("ERROR: Error counts too high - reset required!");
      KTANE_CONSOLE_OUT.println("Press 'R' to manually reset");
    }
  }
  delay(100);   // send data per 100ms
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/

