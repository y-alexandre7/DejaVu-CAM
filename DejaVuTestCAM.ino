/*
  AI Thinker ESP32-CAM Bluetooth Classic serial bridge

  Advertises as "DejaVu Note 1". Connect to it from a phone or computer
  with an SPP-compatible Bluetooth serial app, then exchange text through
  the AI Thinker board's UART0 (Serial), exposed on GPIO 3 (RX) and GPIO 1
  (TX).
*/

#if !defined(ARDUINO_ARCH_ESP32)
#error "Select an ESP32 board, such as AI Thinker ESP32-CAM, in Arduino IDE."
#endif

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is disabled in this ESP32 board configuration."
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error "Bluetooth Classic SPP is unavailable. Use an original ESP32 board, such as the ESP32-CAM AI Thinker."
#endif

#include <BluetoothSerial.h>

BluetoothSerial Bluetooth;
const char* const BLUETOOTH_DEVICE_NAME = "DejaVu Note 1";
bool previousClientState = false;

void setup() {
  Serial.begin(115200);
  delay(300);

  if (!Bluetooth.begin(BLUETOOTH_DEVICE_NAME)) {
    Serial.println("Bluetooth startup failed.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Bluetooth is ready.");
  Serial.println("Pair/connect to: DejaVu Note 1");
  Serial.println("Serial bridge is active at 115200 baud.");
}

void loop() {
  const bool clientConnected = Bluetooth.hasClient();
  if (clientConnected != previousClientState) {
    Serial.println(clientConnected
                       ? "Bluetooth client connected."
                       : "Bluetooth client disconnected.");
    previousClientState = clientConnected;
  }

  while (Bluetooth.available() > 0) {
    Serial.write(Bluetooth.read());
  }

  while (Serial.available() > 0) {
    Bluetooth.write(Serial.read());
  }
}
