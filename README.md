# Projeto esp32

Projeto de dispositivo eletronico usando ESP32, tela, audio e botoes.

## Objetivo

Criar uma experiencia embarcada com cenas interativas: textos, imagens, GIFs, musicas por tela e uma etapa final com escolha entre "sim" e "nao".

## Estrutura

```text
assets/
  audio/        Musicas e efeitos sonoros originais/exportados
  gifs/         GIFs ou sequencias de frames
  images/       Imagens personalizadas
docs/           Decisoes tecnicas, hardware e roteiro
firmware/       Codigo do ESP32 em PlatformIO
server/         Mock/servico de configuracao remota
simulator/      Simulador web para testar o fluxo sem hardware
```

## Comeco recomendado

1. Testar o motor de dialogo no Serial Monitor.
2. Ajustar o roteiro em `docs/storyboard.md`.
3. Definir hardware em `docs/hardware.md`.
4. Adaptar o renderer serial para o display 240x240 quando ele chegar.

## Fluxo atual

O firmware atual foca no motor de dialogo retro:

```text
texto aparece aos poucos
confirmar acelera ou avanca
pergunta final mostra opcoes
navegar alterna opcoes
```

Enquanto o display 240x240 nao chegou, o Serial Monitor funciona como display virtual. Veja `docs/dialog-engine.md`.

## Gravacao do firmware

Veja `docs/platformio.md` para o fluxo com PlatformIO.

## Display

O display alvo atual e ST7789 240x240 via LovyanGFX. Veja `docs/st7789-display.md`.

## Ideias de hardware

- ESP32 DevKit.
- Display TFT SPI colorido, por exemplo ILI9341 240x320 ou ST7789 240x240.
- Audio via DFPlayer Mini ou I2S DAC/amplificador.
- 2 botoes: navegar e confirmar.

## Modo remoto

No boot, o firmware consulta uma configuracao remota. Essa config pode liberar o fluxo normal, bloquear o dispositivo de forma reversivel ou trocar mensagens/variantes.
