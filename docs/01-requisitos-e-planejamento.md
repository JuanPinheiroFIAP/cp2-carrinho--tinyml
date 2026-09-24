# Requisitos, planejamento e evolução

[← Voltar ao README](../README.md)

## 1. Requisitos

### Requisitos funcionais

| ID | Requisito | Status |
|---|---|---|
| RF01 | O carrinho deve andar para frente e para trás | ✅ atendido |
| RF02 | O carrinho deve fazer curvas para a esquerda e para a direita | ✅ atendido |
| RF03 | O carrinho deve ser controlado sem fio pelo celular (Bluetooth) | ✅ atendido |
| RF04 | O carrinho deve detectar obstáculos à frente com um sensor | ✅ atendido |
| RF05 | O carrinho deve parar sozinho antes de colidir (limite de 15 cm) | ✅ atendido |
| RF06 | O carrinho deve recusar o comando "frente" enquanto houver obstáculo | ✅ atendido |
| RF07 | O carrinho deve emitir um alerta sonoro ao detectar obstáculo | ⚠️ ver [testes](05-testes-e-resultados.md#t9--buzzer) |
| RF08 | O carrinho deve enviar a distância medida para o app | ✅ atendido |

### Requisitos não funcionais

| ID | Requisito | Status |
|---|---|---|
| RNF01 | Funcionar só com bateria, sem cabo USB | ✅ atendido |
| RNF02 | Responder aos comandos sem atraso perceptível | ✅ atendido |
| RNF03 | Não parar por leituras falsas do sensor | ✅ atendido (filtro de mediana) |
| RNF04 | Funcionar de forma estável, sem falhas intermitentes | ✅ atendido após correções |
| RNF05 | Ter carenagem que permita acesso aos componentes | PREENCHER |
| RNF06 | Código organizado e comentado | ✅ atendido |

## 2. Proposta inicial e esboços

<!-- PREENCHER: coloque aqui os esboços feitos em aula (fotos/desenhos) em docs/img/ -->

A proposta inicial foi um carrinho de duas rodas motrizes com uma roda livre, controlado pelo celular via Bluetooth e com um sensor ultrassônico na frente para evitar colisões.

![Esboço inicial](img/esboco-inicial.jpg)

## 3. MVP (produto mínimo viável)

O MVP definido foi:

1. Carrinho se move para frente, para trás e faz curvas.
2. Controle pelo celular via Bluetooth.
3. Sensor ultrassônico para o carrinho **antes de bater** quando anda para frente.
4. Tudo alimentado por bateria.

Itens além do MVP: alerta sonoro (buzzer), envio da distância para o app, filtro de leituras do sensor, carenagem.

## 4. Backlog

| # | Item | Prioridade | Status |
|---|---|---|---|
| 1 | Montar chassi com motores e roda livre | Alta | ✅ Feito |
| 2 | Ligar ponte H L298N e testar motores | Alta | ✅ Feito |
| 3 | Programar frente, ré, esquerda, direita e parar | Alta | ✅ Feito |
| 4 | Integrar HC-05 e controlar pelo app | Alta | ✅ Feito |
| 5 | Integrar HC-SR04 e parar em obstáculos | Alta | ✅ Feito |
| 6 | Alimentação por bateria (sem USB) | Alta | ✅ Feito |
| 7 | Corrigir falhas intermitentes dos motores | Alta | ✅ Feito |
| 8 | Filtrar leituras falsas do sensor | Média | ✅ Feito |
| 9 | Buzzer de alerta | Média | ⚠️ Em verificação |
| 10 | Enviar distância para o app | Baixa | ✅ Feito |
| 11 | Compensar diferença de velocidade entre as rodas | Baixa | ❌ Testado e descartado (ver decisões) |
| 12 | Versão com app Dabble | Baixa | ✅ Feito (versão alternativa) |
| 13 | Carenagem | Média | PREENCHER |
| 14 | Documentação no GitHub | Alta | ✅ Feito |

## 5. Planejamento

| Etapa | O que foi feito |
|---|---|
| 1. Projeto | Definição de requisitos, esboços e lista de componentes |
| 2. Mecânica | Chassi e fixação de motores, rodas e placas |
| 3. Montagem elétrica | Ligação de Arduino, ponte H, sensor, Bluetooth e buzzer |
| 4. Programação | Código de controle dos motores, comandos Bluetooth e sensor |
| 5. Testes por módulo | Cada parte testada isolada com um código de teste próprio |
| 6. Integração | Código final com todas as partes juntas |
| 7. Acabamento | Carenagem |
| 8. Documentação | README, documentos e evidências |

<!-- PREENCHER (opcional): datas de cada etapa -->

## 6. Decisões de projeto

| Decisão | Motivo |
|---|---|
| **Arduino Uno** como controlador | Disponível no laboratório, fácil de programar e com pinos suficientes |
| **Ponte H L298N** | Controla 2 motores DC nos dois sentidos, comum e barata |
| **Bluetooth HC-05 via SoftwareSerial (D10/D11)** | Deixa a serial USB (D0/D1) livre para gravar o código e depurar pelo Monitor Serial |
| **Alimentação separada** (4 AA nos motores, 9 V no Arduino) | Evita que o pico de corrente dos motores reinicie o Arduino |
| **GND comum ligado direto no borne da ponte H** | O GND passando pela protoboard causava mau contato e falhas intermitentes (ver T1) |
| **Sensor lido a cada 100 ms, sem travar o programa** (`millis()`) | O carrinho continua recebendo comandos enquanto mede a distância |
| **Mediana das 3 últimas leituras** | Leituras isoladas erradas paravam o carrinho sem motivo ou liberavam com o obstáculo ainda na frente (ver T4) |
| **`pulseInLong()` no lugar de `pulseIn()`** | `pulseIn()` encurta a medição quando chega dado do Bluetooth no meio dela |
| **ENA/ENB com jumper (velocidade máxima)** | O controle de velocidade por PWM foi testado para corrigir o desvio lateral, mas com 4 pilhas os motores perdiam força para arrancar e a ré falhava; a equipe voltou para velocidade máxima (ver T8) |
| **App serial (letras F/B/L/R/S)** como controle principal | Simples, mostra respostas do carrinho (distância, obstáculo); a versão Dabble foi mantida como alternativa |
| **Testar peça por peça** | Facilita achar a causa de cada problema (ver [testes](05-testes-e-resultados.md)) |

## 7. Evolução e alterações

| Versão | Mudança |
|---|---|
| v1 | Código inicial: comandos Bluetooth, motores, sensor com parada por obstáculo e buzzer |
| v2 | Correção de falhas intermitentes: GND comum direto no borne da ponte H (hardware) |
| v2.1 | Uma única medição por ciclo; `pulseInLong()`; confirmação de obstáculo com 2 leituras seguidas |
| v2.2 | Filtro trocado para **mediana das 3 últimas leituras** (resolve leituras falsas "perto" e "longe") |
| v3 | Teste de controle de velocidade por PWM (ENA/ENB) com tranco de arranque — **revertido** |
| v3.1 | Versão alternativa com o app **Dabble** (módulo Gamepad) — mantida em `src/arduino/carrinho_dabble` |
| v4 (final) | Volta para o app serial, motores em velocidade máxima, código reorganizado e comentado |

Os códigos de teste criados durante a evolução estão em `src/arduino/teste_*` e estão descritos em [Software](04-software.md#códigos-de-teste).
