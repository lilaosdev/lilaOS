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
constexpr uint16_t COLOR_TERM_BG   = 0x1005;
constexpr uint16_t COLOR_TERM_DARK = 0x380D;
constexpr uint16_t COLOR_TERM_TEXT = 0xFE7A;
constexpr uint16_t COLOR_TERM_DIM  = 0xB135;

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

  // preview serial apenas no firstFrame — evita Serial.print a cada char do typewriter
  // que bloqueava a CPU e causava underrun no decoder MP3
  if (firstFrame) {
    drawSerialDialogPreview(
      speaker, text, visibleChars,
      hasQuestion, firstOption, secondOption,
      selectedOption, textComplete
    );
  }
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

// --- DisplayDevice system UI -------------------------------------------------

// helpers de tema — fora do namespace anonimo pois sao usados pelas funcoes publicas abaixo
static uint16_t systemBg(uint8_t themeId)     { return themeId == 0 ? COLOR_BG       : COLOR_BLACK; }
static uint16_t systemText(uint8_t themeId)   { return themeId == 0 ? COLOR_BLACK    : 0xFFFF; }
static uint16_t systemAccent(uint8_t themeId) { return themeId == 0 ? COLOR_PINK_DARK : COLOR_RED; }
static uint16_t systemSoft(uint8_t themeId)   { return themeId == 0 ? COLOR_PINK     : 0x8000; }
static const char* themeName(uint8_t themeId) { return themeId == 0 ? "LILA" : "KALSING"; }

static void drawSystemBorder(uint8_t themeId) {
  uint16_t accent = systemAccent(themeId);
  uint16_t soft   = systemSoft(themeId);
  lcd.drawRect(4, 4, 232, 232, accent);
  lcd.drawRect(7, 7, 226, 226, soft);
  lcd.fillRect(4,   4,   8, 8, accent);
  lcd.fillRect(228, 4,   8, 8, accent);
  lcd.fillRect(4,   228, 8, 8, accent);
  lcd.fillRect(228, 228, 8, 8, accent);
}

static void drawSystemShell(const char* title, uint8_t themeId) {
  uint16_t bg     = systemBg(themeId);
  uint16_t fg     = systemText(themeId);
  uint16_t accent = systemAccent(themeId);
  lcd.fillScreen(bg);
  drawSystemBorder(themeId);
  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(accent, bg);
  lcd.setCursor(18, 14); lcd.print("pedido.os / "); lcd.print(themeName(themeId));
  lcd.setTextSize(2);
  lcd.setTextColor(fg, bg);
  lcd.setCursor(18, 32); lcd.print(title);
}

static void drawSelectableRow(int32_t y, const char* label, bool selected, uint8_t themeId) {
  uint16_t bg     = systemBg(themeId);
  uint16_t fg     = systemText(themeId);
  uint16_t accent = systemAccent(themeId);
  uint16_t soft   = systemSoft(themeId);
  if (selected) {
    lcd.fillRect(18, y, 204, 24, accent);
    lcd.setTextColor(bg, accent);
  } else {
    lcd.fillRect(18, y, 204, 24, bg);
    lcd.drawRect(18, y, 204, 24, soft);
    lcd.setTextColor(fg, bg);
  }
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setCursor(26, y + 5); lcd.print(label);
}

// pixel-art: nota musical
static void drawIconMusic(int32_t x, int32_t y, uint16_t color) {
  lcd.fillRect(x,     y,      3, 14, color); // haste esquerda
  lcd.fillRect(x,     y,      8,  3, color); // barra horizontal
  lcd.fillRect(x + 8, y,      3, 12, color); // haste direita
  lcd.fillRect(x - 3, y + 11, 9,  4, color); // bolinha esquerda
  lcd.fillRect(x + 5, y +  9, 9,  4, color); // bolinha direita
}

