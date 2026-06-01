#include <Arduino.h>
#include "AppConfig.h"
#include "Devices.h"
#include <AnimatedGIF.h>
#include <SPIFFS.h>

namespace {
DisplayDevice display;
AudioDevice audio;
ButtonInput buttons;
AnimatedGIF gif;
File gifFile;
bool gifActive = false;
uint32_t lastGifFrameAt = 0;

void* gifOpenFile(const char* filename, int32_t* size) {
  gifFile = SPIFFS.open(filename, "r");
  if (!gifFile) {
    Serial.print("[gif] open failed: ");
    Serial.println(filename);
    *size = 0;
    return nullptr;
  }

  *size = gifFile.size();
  return &gifFile;
}

void gifCloseFile(void* handle) {
  File* file = static_cast<File*>(handle);
  if (file != nullptr) {
    file->close();
  }
}

int32_t gifReadFile(GIFFILE* file, uint8_t* buffer, int32_t length) {
  File* handle = static_cast<File*>(file->fHandle);
  if (handle == nullptr) return 0;

  int32_t remaining = file->iSize - file->iPos;
  if (length > remaining) length = remaining;
  if (length <= 0) return 0;

  int32_t bytesRead = handle->read(buffer, length);
  file->iPos = handle->position();
  return bytesRead;
}

int32_t gifSeekFile(GIFFILE* file, int32_t position) {
  File* handle = static_cast<File*>(file->fHandle);
  if (handle == nullptr) return 0;

  handle->seek(position);
  file->iPos = handle->position();
  return file->iPos;
}

void gifDraw(GIFDRAW* draw) {
  if (draw == nullptr) return;

  uint16_t* palette = draw->pPalette;
  if (palette == nullptr) return;

  uint8_t* source = draw->pPixels;
  if (source == nullptr) return;

  constexpr int16_t IMAGE_W = 84;
  constexpr int16_t IMAGE_H = 84;
  constexpr int16_t imageX  = 78;
  constexpr int16_t imageY  = 156;

  // iX e iWidth vem do frame — podem extrapolar o canvas se o GIF
  // tiver subframes posicionados. Clampamos tudo antes de qualquer acesso.
  int16_t srcOffset = 0;
  int16_t frameX    = (int16_t)draw->iX;
  int16_t frameW    = (int16_t)draw->iWidth;
  int16_t frameY    = (int16_t)draw->iY + (int16_t)draw->y;

  // corta pela esquerda
  if (frameX < 0) {
    srcOffset = -frameX;
    frameW   -= srcOffset;
    frameX    = 0;
  }

  // corta pela direita
  if (frameX + frameW > IMAGE_W) {
    frameW = IMAGE_W - frameX;
  }

  // descarta frame fora da area vertical
  if (frameY < 0 || frameY >= IMAGE_H) return;

  // nada a desenhar
  if (frameW <= 0) return;

  // garante que nao vai estourar o buffer de linha
  if (frameW > IMAGE_W) frameW = IMAGE_W;

  int16_t outputX = imageX + frameX;
  int16_t outputY = imageY + frameY;

  uint16_t line[IMAGE_W];

  if (draw->ucHasTransparency) {
    int16_t runStart  = -1;
    int16_t runLength = 0;

    for (int16_t x = 0; x < frameW; x++) {
      uint8_t index = source[srcOffset + x];

      if (index == draw->ucTransparent) {
        if (runLength > 0) {
          display.drawGifLine(outputX + runStart, outputY, runLength, &line[runStart]);
          runStart  = -1;
          runLength = 0;
        }
        continue;
      }

      line[x] = palette[index];
      if (runStart < 0) runStart = x;
      runLength++;
    }

    if (runLength > 0) {
      display.drawGifLine(outputX + runStart, outputY, runLength, &line[runStart]);
    }
    return;
  }

  for (int16_t x = 0; x < frameW; x++) {
    line[x] = palette[source[srcOffset + x]];
  }

  display.drawGifLine(outputX, outputY, frameW, line);
}

// -----------------------------------------------------------------------------

struct DialogStep {
  const char* speaker;
  const char* text;
  bool hasQuestion;
  const char* firstOption;
  const char* secondOption;
  const char* audioPath;
  const char* imagePath;
  bool isGif;
};

DialogStep dialog[] = {
  {
    "SISTEMA",
    "Oi lila, tudo bem?",
    false,
    "",
    "",
    "/audio/minhanamorada20s.mp3",
    "/img/gato3bmp.bmp",
    false
  },
  {
    "EU",
    "desenvolvi esse projeto pensando em uma maneira de presentear uma pessoa tao especial",
    false,
    "",
    "",
    "",
    "/img/gato4bmp.bmp",
    false
  },
  {
    "EU",
    "desde muito tempo venho pensando em uma maneira de enviar essa mensagem de uma maneira legal",
    false,
    "",
    "",
    "",
    "/img/cena2.bmp",
    false
  },
  {
    "EU",
    "e essa foi a forma que encontrei.",
    false,
    "",
    "",
    "",
    "/img/cena2.bmp",
    false
  },
    {
    "EU",
    "toda a ideia, estetica, eletronica e codigo dele foram montadas do zero por mim",
    false,
    "",
    "",
    "",
    "/img/cena2.bmp",
    false
  },
    {
    "EU",
    "foi bem dificil mas nada se comparou a dificuldade de encontrar as palavras que queria colocar aqui",
    false,
    "",
    "",
    "",
    "/img/cena2.bmp",
    false
  },
  {
    "PERGUNTA",
    "Quer namorar comigo?",
    true,
    "Sim",
    "Nao",
    "",
    "/img/cena3.gif",
    true
  },
};

constexpr uint8_t DIALOG_COUNT = sizeof(dialog) / sizeof(dialog[0]);
constexpr uint16_t TYPEWRITER_INTERVAL_MS = 45;

uint8_t  currentDialogIndex = 0;
uint8_t  selectedOption     = 0;
uint16_t visibleChars       = 0;
uint32_t lastTypewriterTick = 0;
bool     finalScreen        = false;
bool     firstFrame         = true;

AppConfig loadRemoteConfig() {
  AppConfig config;
  config.mode    = RemoteMode::Normal;
  config.variant = "default";
  config.message = "dialogo local";
  return config;
}

uint16_t textLength(const char* text) {
  uint16_t length = 0;
  while (text[length] != '\0') length++;
  return length;
}

DialogStep& currentDialog() {
  return dialog[currentDialogIndex];
}

bool isTextComplete() {
  return visibleChars >= textLength(currentDialog().text);
}

void stopGif() {
  if (gifActive) {
    gif.close();
    gifActive = false;
  }
  if (gifFile) {
    gifFile.close();
  }
}

void startGif(const char* imagePath) {
  stopGif();
  display.drawImageArea(nullptr);

  if (imagePath == nullptr || imagePath[0] == '\0') return;

  if (!SPIFFS.exists(imagePath)) {
    Serial.print("[gif] file not found: ");
    Serial.println(imagePath);
    return;
  }

  if (!gif.open(imagePath, gifOpenFile, gifCloseFile, gifReadFile, gifSeekFile, gifDraw)) {
    Serial.print("[gif] decoder failed: ");
    Serial.println(imagePath);
    return;
  }

  gifActive       = true;
  lastGifFrameAt  = millis();
  Serial.print("[gif] started ");
  Serial.println(imagePath);
}

void updateGif() {
  if (!gifActive) return;

  if (!gif.playFrame(false, nullptr)) {
    gif.reset();
  }
}

void drawDialogAsset() {
  DialogStep& step = currentDialog();

  if (step.imagePath == nullptr || step.imagePath[0] == '\0') {
    stopGif();
    display.drawImageArea(nullptr);
    return;
  }

  if (step.isGif) {
    startGif(step.imagePath);
    return;
  }

  stopGif();
  display.drawImageArea(step.imagePath);
}

void lockDevice(const String& reason) {
  audio.stop();
  display.showLocked(reason);
  esp_sleep_enable_timer_wakeup(30ULL * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void renderDialog() {
  DialogStep& step = currentDialog();
  display.showDialog(
    step.speaker,
    step.text,
    visibleChars,
    step.hasQuestion,
    step.firstOption,
    step.secondOption,
    selectedOption,
    isTextComplete(),
    firstFrame
  );
  firstFrame = false;
}

void enterDialog(uint8_t index) {
  currentDialogIndex  = index;
  selectedOption      = 0;
  visibleChars        = 0;
  lastTypewriterTick  = millis();
  firstFrame          = true;

  if (currentDialog().audioPath[0] != '\0') {
    audio.play(currentDialog().audioPath);
  }

  renderDialog();
  drawDialogAsset();
}

void showFinalAnswer(const char* title, const char* body) {
  finalScreen = true;
  stopGif();
  audio.stop();
  display.showEnding(0, 1, title, body);
}

void handleConfirm() {
  if (finalScreen) return;

  if (!isTextComplete()) {
    visibleChars = textLength(currentDialog().text);
    renderDialog();
    return;
  }

  if (currentDialog().hasQuestion) {
    if (selectedOption == 0) {
      showFinalAnswer("RESPOSTA", "Final SIM selecionado. :)");
    } else {
      showFinalAnswer("RESPOSTA", "Final NAO selecionado.");
    }
    return;
  }

  if (currentDialogIndex + 1 < DIALOG_COUNT) {
    enterDialog(currentDialogIndex + 1);
  }
}

void handleNavigate() {
  if (!finalScreen && currentDialog().hasQuestion && isTextComplete()) {
    selectedOption = selectedOption == 0 ? 1 : 0;
    renderDialog();
  }
}

void updateTypewriter() {
  if (finalScreen || isTextComplete()) return;

  const uint32_t now = millis();
  if (now - lastTypewriterTick < TYPEWRITER_INTERVAL_MS) return;

  lastTypewriterTick = now;
  visibleChars++;
  renderDialog();
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  display.begin();
  gif.begin(GIF_PALETTE_RGB565_BE);
  audio.begin();
  buttons.begin();

  AppConfig config = loadRemoteConfig();
  display.showBoot(config.message);

  if (config.mode == RemoteMode::Locked) {
    lockDevice("bloqueado por configuracao remota");
  }

  enterDialog(0);
}

void loop() {
  audio.update();
  updateGif();

  SceneAction action = buttons.readAction();

  if (action == SceneAction::Confirm) {
    handleConfirm();
  } else if (action == SceneAction::Navigate) {
    handleNavigate();
  }

  updateTypewriter();
  audio.update();
  delay(5);
}