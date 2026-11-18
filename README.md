# Carrinho-Robô — Project-based Maker Lab (FIAP)

| Integrante | RM |
|---|---|
| Guilherme Morais Barbosa | 551981 |
| Matheus Gusmão Aragão | 550826 |
| Júlia Marques Mendes das Neves | 98680 |

---

## Global Solution CitizenTech — Robô Inspetor de Acessibilidade

Entrega de **Project-Based Maker Lab** (componente *Físico*): o carrinho-robô evoluído para registrar barreiras de acessibilidade em calçadas, corredores e rampas, com um sensor novo (**MPU-6050**) e um modelo de **TinyML** que classifica o piso pela vibração.

**Vídeo demonstrativo (até 6 min):** <!-- PREENCHER: link do YouTube (público) --> *em breve*

### Ficha Técnica do MVP

| Campo | Resposta |
|---|---|
| **Nome do produto** | Robô Inspetor de Acessibilidade (nome provisório, a alinhar com o grupo) |
| **Cidadão e território** | pessoas com deficiência ou mobilidade reduzida e idosos; calçadas, corredores e rampas de áreas urbanas; agentes públicos que cuidam da manutenção |
| **Problema, em uma frase** | obstáculos, degraus, pisos irregulares e rampas íngremes demais impedem a circulação de quem tem mobilidade reduzida e só são conhecidos quando alguém reclama |
| **Proposta de valor** | um robô de baixo custo que percorre o trecho sozinho, detecta cada barreira e gera um relatório para apoiar a manutenção |
| **Arquitetura: componentes** | ESP32, ponte H L298N com 4 motores, HC-SR04 (distância), **MPU-6050 (sensor novo)**, controle de PS4, buzzer e modelo de TinyML embarcado |
| **Fontes de dados** | dados experimentais gerados pelo próprio robô em pisos de teste (liso, irregular, degrau, parado), com a geração e os descartes documentados em [`dados/gs-piso`](dados/gs-piso/README.md) |
| **Modelos de IA** | rede neural pequena (Edge Impulse, Spectral Analysis, int8) que classifica 1 s de vibração em 4 classes: 97,6% na validação e **77,3% no teste com amostras nunca vistas** |
| **URL do MVP e repositório** | este repositório; projeto público do modelo: <https://studio.edgeimpulse.com/public/1167661/live> |
| **Indicadores de impacto** | barreiras detectadas por trecho; metros inspecionados; acerto do modelo de piso no teste (77,3%) *(tempo de inspeção em comparação com a inspeção manual: a medir)* |
| **ODS conectados** | **11** (meta 11.7, e 11.2), **10** (meta 10.2) e **9** (meta 9.1), explicados na [documentação](docs/09-gs-robo-inspetor.md#11-ods) |

**Documentação completa:** [docs/09-gs-robo-inspetor.md](docs/09-gs-robo-inspetor.md) (problema, solução, ligação, integração do sensor com a IA, dados, modelo, autonomia, como instalar e executar, testes, IA responsável e ODS). **Código:** [`src/arduino/gs_robo_inspetor`](src/arduino/gs_robo_inspetor/gs_robo_inspetor.ino).

---

## Versões

| Versão | Trabalho | Controlador | Controle | Destaque |
|---|---|---|---|---|
| **1** | Check Point 01 | Arduino Uno | app serial Bluetooth (HC-05) | para sozinho ao detectar obstáculo |
| **2** | Trabalho com TinyML | ESP32 | controle de PS4 (Bluepad32) | modelo treinado no Edge Impulse classifica a distância |
| **3** | Global Solution CitizenTech | ESP32 | controle de PS4 e modo patrulha autônomo | MPU-6050 e modelo de piso: registra obstáculos, degraus, pisos irregulares, rampas e tombo |

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
cp2-carrinho--tinyml/
├── README.md                          ← este arquivo
├── docs/
│   ├── 01-requisitos-e-planejamento.md    versão 1: requisitos, MVP, backlog e decisões
│   ├── 07-versao-esp32-ps4.md             versão 2: hardware, controles, códigos e problemas resolvidos
│   ├── 08-tinyml.md                       relatório do TinyML (Aulas 19 e 20)
│   ├── 09-gs-robo-inspetor.md             Global Solution: documentação do robô inspetor de acessibilidade
│   ├── roteiro-video-gs.md                roteiro do vídeo da Global Solution
│   └── img/                               prints do Edge Impulse
├── dados/tinyml/                      ← dataset, scripts de captura e biblioteca do modelo de distância (Check Point 2)
├── dados/gs-piso/                     ← dataset de vibração do piso, scripts de coleta e biblioteca do modelo (Global Solution)
└── src/arduino/
    ├── gs_robo_inspetor/              ← CÓDIGO PRINCIPAL da Global Solution (PS4 + HC-SR04 + MPU-6050 + modelo de piso + patrulha)
    ├── gs_coleta_piso/                ← coleta de vibração do piso (gera o dataset do modelo)
    ├── teste_mpu6050/                 ← teste isolado do MPU-6050
    ├── carrinho_esp32_ps4_ia/         ← versão 2 (PS4 + sensor de distância + modelo de distância)
    ├── carrinho_esp32_ps4/            ← versão 2 só com o controle de PS4
    ├── carrinho_esp32_wifi/           ← alternativa: página web pelo Wi-Fi (iPhone e Android)
    ├── carrinho_esp32/                ← alternativa: Bluetooth clássico (só Android)
    ├── tinyml_a_leitura_bruta/        ← TinyML, parte A: leitura crua do sensor
    ├── tinyml_b_preprocessamento/     ← TinyML, parte B: pré-processamento, classes e CSV
    ├── tinyml_c_modelo/               ← TinyML, parte C: modelo rodando na ESP32
    ├── arduino_carrinho_codigo/       ← versão 1: código principal do Uno (app serial)
    └── carrinho_dabble/               ← versão 1: alternativa com o app Dabble
```

Os códigos de teste isolado da versão 1 (motores, Bluetooth, sensor e buzzer) ficam no repositório do grupo: <https://github.com/kaikyalvaro1708/cp-arduino-car> (pasta `code/testes/`).

## Documentação

| Documento | Conteúdo |
|---|---|
| [Versão 2: ESP32 e PS4](docs/07-versao-esp32-ps4.md) | componentes, pinos, controles, como compilar, problemas e soluções |
| [**Global Solution: robô inspetor**](docs/09-gs-robo-inspetor.md) | problema, solução, ligação, integração do MPU-6050 com a IA, dados, modelo, patrulha, instalação, testes, IA responsável e ODS |
| [Roteiro do vídeo da GS](docs/roteiro-video-gs.md) | roteiro de até 6 minutos |
| [TinyML](docs/08-tinyml.md) | sensor, pré-processamento, classes, dataset, treino no Edge Impulse, teste na placa e integração (Check Point 2) |
| [Dados do TinyML](dados/tinyml/README.md) | arquivos e passo a passo para reproduzir |
| [Requisitos e evolução (versão 1)](docs/01-requisitos-e-planejamento.md) | requisitos, proposta, MVP, backlog e decisões do Check Point 01 |

Os documentos do Check Point 01 sobre projeto mecânico, hardware, software, testes e evidências (numerados de 02 a 06) estão no repositório do grupo: <https://github.com/kaikyalvaro1708/cp-arduino-car>. Por isso `docs/01` tem links para arquivos que não estão nesta pasta.

## Tecnologias

Versão 2: ESP32 · C++ (Arduino IDE) · Bluepad32 · Edge Impulse (TinyML) · Python (pyserial) · L298N · HC-SR04

Versão 1: Arduino Uno · C++ (Arduino IDE) · L298N · HC-05 (Bluetooth) · HC-SR04 · Tinkercad

## Licença

[MIT](LICENSE)