// pixel-art: engrenagem simplificada
static void drawIconSettings(int32_t x, int32_t y, uint16_t color, uint16_t bg) {
  lcd.fillRect(x + 3, y,      4,  3, color); // dente cima
  lcd.fillRect(x,     y + 3,  10, 7, color); // corpo
  lcd.fillRect(x + 3, y + 10, 4,  3, color); // dente baixo
  lcd.fillRect(x + 4, y + 4,  2,  5, bg);    // furo central
}

// pixel-art: carta (Ás)
static void drawIconCard(int32_t x, int32_t y, uint16_t color, uint16_t bg) {
  lcd.drawRect(x, y, 10, 14, color);
  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(color, bg);
  lcd.setCursor(x + 2, y + 2); lcd.print("A");
  lcd.setCursor(x + 3, y + 8); lcd.print("*");
}

// pixel-art: ícone play (triângulo em escada)
static void drawIconTerminal(int32_t x, int32_t y, uint16_t color) {
  lcd.drawRect(x, y, 14, 12, color);
  lcd.drawLine(x + 2, y + 3, x + 5, y + 5, color);
  lcd.drawLine(x + 2, y + 7, x + 5, y + 5, color);
  lcd.drawLine(x + 7, y + 8, x + 11, y + 8, color);
}

static void drawIconPlay(int32_t x, int32_t y, uint16_t color) {
  lcd.fillRect(x,     y,      3, 10, color);
  lcd.fillRect(x + 3, y + 2,  3,  6, color);
  lcd.fillRect(x + 6, y + 4,  3,  2, color);
}

static void drawLauncherRow(
  int32_t     y,
  const char* label,
  uint8_t     iconId,   // 0=music 1=settings 2=terminal 3=card
  bool        selected,
  uint8_t     themeId
) {
  constexpr int32_t ROW_X   = 18;
  constexpr int32_t ROW_W   = 204;
  constexpr int32_t ROW_H   = 36;
  constexpr int32_t ICON_X  = 30;
  constexpr int32_t LABEL_X = 52;

  uint16_t bg     = systemBg(themeId);
  uint16_t fg     = systemText(themeId);
  uint16_t accent = systemAccent(themeId);
  uint16_t soft   = systemSoft(themeId);

  uint16_t rowBg = selected ? accent : bg;
  uint16_t rowFg = selected ? bg     : fg;

  lcd.fillRect(ROW_X, y, ROW_W, ROW_H, rowBg);
  if (!selected) lcd.drawRect(ROW_X, y, ROW_W, ROW_H, soft);

  int32_t iconY = y + 11;
  if      (iconId == 0) drawIconMusic(ICON_X,    iconY, rowFg);
  else if (iconId == 1) drawIconSettings(ICON_X, iconY, rowFg, rowBg);
  else if (iconId == 2) drawIconTerminal(ICON_X, iconY, rowFg);
  else                  drawIconCard(ICON_X,     iconY, rowFg, rowBg);

  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(rowFg, rowBg);
  lcd.setCursor(LABEL_X, y + 13);
  lcd.print(label);
}

void DisplayDevice::showStart() {
  lcd.fillScreen(COLOR_BG);
  drawScreenBorder();
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setTextColor(COLOR_BLACK, COLOR_BG);
  lcd.setCursor(34, 90); lcd.print("PEDIDO.EXE");
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_PINK_DARK, COLOR_BG);
  lcd.setCursor(46, 118); lcd.print("pressione [c]");
  lcd.setCursor(54, 132); lcd.print("para iniciar");
}

