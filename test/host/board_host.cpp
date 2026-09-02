// 电脑上的板级替身：灯状态写到 PIN_FILE，滚动文字写到 DISPLAY_FILE。
#include "Arduino.h"
#include "board.h"
#include "config.h"

#include <cstring>
#include <fstream>
#include <cstdlib>

static bool lightOn = false;
static char scrollText[48] = "";

static void writeDisplay(const char* text) {
  const char* path = std::getenv("DISPLAY_FILE");
  if (path == nullptr || path[0] == '\0') {
    return;
  }
  std::ofstream out(path, std::ios::trunc);
  out << text;
}

static void applyRelay() {
  const bool level = lightOn ? RELAY_ACTIVE_HIGH : !RELAY_ACTIVE_HIGH;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);
}

void boardBegin() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  lightOn = false;
  applyRelay();
}

void setLight(bool on) {
  lightOn = on;
  applyRelay();
}

void toggleLight() {
  setLight(!lightOn);
}

bool isLightOn() {
  return lightOn;
}

void displayScroll(const char* text) {
  strncpy(scrollText, text, sizeof(scrollText) - 1);
  scrollText[sizeof(scrollText) - 1] = '\0';
  writeDisplay(scrollText);
  Serial.print("DISPLAY ");
  Serial.println(scrollText);
}

void displayPoll() {}
