# Display ST7789 240x240

O projeto agora usa LovyanGFX para desenhar no ST7789.

## Pinagem

A pinagem fica em `firmware/include/DisplayPins.h`.

Defaults atuais:

| Sinal | GPIO |
| --- | --- |
| SCLK | 18 |
| MOSI | 23 |
| MISO | -1 |
| DC | 2 |
| CS | 5 |
| RST | 4 |
| BL | 15 |

Se o seu modulo nao tiver `CS`, use `-1`. Se o backlight ficar sempre ligado direto no VCC, use `-1` em `DISPLAY_PIN_BL`.

## Visual atual

- Tela 240x240.
- Fundo escuro.
- Cantos com pixels decorativos.
- Nome do personagem/topico no topo.
- Caixa central de dialogo.
- Texto pixelado aparecendo aos poucos.
- Opcoes aparecem no rodape quando a pergunta termina.

## Preview no PC

Sem criar simulador, existem duas formas praticas:

- Serial Monitor: mostra o estado do dialogo em texto, util para logica.
- Display real: mostra o visual final.

Para ver no PC exatamente o que o display desenharia, precisamos de um espelho de framebuffer ou de um mini preview local. Isso e possivel, mas e outra camada; por enquanto o firmware ja desenha no ST7789 e mantem o preview textual no Serial.
