#include <Arduino.h>
#include "AppConfig.h"
#include "Devices.h"
#include "StorageDevice.h"
#include <AnimatedGIF.h>

namespace {
DisplayDevice display;
AudioDevice audio;
ButtonInput buttons;
AnimatedGIF gif;
File gifFile;
bool gifActive = false;

enum class SystemScreen {
  Home,
  Music,
  Settings,
  Terminal,
  Placeholder,
  Start,
  Dialog,
  Final,
};

enum class ThemeId : uint8_t {
  Lila    = 0,
  Kalsing = 1,
};

struct MusicTrack {
  const char* title;
  const char* path;
};

constexpr MusicTrack LILA_PLAYLIST[] = {
  { "Minha Namorada20S", "/audio/papodeundaia.mp3" },
  { "Minha Namorada",    "/audio/Minha_Namorada_Mixado.mp3" },
  { "Safeword",          "/audio/Safeword_Mixado.mp3" },
  { "Romance",           "/audio/Romance_Mixado.mp3" },
  { "Carta ao Tom 74",   "/audio/Carta74_Mixado.mp3" },
  { "T.B.H",             "/audio/QWER_Mixado.mp3" },
};

constexpr MusicTrack KALSING_PLAYLIST[] = {
  { "Beamer Boy", "/audio/BeamerBoy_Mixado.mp3" },
  { "Harvest Moon", "/audio/Harvest_Moon_Mixado.mp3" },
  { "My Flaws Burn Through My Skin", "/audio/MyFlawsBurn_Mixado.mp3" },
  { "Fuck Love", "/audio/FuckLove_Mixado.mp3" },
  { "Witchblades", "/audio/Witchblades_Mixado.mp3" },
};

SystemScreen currentScreen   = SystemScreen::Home;
ThemeId      currentTheme    = ThemeId::Lila;
uint8_t      homeSelection   = 0;
uint8_t      settingsSelection = 0;
uint8_t      musicSelection  = 1;
uint8_t      terminalCommand = 0;
uint8_t      volumeLevel     = 3;
uint8_t      musicTrackIndex = 0;
bool         musicPaused     = false;
uint32_t     lastMusicRetryAt = 0;
bool         terminalExecuted = false;
bool         terminalCursorVisible = true;
uint32_t     lastTerminalBlinkAt = 0;

// --- GIF timing ---
// GIF so avanca frame quando o intervalo minimo passou,
// evitando que o SPI seja tomado a cada iteracao do loop.
constexpr uint32_t GIF_MIN_FRAME_MS = 40; // ~25fps maximo
uint32_t lastGifFrameAt = 0;

void stopGif();

const MusicTrack* currentPlaylist(uint8_t* count = nullptr) {
  if (currentTheme == ThemeId::Lila) {
    if (count) *count = sizeof(LILA_PLAYLIST) / sizeof(LILA_PLAYLIST[0]);
    return LILA_PLAYLIST;
  }
  if (count) *count = sizeof(KALSING_PLAYLIST) / sizeof(KALSING_PLAYLIST[0]);
  return KALSING_PLAYLIST;
}

const MusicTrack& currentTrack() {
  uint8_t count = 0;
  const MusicTrack* playlist = currentPlaylist(&count);
  if (musicTrackIndex >= count) musicTrackIndex = 0;
  return playlist[musicTrackIndex];
}

const char* musicHeading() {
  return currentTheme == ThemeId::Lila ? "musicas da lila" : "musicas do kalsing";
}

float gainForVolume(uint8_t level) {
  constexpr float gains[] = { 0.0f, 0.08f, 0.20f, 0.3f, 0.4f, 0.6f };
  if (level > 5) level = 5;
  return gains[level];
}

uint8_t themeByte() { return static_cast<uint8_t>(currentTheme); }

void applyVolume() { audio.setGain(gainForVolume(volumeLevel)); }

void playCurrentTrack() {
  if (musicPaused || volumeLevel == 0) { audio.stop(); return; }
  uint8_t count = 0;
  const MusicTrack* playlist = currentPlaylist(&count);
  for (uint8_t attempts = 0; attempts < count; attempts++) {
    const MusicTrack& track = playlist[musicTrackIndex];
    if (storage.exists(track.path)) { audio.play(track.path); return; }
    musicTrackIndex = (musicTrackIndex + 1) % count;
  }
  Serial.println("[music] no tracks found");
}

void restartThemePlaylist() {
  musicTrackIndex = 0;
  audio.stop();
  playCurrentTrack();
}

void nextTrack() {
  uint8_t count = 0;
  currentPlaylist(&count);
  musicTrackIndex = (musicTrackIndex + 1) % count;
  audio.stop();
  playCurrentTrack();
}

void previousTrack() {
  uint8_t count = 0;
  currentPlaylist(&count);
  musicTrackIndex = musicTrackIndex == 0 ? count - 1 : musicTrackIndex - 1;
  audio.stop();
  playCurrentTrack();
}

void togglePause() {
  musicPaused = !musicPaused;
  if (musicPaused) audio.stop();
  else playCurrentTrack();
}

void updateGlobalMusic() {
  if (musicPaused || volumeLevel == 0 || audio.isPlaying()) return;
  uint32_t now = millis();
  if (now - lastMusicRetryAt < 800) return;
  lastMusicRetryAt = now;
  playCurrentTrack();
}

void renderStart() {
  currentScreen = SystemScreen::Start;
  stopGif();
  display.showStart();
}

void renderHome() {
  currentScreen = SystemScreen::Home;
  stopGif();
  display.showLauncher(homeSelection, themeByte(), currentTrack().title);
}

void renderSettings() {
  currentScreen = SystemScreen::Settings;
  stopGif();
  display.showSettings(settingsSelection, volumeLevel, themeByte());
}

void renderTerminal() {
  currentScreen = SystemScreen::Terminal;
  stopGif();
  display.showTerminal(terminalCommand, terminalExecuted, terminalCursorVisible, themeByte());
}

void renderMusic() {
  currentScreen = SystemScreen::Music;
  stopGif();
  display.showMusicPlayer(
    musicHeading(), currentTrack().title,
    musicSelection, musicPaused || volumeLevel == 0, themeByte()
  );
}

void renderPlaceholder() {
  currentScreen = SystemScreen::Placeholder;
  stopGif();
  display.showPlaceholderApp("blackjack", "em breve", themeByte());
}

// --- GIF callbacks ---

void* gifOpenFile(const char* filename, int32_t* size) {
  gifFile = storage.open(filename, "r");
  if (!gifFile) {
    Serial.print("[gif] open failed: "); Serial.println(filename);
    *size = 0;
    return nullptr;
  }
  *size = gifFile.size();
  return &gifFile;
}

void gifCloseFile(void* handle) {
  File* file = static_cast<File*>(handle);
  if (file) file->close();
}

int32_t gifReadFile(GIFFILE* file, uint8_t* buffer, int32_t length) {
  File* handle = static_cast<File*>(file->fHandle);
  if (!handle) return 0;
  int32_t remaining = file->iSize - file->iPos;
  if (length > remaining) length = remaining;
  if (length <= 0) return 0;
  int32_t n = handle->read(buffer, length);
  file->iPos = handle->position();
  return n;
}

int32_t gifSeekFile(GIFFILE* file, int32_t position) {
  File* handle = static_cast<File*>(file->fHandle);
  if (!handle) return 0;
  handle->seek(position);
  file->iPos = handle->position();
  return file->iPos;
}

void gifDraw(GIFDRAW* draw) {
  if (!draw) return;
  uint16_t* palette = draw->pPalette;
  if (!palette) return;
  uint8_t* source = draw->pPixels;
  if (!source) return;

  constexpr int16_t IMAGE_W = 84;
  constexpr int16_t IMAGE_H = 84;
  constexpr int16_t imageX  = 78;
  constexpr int16_t imageY  = 156;

  int16_t srcOffset = 0;
  int16_t frameX    = (int16_t)draw->iX;
  int16_t frameW    = (int16_t)draw->iWidth;
  int16_t frameY    = (int16_t)draw->iY + (int16_t)draw->y;

  if (frameX < 0) { srcOffset = -frameX; frameW -= srcOffset; frameX = 0; }
  if (frameX + frameW > IMAGE_W) frameW = IMAGE_W - frameX;
  if (frameY < 0 || frameY >= IMAGE_H || frameW <= 0) return;
  if (frameW > IMAGE_W) frameW = IMAGE_W;

  uint16_t line[IMAGE_W];
  int16_t outputX = imageX + frameX;
  int16_t outputY = imageY + frameY;

  if (draw->ucHasTransparency) {
    int16_t runStart = -1, runLength = 0;
    for (int16_t x = 0; x < frameW; x++) {
      uint8_t index = source[srcOffset + x];
      if (index == draw->ucTransparent) {
        if (runLength > 0) {
          display.drawGifLine(outputX + runStart, outputY, runLength, &line[runStart]);
          runStart = -1; runLength = 0;
        }
        continue;
      }
      line[x] = palette[index];
      if (runStart < 0) runStart = x;
      runLength++;
    }
    if (runLength > 0) display.drawGifLine(outputX + runStart, outputY, runLength, &line[runStart]);
    return;
  }

  for (int16_t x = 0; x < frameW; x++) line[x] = palette[source[srcOffset + x]];
  display.drawGifLine(outputX, outputY, frameW, line);
}

// --- Dialog ---

struct DialogStep {
  const char* speaker;
  const char* text;
  bool        hasQuestion;
  const char* firstOption;
  const char* secondOption;
  const char* audioPath;
  const char* imagePath;
  bool        isGif;
};

DialogStep dialog[] = {
  { "SISTEMA",  "oi lila, tudo bem?",                                                                    false, "", "", "", "/img/gatinho8bmp.bmp",    false },
  { "EU",       "desenvolvi esse projeto pensando em uma maneira de presentear uma pessoa tao especial", false, "", "", "", "/img/gatinho7bmp.bmp",    false },
  { "EU",       "desde muito tempo venho pensando em como de enviar essa mensagem de uma maneira legal", false, "", "", "", "/img/gato5bmp.bmp",    false },
  { "EU",       "e essa foi a forma que encontrei. espero que goste :)",                                 false, "", "", "", "/img/gato4bmp.bmp",    false },
  { "EU",       "toda a ideia, estetica, eletronica e codigo dele foram montadas por mim",              false, "", "", "", "/img/gatinho6bmp.bmp", false },
  { "EU",       "foi bem dificil mas nada se comparou a dificuldade de encontrar as palavras que queria colocar aqui", false, "", "", "", "/img/gato1bmptrue.bmp", false },
  { "EU",       "o kalsing desde de 2024 queria muito te contar uma coisa", false, "", "", "", "/img/gato1bmptrue.bmp", false },
  { "EU",       "mas ele deixou pro kalsing de 2026 falar..", false, "", "", "", "/img/gato1bmptrue.bmp", false },
  { "EU",       "que te ama muito muito muito muito demais da conta", false, "", "", "", "/img/gato1bmptrue.bmp", false },

  { "EU",       "e que por isso queria perguntar se", false, "", "", "", "/img/gato1bmptrue.bmp", false },
  { "PERGUNTA", "Quer ser minha amada pra valer?",                                                                  true, "Sim", "Nao", "", "/img/rezegif3.gif", true  },
};

constexpr uint8_t  DIALOG_COUNT          = sizeof(dialog) / sizeof(dialog[0]);
constexpr uint16_t TYPEWRITER_INTERVAL_MS = 50;

uint8_t  currentDialogIndex = 0;
uint8_t  selectedOption     = 0;
uint16_t visibleChars       = 0;
uint32_t lastTypewriterTick = 0;
bool     finalScreen        = false;
bool     firstFrame         = true;

constexpr uint32_t DIALOG_AUTO_ADVANCE_MS = 1000;
bool     waitingToAdvance = false;
uint32_t advanceReadyAt   = 0;

uint16_t textLength(const char* text) {
  uint16_t n = 0;
  while (text[n]) n++;
  return n;
}

DialogStep& currentDialog() { return dialog[currentDialogIndex]; }

bool isTextComplete() {
  return visibleChars >= textLength(currentDialog().text);
}

void stopGif() {
  if (gifActive) { gif.close(); gifActive = false; }
  if (gifFile)   gifFile.close();
}

void startGif(const char* path) {
  stopGif();
  display.drawImageArea(nullptr);
  if (!path || !path[0]) return;
  if (!storage.exists(path)) { Serial.print("[gif] not found: "); Serial.println(path); return; }
  if (!gif.open(path, gifOpenFile, gifCloseFile, gifReadFile, gifSeekFile, gifDraw)) {
    Serial.print("[gif] decoder failed: "); Serial.println(path); return;
  }
  gifActive      = true;
  lastGifFrameAt = millis();
  Serial.print("[gif] started "); Serial.println(path);
}

// GIF avanca frame apenas se o intervalo minimo passou — evita SPI contínuo.
void updateGif() {
  if (!gifActive) return;
  uint32_t now = millis();
  if (now - lastGifFrameAt < GIF_MIN_FRAME_MS) return;
  lastGifFrameAt = now;
  if (!gif.playFrame(false, nullptr)) gif.reset();
}

void drawDialogAsset() {
  DialogStep& step = currentDialog();
  if (!step.imagePath || !step.imagePath[0]) { stopGif(); display.drawImageArea(nullptr); return; }
  if (step.isGif) { startGif(step.imagePath); return; }
  stopGif();
  display.drawImageArea(step.imagePath);
}

void lockDevice(const String& reason) {
  audio.stop();
  display.showLocked(reason);
  esp_sleep_enable_timer_wakeup(30ULL * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void renderDialogFull() {
  DialogStep& step = currentDialog();
  display.showDialog(
    step.speaker, step.text, visibleChars,
    step.hasQuestion, step.firstOption, step.secondOption,
    selectedOption, isTextComplete(), firstFrame
  );
  firstFrame = false;
}

void renderNextChar() {
  display.drawNextChar(currentDialog().text, visibleChars, isTextComplete());
}

void enterDialog(uint8_t index) {
  currentScreen      = SystemScreen::Dialog;
  currentDialogIndex = index;
  selectedOption     = 0;
  visibleChars       = 0;
  lastTypewriterTick = millis();
  firstFrame         = true;
  waitingToAdvance   = false;

  if (currentDialog().audioPath[0] != '\0') audio.play(currentDialog().audioPath);

  renderDialogFull();
  drawDialogAsset();
}

void showFinalAnswer(const char* title, const char* body) {
  currentScreen = SystemScreen::Final;
  finalScreen   = true;
  stopGif();
  // nao para musica no final do dialogo — pode querer musica no final
  display.showEnding(0, 1, title, body);
}

void handleDialogConfirm() {
  if (finalScreen) return;
  if (!isTextComplete()) {
    visibleChars = textLength(currentDialog().text);
    renderDialogFull();
    return;
  }
  if (currentDialog().hasQuestion) {
    if (selectedOption == 0) showFinalAnswer("RESPOSTA", "Final SIM selecionado. :)");
    else                     showFinalAnswer("RESPOSTA", "Final NAO selecionado.");
    return;
  }
  if (currentDialogIndex + 1 < DIALOG_COUNT) enterDialog(currentDialogIndex + 1);
}

void handleDialogNavigate() {
  if (!finalScreen && currentDialog().hasQuestion && isTextComplete()) {
    selectedOption = selectedOption == 0 ? 1 : 0;
    display.redrawOptions(currentDialog().firstOption, currentDialog().secondOption, selectedOption);
  }
}

void updateTypewriter() {
  if (currentScreen != SystemScreen::Dialog || finalScreen || isTextComplete()) return;
  uint32_t now = millis();
  if (now - lastTypewriterTick < TYPEWRITER_INTERVAL_MS) return;
  lastTypewriterTick = now;
  visibleChars++;
  renderNextChar();

  if (isTextComplete() && !currentDialog().hasQuestion) {
    waitingToAdvance = true;
    advanceReadyAt   = millis() + DIALOG_AUTO_ADVANCE_MS;
  }
}

void updateDialogAutoAdvance() {
  if (!waitingToAdvance || currentScreen != SystemScreen::Dialog || finalScreen) return;
  if (millis() < advanceReadyAt) return;
  waitingToAdvance = false;
  if (currentDialogIndex + 1 < DIALOG_COUNT) enterDialog(currentDialogIndex + 1);
}

void updateTerminalCursor() {
  if (currentScreen != SystemScreen::Terminal || terminalExecuted) return;
  uint32_t now = millis();
  if (now - lastTerminalBlinkAt < 500) return;
  lastTerminalBlinkAt = now;
  terminalCursorVisible = !terminalCursorVisible;
  renderTerminal();
}

// --- Handlers de telas do sistema ---

void handleHomeConfirm() {
  if (homeSelection == 0)      renderMusic();
  else if (homeSelection == 1) renderSettings();
  else {
    terminalCommand = 0;
    terminalExecuted = false;
    terminalCursorVisible = true;
    lastTerminalBlinkAt = millis();
    renderTerminal();
  }
}

void handleSettingsConfirm() {
  if (settingsSelection == 0) {
    bool wasMuted = volumeLevel == 0;
    volumeLevel = (volumeLevel + 1) % 6;
    applyVolume();
    if (volumeLevel == 0) audio.stop();
    else if (wasMuted && !musicPaused) playCurrentTrack();
    renderSettings();
  } else if (settingsSelection == 1) {
    currentTheme = currentTheme == ThemeId::Lila ? ThemeId::Kalsing : ThemeId::Lila;
    restartThemePlaylist();
    renderSettings();
  } else {
    renderHome();
  }
}

void handleMusicConfirm() {
  if      (musicSelection == 0) previousTrack();
  else if (musicSelection == 1) togglePause();
  else if (musicSelection == 2) nextTrack();
  else                          renderHome();
  if (currentScreen == SystemScreen::Music) renderMusic();
}

void handleTerminalConfirm() {
  if (!terminalExecuted && terminalCommand == 2) {
    renderHome();
    return;
  }
  terminalExecuted = !terminalExecuted;
  terminalCursorVisible = true;
  lastTerminalBlinkAt = millis();
  renderTerminal();
}

void handleConfirm() {
  switch (currentScreen) {
    case SystemScreen::Start:       enterDialog(0);          break;
    case SystemScreen::Home:        handleHomeConfirm();     break;
    case SystemScreen::Settings:    handleSettingsConfirm(); break;
    case SystemScreen::Music:       handleMusicConfirm();    break;
    case SystemScreen::Terminal:    handleTerminalConfirm(); break;
    case SystemScreen::Placeholder: renderHome();            break;
    case SystemScreen::Dialog:      handleDialogConfirm();   break;
    case SystemScreen::Final:       renderHome();            break;
  }
}

void handleNavigate() {
  switch (currentScreen) {
    case SystemScreen::Home:
      homeSelection = (homeSelection + 1) % 3;
      renderHome();
      break;
    case SystemScreen::Settings:
      settingsSelection = (settingsSelection + 1) % 3;
      renderSettings();
      break;
    case SystemScreen::Music:
      musicSelection = (musicSelection + 1) % 4;
      renderMusic();
      break;
    case SystemScreen::Terminal:
      terminalCommand = (terminalCommand + 1) % (terminalExecuted ? 2 : 3);
      terminalCursorVisible = true;
      lastTerminalBlinkAt = millis();
      renderTerminal();
      break;
    case SystemScreen::Dialog:
      handleDialogNavigate();
      break;
    case SystemScreen::Start:
    case SystemScreen::Placeholder:
    case SystemScreen::Final:
      break;
  }
}

AppConfig loadRemoteConfig() {
  AppConfig config;
  config.mode    = RemoteMode::Normal;
  config.variant = "default";
  config.message = "dialogo local";
  return config;
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  display.begin();
  gif.begin(GIF_PALETTE_RGB565_BE);
  audio.begin();
  buttons.begin();
  applyVolume();

  AppConfig config = loadRemoteConfig();
  display.showBoot(config.message);

  if (config.mode == RemoteMode::Locked) {
    lockDevice("bloqueado por configuracao remota");
  }

  delay(650);
  renderStart();
  playCurrentTrack();
}

void loop() {
  // 1. audio primeiro — maxima prioridade, sem nada antes que bloqueie
  audio.update();

  // 2. input — dev phase overrides & botões físicos
  if (Serial.available()) {
    char devInput = Serial.peek();
    if (devInput == 'b' || devInput == 'B') {
      Serial.read();
      renderHome();
    } else if (devInput == 'u' || devInput == 'U') {
      Serial.read();
      renderStart();
    }
  }

  // Processamento normal dos botões (físicos ou comandos 'c', 'n', 'i')
  SceneAction action = buttons.readAction();
  if (action == SceneAction::Confirm) {
    handleConfirm();
  } else if (action == SceneAction::Navigate) {
    handleNavigate();
  } else if (action == SceneAction::OpenHome) {
    renderHome();
  }

  // 3. typewriter — so desenha um char por iteracao, custo minimo de SPI
  updateTypewriter();

  // 4. avanço automático entre dialogs normais
  updateDialogAutoAdvance();

  // 5. gif — so avanca frame se o intervalo minimo passou (GIF_MIN_FRAME_MS)
  if (currentScreen == SystemScreen::Dialog) updateGif();

  // 6. cursor do terminal fake
  updateTerminalCursor();

  // 7. musica ambiente — verifica se precisa reiniciar track
  updateGlobalMusic();

  // 8. audio de novo no final — garante que o buffer nao seca entre frames
  audio.update();

  // sem delay fixo — loop roda na velocidade maxima do ESP32
}
