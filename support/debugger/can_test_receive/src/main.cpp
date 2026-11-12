// CAN Receive Example
// Based on: https://github.com/coryjfowler/MCP_CAN_lib/blob/master/examples/CAN_receive/CAN_receive.ino

#include <Arduino.h>
#include <ktane_console.h>
#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string

#define CAN0_INT 20                              // Set INT to pin 20
#define CAN0_CS 17                               // Set CS to pin 17

MCP_CAN CAN0(CAN0_CS);                           // Set CS to pin 17

void setup()
{
  ktaneConsoleInit(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println("CAN Test Receive Module");
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
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT_PULLUP);                     // Configuring pin for /INT input with pullup
  
  KTANE_CONSOLE_OUT.println("MCP2515 Library Receive Example...");
  KTANE_CONSOLE_OUT.println("Waiting for messages...");
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("=== DIAGNOSTICS ===");
  KTANE_CONSOLE_OUT.print("Interrupt pin (");
  KTANE_CONSOLE_OUT.print(CAN0_INT);
  KTANE_CONSOLE_OUT.print(") state: ");
  KTANE_CONSOLE_OUT.println(digitalRead(CAN0_INT) ? "HIGH (idle, no messages)" : "LOW (interrupt active)");
  KTANE_CONSOLE_OUT.println("Expected: HIGH when idle, LOW when message received");
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println();
}

void loop()
{
  static unsigned long lastDiagnostic = 0;
  unsigned long now = millis();
  
  // Print diagnostics every 2 seconds
  if(now - lastDiagnostic >= 2000) {
    lastDiagnostic = now;
    KTANE_CONSOLE_OUT.print("[DIAG] INT pin: ");
    KTANE_CONSOLE_OUT.print(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
    KTANE_CONSOLE_OUT.print(" | CheckReceive: ");
    byte checkStatus = CAN0.checkReceive();
    KTANE_CONSOLE_OUT.print(checkStatus);
    KTANE_CONSOLE_OUT.print(" - ");
    switch(checkStatus) {
      case 0: KTANE_CONSOLE_OUT.print("CAN_NOMSG (no message)");
        break;
      case 1: KTANE_CONSOLE_OUT.print("CAN_MSGAVAIL (message available)");
        break;
      case 2: KTANE_CONSOLE_OUT.print("CAN_FAIL (check failed)");
        break;
      case 3: KTANE_CONSOLE_OUT.print("CAN_MSGAVAIL (message available)");
        break;
      case 4: KTANE_CONSOLE_OUT.print("ERROR STATE (checkReceive failed)");
        break;
      default: KTANE_CONSOLE_OUT.print("UNKNOWN");
        break;
    }
    KTANE_CONSOLE_OUT.println();
    
    // Also check error status
    byte error = CAN0.getError();
    KTANE_CONSOLE_OUT.print("[DIAG] Error Status: 0x");
    KTANE_CONSOLE_OUT.print(error, HEX);
    KTANE_CONSOLE_OUT.print(" | TX Errors: ");
    KTANE_CONSOLE_OUT.print(CAN0.errorCountTX());
    KTANE_CONSOLE_OUT.print(" | RX Errors: ");
    KTANE_CONSOLE_OUT.println(CAN0.errorCountRX());
    KTANE_CONSOLE_OUT.println();
  }
  
  // Method 1: Check interrupt pin (active-low interrupt)
  if(!digitalRead(CAN0_INT))                         // If CAN0_INT pin is low, read receive buffer
  {
    KTANE_CONSOLE_OUT.println("[INTERRUPT] INT pin went LOW - message detected!");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    KTANE_CONSOLE_OUT.print(msgString);
  
    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      KTANE_CONSOLE_OUT.print(msgString);
    } else {
      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        KTANE_CONSOLE_OUT.print(msgString);
      }
    }
        
    KTANE_CONSOLE_OUT.println();
  }
  
  // Method 2: Also check checkReceive() in case interrupt isn't working
  if(CAN0.checkReceive() == CAN_MSGAVAIL) {
    KTANE_CONSOLE_OUT.println("[CHECK] Message available via checkReceive()!");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    
    if((rxId & 0x80000000) == 0x80000000)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    KTANE_CONSOLE_OUT.print(msgString);
  
    if((rxId & 0x40000000) == 0x40000000){
      sprintf(msgString, " REMOTE REQUEST FRAME");
      KTANE_CONSOLE_OUT.print(msgString);
    } else {
      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        KTANE_CONSOLE_OUT.print(msgString);
      }
    }
        
    KTANE_CONSOLE_OUT.println();
  }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/

