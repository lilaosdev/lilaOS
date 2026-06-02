#include "Devices.h"
#include "AudioPins.h"
#include "DisplayPins.h"
#include "InputPins.h"
#include "StorageDevice.h"
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <LovyanGFX.hpp>

namespace {
constexpr uint8_t NAVIGATE_BUTTON_PIN = INPUT_PIN_NAVIGATE;
constexpr uint8_t CONFIRM_BUTTON_PIN  = INPUT_PIN_CONFIRM;

constexpr uint16_t COLOR_BG        = 0xFEF7;
constexpr uint16_t COLOR_BLACK     = 0x0000;
constexpr uint16_t COLOR_PINK      = 0xF8B2;
constexpr uint16_t COLOR_PINK_DARK = 0xF389;
constexpr uint16_t COLOR_RED       = 0xF800;

constexpr int32_t TEXT_X = 18;
constexpr int32_t TEXT_Y = 20;
constexpr int32_t TEXT_W = 204;
constexpr int32_t TEXT_H = 136;
constexpr int32_t LINE_H = 20;
constexpr int32_t CHAR_W = 12;

constexpr int32_t OPT_Y  = 148;
constexpr int32_t OPT_H  = 22;
constexpr int32_t OPT1_X = 18;
constexpr int32_t OPT2_X = 126;
constexpr int32_t OPT_W  = 96;

constexpr int32_t IMAGE_AREA_Y = 156;
constexpr int32_t IMAGE_AREA_H = 84;
constexpr int32_t IMAGE_SIZE   = 84;
constexpr int32_t IMAGE_X      = (DISPLAY_WIDTH - IMAGE_SIZE) / 2;

// Estado do cursor — persistente entre chamadas de drawNextChar.
// Resetado quando showDialog e chamado com firstFrame=true.
int32_t cursorCx = TEXT_X;
int32_t cursorCy = TEXT_Y;

class LGFX_ST7789 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI      bus;
  lgfx::Light_PWM    light;
public:
  LGFX_ST7789() {
    {
      auto cfg        = bus.config();
      cfg.spi_host    = VSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk    = DISPLAY_PIN_SCLK;
      cfg.pin_mosi    = DISPLAY_PIN_MOSI;
      cfg.pin_miso    = DISPLAY_PIN_MISO;
      cfg.pin_dc      = DISPLAY_PIN_DC;
      bus.config(cfg);
      panel.setBus(&bus);
    }
    {
      auto cfg             = panel.config();
      cfg.pin_cs           = DISPLAY_PIN_CS;
      cfg.pin_rst          = DISPLAY_PIN_RST;
      cfg.pin_busy         = -1;
      cfg.panel_width      = DISPLAY_WIDTH;
      cfg.panel_height     = DISPLAY_HEIGHT;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      panel.config(cfg);
    }
    {
      auto cfg        = light.config();
      cfg.pin_bl      = DISPLAY_PIN_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      light.config(cfg);
      panel.setLight(&light);
    }
    setPanel(&panel);
  }
};

LGFX_ST7789 lcd;
AudioGeneratorMP3*  mp3         = nullptr;
AudioFileSourceFS*  audioFile   = nullptr;
AudioOutputI2S*     audioOutput = nullptr;
uint32_t lastAudioStatusLog     = 0;

// -----------------------------------------------------------------------------

bool isPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

uint16_t textLength(const char* text) {
  uint16_t len = 0;
  while (text[len] != '\0') len++;
  return len;
}

void drawScreenBorder() {
  lcd.drawRect(4, 4, 232, 232, COLOR_PINK_DARK);
  lcd.drawRect(7, 7, 226, 226, COLOR_PINK);
  lcd.fillRect(4,   4,   8, 8, COLOR_PINK_DARK);
  lcd.fillRect(228, 4,   8, 8, COLOR_PINK_DARK);
  lcd.fillRect(4,   228, 8, 8, COLOR_PINK_DARK);
  lcd.fillRect(228, 228, 8, 8, COLOR_PINK_DARK);
}

void clearImageArea() {
  lcd.fillRect(0, IMAGE_AREA_Y, DISPLAY_WIDTH, IMAGE_AREA_H, COLOR_BG);
}