void DisplayDevice::showLauncher(uint8_t selectedApp, uint8_t themeId, const char* nowPlaying) {
  uint16_t bg     = systemBg(themeId);
  uint16_t accent = systemAccent(themeId);
  uint16_t soft   = systemSoft(themeId);
  uint16_t fg     = systemText(themeId);

  lcd.fillScreen(bg);
  drawSystemBorder(themeId);

  // rows sem header nem título
  drawLauncherRow(60,  "musicas",   0, selectedApp == 0, themeId);
  drawLauncherRow(102, "ajustes",   1, selectedApp == 1, themeId);
  drawLauncherRow(144, "terminal",  2, selectedApp == 2, themeId);

  // barra now playing
  constexpr int32_t BAR_X = 12;
  constexpr int32_t BAR_Y = 196;
  constexpr int32_t BAR_W = 216;
  constexpr int32_t BAR_H = 32;

  lcd.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, bg);
  lcd.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, soft);

  drawIconPlay(BAR_X + 8, BAR_Y + 11, accent);

  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(accent, bg);
  lcd.setCursor(BAR_X + 22, BAR_Y + 8);
  lcd.print("NOW PLAYING");

  lcd.setTextColor(fg, bg);
  lcd.setCursor(BAR_X + 22, BAR_Y + 19);
  if (nowPlaying && nowPlaying[0] != '\0') lcd.print(nowPlaying);
  else lcd.print("...");
}

void DisplayDevice::showSettings(uint8_t selectedRow, uint8_t volumeLevel, uint8_t themeId) {
  drawSystemShell("ajustes", themeId);
  char volumeLabel[20]; snprintf(volumeLabel, sizeof(volumeLabel), "volume %u/5", volumeLevel);
  char themeLabel[24];  snprintf(themeLabel,  sizeof(themeLabel),  "tema %s", themeName(themeId));
  drawSelectableRow(78,  volumeLabel, selectedRow == 0, themeId);
  drawSelectableRow(108, themeLabel,  selectedRow == 1, themeId);
  drawSelectableRow(138, "voltar",    selectedRow == 2, themeId);
  uint16_t bg     = systemBg(themeId);
  uint16_t accent = systemAccent(themeId);
  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(accent, bg);
  lcd.setCursor(20, 206); lcd.print("c altera / abre  n navega");
}

void DisplayDevice::showMusicPlayer(
  const char* heading,
  const char* trackName,
  uint8_t     selectedControl,
  bool        paused,
  uint8_t     themeId
) {
  drawSystemShell("musicas", themeId);
  uint16_t bg     = systemBg(themeId);
  uint16_t fg     = systemText(themeId);
  uint16_t accent = systemAccent(themeId);
  uint16_t soft   = systemSoft(themeId);

  lcd.setTextFont(1); lcd.setTextSize(1);
  lcd.setTextColor(accent, bg);
  lcd.setCursor(60, 58); lcd.print(heading);

  lcd.drawRect(78, 74, 84, 84, soft);
  drawIconMusic(99, 92, accent);

  lcd.setTextColor(fg, bg);
  lcd.setCursor(28, 166); lcd.print(trackName);
  lcd.setCursor(94, 180); lcd.print(paused ? "pausado" : "tocando");

  const char* controls[] = { "<", paused ? ">" : "||", ">>", "home" };
  int xs[] = { 28, 76, 124, 166 };
  for (uint8_t i = 0; i < 4; i++) {
    bool sel = selectedControl == i;
    int w = (i == 3) ? 50 : 34;
    if (sel) {
      lcd.fillRect(xs[i], 198, w, 22, accent);
      lcd.setTextColor(bg, accent);
    } else {
      lcd.fillRect(xs[i], 198, w, 22, bg);
      lcd.drawRect(xs[i], 198, w, 22, soft);
      lcd.setTextColor(fg, bg);
    }
    lcd.setTextSize(1);
    lcd.setCursor(xs[i] + 8, 205); lcd.print(controls[i]);
  }
}

static void drawTerminalShell() {
  lcd.fillScreen(COLOR_TERM_BG);
  lcd.drawRect(4, 4, 232, 232, COLOR_TERM_DARK);
  lcd.drawRect(7, 7, 226, 226, COLOR_TERM_DIM);
  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(16, 14); lcd.print("pedido.os / terminal");
}

static void drawTerminalPrompt(const char* command, bool cursorVisible) {
  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(16, 34); lcd.print("lila@pedido:~$ ");
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.print(command);
  if (cursorVisible) {
    lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
    lcd.print(" >");
  }
}

