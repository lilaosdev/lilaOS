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
  // Desenha apenas o proximo caractere — sem fullredraw, sem fillRect na area de texto.
  // Chame no lugar de showDialog durante o typewriter normal.
  void drawNextChar(const char* text, uint16_t visibleChars, bool textComplete, bool hasQuestion);
  // Redesenha so as opcoes de escolha — sem tocar texto nem gif.
  void redrawOptions(const char* firstOption, const char* secondOption, uint8_t selectedOption);
  void showEnding(
    uint8_t pageIndex,
    uint8_t pageCount,
    const char* title,
    const char* body
  );
  void showLocked(const String& message);
  void showStart();
  void showLauncher(uint8_t selectedApp, uint8_t themeId, const char* nowPlaying);
  void showSettings(uint8_t selectedRow, uint8_t volumeLevel, uint8_t themeId);
  void showMusicPlayer(const char* heading, const char* trackName, uint8_t selectedControl, bool paused, uint8_t themeId);
  void showTerminal(uint8_t commandIndex, bool executed, bool cursorVisible, uint8_t themeId);
  void showPlaceholderApp(const char* title, const char* body, uint8_t themeId);
  void drawImageArea(const char* imagePath);
  void drawGifLine(int16_t x, int16_t y, int16_t width, const uint16_t* pixels);
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
