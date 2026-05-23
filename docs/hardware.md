# Hardware

Ainda nao ha hardware definido, entao o firmware usa interfaces abstratas para tela, audio e botoes.

## Decisoes pendentes

- Modelo da tela e resolucao.
- Metodo de audio: DFPlayer Mini, I2S ou buzzer.
- Quantidade e layout dos botoes.
- Alimentacao: USB, powerbank ou bateria.
- Pinagem final.

## Recomendacao inicial

Para uma experiencia visual bonita, uma tela TFT colorida e melhor que OLED monocromatico.

Opcao simples:

- ESP32 DevKit.
- TFT ST7789 240x240 ou ILI9341 240x320.
- DFPlayer Mini com cartao microSD.
- 3 botoes com pull-up interno.

