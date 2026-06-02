#pragma once

#include <Arduino.h>

// microSD via SPI compartilhado com o display.
// Troque aqui quando definir a fiacao final do modulo.
constexpr uint8_t SD_PIN_SCLK = 14;
constexpr uint8_t SD_PIN_MISO = 19;
constexpr uint8_t SD_PIN_MOSI = 13;
constexpr uint8_t SD_PIN_CS   = 33;

constexpr uint32_t SD_SPI_FREQUENCY = 1000000;
