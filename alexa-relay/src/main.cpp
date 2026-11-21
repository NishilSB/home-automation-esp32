#include <Arduino.h>
#include <WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include "secrets.h"

// Wiring (2-Channel 5V Active-LOW Relay Module):
//   ESP32 5V (VIN) -> Relay VCC   (or remove RY-VCC jumper: RY-VCC->5V, VCC->3.3V)
//   ESP32 GND      -> Relay GND
//   ESP32 GPIO 26  -> Relay IN1   (Left relay — Alexa + PIR Auto-Light)
//   ESP32 GPIO 27  -> Relay IN2   (Right relay — Alexa)
//
// Wiring (PIR Motion Sensor, e.g. HC-SR501):
//   ESP32 5V (VIN) -> PIR VCC
//   ESP32 GND      -> PIR GND
//   ESP32 GPIO 33  -> PIR OUT     (3.3V logic output, safe for ESP32)
//
// Optional physical pushbuttons (connect between GPIO and GND):
//   ESP32 GPIO 18  -> Button 1 (toggles Relay 1 & syncs to Alexa)
//   ESP32 GPIO 19  -> Button 2 (toggles Relay 2 & syncs to Alexa)
static constexpr int RELAY1_PIN = 26;
static constexpr int RELAY2_PIN = 27;
static constexpr int PIR_PIN    = 33;
static constexpr int BTN1_PIN   = 18;
static constexpr int BTN2_PIN   = 19;
static constexpr int LED_PIN    = 2;   // Onboard blue LED (ON when any relay is ON)

// Your JQC3F-05VDC-C opto-isolated board is Active-LOW (LOW = ON, HIGH = OFF)
static constexpr bool RELAY_ACTIVE_LOW = true;

// PIR Auto-Light timing (change to 300000UL for 5 minutes once tested)
static constexpr unsigned long PIR_TIMEOUT_MS             = 60000UL; // Auto-OFF after 60s of no motion
static constexpr unsigned long PIR_MANUAL_OFF_COOLDOWN_MS = 15000UL; // Ignore PIR for 15s after manual OFF

bool relay1State = false;
bool relay2State = false;

// Smart occupancy state:
//   true  = Relay 1 was turned ON by PIR (will auto-OFF after PIR_TIMEOUT_MS)
//   false = Relay 1 was turned ON manually via Alexa/Button (stays ON even if motionless)
bool relay1AutoMode = false;
bool lastPirReading = false;
unsigned long lastMotionMs = 0;
unsigned long pirIgnoreUntilMs = 0;

bool lastBtn1 = HIGH;
bool lastBtn2 = HIGH;
unsigned long lastDebounce1 = 0;
unsigned long lastDebounce2 = 0;

void applyRelays() {
  digitalWrite(RELAY1_PIN, (relay1State ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, (relay2State ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
  digitalWrite(LED_PIN, (relay1State || relay2State) ? HIGH : LOW);
}

bool onPowerState1(const String &deviceId, bool &state) {
  relay1State = state;
  relay1AutoMode = false; // Manual/Voice override disables PIR auto-OFF
  if (!state) {
    pirIgnoreUntilMs = millis() + PIR_MANUAL_OFF_COOLDOWN_MS;
  }
  applyRelays();
  Serial.printf("[Alexa] Relay 1 (%s) -> %s (voice override)\n",
                deviceId.c_str(), state ? "ON" : "OFF");
  return true;
}

bool onPowerState2(const String &deviceId, bool &state) {
  relay2State = state;
  applyRelays();
  Serial.printf("[Alexa] Relay 2 (%s) -> %s\n", deviceId.c_str(), state ? "ON" : "OFF");
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_MIN_MODEM); // Low-power Wi-Fi Modem Sleep (keeps Alexa responsive)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to Wi-Fi '%s'", WIFI_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts >= 20) {
      Serial.println("\nRetrying Wi-Fi...");
      WiFi.disconnect();
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      attempts = 0;
    }
  }
  Serial.printf("\nWi-Fi connected! IP: %s (Modem Sleep enabled, CPU %lu MHz)\n",
                WiFi.localIP().toString().c_str(), getCpuFrequencyMhz());
}

