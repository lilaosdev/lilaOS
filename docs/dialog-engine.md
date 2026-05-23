# Motor de dialogo

Esta etapa foca apenas na parte principal do projeto: dialogos em estilo retro.

## Controles atuais

- GPIO 25: navegar entre opcoes quando uma pergunta estiver visivel.
- GPIO 26: confirmar.

Ambos usam `INPUT_PULLUP`, entao o botao deve ligar o pino ao GND quando pressionado.

## Comportamento

- O texto aparece aos poucos, como dialogo de jogo.
- Se apertar confirmar durante a digitacao, o texto aparece inteiro.
- Se apertar confirmar depois que o texto terminou, avanca para o proximo dialogo.
- No dialogo final, as opcoes aparecem apenas depois do texto terminar.
- O botao de navegar alterna entre as opcoes apenas quando a pergunta final esta pronta.

## Preview sem display

Enquanto o display 240x240 nao chegou, o Serial Monitor funciona como display virtual.

```powershell
cd firmware
pio device monitor
```

A renderizacao aparece em uma moldura de terminal. Quando o display real chegar, a mesma logica de dialogo sera mantida e apenas o renderer serial sera substituido por um renderer TFT.
