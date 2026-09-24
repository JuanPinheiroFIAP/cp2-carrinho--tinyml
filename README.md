# Carrinho-Robô — Project-based Maker Lab (FIAP)

| Integrante | RM |
|---|---|
| Juan Pinheiro de França | 552202 |
| PREENCHER | PREENCHER |
| PREENCHER | PREENCHER |

---

## Versões

| Versão | Trabalho | Controlador | Controle | Destaque |
|---|---|---|---|---|
| **1** | Check Point 01 | Arduino Uno | app serial Bluetooth (HC-05) | para sozinho ao detectar obstáculo |
| **2** | Trabalho com TinyML | ESP32 | controle de PS4 (Bluepad32) | modelo treinado no Edge Impulse classifica a distância |

---

## Versão 2 — ESP32, controle de PS4 e TinyML

O carrinho usa uma **ESP32**, uma ponte H **L298N** com 4 motores DC (dois de cada lado) e um sensor ultrassônico **HC-SR04**. É dirigido por um **controle de PS4**, no estilo de jogo de corrida, e o carrinho **responde no controle** com a cor da luz e vibração. Um **modelo de TinyML**, treinado no Edge Impulse com dados reais do sensor, roda dentro da ESP32 e classifica o que está à frente em **perto**, **médio** ou **longe**.

- **Projeto no Edge Impulse (público):** <https://studio.edgeimpulse.com/public/1120730/live>
- Modelo: 3 classes, **98,5% de acurácia** na validação, quantizado em int8, 0 a 1 ms por previsão na ESP32.

### Controles

| Controle | Ação |
|---|---|
| **R2** | acelerar (frente) |
| **L2** | ré |
| **R2 + L2** juntos | freio |
| **Analógico esquerdo** (ou D-pad esquerda e direita) | virar |
| **✕** | parada de emergência |

### O que o modelo faz no carrinho

| O modelo diz | O que acontece |
|---|---|
| **PERTO** (menos de ~20 cm) | a frente é bloqueada, com luz vermelha e vibração forte; a ré continua livre |
| **MEDIO** (de ~20 a ~60 cm) | andando para a frente, luz roxa e vibração leve de aviso |
| **LONGE** | funcionamento normal |

### Como usar

1. Ligue as pilhas da ponte H e conecte a ESP32 ao USB.
2. Segure **SHARE + PS** no controle até a luz piscar. Ele fica verde e vibra ao conectar.
3. Dirija com R2, L2 e o analógico.

Para compilar, ligações, problemas resolvidos e alternativas (Wi-Fi e Bluetooth): [docs/07-versao-esp32-ps4.md](docs/07-versao-esp32-ps4.md). O relatório do TinyML está em [docs/08-tinyml.md](docs/08-tinyml.md).

---

## Versão 1 — Arduino Uno (Check Point 01)

**Carrinho-Robô Bluetooth com Detecção de Obstáculos.** Um carrinho-robô controlado pelo celular via Bluetooth, que **detecta obstáculos à frente e para sozinho antes de colidir**, integrando projeto mecânico, eletrônica, programação e documentação.

### Descrição

O carrinho usa um **Arduino Uno** como controlador, uma **ponte H L298N** para acionar dois motores DC com caixa de redução, um módulo **Bluetooth HC-05** para receber comandos de um app no celular e um **sensor ultrassônico HC-SR04** na frente. Enquanto anda para frente, o Arduino mede a distância 10 vezes por segundo; se algo aparece a menos de 15 cm, ele para os motores, apita o **buzzer** e bloqueia o avanço até o caminho ficar livre.

A alimentação é separada: **4 pilhas AA** para os motores (ponte H) e **bateria 9 V** para o Arduino, com o GND das duas ligado em comum.

### Principais funcionalidades

- **Controle remoto sem fio** por Bluetooth (app serial no celular): frente, ré, esquerda, direita e parar.
- **Parada automática por obstáculo** com o sensor ultrassônico (limite de 15 cm).
- **Bloqueio de avanço**: com obstáculo à frente, o comando "frente" é recusado; ré e curvas continuam liberadas.
- **Alerta sonoro** (buzzer) ao detectar obstáculo ou recusar o avanço.
- **Filtro de leituras** (mediana das 3 últimas) para ignorar leituras falsas do sensor.
- **Telemetria**: o carrinho envia a distância medida para o app a cada 300 ms.
- **Alimentação por bateria**, sem depender do cabo USB.