void setupSinricPro() {
  SinricProSwitch &sw1 = SinricPro[RELAY1_ID];
  sw1.onPowerState(onPowerState1);

  SinricProSwitch &sw2 = SinricPro[RELAY2_ID];
  sw2.onPowerState(onPowerState2);

  SinricPro.onConnected([]() {
    Serial.println("Connected to SinricPro! Ready for Alexa + PIR.");
  });
  SinricPro.onDisconnected([]() {
    Serial.println("Disconnected from SinricPro, will reconnect...");
  });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void pollPir() {
  unsigned long now = millis();
  bool motion = (digitalRead(PIR_PIN) == HIGH);

  if (motion) {
    lastMotionMs = now;
    if (!lastPirReading) {
      Serial.println("[PIR] Motion detected!");
    }
    if (!relay1State && (long)(now - pirIgnoreUntilMs) >= 0) {
      relay1State = true;
      relay1AutoMode = true;
      applyRelays();
      Serial.printf("[PIR] Auto-ON Relay 1 (auto-OFF after %lus idle)\n", PIR_TIMEOUT_MS / 1000UL);
      SinricProSwitch &sw1 = SinricPro[RELAY1_ID];
      sw1.sendPowerStateEvent(true);
    }
  } else if (relay1State && relay1AutoMode && (now - lastMotionMs) >= PIR_TIMEOUT_MS) {
    relay1State = false;
    relay1AutoMode = false;
    applyRelays();
    Serial.println("[PIR] No motion timeout -> Auto-OFF Relay 1");
    SinricProSwitch &sw1 = SinricPro[RELAY1_ID];
    sw1.sendPowerStateEvent(false);
  }

  lastPirReading = motion;
}

void pollButtons() {
  unsigned long now = millis();

  bool b1 = digitalRead(BTN1_PIN);
  if (b1 == LOW && lastBtn1 == HIGH && (now - lastDebounce1) > 250) {
    lastDebounce1 = now;
    relay1State = !relay1State;
    relay1AutoMode = false; // Manual button overrides PIR auto-OFF
    if (!relay1State) {
      pirIgnoreUntilMs = now + PIR_MANUAL_OFF_COOLDOWN_MS;
    }
    applyRelays();
    Serial.printf("[Button 1] Relay 1 -> %s (manual override)\n", relay1State ? "ON" : "OFF");
    SinricProSwitch &sw1 = SinricPro[RELAY1_ID];
    sw1.sendPowerStateEvent(relay1State);
  }
  lastBtn1 = b1;

  bool b2 = digitalRead(BTN2_PIN);
  if (b2 == LOW && lastBtn2 == HIGH && (now - lastDebounce2) > 250) {
    lastDebounce2 = now;
    relay2State = !relay2State;
    applyRelays();
    Serial.printf("[Button 2] Relay 2 -> %s\n", relay2State ? "ON" : "OFF");
    SinricProSwitch &sw2 = SinricPro[RELAY2_ID];
    sw2.sendPowerStateEvent(relay2State);
  }
  lastBtn2 = b2;
}

void setup() {
  // Low-power setup: disable Bluetooth & drop CPU clock to 80 MHz
  btStop();
  setCpuFrequencyMhz(80);

  // Set relay pins OFF before configuring as OUTPUT to prevent boot click
  digitalWrite(RELAY1_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(RELAY2_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT_PULLDOWN); // Reads LOW cleanly even when PIR is unplugged
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  applyRelays();

  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 Sinric Pro 2-Channel Relay + PIR + Low Power ===");

  connectWiFi();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  pollPir();
  pollButtons();
  delay(10); // Yield to FreeRTOS idle task for lower power consumption
}