static void drawEsp32Asset(int32_t x, int32_t y) {
  lcd.drawRect(x, y, 58, 70, COLOR_TERM_DIM);
  lcd.drawRect(x + 4, y + 4, 50, 62, COLOR_TERM_DARK);
  lcd.fillRect(x + 16, y + 10, 26, 20, COLOR_TERM_DARK);
  lcd.drawRect(x + 16, y + 10, 26, 20, COLOR_TERM_TEXT);
  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(x + 21, y + 16); lcd.print("ESP");
  for (uint8_t i = 0; i < 9; i++) {
    lcd.fillRect(x - 3, y + 8 + i * 6, 3, 2, COLOR_TERM_DIM);
    lcd.fillRect(x + 58, y + 8 + i * 6, 3, 2, COLOR_TERM_DIM);
  }
  lcd.drawRect(x + 20, y + 42, 18, 10, COLOR_TERM_DIM);
  lcd.drawLine(x + 24, y + 58, x + 34, y + 58, COLOR_TERM_TEXT);
}

static void drawTerminalNeofetch() {
  drawEsp32Asset(18, 62);
  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(88, 60); lcd.print("lila-gift");
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(88, 72); lcd.print("----------");
  lcd.setCursor(88, 88); lcd.print("MCU: ESP32");
  lcd.setCursor(88, 100); lcd.print("DISPLAY: ST7789");
  lcd.setCursor(88, 112); lcd.print("RES: 240x240");
  lcd.setCursor(88, 124); lcd.print("AUDIO: MAX98357A");
  lcd.setCursor(88, 136); lcd.print("FS: microSD");
  lcd.setCursor(88, 148); lcd.print("UI: LovyanGFX");
  lcd.setCursor(88, 160); lcd.print("BTN: NAV + OK");
}

static void drawTerminalStats() {
  lcd.setTextFont(1);
  lcd.setTextSize(1);
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(18, 62); lcd.print("STATS");
  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(18, 82); lcd.print("linhas de codigo: ...");
  lcd.setCursor(18, 98); lcd.print("horas de trabalho: ...");
  lcd.setCursor(18, 114); lcd.print("testes de hardware: ...");
  lcd.setCursor(18, 130); lcd.print("musicas escolhidas: ...");
  lcd.setCursor(18, 146); lcd.print("mensagens escritas: ...");
  lcd.setCursor(18, 162); lcd.print("carinho investido: muito");
}

void DisplayDevice::showTerminal(uint8_t commandIndex, bool executed, bool cursorVisible, uint8_t themeId) {
  (void)themeId;
  const char* command = commandIndex == 0 ? "NEOFETCH" : commandIndex == 1 ? "STATS" : "EXIT";
  drawTerminalShell();
  drawTerminalPrompt(command, !executed && cursorVisible);

  if (executed) {
    if (commandIndex == 0) drawTerminalNeofetch();
    else                  drawTerminalStats();
    lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
    lcd.setCursor(18, 208); lcd.print("c limpa  n troca");
    return;
  }

  lcd.setTextColor(COLOR_TERM_TEXT, COLOR_TERM_BG);
  lcd.setCursor(18, 82);  lcd.print("n alterna comando");
  lcd.setCursor(18, 98);  lcd.print("c executa");
  lcd.setCursor(18, 114); lcd.print("EXIT volta ao menu");
}

void DisplayDevice::showPlaceholderApp(const char* title, const char* body, uint8_t themeId) {
  drawSystemShell(title, themeId);
  uint16_t bg     = systemBg(themeId);
  uint16_t fg     = systemText(themeId);
  uint16_t accent = systemAccent(themeId);
  lcd.setTextFont(1); lcd.setTextSize(2);
  lcd.setTextColor(fg, bg);
  lcd.setCursor(28, 96); lcd.print(body);
  lcd.setTextSize(1);
  lcd.setTextColor(accent, bg);
  lcd.setCursor(36, 208); lcd.print("c ou i para voltar");
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
