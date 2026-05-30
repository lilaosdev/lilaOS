# Audio MP3 com MAX98357A

O firmware usa o MAX98357A como saida I2S mono e toca arquivos MP3 salvos no filesystem SPIFFS do ESP32.

## Pinagem

MAX98357A:

| Sinal | GPIO |
| --- | --- |
| DIN | 25 |
| LRC / WS | 26 |
| BCLK | 27 |
| VIN | 3.3V |
| GND | GND |

A pinagem fica em `firmware/include/AudioPins.h`.

## Botoes

Como `GPIO25`, `GPIO26` e `GPIO27` agora estao reservados para audio, os botoes foram movidos para:

| Acao | GPIO |
| --- | --- |
| Navegar | 32 |
| Confirmar | 33 |

A pinagem fica em `firmware/include/InputPins.h`.

## Onde colocar MP3

Coloque os arquivos dentro de:

```text
firmware/data/audio/
```

Exemplo:

```text
firmware/data/audio/question.mp3
```

No codigo, o caminho deve comecar com `/`:

```cpp
"/audio/question.mp3"
```

## Como enviar os MP3 para o ESP32

Dentro da pasta `firmware`:

```powershell
pio run --target uploadfs
```

Depois grave o firmware:

```powershell
pio run --target upload
```

E abra o monitor:

```powershell
pio device monitor
```

## Como funciona no codigo

- `AudioDevice::begin()` monta o SPIFFS, cria a saida I2S e prepara o decoder MP3.
- `AudioDevice::play("/audio/question.mp3")` abre e toca um arquivo.
- `AudioDevice::update()` precisa ser chamado sempre no `loop()` para alimentar o decoder.
- `AudioDevice::stop()` para a musica atual.

O fluxo de dialogo em `firmware/src/main.cpp` ja aceita um `audioPath` opcional por dialogo.

## Se nao tocar

- Confirme se rodou `pio run --target uploadfs`.
- Confira se o nome do arquivo bate exatamente com o caminho no codigo.
- Feche/reabra o monitor serial e veja mensagens `[audio]`.
- Comece com volume baixo: o ganho default esta em `AUDIO_DEFAULT_GAIN`.
- Confirme GND comum entre ESP32 e MAX98357A.