// Logica de avanco de cursor — identica em redrawTextArea e drawNextChar.
// Retorna true se precisa quebrar linha antes de desenhar o char em 'i'.
bool advanceCursor(const char* text, uint16_t i, int32_t& cx, int32_t& cy) {
  const int32_t lineRight = TEXT_X + TEXT_W;
  char c = text[i];

  if (c == '\n' || c == '|') {
    cx = TEXT_X;
    cy += LINE_H;
    return false; // nao desenha nada
  }

  if (c == ' ') {
    if (cx != TEXT_X && cx + CHAR_W <= lineRight) cx += CHAR_W;
    return false;
  }

  // word wrap
  const bool beginsWord = (i == 0 || text[i-1]==' ' || text[i-1]=='\n' || text[i-1]=='|');
  if (beginsWord && cx != TEXT_X) {
    uint16_t wordChars = 0;
    for (uint16_t j = i; text[j] != '\0'; j++) {
      if (text[j]==' ' || text[j]=='\n' || text[j]=='|') break;
      wordChars++;
    }
    if (cx + (int32_t)(wordChars * CHAR_W) > lineRight) {
      cx = TEXT_X;
      cy += LINE_H;
    }
  }

  if (cx + CHAR_W > lineRight) {
    cx = TEXT_X;
    cy += LINE_H;
  }

  return true; // char normal, deve ser desenhado
}

// Fullredraw da area de texto — usado no firstFrame e ao acelerar.
// Tambem sincroniza cursorCx/cursorCy com a posicao final.
void redrawTextArea(
  const char* text,
  uint16_t    visibleChars,
  bool        textComplete,
  bool        hasQuestion
) {
  lcd.fillRect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, COLOR_BG);
  lcd.setTextFont(1);
  lcd.setTextSize(2);
  lcd.setTextColor(COLOR_BLACK, COLOR_BG);

  int32_t  cx      = TEXT_X;
  int32_t  cy      = TEXT_Y;
  uint16_t printed = 0;

  for (uint16_t i = 0; text[i] != '\0' && printed < visibleChars; i++) {
    bool draw = advanceCursor(text, i, cx, cy);
    printed++;
    if (!draw) continue;
    lcd.setCursor(cx, cy);
    lcd.print(text[i]);
    cx += CHAR_W;
  }

  // sincroniza estado do cursor
  cursorCx = cx;
  cursorCy = cy;

  if (!textComplete) {
    lcd.fillRect(cursorCx, cursorCy + 2, 6, 12, COLOR_PINK_DARK);
  }

  if (textComplete && !hasQuestion) {
    lcd.setTextFont(1);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_PINK_DARK, COLOR_BG);
    lcd.setCursor(218, 148);
    lcd.print("v");
  }
}

void redrawOptions(
  const char* firstOption,
  const char* secondOption,
  uint8_t     selectedOption
) {
  lcd.fillRect(OPT1_X, OPT_Y, OPT_W + (OPT2_X - OPT1_X - OPT_W) + OPT_W, OPT_H, COLOR_BG);
  lcd.setTextFont(1);
  lcd.setTextSize(2);

  if (selectedOption == 0) {
    lcd.fillRect(OPT1_X, OPT_Y, OPT_W, OPT_H, COLOR_BLACK);
    lcd.setTextColor(COLOR_BG, COLOR_BLACK);
  } else {
    lcd.fillRect(OPT1_X, OPT_Y, OPT_W, OPT_H, COLOR_BG);
    lcd.drawRect(OPT1_X, OPT_Y, OPT_W, OPT_H, COLOR_BLACK);
    lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  }
  lcd.setCursor(OPT1_X + 6, OPT_Y + 4);
  lcd.print(firstOption);

  if (selectedOption == 1) {
    lcd.fillRect(OPT2_X, OPT_Y, OPT_W, OPT_H, COLOR_BLACK);
    lcd.setTextColor(COLOR_BG, COLOR_BLACK);
  } else {
    lcd.fillRect(OPT2_X, OPT_Y, OPT_W, OPT_H, COLOR_BG);
    lcd.drawRect(OPT2_X, OPT_Y, OPT_W, OPT_H, COLOR_BLACK);
    lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  }
  lcd.setCursor(OPT2_X + 6, OPT_Y + 4);
  lcd.print(secondOption);
}

void drawSerialDialogPreview(
  const char* speaker,
  const char* text,
  uint16_t    visibleChars,
  bool        hasQuestion,
  const char* firstOption,
  const char* secondOption,
  uint8_t     selectedOption,
  bool        textComplete
) {
  const uint16_t total = textLength(text);
  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| "); Serial.println(speaker);
  Serial.println("+--------------------------------------+");
  Serial.print("| ");
  for (uint16_t i = 0; text[i] != '\0' && i < visibleChars; i++) Serial.print(text[i]);
  if (!textComplete) Serial.print("_");
  Serial.println();
  Serial.println("|");
  if (hasQuestion && textComplete) {
    Serial.print("| "); Serial.print(selectedOption == 0 ? "> " : "  "); Serial.println(firstOption);
    Serial.print("| "); Serial.print(selectedOption == 1 ? "> " : "  "); Serial.println(secondOption);
  } else if (textComplete) {
    Serial.println("| [CONFIRMAR] continuar");
  } else {
    Serial.print("| digitando "); Serial.print(visibleChars); Serial.print("/"); Serial.println(total);
  }
  Serial.println("+--------------------------------------+");
}

} // namespace

