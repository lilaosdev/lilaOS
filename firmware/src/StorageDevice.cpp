#include "StorageDevice.h"
#include "SdPins.h"

#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>

namespace {
SPIClass sdSpi(VSPI);

void printDirectory(fs::FS& fs, const char* label, const char* dirname, uint8_t depth) {
  File root = fs.open(dirname);
  if (!root) {
    Serial.print("[storage] ");
    Serial.print(label);
    Serial.print(" open failed: ");
    Serial.println(dirname);
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.print("[storage]   ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(file.name());
    if (file.isDirectory()) {
      Serial.println("/");
      if (depth > 0) {
        printDirectory(fs, label, file.path(), depth - 1);
      }
    } else {
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
  }
}
}

StorageDevice storage;

void StorageDevice::begin() {
  spiffsReady = SPIFFS.begin(true);
  if (spiffsReady) {
    Serial.println("[storage] SPIFFS mounted");
  } else {
    Serial.println("[storage] SPIFFS mount failed");
  }

  sdSpi.begin(SD_PIN_SCLK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  sdReady = SD.begin(SD_PIN_CS, sdSpi, SD_SPI_FREQUENCY);
  if (sdReady) {
    uint64_t cardMb = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.print("[storage] SD mounted: ");
    Serial.print(cardMb);
    Serial.println(" MB");
  } else {
    Serial.print("[storage] SD mount failed/not present: CS=");
    Serial.print(SD_PIN_CS);
    Serial.print(" SCLK=");
    Serial.print(SD_PIN_SCLK);
    Serial.print(" MISO=");
    Serial.print(SD_PIN_MISO);
    Serial.print(" MOSI=");
    Serial.println(SD_PIN_MOSI);
  }

  if (sdReady) {
    Serial.println("[storage] SD files:");
    printDirectory(SD, "SD", "/", 2);
  }

  if (spiffsReady) {
    Serial.println("[storage] SPIFFS files:");
    printDirectory(SPIFFS, "SPIFFS", "/", 2);
  }
}

bool StorageDevice::exists(const char* path) {
  return fileSystemFor(path) != nullptr;
}

File StorageDevice::open(const char* path, const char* mode) {
  fs::FS* fs = fileSystemFor(path);
  if (fs == nullptr) {
    return File();
  }
  return fs->open(path, mode);
}

fs::FS* StorageDevice::fileSystemFor(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return nullptr;
  }

  if (sdReady && SD.exists(path)) {
    return &SD;
  }

  if (spiffsReady && SPIFFS.exists(path)) {
    return &SPIFFS;
  }

  return nullptr;
}

const char* StorageDevice::sourceNameFor(const char* path) {
  fs::FS* fs = fileSystemFor(path);
  if (fs == &SD) return "SD";
  if (fs == &SPIFFS) return "SPIFFS";
  return "missing";
}

bool StorageDevice::isSdReady() const {
  return sdReady;
}
