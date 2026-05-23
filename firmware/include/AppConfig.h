#pragma once

#include <Arduino.h>

enum class RemoteMode {
  Normal,
  Locked,
  OfflineOk,
};

struct AppConfig {
  RemoteMode mode = RemoteMode::OfflineOk;
  String variant = "default";
  String message = "offline";
};

