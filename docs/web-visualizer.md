# Visualizador web integrado

O simulador web pode funcionar em dois modos:

- Simulador local: o botao na pagina avanca a interface sem placa conectada.
- Integrado ao ESP32: a pagina le o Serial do ESP32 e mostra o mesmo estado visual emitido pelo firmware.

## Como usar integrado ao ESP32

1. Grave o firmware no ESP32.
2. Feche o Serial Monitor, porque apenas um programa por vez consegue usar a porta COM.
3. Entre na pasta `simulator`.
4. Rode:

```powershell
npm start
```

5. Abra `http://localhost:5173` no Chrome ou Edge.
6. Clique em `Conectar ESP32`.
7. Escolha a porta COM do ESP32.
8. Aperte o botao fisico na protoboard.

Quando o firmware renderiza uma pagina, ele tambem escreve uma linha assim no Serial:

```text
VISUAL:{"pageIndex":0,"pageCount":2,"title":"Oi, meu amor","text":"...","options":["Continuar","Respirar"],"selectedOption":0}
```

A pagina web escuta essas linhas e atualiza a tela.

## Observacoes

- Web Serial exige Chrome/Edge e contexto seguro. `localhost` funciona; abrir o HTML direto como arquivo pode nao funcionar.
- Se a porta estiver ocupada pelo monitor serial do PlatformIO, a conexao web falha.

## Erro: Failed to open serial port

Esse erro significa que o navegador recebeu permissao para a porta, mas o Windows nao deixou abrir. Verifique:

- Feche o Serial Monitor do PlatformIO.
- Feche o Serial Monitor da Arduino IDE.
- Feche outros programas que possam estar usando a COM do ESP32.
- Feche outras abas do Chrome/Edge que possam ter conectado na mesma porta.
- Tire e coloque o cabo USB de novo.
- Confira se escolheu a porta COM correta.
- Tente outro cabo USB, especialmente se o cabo for somente de carga.

Depois disso, recarregue `http://localhost:5173` e conecte novamente.