### Como usar (resumo)

1. Ligue as pilhas da ponte H e a bateria 9 V do Arduino.
2. Pareie o celular com o **HC-05** (senha `1234` ou `0000`).
3. Conecte pelo app de controle serial Bluetooth e use os botões:

| Comando | Letra enviada | Ação |
|---|---|---|
| Frente | `F` | anda para frente (se não houver obstáculo) |
| Ré | `B` | anda para trás |
| Esquerda | `L` | vira para a esquerda |
| Direita | `R` | vira para a direita |
| Parar | `S` | para os motores |

O planejamento, os requisitos e a evolução dessa versão estão em [docs/01-requisitos-e-planejamento.md](docs/01-requisitos-e-planejamento.md).

---

## Carrinho finalizado

<!-- PREENCHER: salve a foto em docs/img/carrinho-finalizado.jpg e tire este comentário
![Carrinho finalizado](docs/img/carrinho-finalizado.jpg)
-->

## Organização do repositório

```
carrinho/
├── README.md                          ← este arquivo
├── docs/
│   ├── 01-requisitos-e-planejamento.md    versão 1: requisitos, MVP, backlog e decisões
│   ├── 07-versao-esp32-ps4.md             versão 2: hardware, controles, códigos e problemas resolvidos
│   ├── 08-tinyml.md                       relatório do TinyML (Aulas 19 e 20)
│   └── img/                               prints do Edge Impulse
├── dados/tinyml/                      ← dataset, scripts de captura e biblioteca do modelo (ver README da pasta)
└── src/arduino/
    ├── carrinho_esp32_ps4_ia/         ← CÓDIGO PRINCIPAL da versão 2 (PS4 + sensor + modelo)
    ├── carrinho_esp32_ps4/            ← versão 2 só com o controle de PS4
    ├── carrinho_esp32_wifi/           ← alternativa: página web pelo Wi-Fi (iPhone e Android)
    ├── carrinho_esp32/                ← alternativa: Bluetooth clássico (só Android)
    ├── teste_motores_esp32/           ← teste isolado da ponte H na ESP32
    ├── tinyml_a_leitura_bruta/        ← TinyML, parte A: leitura crua do sensor
    ├── tinyml_b_preprocessamento/     ← TinyML, parte B: pré-processamento, classes e CSV
    ├── tinyml_c_modelo/               ← TinyML, parte C: modelo rodando na ESP32
    ├── arduino_carrinho_codigo/       ← versão 1: código principal do Uno (app serial)
    ├── carrinho_dabble/               ← versão 1: alternativa com o app Dabble
    ├── teste_motores/                 ← versão 1: teste da ponte H
    ├── teste_bluetooth/               ← versão 1: teste do HC-05
    ├── teste_sensor/                  ← versão 1: teste do HC-SR04
    └── teste_buzzer/                  ← versão 1: teste do buzzer
```

## Documentação

| Documento | Conteúdo |
|---|---|
| [Versão 2: ESP32 e PS4](docs/07-versao-esp32-ps4.md) | componentes, pinos, controles, como compilar, problemas e soluções |
| [TinyML](docs/08-tinyml.md) | sensor, pré-processamento, classes, dataset, treino no Edge Impulse, teste na placa e integração |
| [Dados do TinyML](dados/tinyml/README.md) | arquivos e passo a passo para reproduzir |
| [Requisitos e evolução (versão 1)](docs/01-requisitos-e-planejamento.md) | requisitos, proposta, MVP, backlog e decisões do Check Point 01 |

Os documentos do Check Point 01 sobre projeto mecânico, hardware, software, testes e evidências (numerados de 02 a 06) estão no repositório do grupo: <https://github.com/kaikyalvaro1708/cp-arduino-car>. Por isso `docs/01` tem links para arquivos que não estão nesta pasta.

## Tecnologias

Versão 2: ESP32 · C++ (Arduino IDE) · Bluepad32 · Edge Impulse (TinyML) · Python (pyserial) · L298N · HC-SR04

Versão 1: Arduino Uno · C++ (Arduino IDE) · L298N · HC-05 (Bluetooth) · HC-SR04 · Tinkercad

## Licença

[MIT](LICENSE)