// --- DisplayDevice -----------------------------------------------------------

void DisplayDevice::begin() {
  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);
  lcd.setBrightness(220);
  lcd.fillScreen(COLOR_BG);
  Serial.println("[display] ST7789 240x240 via LovyanGFX");
}

void DisplayDevice::showBoot(const String& message) {
  Serial.print("[boot] "); Serial.println(message);
  lcd.fillScreen(COLOR_BG);
  drawScreenBorder();
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  lcd.setCursor(34, 96); lcd.print("PEDIDO.EXE");
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_PINK_DARK, COLOR_BG);
  lcd.setCursor(68, 126); lcd.print(message);
}

void DisplayDevice::showScene(const Scene& scene) {
  Serial.print("[scene] "); Serial.print(scene.id); Serial.print(": "); Serial.println(scene.text);
}

void DisplayDevice::showDialog(
  const char* speaker,
  const char* text,
  uint16_t    visibleChars,
  bool        hasQuestion,
  const char* firstOption,
  const char* secondOption,
  uint8_t     selectedOption,
  bool        textComplete,
  bool        firstFrame
) {
  if (firstFrame) {
    lcd.fillScreen(COLOR_BG);
    drawScreenBorder();
    // reseta cursor para o inicio da area de texto
    cursorCx = TEXT_X;
    cursorCy = TEXT_Y;
    if (hasQuestion && textComplete) {
      redrawOptions(firstOption, secondOption, selectedOption);
    }
  }

  redrawTextArea(text, visibleChars, textComplete, hasQuestion);

  if (hasQuestion && textComplete) {
    redrawOptions(firstOption, secondOption, selectedOption);
  }

  drawSerialDialogPreview(
    speaker, text, visibleChars,
    hasQuestion, firstOption, secondOption,
    selectedOption, textComplete
  );
}

// Desenha apenas o proximo caractere sem apagar nada.
// visibleChars ja foi incrementado — representa o indice do char recem adicionado.
void DisplayDevice::drawNextChar(
  const char* text,
  uint16_t    visibleChars,
  bool        textComplete
) {
  if (visibleChars == 0 || text == nullptr) return;

  // cursorCx/cursorCy ja esta na posicao correta para o char atual.
  // Nao recalcula chars anteriores — apenas processa o char em visibleChars-1.
  uint16_t index = visibleChars - 1;
  char c = text[index];
  if (c == '\0') return;

  // apaga cursor antigo antes de qualquer movimentacao
  lcd.fillRect(cursorCx, cursorCy + 2, 6, 12, COLOR_BG);

  bool draw = advanceCursor(text, index, cursorCx, cursorCy);

  if (draw) {
    lcd.setTextFont(1);
    lcd.setTextSize(2);
    lcd.setTextColor(COLOR_BLACK, COLOR_BG);
    lcd.setCursor(cursorCx, cursorCy);
    lcd.print(c);
    cursorCx += CHAR_W;
  }

  // cursor na nova posicao ou indicador de completo
  if (textComplete) {
    lcd.setTextFont(1);
    lcd.setTextSize(1);
    lcd.setTextColor(COLOR_PINK_DARK, COLOR_BG);
    lcd.setCursor(218, 148);
    lcd.print("v");
  } else {
    lcd.fillRect(cursorCx, cursorCy + 2, 6, 12, COLOR_PINK_DARK);
  }
}

void DisplayDevice::redrawOptions(
  const char* firstOption,
  const char* secondOption,
  uint8_t     selectedOption
) {
  ::redrawOptions(firstOption, secondOption, selectedOption);
}

void DisplayDevice::showEnding(
  uint8_t     pageIndex,
  uint8_t     pageCount,
  const char* title,
  const char* body
) {
  Serial.println();
  Serial.println("+--------------------------------------+");
  Serial.print("| final "); Serial.print(pageIndex + 1); Serial.print("/"); Serial.println(pageCount);
  Serial.print("| "); Serial.println(title);
  Serial.println("+--------------------------------------+");
  Serial.print("| "); Serial.println(body);
  Serial.println("+--------------------------------------+");

  lcd.fillScreen(COLOR_BG);
  drawScreenBorder();
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  lcd.setCursor(18, 70); lcd.print(title);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  lcd.setCursor(18, 106); lcd.print(body);
  lcd.setTextColor(COLOR_PINK_DARK, COLOR_BG);
  lcd.setCursor(18, 220); lcd.print(pageIndex + 1); lcd.print("/"); lcd.print(pageCount);
}

