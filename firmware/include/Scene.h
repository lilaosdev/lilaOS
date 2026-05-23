#pragma once

#include <Arduino.h>

enum class SceneAction {
  None,
  Navigate,
  Confirm,
  LockDevice,
  Finish,
};

struct Scene {
  const char* id;
  const char* text;
  const char* image;
  const char* audio;
  const char* next;
  const char* yesNext;
  const char* noNext;
};
