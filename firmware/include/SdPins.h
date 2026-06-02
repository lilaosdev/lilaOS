#pragma once

#include <Arduino.h>

// microSD via SPI compartilhado com o display.
// Troque aqui quando definir a fiacao final do modulo.
constexpr uint8_t SD_PIN_SCLK = 18;
constexpr uint8_t SD_PIN_MISO = 19;
constexpr uint8_t SD_PIN_MOSI = 23;
constexpr uint8_t SD_PIN_CS   = 13;

constexpr uint32_t SD_SPI_FREQUENCY = 4000000;
