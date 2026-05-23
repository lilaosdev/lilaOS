#pragma once

#include <Arduino.h>
#include "Scene.h"

class DisplayDevice {
public:
  void begin();
  void showBoot(const String& message);
  void showScene(const Scene& scene);
  void showChoicePage(
    uint8_t pageIndex,
    uint8_t pageCount,
    const char* title,
    const char* body,
    const char* leftLabel,
    const char* rightLabel,
    uint8_t selectedOption
  );
  void showLocked(const String& message);
};

class AudioDevice {
public:
  void begin();
  void play(const char* audioPath);
  void stop();
};

class ButtonInput {
public:
  void begin();
  SceneAction readAction();
};
