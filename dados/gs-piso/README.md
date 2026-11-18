# Dados do piso (GS CitizenTech)

Vibração do piso medida por um MPU-6050 fixo no carrinho, usada para treinar um modelo de TinyML que classifica o piso por onde o robô anda.

| Classe | Como foi coletada |
|---|---|
| `liso` | piso liso da casa, trecho fixo, corridas de 3 s |
| `irregular` | tapete / papelão amassado, corridas de 3 s |
| `degrau` | régua atravessada no caminho, corridas de 2 s |
| `parado` | motores desligados |

## Arquivos

| Pasta ou arquivo | O que é |
|---|---|
| `amostras/` | os CSVs que vão para o Edge Impulse (`classe.NNN.csv`) |
| `brutas/` | os originais da classe `degrau`, antes do recorte |
| `descartadas/` | corridas em que o carrinho girou ou bateu; o motivo está em `descartadas/LEIA-ME.md` |
| `capturar_piso.py` | grava as corridas pela serial, com o sketch `src/arduino/gs_coleta_piso` |

Formato dos CSVs: `timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ`, a 200 leituras por segundo, aceleração em g e giro em graus/s (faixas de ±8 g e ±500 graus/s).

## Como as amostras foram preparadas

- **Arrancada e freada:** os primeiros 0,7 s e os últimos 0,3 s de cada corrida são jogados fora.
- **Degrau:** o recorte vai de 0,3 s a 1,5 s da corrida bruta, para ficar a batida da régua no meio e sem a arrancada.
- **Corridas descartadas:** o critério é o giroscópio (desvio padrão do giro em Z acima de 8 graus/s quer dizer que o carrinho desviou ou raspou na parede), e não o nível de vibração, para não escolher os dados pela resposta que se espera do modelo.

## Como coletar de novo

1. Grave `src/arduino/gs_coleta_piso` na ESP32 (placa DOIT ESP32 DEVKIT V1) e **feche o Monitor Serial**.
2. Instale a dependência: `pip install pyserial`.
3. Rode, com as pilhas dos motores ligadas e o carrinho no começo do trecho. O carrinho apita 3 vezes, anda para a frente gravando e volta de ré sozinho:

```bash
python capturar_piso.py --classe liso --corridas 10 --duracao 3000
python capturar_piso.py --classe irregular --corridas 10 --duracao 3000
python capturar_piso.py --classe degrau --corridas 10 --duracao 2000 --descartar-inicio 0 --descartar-fim 0
python capturar_piso.py --classe parado --corridas 3 --duracao 3000
```

## Como treinar no Edge Impulse

1. Crie um projeto novo, **público** (por exemplo `gs-piso-robo-inspetor`).
2. *Data acquisition > Upload data*: escolha todos os arquivos de `amostras/`, deixe *Automatically split between training and testing* e *Infer from filename*.
3. *Create impulse*:
   - Time series data: **window size 1000 ms**, **window increase 200 ms**, frequência **200 Hz**, marque só os eixos **accX, accY e accZ**.
   - Processing block: **Spectral Analysis**.
   - Learning block: **Classification**.
4. Gere as features e treine: **100 ciclos**, learning rate 0,0005 e, em *Advanced training settings*, **Auto-weight classes** ligado (com ele desligado a acurácia na validação foi de 82,9%).
5. *Deployment > Arduino library > Build* e baixe o zip.

O projeto que gerou o modelo usado no robô é público: <https://studio.edgeimpulse.com/public/1167661/live>. O zip da biblioteca está em `ei-gs-piso-robo-inspetor-arduino-1.0.1.zip`.

## Resultados do modelo

| Medida | Resultado |
|---|---|
| Validação (janelas de arquivos que também aparecem no treino) | 97,6% de acurácia, loss 0,30 |
| Teste com 7 corridas que o modelo nunca viu | **77,3%** (liso 91%, degrau 50%, irregular 17%) |

A validação é otimista, porque janelas vizinhas de uma mesma corrida caem em treino e validação. O teste é o número honesto, mas tem só 1 corrida de `irregular` e 2 de `degrau`. A discussão completa está em [`docs/09-gs-robo-inspetor.md`](../../docs/09-gs-robo-inspetor.md#6-modelo-edge-impulse).

## Teste offline

Antes de treinar, um classificador simples (vizinho mais próximo, janelas de 1 s, deixando uma corrida inteira de fora por vez) acertou 93,6% das janelas. Os erros são sobretudo `irregular` e `degrau` confundidos com `liso`, porque essas classes têm poucas amostras.
