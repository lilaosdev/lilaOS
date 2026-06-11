#pragma once

// Ajuste estes pinos para bater com a sua ligacao fisica do ST7789.
// Defaults comuns para ESP32 DevKit + display SPI.
#define DISPLAY_PIN_SCLK 18
#define DISPLAY_PIN_MOSI 23
#define DISPLAY_PIN_MISO -1
#define DISPLAY_PIN_DC 2
#define DISPLAY_PIN_CS 5
#define DISPLAY_PIN_RST 4
#define DISPLAY_PIN_BL 15

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 2
