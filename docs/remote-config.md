# Configuracao remota

No boot, o ESP32 pode consultar um endpoint HTTP e receber um JSON simples.

## Exemplo

```json
{
  "mode": "normal",
  "variant": "default",
  "message": "ok"
}
```

## Modos sugeridos

- `normal`: executa a experiencia.
- `locked`: entra em bloqueio reversivel.
- `offline_ok`: usa a ultima configuracao salva quando nao houver rede.

## Seguranca

Este controle e uma camada pratica, nao seguranca forte. Para evitar alteracao acidental:

- Validar formato do JSON.
- Salvar ultima configuracao valida em memoria persistente.
- Definir comportamento claro para falha de rede.

