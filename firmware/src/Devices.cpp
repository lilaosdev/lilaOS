#include "Devices.h"

namespace {
constexpr uint8_t NEXT_BUTTON_PIN = 25;
constexpr uint8_t YES_BUTTON_PIN = 26;
constexpr uint8_t NO_BUTTON_PIN = 27;

bool isPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}
}

void DisplayDevice::begin() {
  Serial.println("[display] mock begin");
}

void DisplayDevice::showBoot(const String& message) {
  Serial.print("[display] boot: ");
  Serial.println(message);
}

void DisplayDevice::showScene(const Scene& scene) {
  Serial.print("[display] scene ");
  Serial.print(scene.id);
  Serial.print(": ");
  Serial.println(scene.text);
}

void DisplayDevice::showChoicePage(
  uint8_t pageIndex,
  uint8_t pageCount,
  const char* title,
  const char* body,
  const char* leftLabel,
  const char* rightLabel,
  uint8_t selectedOption
) {
  Serial.println();
  Serial.println("------------- DISPLAY -------------");
  Serial.print("Pagina ");
  Serial.print(pageIndex + 1);
  Serial.print("/");
  Serial.println(pageCount);
  Serial.println(title);
  Serial.println(body);
  Serial.println();

  Serial.print(selectedOption == 0 ? "> [" : "  [");
  Serial.print(leftLabel);
  Serial.print("]   ");
  Serial.print(selectedOption == 1 ? "> [" : "  [");
  Serial.print(rightLabel);
  Serial.println("]");
  Serial.println("-----------------------------------");
}

void DisplayDevice::showLocked(const String& message) {
  Serial.print("[display] locked: ");
  Serial.println(message);
}

void AudioDevice::begin() {
  Serial.println("[audio] mock begin");
}

void AudioDevice::play(const char* audioPath) {
  if (audioPath == nullptr || audioPath[0] == '\0') {
    return;
  }

  Serial.print("[audio] play ");
  Serial.println(audioPath);
}

void AudioDevice::stop() {
  Serial.println("[audio] stop");
}

void ButtonInput::begin() {
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(YES_BUTTON_PIN, INPUT_PULLUP);
  pinMode(NO_BUTTON_PIN, INPUT_PULLUP);
}

SceneAction ButtonInput::readAction() {
  if (isPressed(YES_BUTTON_PIN)) {
    delay(180);
    return SceneAction::ChooseYes;
  }

  if (isPressed(NO_BUTTON_PIN)) {
    delay(180);
    return SceneAction::ChooseNo;
  }

  if (isPressed(NEXT_BUTTON_PIN)) {
    delay(180);
    return SceneAction::Advance;
  }

  return SceneAction::None;
}
