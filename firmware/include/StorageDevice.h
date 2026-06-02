#pragma once

#include <Arduino.h>
#include <FS.h>

class StorageDevice {
public:
  void begin();
  bool exists(const char* path);
  File open(const char* path, const char* mode = "r");
  fs::FS* fileSystemFor(const char* path);
  const char* sourceNameFor(const char* path);
  bool isSdReady() const;

private:
  bool spiffsReady = false;
  bool sdReady = false;
};

extern StorageDevice storage;
