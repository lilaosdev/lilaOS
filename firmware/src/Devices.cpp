#include "Devices.h"
#include "DisplayPins.h"
#include <LovyanGFX.hpp>

namespace {
constexpr uint8_t NAVIGATE_BUTTON_PIN = 25;
constexpr uint8_t CONFIRM_BUTTON_PIN = 26;

constexpr uint16_t COLOR_BG = 0x1082;
constexpr uint16_t COLOR_PANEL = 0x2104;
constexpr uint16_t COLOR_PANEL_EDGE = 0x632C;
constexpr uint16_t COLOR_TEXT = 0xF7BE;
constexpr uint16_t COLOR_MUTED = 0x8C71;
constexpr uint16_t COLOR_ACCENT = 0xFEA0;
constexpr uint16_t COLOR_CHOICE = 0x39E7;
constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_RED = 0xF800;

class LGFX_ST7789 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

public:
  LGFX_ST7789() {
    {
      auto cfg = bus.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = DISPLAY_PIN_SCLK;
      cfg.pin_mosi = DISPLAY_PIN_MOSI;
      cfg.pin_miso = DISPLAY_PIN_MISO;
      cfg.pin_dc = DISPLAY_PIN_DC;
      bus.config(cfg);
      panel.setBus(&bus);
    }

    {
      auto cfg = panel.config();
      cfg.pin_cs = DISPLAY_PIN_CS;
      cfg.pin_rst = DISPLAY_PIN_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = DISPLAY_WIDTH;
      cfg.panel_height = DISPLAY_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel.config(cfg);
    }

    {
      auto cfg = light.config();
      cfg.pin_bl = DISPLAY_PIN_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light.config(cfg);
      panel.setLight(&light);
    }

    setPanel(&panel);
  }
};

LGFX_ST7789 lcd;

bool isPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

uint16_t textLength(const char* text) {
  uint16_t length = 0;
  while (text[length] != '\0') {
    length++;
  }

  return length;
}

void drawCornerPixels() {
  lcd.fillRect(8, 8, 8, 8, COLOR_ACCENT);
  lcd.fillRect(224, 8, 8, 8, COLOR_ACCENT);
  lcd.fillRect(8, 224, 8, 8, COLOR_ACCENT);
  lcd.fillRect(224, 224, 8, 8, COLOR_ACCENT);
}

void drawPixelFrame(int32_t x, int32_t y, int32_t width, int32_t height) {
  lcd.drawRect(x, y, width, height, COLOR_PANEL_EDGE);
  lcd.drawRect(x + 2, y + 2, width - 4, height - 4, COLOR_PANEL_EDGE);
}

void drawWrappedText(
  const char* text,
  uint16_t visibleChars,
  int32_t x,
  int32_t y,
  int32_t maxWidth,
  int32_t lineHeight
) {
  int32_t cursorX = x;
  int32_t cursorY = y;
  uint16_t printed = 0;

  lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
  lcd.setTextSize(2);
  lcd.setTextFont(1);

  for (uint16_t index = 0; text[index] != '\0' && printed < visibleChars; index++) {
    char current = text[index];

    if (current == '\n') {
      cursorX = x;
      cursorY += lineHeight;
      printed++;
      continue;
    }

    if (cursorX + 12 > x + maxWidth || current == '|' ) {
      cursorX = x;
      cursorY += lineHeight;

      if (current == '|') {
        printed++;
        continue;
      }
    }

    lcd.setCursor(cursorX, cursorY);
    lcd.print(current);
    cursorX += 12;
    printed++;
  }
}

void drawSerialDialogPreview(
  const char* speaker,
  const char* text,
  uint16_t visibleChars,
  bool hasQuestion,
  const char* firstOption,
  const char* secondOption,
  uint8_t selectedOption,
  bool textComplete
) {
  const uint16_t totalChars = textLength(text);

  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  Serial.println(speaker);
  Serial.println("+--------------------------------------+");
  Serial.print("| ");

  for (uint16_t index = 0; text[index] != '\0' && index < visibleChars; index++) {
    Serial.print(text[index]);
  }

  if (!textComplete) {
    Serial.print("_");
  }

  Serial.println();
  Serial.println("|");

  if (hasQuestion && textComplete) {
    Serial.print("| ");
    Serial.print(selectedOption == 0 ? "> " : "  ");
    Serial.println(firstOption);
    Serial.print("| ");
    Serial.print(selectedOption == 1 ? "> " : "  ");
    Serial.println(secondOption);
  } else if (textComplete) {
    Serial.println("| [CONFIRMAR] continuar");
  } else {
    Serial.print("| digitando ");
    Serial.print(visibleChars);
    Serial.print("/");
    Serial.println(totalChars);
  }

  Serial.println("+--------------------------------------+");
}
}

void DisplayDevice::begin() {
  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);
  lcd.setBrightness(220);
  lcd.fillScreen(COLOR_BG);
  Serial.println("[display] ST7789 240x240 via LovyanGFX");
}

