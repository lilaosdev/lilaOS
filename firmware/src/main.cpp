#include <Arduino.h>
#include "AppConfig.h"
#include "Devices.h"
#include "Scene.h"

namespace {
DisplayDevice display;
AudioDevice audio;
ButtonInput buttons;

struct ChoicePage {
  const char* title;
  const char* body;
  const char* leftLabel;
  const char* rightLabel;
};

ChoicePage pages[] = {
  {
    "Oi",
    "Preparei uma coisinha pequena, mas feita com todo cuidado.",
    "Continuar",
    "Respirar"
  },
  {
    "Prometo ser breve",
    "So preciso de mais alguns cliques antes da pergunta importante.",
    "Anterior",
    "Proxima"
  },
};

constexpr uint8_t PAGE_COUNT = sizeof(pages) / sizeof(pages[0]);
uint8_t currentPageIndex = 0;
uint8_t selectedOption = 0;

AppConfig loadRemoteConfig() {
  // Mock inicial. Depois entra Wi-Fi + HTTP + cache em NVS.
  AppConfig config;
  config.mode = RemoteMode::Normal;
  config.variant = "default";
  config.message = "config local";
  return config;
}

void lockDevice(const String& reason) {
  audio.stop();
  display.showLocked(reason);

  // Bloqueio reversivel: acorda periodicamente para permitir nova config remota.
  esp_sleep_enable_timer_wakeup(30ULL * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void renderCurrentPage() {
  ChoicePage& page = pages[currentPageIndex];
  display.showChoicePage(
    currentPageIndex,
    PAGE_COUNT,
    page.title,
    page.body,
    page.leftLabel,
    page.rightLabel,
    selectedOption
  );
}

void moveCursor() {
  selectedOption++;

  if (selectedOption > 1) {
    selectedOption = 0;
    currentPageIndex = (currentPageIndex + 1) % PAGE_COUNT;
  }

  renderCurrentPage();
}
}

void setup() {
  Serial.begin(115200);
  delay(200);

  display.begin();
  audio.begin();
  buttons.begin();

  AppConfig config = loadRemoteConfig();
  display.showBoot(config.message);

  if (config.mode == RemoteMode::Locked) {
    lockDevice("bloqueado por configuracao remota");
  }

  renderCurrentPage();
}

void loop() {
  if (buttons.readAction() == SceneAction::Advance) {
    moveCursor();
  }

  delay(20);
}
