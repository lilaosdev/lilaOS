#pragma once

#include <Arduino.h>
#include "Scene.h"

class DisplayDevice {
public:
  void begin();
  void showBoot(const String& message);
  void showScene(const Scene& scene);
  void showDialog(
    const char* speaker,
    const char* text,
    uint16_t visibleChars,
    bool hasQuestion,
    const char* firstOption,
    const char* secondOption,
    uint8_t selectedOption,
    bool textComplete,
    bool firstFrame = false
  );
  void showEnding(
    uint8_t pageIndex,
    uint8_t pageCount,
    const char* title,
    const char* body
  );
  void showLocked(const String& message);
};

class AudioDevice {
public:
  void begin();
  void play(const char* audioPath);
  void stop();
  void update();
  bool isPlaying();
  void setGain(float gain);
};

class ButtonInput {
public:
  void begin();
  SceneAction readAction();
};
