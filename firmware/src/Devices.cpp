#include "Devices.h"

namespace {
constexpr uint8_t NAVIGATE_BUTTON_PIN = 25;
constexpr uint8_t CONFIRM_BUTTON_PIN = 26;

bool isPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

uint16_t textLength(const char* text) {
  uint16_t length = 0;
  while (text[length] != '\0') {
    length++;
  }

  return length;
}

void printVisibleText(const char* text, uint16_t visibleChars) {
  for (uint16_t index = 0; text[index] != '\0' && index < visibleChars; index++) {
    Serial.print(text[index]);
  }
}
}

void DisplayDevice::begin() {
  Serial.println("[display] serial retro renderer");
}

void DisplayDevice::showBoot(const String& message) {
  Serial.print("[boot] ");
  Serial.println(message);
}

void DisplayDevice::showScene(const Scene& scene) {
  Serial.print("[scene] ");
  Serial.print(scene.id);
  Serial.print(": ");
  Serial.println(scene.text);
}

void DisplayDevice::showDialog(
  const char* speaker,
  const char* text,
  uint16_t visibleChars,
  bool hasQuestion,
  const char* firstOption,
  const char* secondOption,
  uint8_t selectedOption,
  bool textComplete
) {
  const uint16_t totalChars = textLength(text);

  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  Serial.println(speaker);
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  printVisibleText(text, visibleChars);

  if (!textComplete) {
    Serial.print("_");
  }

  Serial.println();
  Serial.println("|");

  if (hasQuestion && textComplete) {
    Serial.print("| ");
    Serial.print(selectedOption == 0 ? "> " : "  ");
    Serial.println(firstOption);
    Serial.print("| ");
    Serial.print(selectedOption == 1 ? "> " : "  ");
    Serial.println(secondOption);
  } else if (textComplete) {
    Serial.println("| [CONFIRMAR] continuar");
  } else {
    Serial.print("| digitando ");
    Serial.print(visibleChars);
    Serial.print("/");
    Serial.println(totalChars);
  }

  Serial.println("+--------------------------------------+");
}

void DisplayDevice::showEnding(
  uint8_t pageIndex,
  uint8_t pageCount,
  const char* title,
  const char* body
) {
  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| final ");
  Serial.print(pageIndex + 1);
  Serial.print("/");
  Serial.println(pageCount);
  Serial.print("| ");
  Serial.println(title);
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  Serial.println(body);
  Serial.println("+--------------------------------------+");
}

void DisplayDevice::showLocked(const String& message) {
  Serial.print("[locked] ");
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
  pinMode(NAVIGATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);
}

SceneAction ButtonInput::readAction() {
  if (isPressed(CONFIRM_BUTTON_PIN)) {
    delay(180);
    return SceneAction::Confirm;
  }

  if (isPressed(NAVIGATE_BUTTON_PIN)) {
    delay(180);
    return SceneAction::Navigate;
  }

  return SceneAction::None;
}
