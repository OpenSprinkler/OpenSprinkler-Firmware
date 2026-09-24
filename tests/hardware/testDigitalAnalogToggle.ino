/*
 * ESP32-C6 Configurable Sensor Inputs
 *
 * Sensor Pins: 1, 2, 3, 6
 * Pull-up Pins: 4, 16, 17, 5 (connected via 10K resistors)
 *
 * Commands:
 *   [PIN]:[MODE] (e.g., 1:a for analog, 1:d for digital)
 */

struct SensorPort {
  uint8_t sensorPin;
  uint8_t pullupPin;
  bool isAnalog; // false = digital, true = analog
};

// Define the 4 sensor ports and their default states (digital)
SensorPort ports[4] = {
  {1, 4, false},
  {2, 16, false},
  {3, 17, false},
  {6, 5, false}
};

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 2000;

void applyPortConfiguration(int index) {
  if (ports[index].isAnalog) {
    // Analog Mode: Engage the internal pull-down on the control pin
    pinMode(ports[index].pullupPin, INPUT_PULLDOWN);
    // Ensure the sensor pin itself remains a standard input
    pinMode(ports[index].sensorPin, INPUT);
  } else {
    // Digital Mode: Drive the pull-up pin HIGH
    pinMode(ports[index].pullupPin, OUTPUT);
    digitalWrite(ports[index].pullupPin, HIGH);
    // Set sensor pin to standard digital input
    pinMode(ports[index].sensorPin, INPUT);
  }
}

void setup() {
  Serial.begin(115200);

  // Wait a moment for serial monitor to connect (useful for native USB boards)
  delay(1000);
  Serial.println("ESP32-C6 Sensor Node Started.");
  Serial.println("Send commands like '1:a' (analog) or '6:d' (digital).");

  // Initialize all ports to their default state (digital)
  for (int i = 0; i < 4; i++) {
    applyPortConfiguration(i);
  }
}

void loop() {
  // 1. Handle incoming Serial commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); // Remove whitespace and \r

    int colonPos = cmd.indexOf(':');
    if (colonPos != -1) {
      int targetPin = cmd.substring(0, colonPos).toInt();
      char modeChar = cmd.charAt(colonPos + 1);

      // Find the corresponding port and update it
      for (int i = 0; i < 4; i++) {
        if (ports[i].sensorPin == targetPin) {
          if (modeChar == 'a' || modeChar == 'A') {
            ports[i].isAnalog = true;
            applyPortConfiguration(i);
            Serial.printf(">> Pin %d configured as ANALOG\n", targetPin);
          } else if (modeChar == 'd' || modeChar == 'D') {
            ports[i].isAnalog = false;
            applyPortConfiguration(i);
            Serial.printf(">> Pin %d configured as DIGITAL\n", targetPin);
          } else {
            Serial.println(">> Error: Unknown mode. Use 'a' or 'd'.");
          }
          break;
        }
      }
    }
  }

  // 2. Handle 2-second interval reporting
  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();

    Serial.println("--- Sensor Status ---");
    for (int i = 0; i < 4; i++) {
      Serial.printf("GPIO %d: ", ports[i].sensorPin);

      if (ports[i].isAnalog) {
        // analogReadMilliVolts uses the ESP32's internal calibration for better accuracy
        uint32_t mv = analogReadMilliVolts(ports[i].sensorPin);
        float voltage = mv / 1000.0;
        Serial.printf("ANALOG, Voltage: %.2f V\n", voltage);
      } else {
        int val = digitalRead(ports[i].sensorPin);
        Serial.printf("DIGITAL, Value: %s\n", val == HIGH ? "HIGH" : "LOW");
      }
    }
    Serial.println();
  }
}
