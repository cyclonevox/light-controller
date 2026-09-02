// 继电器接 D8；点阵用来滚 IP / 配网提示。灯是否点亮的状态也放在这里。
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "TextAnimation.h"

#include "board.h"
#include "config.h"

TEXT_ANIMATION_DEFINE(anim, 160)

static ArduinoLEDMatrix matrix;
static bool lightOn = false;
static char scrollText[48] = "";
static volatile bool scrollAgain = false;

static void applyRelay() {
  const bool level = lightOn ? RELAY_ACTIVE_HIGH : !RELAY_ACTIVE_HIGH;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);
}

static void matrixCallback() {
  scrollAgain = true;
}

void boardBegin() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  lightOn = false;
  applyRelay();

  matrix.begin();
  matrix.setCallback(matrixCallback);
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

  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_5x7);
  matrix.textScrollSpeed(70);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print("   ");
  matrix.println(scrollText);
  matrix.endTextAnimation(SCROLL_LEFT, anim);
  matrix.loadTextAnimationSequence(anim);
  matrix.play();
}

void displayPoll() {
  if (scrollAgain && scrollText[0] != '\0') {
    scrollAgain = false;
    displayScroll(scrollText);
  }
}
