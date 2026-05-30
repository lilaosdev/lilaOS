#include <Arduino.h>
#include "AppConfig.h"
#include "Devices.h"

namespace {
DisplayDevice display;
AudioDevice audio;
ButtonInput buttons;

struct DialogStep {
  const char* speaker;
  const char* text;
  bool hasQuestion;
  const char* firstOption;
  const char* secondOption;
  const char* audioPath;
};

// Audio: uma musica de fundo roda do inicio ao fim da narrativa.
// O campo audioPath so precisa ser preenchido no primeiro dialogo.
// Os demais ficam vazios — audio.update() no loop mantem a reproducao.
// Quando chegar na pergunta, troca pra uma musica especifica se quiser.
// Caminhos no SPIFFS: coloque os arquivos em data/ e use pio run -t uploadfs
DialogStep dialog[] = {
  {
    "SISTEMA",
    "Inicializacao",
    false,
    "",
    "",
    "/audio/lilpeep4.mp3"       // <-- musica de fundo, toca do inicio
  },
  {
    "EU",
    "Dialogooooooooooooooooooooooooo 1",
    false,
    "",
    "",
    ""                    // vazio: audio continua do passo anterior
  },
  {
    "EU",
    "Dialogooooooooooooooooooooooooo 2",
    false,
    "",
    "",
    ""
  },
  {
    "PERGUNTA",
    "Quer namorar comigo?",
    true,
    "Sim",
    "Nao",
    ""
  },
};

constexpr uint8_t DIALOG_COUNT = sizeof(dialog) / sizeof(dialog[0]);
constexpr uint16_t TYPEWRITER_INTERVAL_MS = 45;

uint8_t currentDialogIndex = 0;
uint8_t selectedOption = 0;
uint16_t visibleChars = 0;
uint32_t lastTypewriterTick = 0;
bool finalScreen = false;
bool firstFrame = true;

AppConfig loadRemoteConfig() {
  AppConfig config;
  config.mode = RemoteMode::Normal;
  config.variant = "default";
  config.message = "dialogo local";
  return config;
}

uint16_t textLength(const char* text) {
  uint16_t length = 0;
  while (text[length] != '\0') {
    length++;
  }
  return length;
}

DialogStep& currentDialog() {
  return dialog[currentDialogIndex];
}

bool isTextComplete() {
  return visibleChars >= textLength(currentDialog().text);
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
  currentDialogIndex = index;
  selectedOption = 0;
  visibleChars = 0;
  lastTypewriterTick = millis();
  firstFrame = true;

  // So troca o audio se o passo tiver um caminho definido.
  // Caso contrario, deixa o audio atual continuar tocando.
  if (currentDialog().audioPath[0] != '\0') {
    audio.play(currentDialog().audioPath);
  }

  renderDialog();
}

void showFinalAnswer(const char* title, const char* body) {
  finalScreen = true;
  audio.stop();
  display.showEnding(0, 1, title, body);
}

void handleConfirm() {
  if (finalScreen) {
    return;
  }

  if (!isTextComplete()) {
    visibleChars = textLength(currentDialog().text);
    renderDialog();
    return;
  }

  if (currentDialog().hasQuestion) {
    if (selectedOption == 0) {
      showFinalAnswer("RESPOSTA", "Final SIM selecionado. :)");
    } else {
      showFinalAnswer("RESPOSTA", "Final NAO selecionado. Bloqueio reversivel entra depois.");
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
  if (finalScreen || isTextComplete()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTypewriterTick < TYPEWRITER_INTERVAL_MS) {
    return;
  }

  lastTypewriterTick = now;
  visibleChars++;
  renderDialog();
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  display.begin();
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
  audio.update(); // precisa rodar com alta frequencia para alimentar o decoder MP3

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