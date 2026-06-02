# Conteudo do microSD

Esta pasta representa a raiz do cartao microSD.
Copie o conteudo desta pasta para o cartao, mantendo os caminhos:

- /audio/*.mp3
- /img/*.bmp
- /img/*.gif

O firmware procura assets primeiro no SD e so depois no SPIFFS.
Use firmware/data apenas para arquivos pequenos que precisam ir para a memoria interna do ESP32.