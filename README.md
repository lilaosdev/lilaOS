# Pedido ESP32

Projeto de dispositivo eletronico para um pedido de namoro usando ESP32, tela, audio e botoes.

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

1. Ajustar o roteiro em `docs/storyboard.md`.
2. Testar o fluxo no simulador em `simulator/index.html`.
3. Definir hardware em `docs/hardware.md`.
4. Adaptar os drivers reais no firmware quando os componentes chegarem.

## Fluxo atual

O firmware atual mostra duas paginas de texto e dois botoes desenhados na tela. Um unico botao fisico, inicialmente no GPIO 25 com `INPUT_PULLUP`, percorre:

```text
pagina 1 / botao esquerdo
pagina 1 / botao direito
pagina 2 / botao esquerdo
pagina 2 / botao direito
```

No simulador, o botao "Botao fisico" reproduz esse mesmo comportamento.

## Gravacao do firmware

Veja `docs/platformio.md` para o fluxo com PlatformIO.

## Ideias de hardware

- ESP32 DevKit.
- Display TFT SPI colorido, por exemplo ILI9341 240x320 ou ST7789 240x240.
- Audio via DFPlayer Mini ou I2S DAC/amplificador.
- 3 botoes: avancar, sim/esquerda, nao/direita.

## Modo remoto

No boot, o firmware consulta uma configuracao remota. Essa config pode liberar o fluxo normal, bloquear o dispositivo de forma reversivel ou trocar mensagens/variantes.
