#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>
#include <can_bus_test_protocol.h>

// Holds DEBUGGER_SENDER instance 1 — answers ID_PROBE so a peer must take instance 2+.

void onRawNegotiation(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len,
                      unsigned long timestamp) {
  (void)senderId;
  (void)timestamp;
  if (len != 3 || data[1] != CAN_BUS_TEST_SLAVE_TYPE) {
    return;
  }
  if (data[0] == ID_PROBE) {
    KTANE_CONSOLE_OUT.print("[occupier] saw PROBE inst ");
    KTANE_CONSOLE_OUT.println(data[2]);
  }
}

void setup() {
  ktaneConsoleInit(115200);
  KTANE_CONSOLE_OUT.println("CAN test OCCUPIER — DEBUGGER_SENDER instance 1");
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER_SENDER, 0x01));
  registerRawCanCallback(onRawNegotiation);
}

void loop() {
  handleCanMessages();
}
