# Como gravar codigo com PlatformIO

O projeto deixou de depender da Arduino IDE e agora usa PlatformIO. Isso muda o fluxo, mas o firmware continua sendo codigo Arduino por baixo.

## Onde fica o codigo

- `firmware/platformio.ini`: configuracao da placa, framework e bibliotecas.
- `firmware/src/main.cpp`: entrada principal do programa.
- `firmware/src/*.cpp`: implementacoes auxiliares.
- `firmware/include/*.h`: headers compartilhados.
- `firmware/data`: arquivos que podem ir para o filesystem do ESP32 no futuro.

## Comandos principais

Execute os comandos dentro da pasta `firmware`.

```powershell
pio run
```

Compila o firmware.

```powershell
pio run --target upload
```

Compila e grava no ESP32 conectado via USB.

```powershell
pio device monitor
```

Abre o monitor serial.

```powershell
pio run --target uploadfs
```

Envia arquivos da pasta `data` para o filesystem da placa, quando ativarmos essa parte.

## Se usar VS Code

1. Instale a extensao PlatformIO IDE.
2. Abra a pasta `firmware`.
3. Use os botoes de build, upload e monitor na barra do PlatformIO.

## Portas seriais

Normalmente o PlatformIO detecta a porta sozinho. Se precisar fixar, adicione em `platformio.ini`:

```ini
upload_port = COM3
monitor_port = COM3
```

Troque `COM3` pela porta real do ESP32.