void DisplayDevice::showBoot(const String& message) {
  Serial.print("[boot] ");
  Serial.println(message);

  lcd.fillScreen(COLOR_BG);
  drawCornerPixels();
  lcd.setTextColor(COLOR_ACCENT, COLOR_BG);
  lcd.setTextFont(1);
  lcd.setTextSize(2);
  lcd.setCursor(32, 104);
  lcd.print("PEDIDO.EXE");
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_MUTED, COLOR_BG);
  lcd.setCursor(70, 128);
  lcd.print(message);
}

void DisplayDevice::showScene(const Scene& scene) {
  Serial.print("[scene] ");
  Serial.print(scene.id);
  Serial.print(": ");
  Serial.println(scene.text);
}

void DisplayDevice::showDialog(
  const char* speaker,
  const char* text,
  uint16_t visibleChars,
  bool hasQuestion,
  const char* firstOption,
  const char* secondOption,
  uint8_t selectedOption,
  bool textComplete
) {
  lcd.fillScreen(COLOR_BG);
  drawCornerPixels();

  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_MUTED, COLOR_BG);
  lcd.setCursor(14, 18);
  lcd.print("DIALOGO");

  lcd.setTextSize(2);
  lcd.setTextColor(COLOR_ACCENT, COLOR_BG);
  lcd.setCursor(14, 34);
  lcd.print(speaker);

  lcd.fillRect(14, 72, 212, 124, COLOR_PANEL);
  drawPixelFrame(14, 72, 212, 124);
  drawWrappedText(text, visibleChars, 28, 90, 184, 18);

  if (!textComplete) {
    lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
    lcd.setTextSize(2);
    lcd.setCursor(202, 166);
    lcd.print("_");
  }

  if (hasQuestion && textComplete) {
    lcd.fillRect(18, 202, 98, 26, selectedOption == 0 ? COLOR_ACCENT : COLOR_CHOICE);
    lcd.fillRect(124, 202, 98, 26, selectedOption == 1 ? COLOR_ACCENT : COLOR_CHOICE);
    lcd.setTextColor(COLOR_BG, selectedOption == 0 ? COLOR_ACCENT : COLOR_CHOICE);
    lcd.setCursor(44, 208);
    lcd.print(firstOption);
    lcd.setTextColor(COLOR_BG, selectedOption == 1 ? COLOR_ACCENT : COLOR_CHOICE);
    lcd.setCursor(154, 208);
    lcd.print(secondOption);
  } else if (textComplete) {
    lcd.setTextFont(1);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_MUTED, COLOR_BG);
    lcd.setCursor(55, 210);
    lcd.print("CONFIRMAR PARA AVANCAR");
  }

  drawSerialDialogPreview(
    speaker,
    text,
    visibleChars,
    hasQuestion,
    firstOption,
    secondOption,
    selectedOption,
    textComplete
  );
}

void DisplayDevice::showEnding(
  uint8_t pageIndex,
  uint8_t pageCount,
  const char* title,
  const char* body
) {
  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| final ");
  Serial.print(pageIndex + 1);
  Serial.print("/");
  Serial.println(pageCount);
  Serial.print("| ");
  Serial.println(title);
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  Serial.println(body);
  Serial.println("+--------------------------------------+");

  lcd.fillScreen(COLOR_BG);
  drawCornerPixels();
  lcd.fillRect(18, 68, 204, 104, COLOR_PANEL);
  drawPixelFrame(18, 68, 204, 104);
  lcd.setTextFont(1);
  lcd.setTextSize(2);
  lcd.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  lcd.setCursor(42, 90);
  lcd.print(title);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TEXT, COLOR_PANEL);
  lcd.setCursor(34, 126);
  lcd.print(body);
}

void DisplayDevice::showLocked(const String& message) {
  Serial.print("[locked] ");
  Serial.println(message);

  lcd.fillScreen(COLOR_BLACK);
  lcd.setTextColor(COLOR_RED, COLOR_BLACK);
  lcd.setTextFont(1);
  lcd.setTextSize(2);
  lcd.setCursor(36, 104);
  lcd.print("BLOQUEADO");
}

void AudioDevice::begin() {
  Serial.println("[audio] mock begin");
}

void AudioDevice::play(const char* audioPath) {
  if (audioPath == nullptr || audioPath[0] == '\0') {
    return;
  }

  Serial.print("[audio] play ");
  Serial.println(audioPath);
}

void AudioDevice::stop() {
  Serial.println("[audio] stop");
}

void ButtonInput::begin() {
  pinMode(NAVIGATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);
}

SceneAction ButtonInput::readAction() {
  if (isPressed(CONFIRM_BUTTON_PIN)) {
    delay(180);
    return SceneAction::Confirm;
  }

  if (isPressed(NAVIGATE_BUTTON_PIN)) {
    delay(180);
    return SceneAction::Navigate;
  }

  return SceneAction::None;
}
