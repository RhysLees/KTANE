// CAN Receive Example
// Based on: https://github.com/coryjfowler/MCP_CAN_lib/blob/master/examples/CAN_receive/CAN_receive.ino

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
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("==========================================");
  Serial.println("CAN Test Receive Module");
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
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT_PULLUP);                     // Configuring pin for /INT input with pullup
  
  Serial.println("MCP2515 Library Receive Example...");
  Serial.println("Waiting for messages...");
  Serial.println();
  Serial.println("=== DIAGNOSTICS ===");
  Serial.print("Interrupt pin (");
  Serial.print(CAN0_INT);
  Serial.print(") state: ");
  Serial.println(digitalRead(CAN0_INT) ? "HIGH (idle, no messages)" : "LOW (interrupt active)");
  Serial.println("Expected: HIGH when idle, LOW when message received");
  Serial.println("==========================================");
  Serial.println();
}

void loop()
{
  static unsigned long lastDiagnostic = 0;
  unsigned long now = millis();
  
  // Print diagnostics every 2 seconds
  if(now - lastDiagnostic >= 2000) {
    lastDiagnostic = now;
    Serial.print("[DIAG] INT pin: ");
    Serial.print(digitalRead(CAN0_INT) ? "HIGH" : "LOW");
    Serial.print(" | CheckReceive: ");
    byte checkStatus = CAN0.checkReceive();
    Serial.print(checkStatus);
    Serial.print(" - ");
    switch(checkStatus) {
      case 0: Serial.print("CAN_NOMSG (no message)");
        break;
      case 1: Serial.print("CAN_MSGAVAIL (message available)");
        break;
      case 2: Serial.print("CAN_FAIL (check failed)");
        break;
      case 3: Serial.print("CAN_MSGAVAIL (message available)");
        break;
      case 4: Serial.print("ERROR STATE (checkReceive failed)");
        break;
      default: Serial.print("UNKNOWN");
        break;
    }
    Serial.println();
    
    // Also check error status
    byte error = CAN0.getError();
    Serial.print("[DIAG] Error Status: 0x");
    Serial.print(error, HEX);
    Serial.print(" | TX Errors: ");
    Serial.print(CAN0.errorCountTX());
    Serial.print(" | RX Errors: ");
    Serial.println(CAN0.errorCountRX());
    Serial.println();
  }
  
  // Method 1: Check interrupt pin (active-low interrupt)
  if(!digitalRead(CAN0_INT))                         // If CAN0_INT pin is low, read receive buffer
  {
    Serial.println("[INTERRUPT] INT pin went LOW - message detected!");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
    if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    Serial.print(msgString);
  
    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }
        
    Serial.println();
  }
  
  // Method 2: Also check checkReceive() in case interrupt isn't working
  if(CAN0.checkReceive() == CAN_MSGAVAIL) {
    Serial.println("[CHECK] Message available via checkReceive()!");
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    
    if((rxId & 0x80000000) == 0x80000000)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    Serial.print(msgString);
  
    if((rxId & 0x40000000) == 0x40000000){
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for(byte i = 0; i<len; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }
        
    Serial.println();
  }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/

