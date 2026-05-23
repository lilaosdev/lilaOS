# Storyboard

Este arquivo descreve a experiencia antes de prendermos o projeto a um hardware especifico.

## Fluxo inicial

| Cena | Tipo | Texto | Midia | Audio | Acao |
| --- | --- | --- | --- | --- | --- |
| intro | Texto | Tenho uma coisa pra te perguntar... | `images/intro.bmp` | `audio/intro.mp3` | Avancar |
| memory_1 | Texto/imagem | Lembra daquele dia...? | `images/memory_1.bmp` | `audio/memory_1.mp3` | Avancar |
| build_up | GIF/texto | Entao eu pensei em fazer isso do meu jeito. | `gifs/build_up.gif` | `audio/build_up.mp3` | Avancar |
| question | Escolha | Quer namorar comigo? | `images/question.bmp` | `audio/question.mp3` | Sim/Nao |
| yes | Final | Eu sabia. :) | `gifs/yes.gif` | `audio/yes.mp3` | Encerrar |
| no | Bloqueio | Sistema emocional indisponivel. | `images/no.bmp` | `audio/no.mp3` | Bloquear reversivelmente |

## Observacoes

- Textos devem ser curtos para caberem bem em telas pequenas.
- GIFs provavelmente serao convertidos para frames ou formato otimizado depois.
- O final "nao" deve bloquear por software, sem danificar nem apagar a placa.