void DisplayDevice::showLocked(const String& message) {
  Serial.print("[locked] "); Serial.println(message);
  lcd.fillScreen(COLOR_BLACK);
  lcd.setTextColor(COLOR_RED, COLOR_BLACK);
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setCursor(28, 104); lcd.print("BLOQUEADO");
}

void DisplayDevice::drawImageArea(const char* imagePath) {
  clearImageArea();
  if (imagePath == nullptr || imagePath[0] == '\0') return;

  fs::FS* fs = storage.fileSystemFor(imagePath);
  if (fs == nullptr) {
    Serial.print("[display] image not found: "); Serial.println(imagePath);
    return;
  }
  Serial.print("[display] draw BMP "); Serial.println(imagePath);
  lcd.drawBmpFile(*fs, imagePath, IMAGE_X, IMAGE_AREA_Y);
}

void DisplayDevice::drawGifLine(int16_t x, int16_t y, int16_t width, const uint16_t* pixels) {
  if (pixels == nullptr || width <= 0) return;
  if (y < IMAGE_AREA_Y || y >= IMAGE_AREA_Y + IMAGE_AREA_H) return;
  if (x < IMAGE_X) {
    int16_t clipped = IMAGE_X - x;
    pixels += clipped;
    width  -= clipped;
    x       = IMAGE_X;
  }
  int16_t maxRight = IMAGE_X + IMAGE_SIZE;
  if (x + width > maxRight) width = maxRight - x;
  if (width <= 0) return;
  lcd.pushImage(x, y, width, 1, pixels);
}

// --- AudioDevice -------------------------------------------------------------

void AudioDevice::begin() {
  storage.begin();
  audioOutput = new AudioOutputI2S();
  audioOutput->SetPinout(AUDIO_PIN_BCLK, AUDIO_PIN_LRC, AUDIO_PIN_DIN);
  audioOutput->SetOutputModeMono(true);
  audioOutput->SetGain(AUDIO_DEFAULT_GAIN);
  mp3 = new AudioGeneratorMP3();
  Serial.print("[audio] I2S MAX98357A ready: BCLK=");
  Serial.print(AUDIO_PIN_BCLK); Serial.print(" LRC=");
  Serial.print(AUDIO_PIN_LRC);  Serial.print(" DIN=");
  Serial.println(AUDIO_PIN_DIN);
}

void AudioDevice::play(const char* audioPath) {
  if (audioPath == nullptr || audioPath[0] == '\0') return;
  Serial.print("[audio] play "); Serial.println(audioPath);
  stop();
  fs::FS* fs = storage.fileSystemFor(audioPath);
  if (fs == nullptr) {
    Serial.print("[audio] file not found: "); Serial.println(audioPath);
    return;
  }
  if (mp3 == nullptr || audioOutput == nullptr) {
    Serial.println("[audio] not initialized"); return;
  }
  audioFile = new AudioFileSourceFS(*fs, audioPath);
  if (!mp3->begin(audioFile, audioOutput)) {
    Serial.println("[audio] MP3 decoder failed to start");
    delete audioFile; audioFile = nullptr; return;
  }
  lastAudioStatusLog = millis();
  Serial.println("[audio] MP3 decoder started");
}

void AudioDevice::stop() {
  if (mp3 != nullptr && mp3->isRunning()) mp3->stop();
  if (audioFile != nullptr) { delete audioFile; audioFile = nullptr; }
}

void AudioDevice::update() {
  if (mp3 == nullptr || !mp3->isRunning()) return;
  uint32_t now = millis();
  if (now - lastAudioStatusLog > 5000) {
    lastAudioStatusLog = now;
    Serial.println("[audio] playing");
  }
  if (!mp3->loop()) { stop(); Serial.println("[audio] finished"); }
}

bool AudioDevice::isPlaying() { return mp3 != nullptr && mp3->isRunning(); }

void AudioDevice::setGain(float gain) {
  if (audioOutput != nullptr) audioOutput->SetGain(gain);
}

// --- ButtonInput -------------------------------------------------------------

void ButtonInput::begin() {
  pinMode(NAVIGATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN,  INPUT_PULLUP);
}

SceneAction ButtonInput::readAction() {
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'c') return SceneAction::Confirm;
    if (input == 'n') return SceneAction::Navigate;
    if (input == 'i') return SceneAction::OpenHome;
  }
  if (isPressed(CONFIRM_BUTTON_PIN))  { delay(180); return SceneAction::Confirm; }
  if (isPressed(NAVIGATE_BUTTON_PIN)) { delay(180); return SceneAction::Navigate; }
  return SceneAction::None;
}