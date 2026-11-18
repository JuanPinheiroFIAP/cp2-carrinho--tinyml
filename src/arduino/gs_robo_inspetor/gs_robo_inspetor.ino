// =====================================================
// GS CITIZENTECH - ROBÔ INSPETOR DE ACESSIBILIDADE
//
// O carrinho percorre calçadas, corredores e rampas e registra
// o que atrapalha a circulação de quem usa cadeira de rodas ou
// anda com dificuldade:
//
//   Obstáculo no caminho ........ HC-SR04 (ultrassônico)
//   Rampa ou calçada inclinada .. MPU-6050 (acelerômetro)
//   Tipo de piso e degrau ....... MPU-6050 + modelo de TinyML
//                                 (Edge Impulse), que classifica a
//                                 vibração em liso, irregular,
//                                 degrau e parado
//   Carrinho tombado ............ MPU-6050
//
// Dois jeitos de usar:
//   MANUAL:   dirige com o controle de PS4
//   PATRULHA: o carrinho anda sozinho, desvia dos obstáculos e
//             registra as ocorrências
//
// Controles do PS4:
//   R2 = frente | L2 = ré | R2 + L2 = freio
//   Analógico esquerdo (ou D-pad) = virar
//   △ (triângulo) = liga / desliga a PATRULHA
//   ✕ = parada de emergência (também sai da patrulha)
//   R3 (clique do analógico direito) = buzina
//   Options = imprime o relatório da inspeção na serial
//   Share = recalibra o nível (com o carrinho em piso plano)
//
// A luz do controle PISCA por 2 s na cor da ocorrência:
//   magenta = degrau | laranja = rampa acima da norma
//   amarelo = piso irregular | branco = batida
//   vermelho = obstáculo ou carrinho tombado
//
// O botão BOOT da placa também liga / desliga a patrulha.
//
// Placa na IDE: ESP32 + Bluepad32 Arduino > DOIT ESP32 DEVKIT V1
// Biblioteca do modelo: o zip gerado no Edge Impulse (Deployment >
// Arduino library). Se o nome dela for outro, troque o #include.
// Monitor Serial: 115200
//
// Ligação:
//   Ponte H L298N: IN1 -> GPIO 25, IN2 -> GPIO 26,
//                  IN3 -> GPIO 27, IN4 -> GPIO 14
//   HC-SR04: VCC -> 5V, GND -> GND, TRIG -> GPIO 18,
//            ECHO -> GPIO 19 por divisor de tensão (1 kΩ + 2 kΩ)
//   MPU-6050: VCC -> 3V3, GND -> GND, SDA -> GPIO 21, SCL -> GPIO 22
//             (eixo X apontando para a frente do carrinho)
//   Buzzer ativo: (+) -> GPIO 4, (−) -> GND
//
// Ligue o carrinho em piso PLANO: no começo ele calibra o
// giroscópio e o nível (uns 3 segundos, com ele parado).
// =====================================================

#include <Bluepad32.h>
#include <Wire.h>
#include <gs-piso-robo-inspetor_inferencing.h>

// O modelo foi treinado com os 3 eixos do acelerômetro
static_assert(
  EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 3,
  "Treine o modelo so com os eixos accX, accY e accZ"
);


// =====================================================
// TIPOS
// =====================================================

enum TipoPiso {
  PISO_DESCONHECIDO,
  PISO_LISO,
  PISO_IRREGULAR,
  PISO_DEGRAU,
  PISO_PARADO
};

enum ClasseDistancia {
  DIST_PERTO,
  DIST_MEDIO,
  DIST_LONGE
};

enum TipoEvento {
  EVENTO_OBSTACULO,
  EVENTO_DEGRAU,
  EVENTO_PISO_IRREGULAR,
  EVENTO_RAMPA,
  EVENTO_TOMBADO,
  EVENTO_BATIDA,
  QUANTIDADE_EVENTOS
};

enum EstadoPatrulha {
  PATRULHA_ANDANDO,
  PATRULHA_PARADA,
  PATRULHA_GIRANDO,
  PATRULHA_RE
};

struct Evento {
  unsigned long tempoMs;
  TipoEvento tipo;
  float distanciaCm;
  float inclinacaoPercent;
};

const char* NOMES_EVENTOS[QUANTIDADE_EVENTOS] = {
  "OBSTACULO",
  "DEGRAU",
  "PISO IRREGULAR",
  "RAMPA ACIMA DA NORMA",
  "CARRINHO TOMBADO",
  "BATIDA (COLISAO)"
};


// =====================================================
// PINOS
// =====================================================

// L298N - Motor A (esquerda) e Motor B (direita)
const int IN1 = 25;
const int IN2 = 26;
const int IN3 = 27;
const int IN4 = 14;

// ENA e ENB ficam com jumper na ponte H (velocidade máxima)

const int BUZZER = 4;

const int TRIG = 18;
const int ECHO = 19;

const int PINO_SDA = 21;
const int PINO_SCL = 22;

// Botão BOOT da própria placa (LOW quando apertado)
const int PINO_BOTAO = 0;


// =====================================================
// MPU-6050
// =====================================================

const uint8_t ENDERECO_MPU = 0x68;

const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1 = 0x6B;

// Mesmas faixas usadas na coleta dos dados: ±8 g e ±500 graus/s
const float ACEL_POR_G = 4096.0;
const float GIRO_POR_GRAU_S = 65.5;

// 200 leituras por segundo, como na coleta
const unsigned long PERIODO_MPU_MS = 5;

bool mpuOk = false;

// Erro do giroscópio parado (calibração)
float giroOffsetZ = 0;

// Janela mais recente de aceleração (g), que vai para o modelo.
// Quem escreve é a tarefa do MPU; quem lê é o loop().
const int AMOSTRAS_JANELA = EI_CLASSIFIER_RAW_SAMPLE_COUNT;

float janelaAcel[AMOSTRAS_JANELA][3];

volatile int indiceJanela = 0;

volatile unsigned long amostrasTotal = 0;

volatile unsigned long falhasMpu = 0;

portMUX_TYPE travaJanela = portMUX_INITIALIZER_UNLOCKED;

// Gravidade filtrada (passa-baixa de uns 0,5 s): usada para a inclinação
volatile float gravX = 0;
volatile float gravY = 0;
volatile float gravZ = 1;

const float FILTRO_GRAVIDADE = 0.01;

// Giro acumulado em Z (graus), usado nas curvas da patrulha
volatile float anguloZ = 0;

// Nível de referência (carrinho em piso plano)
float nivelPitch = 0;
float nivelRoll = 0;


// =====================================================
// DISTÂNCIA (HC-SR04)
// =====================================================

const float DIST_MIN = 2;      // cm
const float DIST_MAX = 200;    // cm

const float LIMITE_PERTO = 20;
const float LIMITE_LONGE = 60;

const int JANELA_DIST = 5;     // média móvel

float leiturasDist[JANELA_DIST];

int posicaoDist = 0;

int quantidadeDist = 0;

float distanciaMedia = DIST_MAX;

int classeDistancia = DIST_LONGE;

// A classe só muda depois de algumas leituras iguais seguidas.
// Entrar em PERTO é imediato (segurança); sair de PERTO demora mais.
const int CONFIRMACOES_NORMAL = 3;
const int CONFIRMACOES_LIBERAR = 5;

int classeCandidata = DIST_LONGE;

int confirmacoesDist = 0;

// O eco é medido por interrupção, para não errar quando a tarefa
// do MPU interrompe o loop()
void IRAM_ATTR aoMudarEcho();

volatile unsigned long echoSubida = 0;
volatile unsigned long echoDescida = 0;
volatile bool echoConcluido = false;

bool sensorDisparado = false;

unsigned long disparoMs = 0;


// =====================================================
// INCLINAÇÃO E TOMBO
// =====================================================

// A NBR 9050 limita rampas a 8,33% (1:12). Confira na norma.
const float LIMITE_RAMPA_PERCENT = 8.33;

const float LIMITE_TOMBO_GRAUS = 45;

const float LIMITE_TOMBO_VOLTA_GRAUS = 25;

float inclinacaoPercent = 0;

float pitchGraus = 0;

float rollGraus = 0;

unsigned long inicioRampaMs = 0;

unsigned long ultimaRampaMs = 0;

unsigned long inicioTomboMs = 0;

unsigned long inicioPlanoMs = 0;

bool tombado = false;


// =====================================================
// BATIDA (COLISÃO)
// =====================================================

// Rede de segurança do sensor de distância: um tranco forte na
// direção da frente quer dizer que o carrinho bateu em algo.
// Na coleta de dados, a parede deu um pico de 8 g e a régua (degrau)
// ficou abaixo de 2,5 g.
const float LIMITE_BATIDA_G = 3.0;

// Quando foi o último tranco (escrito pela tarefa do MPU)
volatile unsigned long ultimaBatidaMs = 0;

unsigned long batidaTratadaMs = 0;

unsigned long ultimaBatidaRegistradaMs = 0;


// =====================================================
// PISO (MODELO DE TINYML)
// =====================================================

// O modelo só vale com o carrinho andando para a frente e depois
// da arrancada, como nos dados de treino
const unsigned long ESPERA_APOS_ARRANCADA_MS = 1200;

const unsigned long INTERVALO_INFERENCIA_MS = 200;

const float CONFIANCA_MINIMA = 0.60;

const int TAM_HISTORICO = 5;

TipoPiso historicoPiso[TAM_HISTORICO];

int posicaoHistorico = 0;

int quantidadeHistorico = 0;

TipoPiso pisoAtual = PISO_PARADO;

float confiancaPiso = 0;

unsigned long ultimaInferenciaMs = 0;

unsigned long ultimoDegrauMs = 0;

unsigned long ultimoIrregularMs = 0;

float bufferEntrada[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];


// =====================================================
// OCORRÊNCIAS
// =====================================================

const int MAX_EVENTOS = 40;

Evento eventos[MAX_EVENTOS];

int totalEventos = 0;

int contadores[QUANTIDADE_EVENTOS];

unsigned long ultimoObstaculoMs = 0;


// =====================================================
// CONTROLE
// =====================================================

ControllerPtr controle = nullptr;

// O analógico vai de -511 a +512. Abaixo disso, ignora.
const int ZONA_MORTA = 200;

// R2 e L2 vão de 0 a 1023. Acima disso, o gatilho conta como apertado.
const int GATILHO_MINIMO = 100;

// Coloque true se o controle não parear. Depois volte para false.
const bool ESQUECER_PAREAMENTOS = false;

bool saudacaoPendente = false;

long ultimaCor = -1;

const int BATERIA_BAIXA = 50;

const unsigned long INTERVALO_BATERIA = 30000;

unsigned long ultimaChecagemBateria = 0;

bool trianguloAnterior = false;
bool optionsAnterior = false;
bool shareAnterior = false;
bool botaoAnterior = false;

unsigned long ultimoBotaoMs = 0;

// Luz do controle: pisca na cor da ocorrência por uns segundos
unsigned long luzEventoAte = 0;

uint8_t luzEventoR = 0;
uint8_t luzEventoG = 0;
uint8_t luzEventoB = 0;

const unsigned long DURACAO_LUZ_EVENTO_MS = 2000;


// =====================================================
// ESTADO DO CARRINHO
// =====================================================

// F frente, B ré, L esquerda, R direita, E ré esquerda,
// D ré direita, G girando (patrulha), S parado
char movimentoAtual = 'S';

unsigned long inicioFrenteMs = 0;

bool emergenciaAtiva = false;

bool bloqueadoPorObstaculo = false;

bool modoPatrulha = false;

EstadoPatrulha estadoPatrulha = PATRULHA_ANDANDO;

unsigned long fimParadaMs = 0;

unsigned long inicioGiroMs = 0;

float anguloInicioGiro = 0;

bool pendenteDegrau = false;

bool pendenteRampa = false;

bool pendenteBatida = false;

unsigned long inicioReMs = 0;

unsigned long inicioInspecaoMs = 0;

unsigned long ultimoStatusMs = 0;


// =====================================================
// BUZZER
// =====================================================

// Buzina: R3 segurado
bool buzinaAtiva = false;

// Alerta de tempo fixo
unsigned long buzzerDesligaEm = 0;

// Sequência de bipes curtos
int bipesRestantes = 0;

bool bipeLigado = false;

unsigned long bipeProximaTroca = 0;


// =====================================================
// SETUP
// =====================================================

void setup() {

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Começa parado
  motorA(0);
  motorB(0);

  pinMode(BUZZER, OUTPUT);

  digitalWrite(BUZZER, LOW);

  pinMode(PINO_BOTAO, INPUT_PULLUP);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  digitalWrite(TRIG, LOW);

  Serial.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println("  GS - ROBO INSPETOR");
  Serial.println("================================");

  Serial.print("Modelo do piso: ");
  Serial.println(EI_CLASSIFIER_PROJECT_NAME);

  Serial.println("Ligue o carrinho em piso PLANO e parado.");

  limparHistoricoPiso();

  // MPU-6050
  Wire.begin(PINO_SDA, PINO_SCL);

  Wire.setClock(400000);

  mpuOk = iniciarMpu();

  // Sensor de distância: o eco é medido por interrupção
  attachInterrupt(digitalPinToInterrupt(ECHO), aoMudarEcho, CHANGE);

  // Controle de PS4
  BP32.setup(&aoConectarControle, &aoDesconectarControle);

  if (ESQUECER_PAREAMENTOS) {

    BP32.forgetBluetoothKeys();
  }

  if (mpuOk) {

    // A leitura do MPU fica numa tarefa só dela, a cada 5 ms certinhos.
    // Prioridade maior que a do loop(), para nada atrasar as leituras.
    xTaskCreatePinnedToCore(tarefaMpu, "mpu", 4096, NULL, 3, NULL, 1);

    // Espera o filtro da gravidade assentar e guarda o nível plano
    delay(2000);

    calibrarNivel();

  } else {

    Serial.println("SEM MPU-6050: inclinacao, piso e tombo desligados.");
  }

  inicioInspecaoMs = millis();

  Serial.println("Segure SHARE + PS no controle para parear.");
  Serial.println("Triangulo ou botao BOOT: liga a patrulha.");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  BP32.update();

  lerBotaoBoot();

  atualizarDistancia();

  atualizarInclinacao();

  atualizarBatida();

  atualizarPiso();

  if (controle && controle->isConnected() && controle->isGamepad()) {

    if (saudacaoPendente && controle->hasData()) {

      saudacaoPendente = false;

      saudarControle(controle);
    }

    lerControle(controle);

    checarBateria(controle);
  }

  if (tombado) {

    // Carrinho tombado: nada de motor
    if (modoPatrulha) {

      modoPatrulha = false;

      Serial.println("PATRULHA encerrada: carrinho tombado.");
    }

    mover('S');

  } else if (modoPatrulha) {

    executarPatrulha();

  } else if (!controle) {

    // Sem controle e sem patrulha: garante que está parado
    mover('S');
  }

  atualizarLuzEvento();

  atualizarBuzzer();

  mostrarStatus();

  // Cede o processador por um instante (e não trava o watchdog)
  delay(1);
}


// =====================================================
// MPU-6050 (baixo nível)
// =====================================================

bool iniciarMpu() {

  Wire.beginTransmission(ENDERECO_MPU);

  if (Wire.endTransmission() != 0) {

    Serial.println("ERRO: MPU-6050 nao encontrado (confira VCC, GND, SDA e SCL).");

    return false;
  }

  // Acorda o módulo (ele começa dormindo)
  escreverRegistrador(REG_PWR_MGMT_1, 0x00);

  delay(100);

  // Mesma configuração da coleta de dados: 200 leituras por segundo,
  // filtro de uns 94 Hz, ±500 graus/s e ±8 g
  escreverRegistrador(REG_SMPLRT_DIV, 0x04);
  escreverRegistrador(REG_CONFIG, 0x02);
  escreverRegistrador(REG_GYRO_CONFIG, 0x08);
  escreverRegistrador(REG_ACCEL_CONFIG, 0x10);

  delay(50);

  calibrarGiroscopio();

  return true;
}


// Com o módulo parado o giroscópio devia ler zero, mas lê um
// pequeno erro. Medimos esse erro aqui e descontamos depois.
void calibrarGiroscopio() {

  const int AMOSTRAS = 200;

  float soma = 0;

  int lidas = 0;

  for (int i = 0; i < AMOSTRAS; i++) {

    uint8_t dados[14];

    if (lerBytes(REG_ACCEL_XOUT_H, dados, 14)) {

      soma += (int16_t)((dados[12] << 8) | dados[13]);

      lidas++;
    }

    delay(5);
  }

  if (lidas > 0) {

    giroOffsetZ = soma / lidas / GIRO_POR_GRAU_S;
  }
}


// Tarefa que lê o MPU a cada 5 ms e alimenta a janela do modelo,
// a gravidade filtrada e o giro acumulado
void tarefaMpu(void* parametro) {

  TickType_t ultimaAtivacao = xTaskGetTickCount();

  unsigned long ultimoUs = micros();

  for (;;) {

    vTaskDelayUntil(&ultimaAtivacao, pdMS_TO_TICKS(PERIODO_MPU_MS));

    uint8_t dados[14];

    if (!lerBytes(REG_ACCEL_XOUT_H, dados, 14)) {

      falhasMpu = falhasMpu + 1;

      continue;
    }

    float ax = (int16_t)((dados[0] << 8) | dados[1]) / ACEL_POR_G;
    float ay = (int16_t)((dados[2] << 8) | dados[3]) / ACEL_POR_G;
    float az = (int16_t)((dados[4] << 8) | dados[5]) / ACEL_POR_G;

    float gz = (int16_t)((dados[12] << 8) | dados[13]) / GIRO_POR_GRAU_S - giroOffsetZ;

    // Tranco forte na frente (ou atrás): o carrinho bateu em algo
    if (fabs(ax - gravX) > LIMITE_BATIDA_G) {

      ultimaBatidaMs = millis();
    }

    unsigned long agoraUs = micros();

    float dt = (agoraUs - ultimoUs) / 1000000.0;

    ultimoUs = agoraUs;

    portENTER_CRITICAL(&travaJanela);

    int i = indiceJanela;

    janelaAcel[i][0] = ax;
    janelaAcel[i][1] = ay;
    janelaAcel[i][2] = az;

    indiceJanela = (i + 1) % AMOSTRAS_JANELA;

    amostrasTotal = amostrasTotal + 1;

    portEXIT_CRITICAL(&travaJanela);

    gravX = gravX + FILTRO_GRAVIDADE * (ax - gravX);
    gravY = gravY + FILTRO_GRAVIDADE * (ay - gravY);
    gravZ = gravZ + FILTRO_GRAVIDADE * (az - gravZ);

    anguloZ = anguloZ + gz * dt;
  }
}


bool escreverRegistrador(uint8_t registrador, uint8_t valor) {

  Wire.beginTransmission(ENDERECO_MPU);

  Wire.write(registrador);
  Wire.write(valor);

  return Wire.endTransmission() == 0;
}


bool lerBytes(uint8_t registrador, uint8_t* destino, size_t quantidade) {

  Wire.beginTransmission(ENDERECO_MPU);

  Wire.write(registrador);

  if (Wire.endTransmission(false) != 0) {

    return false;
  }

  size_t recebidos = Wire.requestFrom((int)ENDERECO_MPU, (int)quantidade, (int)true);

  if (recebidos != quantidade) {

    return false;
  }

  for (size_t i = 0; i < quantidade; i++) {

    destino[i] = Wire.read();
  }

  return true;
}


// =====================================================
// INCLINAÇÃO E TOMBO
// =====================================================

float calcularPitchBruto() {

  float ax = gravX;
  float ay = gravY;
  float az = gravZ;

  return atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}


float calcularRollBruto() {

  float ay = gravY;
  float az = gravZ;

  return atan2(ay, az) * 180.0 / PI;
}


// Guarda como "plano" a posição atual do carrinho
void calibrarNivel() {

  nivelPitch = calcularPitchBruto();

  nivelRoll = calcularRollBruto();

  Serial.print("Nivel calibrado (pitch ");
  Serial.print(nivelPitch, 1);
  Serial.print(" graus, roll ");
  Serial.print(nivelRoll, 1);
  Serial.println(" graus).");
}


void atualizarInclinacao() {

  if (!mpuOk) {

    return;
  }

  unsigned long agora = millis();

  pitchGraus = calcularPitchBruto() - nivelPitch;

  rollGraus = calcularRollBruto() - nivelRoll;

  inclinacaoPercent = fabs(tan(pitchGraus * PI / 180.0)) * 100.0;

  // ---------------------------------------------------
  // Tombo: muito inclinado por meio segundo
  // ---------------------------------------------------

  bool muitoInclinado =
    fabs(pitchGraus) > LIMITE_TOMBO_GRAUS ||
    fabs(rollGraus) > LIMITE_TOMBO_GRAUS;

  if (!tombado) {

    if (muitoInclinado) {

      if (inicioTomboMs == 0) {

        inicioTomboMs = agora;
      }

      if (agora - inicioTomboMs > 500) {

        tombado = true;

        registrarEvento(EVENTO_TOMBADO);
      }

    } else {

      inicioTomboMs = 0;
    }

  } else {

    // Volta ao normal quando fica plano por 1 segundo
    bool plano =
      fabs(pitchGraus) < LIMITE_TOMBO_VOLTA_GRAUS &&
      fabs(rollGraus) < LIMITE_TOMBO_VOLTA_GRAUS;

    if (plano) {

      if (inicioPlanoMs == 0) {

        inicioPlanoMs = agora;
      }

      if (agora - inicioPlanoMs > 1000) {

        tombado = false;

        inicioTomboMs = 0;

        inicioPlanoMs = 0;

        Serial.println("Carrinho de novo em pe. Motores liberados.");
      }

    } else {

      inicioPlanoMs = 0;
    }

    return;
  }

  // ---------------------------------------------------
  // Rampa acima da norma: só vale com o carrinho parado ou
  // andando em linha reta, e depois da arrancada (a arrancada
  // também inclina a leitura)
  // ---------------------------------------------------

  bool estavel =
    movimentoAtual == 'S' ||
    (movimentoAtual == 'F' && agora - inicioFrenteMs > ESPERA_APOS_ARRANCADA_MS);

  if (estavel && inclinacaoPercent > LIMITE_RAMPA_PERCENT) {

    if (inicioRampaMs == 0) {

      inicioRampaMs = agora;
    }

    // Inclinado por 1 segundo seguido, e no máximo uma vez a cada 5 s
    if (agora - inicioRampaMs > 1000 && agora - ultimaRampaMs > 5000) {

      ultimaRampaMs = agora;

      registrarEvento(EVENTO_RAMPA);

      if (modoPatrulha) {

        pendenteRampa = true;
      }
    }

  } else {

    inicioRampaMs = 0;
  }
}


// =====================================================
// DISTÂNCIA
// =====================================================

void IRAM_ATTR aoMudarEcho() {

  if (digitalRead(ECHO)) {

    echoSubida = micros();

  } else {

    echoDescida = micros();

    echoConcluido = true;
  }
}


void dispararSensor() {

  echoConcluido = false;

  digitalWrite(TRIG, LOW);

  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  disparoMs = millis();

  sensorDisparado = true;
}


void atualizarDistancia() {

  unsigned long agora = millis();

  bool chegouEco = echoConcluido;

  bool semEco = !chegouEco && sensorDisparado && agora - disparoMs > 40;

  if (chegouEco || semEco) {

    float cm = DIST_MAX;

    if (chegouEco) {

      unsigned long duracao = echoDescida - echoSubida;

      cm = duracao * 0.0343 / 2;
    }

    echoConcluido = false;

    sensorDisparado = false;

    processarDistancia(cm);
  }

  // Uma medição a cada 100 ms
  if (!sensorDisparado && agora - disparoMs >= 100) {

    dispararSensor();
  }
}


// Transforma o tranco medido pela tarefa do MPU em uma ocorrência
void atualizarBatida() {

  if (!mpuOk) {

    return;
  }

  unsigned long agora = millis();

  unsigned long batida = ultimaBatidaMs;

  // Nenhum tranco novo desde a última vez
  if (batida == 0 || batida == batidaTratadaMs) {

    return;
  }

  batidaTratadaMs = batida;

  // Só vale andando para a frente e depois da arrancada (a arrancada
  // e o degrau também dão trancos, mas bem mais fracos)
  if (
    movimentoAtual != 'F' ||
    agora - inicioFrenteMs < ESPERA_APOS_ARRANCADA_MS
  ) {

    return;
  }

  // No máximo uma batida a cada 2 segundos
  if (agora - ultimaBatidaRegistradaMs < 2000) {

    return;
  }

  ultimaBatidaRegistradaMs = agora;

  registrarEvento(EVENTO_BATIDA);

  if (modoPatrulha) {

    pendenteBatida = true;
  }
}


void processarDistancia(float cm) {

  // Limita a faixa
  cm = constrain(cm, DIST_MIN, DIST_MAX);

  // Média móvel
  leiturasDist[posicaoDist] = cm;

  posicaoDist = (posicaoDist + 1) % JANELA_DIST;

  if (quantidadeDist < JANELA_DIST) {

    quantidadeDist++;
  }

  if (quantidadeDist < JANELA_DIST) {

    return;
  }

  float soma = 0;

  for (int i = 0; i < JANELA_DIST; i++) {

    soma += leiturasDist[i];
  }

  distanciaMedia = soma / JANELA_DIST;

  // Classe pelos limites
  int nova = DIST_LONGE;

  if (distanciaMedia < LIMITE_PERTO) {

    nova = DIST_PERTO;

  } else if (distanciaMedia < LIMITE_LONGE) {

    nova = DIST_MEDIO;
  }

  if (nova == classeDistancia) {

    classeCandidata = nova;

    confirmacoesDist = 0;

    return;
  }

  if (nova != classeCandidata) {

    classeCandidata = nova;

    confirmacoesDist = 0;
  }

  confirmacoesDist++;

  int necessarias = CONFIRMACOES_NORMAL;

  if (nova == DIST_PERTO) {

    necessarias = 1;

  } else if (classeDistancia == DIST_PERTO) {

    necessarias = CONFIRMACOES_LIBERAR;
  }

  if (confirmacoesDist >= necessarias) {

    classeDistancia = nova;

    confirmacoesDist = 0;

    atualizarLuz();
  }
}


// =====================================================
// PISO (MODELO DE TINYML)
// =====================================================

int obterDadosJanela(size_t offset, size_t length, float* destino) {

  memcpy(destino, bufferEntrada + offset, length * sizeof(float));

  return 0;
}


void limparHistoricoPiso() {

  posicaoHistorico = 0;

  quantidadeHistorico = 0;

  for (int i = 0; i < TAM_HISTORICO; i++) {

    historicoPiso[i] = PISO_DESCONHECIDO;
  }
}


// Roda o modelo na janela mais recente. Devolve false se falhar.
bool classificarPiso(TipoPiso* tipo, float* confianca) {

  // Copia a janela, da leitura mais antiga para a mais nova,
  // com os 3 eixos intercalados (x, y, z, x, y, z...)
  portENTER_CRITICAL(&travaJanela);

  int inicio = indiceJanela;

  for (int i = 0; i < AMOSTRAS_JANELA; i++) {

    int j = (inicio + i) % AMOSTRAS_JANELA;

    bufferEntrada[i * 3 + 0] = janelaAcel[j][0];
    bufferEntrada[i * 3 + 1] = janelaAcel[j][1];
    bufferEntrada[i * 3 + 2] = janelaAcel[j][2];
  }

  portEXIT_CRITICAL(&travaJanela);

  signal_t sinal;

  sinal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  sinal.get_data = &obterDadosJanela;

  ei_impulse_result_t resultado = { 0 };

  EI_IMPULSE_ERROR erro = run_classifier(&sinal, &resultado, false);

  if (erro != EI_IMPULSE_OK) {

    Serial.print("ERRO no modelo: ");
    Serial.println(erro);

    return false;
  }

  int melhor = 0;

  for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {

    if (resultado.classification[i].value >
        resultado.classification[melhor].value) {

      melhor = i;
    }
  }

  const char* nome = ei_classifier_inferencing_categories[melhor];

  *confianca = resultado.classification[melhor].value;

  if (strcmp(nome, "liso") == 0) {

    *tipo = PISO_LISO;

  } else if (strcmp(nome, "irregular") == 0) {

    *tipo = PISO_IRREGULAR;

  } else if (strcmp(nome, "degrau") == 0) {

    *tipo = PISO_DEGRAU;

  } else if (strcmp(nome, "parado") == 0) {

    *tipo = PISO_PARADO;

  } else {

    *tipo = PISO_DESCONHECIDO;
  }

  return true;
}


void atualizarPiso() {

  if (!mpuOk) {

    return;
  }

  unsigned long agora = millis();

  // Parado: o piso não importa
  if (movimentoAtual == 'S') {

    pisoAtual = PISO_PARADO;

    confiancaPiso = 0;

    limparHistoricoPiso();

    return;
  }

  // Só vale andando para a frente (como nos dados de treino), e
  // depois da arrancada
  if (movimentoAtual != 'F') {

    pisoAtual = PISO_DESCONHECIDO;

    confiancaPiso = 0;

    limparHistoricoPiso();

    return;
  }

  if (agora - inicioFrenteMs < ESPERA_APOS_ARRANCADA_MS) {

    return;
  }

  if (amostrasTotal < (unsigned long)AMOSTRAS_JANELA) {

    return;
  }

  if (agora - ultimaInferenciaMs < INTERVALO_INFERENCIA_MS) {

    return;
  }

  ultimaInferenciaMs = agora;

  TipoPiso tipo = PISO_DESCONHECIDO;

  float confianca = 0;

  if (!classificarPiso(&tipo, &confianca)) {

    return;
  }

  confiancaPiso = confianca;

  // Resposta fraca não conta
  if (confianca < CONFIANCA_MINIMA) {

    tipo = PISO_DESCONHECIDO;
  }

  historicoPiso[posicaoHistorico] = tipo;

  posicaoHistorico = (posicaoHistorico + 1) % TAM_HISTORICO;

  if (quantidadeHistorico < TAM_HISTORICO) {

    quantidadeHistorico++;
  }

  // Conta o que apareceu nas últimas respostas
  int degraus = 0;
  int irregulares = 0;
  int lisos = 0;

  for (int i = 0; i < quantidadeHistorico; i++) {

    if (historicoPiso[i] == PISO_DEGRAU) {

      degraus++;

    } else if (historicoPiso[i] == PISO_IRREGULAR) {

      irregulares++;

    } else if (historicoPiso[i] == PISO_LISO) {

      lisos++;
    }
  }

  // Degrau: um tranco só já basta. No máximo um aviso a cada 2 s.
  if (tipo == PISO_DEGRAU && agora - ultimoDegrauMs > 2000) {

    ultimoDegrauMs = agora;

    registrarEvento(EVENTO_DEGRAU);

    if (modoPatrulha) {

      pendenteDegrau = true;
    }
  }

  // Piso irregular: 3 das últimas 5 respostas
  if (irregulares >= 3) {

    if (pisoAtual != PISO_IRREGULAR && agora - ultimoIrregularMs > 3000) {

      ultimoIrregularMs = agora;

      registrarEvento(EVENTO_PISO_IRREGULAR);
    }

    pisoAtual = PISO_IRREGULAR;

  } else if (lisos >= 3) {

    pisoAtual = PISO_LISO;
  }
}


// =====================================================
// OCORRÊNCIAS
// =====================================================

void registrarEvento(TipoEvento tipo) {

  Evento* e = &eventos[totalEventos % MAX_EVENTOS];

  e->tempoMs = millis() - inicioInspecaoMs;
  e->tipo = tipo;
  e->distanciaCm = distanciaMedia;
  e->inclinacaoPercent = inclinacaoPercent;

  totalEventos++;

  contadores[tipo]++;

  Serial.printf(
    "OCORRENCIA #%d | %s | t=%lu s | dist=%.0f cm | inclinacao=%.1f%%\n",
    totalEventos,
    NOMES_EVENTOS[tipo],
    e->tempoMs / 1000,
    e->distanciaCm,
    e->inclinacaoPercent
  );

  // Aviso no próprio carrinho e no controle
  switch (tipo) {

    case EVENTO_OBSTACULO:

      bipar(1);

      break;

    case EVENTO_DEGRAU:

      bipar(2);

      break;

    case EVENTO_PISO_IRREGULAR:

      bipar(1);

      break;

    case EVENTO_RAMPA:

      bipar(3);

      break;

    case EVENTO_TOMBADO:

      apitar(1500);

      break;

    case EVENTO_BATIDA:

      apitar(300);

      break;

    default:

      break;
  }

  if (controle && controle->isConnected()) {

    iniciarLuzEvento(tipo);

    vibrar(controle, tipo == EVENTO_PISO_IRREGULAR ? 150 : 300);
  }
}


void imprimirRelatorio() {

  Serial.println();
  Serial.println("========== RELATORIO DA INSPECAO ==========");

  Serial.printf(
    "Tempo de inspecao: %lu s\n",
    (millis() - inicioInspecaoMs) / 1000
  );

  Serial.printf("Total de ocorrencias: %d\n", totalEventos);

  for (int i = 0; i < QUANTIDADE_EVENTOS; i++) {

    Serial.printf("  %s: %d\n", NOMES_EVENTOS[i], contadores[i]);
  }

  int quantas = totalEventos < MAX_EVENTOS ? totalEventos : MAX_EVENTOS;

  if (quantas > 0) {

    Serial.println("Ultimas ocorrencias (da mais antiga para a mais nova):");
  }

  for (int k = 0; k < quantas; k++) {

    int numero = totalEventos - quantas + k;

    Evento* e = &eventos[numero % MAX_EVENTOS];

    Serial.printf(
      "  #%d t=%lu s  %s  dist=%.0f cm  inclinacao=%.1f%%\n",
      numero + 1,
      e->tempoMs / 1000,
      NOMES_EVENTOS[e->tipo],
      e->distanciaCm,
      e->inclinacaoPercent
    );
  }

  Serial.println("===========================================");
  Serial.println();
}


// =====================================================
// PATRULHA (MODO AUTÔNOMO)
// =====================================================

void alternarPatrulha() {

  if (modoPatrulha) {

    modoPatrulha = false;

    mover('S');

    Serial.println("PATRULHA desligada.");

    bipar(1);

  } else {

    if (tombado) {

      Serial.println("Nao da para patrulhar: carrinho tombado.");

      return;
    }

    modoPatrulha = true;

    estadoPatrulha = PATRULHA_ANDANDO;

    pendenteDegrau = false;

    pendenteRampa = false;

    pendenteBatida = false;

    Serial.println("PATRULHA ligada: o carrinho anda sozinho.");

    bipar(2);
  }

  atualizarLuz();
}


void iniciarGiro() {

  estadoPatrulha = PATRULHA_GIRANDO;

  inicioGiroMs = millis();

  anguloInicioGiro = anguloZ;

  girar();
}


void executarPatrulha() {

  unsigned long agora = millis();

  switch (estadoPatrulha) {

    case PATRULHA_ANDANDO:

      // Obstáculo perto: registra, para e vira
      if (classeDistancia == DIST_PERTO) {

        if (agora - ultimoObstaculoMs > 3000) {

          ultimoObstaculoMs = agora;

          registrarEvento(EVENTO_OBSTACULO);
        }

        mover('S');

        iniciarGiro();

        return;
      }

      // Bateu em algo (o sensor de distância não viu a tempo): dá ré
      // um instante e vira para outro caminho
      if (pendenteBatida) {

        pendenteBatida = false;

        estadoPatrulha = PATRULHA_RE;

        inicioReMs = agora;

        mover('B');

        return;
      }

      // Degrau: para um instante para avisar e segue
      if (pendenteDegrau) {

        pendenteDegrau = false;

        mover('S');

        estadoPatrulha = PATRULHA_PARADA;

        fimParadaMs = agora + 1500;

        return;
      }

      // Rampa acima da norma: para e vira para outro caminho
      if (pendenteRampa) {

        pendenteRampa = false;

        mover('S');

        iniciarGiro();

        return;
      }

      mover('F');

      break;

    case PATRULHA_RE:

      // Ré por 0,6 s e depois gira para sair do obstáculo
      if (agora - inicioReMs >= 600) {

        mover('S');

        iniciarGiro();

      } else {

        mover('B');
      }

      break;

    case PATRULHA_PARADA:

      mover('S');

      if (agora >= fimParadaMs) {

        estadoPatrulha = PATRULHA_ANDANDO;
      }

      break;

    case PATRULHA_GIRANDO:

      // Gira até uns 90 graus pelo giroscópio (ou 2,5 s, no máximo)
      if (
        fabs(anguloZ - anguloInicioGiro) >= 90 ||
        agora - inicioGiroMs > 2500
      ) {

        mover('S');

        estadoPatrulha = PATRULHA_PARADA;

        fimParadaMs = agora + 300;

      } else {

        girar();
      }

      break;
  }
}


// =====================================================
// CONTROLE
// =====================================================

void aoConectarControle(ControllerPtr ctl) {

  if (controle == nullptr) {

    Serial.println("Controle conectado!");

    controle = ctl;

    saudacaoPendente = true;

    ultimaCor = -1;

    ultimaChecagemBateria = millis();

  } else {

    Serial.println("Ja existe um controle conectado.");
  }
}


void aoDesconectarControle(ControllerPtr ctl) {

  if (controle == ctl) {

    Serial.println("Controle desconectou!");

    controle = nullptr;

    saudacaoPendente = false;

    emergenciaAtiva = false;

    bloqueadoPorObstaculo = false;

    buzinaAtiva = false;

    ultimaCor = -1;

    // Em patrulha o carrinho continua por conta própria;
    // no manual, para.
    if (!modoPatrulha) {

      mover('S');
    }
  }
}


void lerBotaoBoot() {

  bool apertado = digitalRead(PINO_BOTAO) == LOW;

  if (apertado && !botaoAnterior && millis() - ultimoBotaoMs > 300) {

    ultimoBotaoMs = millis();

    alternarPatrulha();
  }

  botaoAnterior = apertado;
}


void lerControle(ControllerPtr c) {

  // Buzina: segurou R3, buzina. Soltou, para na hora.
  buzinaAtiva = c->thumbR();

  // △: liga / desliga a patrulha (só na hora que aperta)
  bool triangulo = c->y();

  if (triangulo && !trianguloAnterior) {

    alternarPatrulha();
  }

  trianguloAnterior = triangulo;

  // Options: relatório na serial
  bool options = c->miscStart();

  if (options && !optionsAnterior) {

    imprimirRelatorio();
  }

  optionsAnterior = options;

  // Share: recalibra o nível (carrinho em piso plano)
  bool share = c->miscSelect();

  if (share && !shareAnterior && mpuOk) {

    calibrarNivel();

    bipar(1);
  }

  shareAnterior = share;

  // Parada de emergência: botão X (✕). Também sai da patrulha.
  if (c->a()) {

    if (!emergenciaAtiva) {

      emergenciaAtiva = true;

      if (modoPatrulha) {

        modoPatrulha = false;

        Serial.println("PATRULHA encerrada pela emergencia.");
      }

      mover('S');

      atualizarLuz();

      vibrar(c, 250);

      apitar(250);
    }

    mover('S');

    return;
  }

  // Soltou o botão X: volta a luz normal
  if (emergenciaAtiva) {

    emergenciaAtiva = false;

    atualizarLuz();
  }

  // Em patrulha ou tombado, o carrinho não obedece ao analógico
  if (modoPatrulha || tombado) {

    return;
  }

  int x = c->axisX();

  uint8_t dpad = c->dpad();

  bool acelerando = c->throttle() > GATILHO_MINIMO;   // R2
  bool dandoRe = c->brake() > GATILHO_MINIMO;         // L2

  bool esq = x < -ZONA_MORTA || (dpad & DPAD_LEFT);
  bool dir = x > ZONA_MORTA || (dpad & DPAD_RIGHT);

  // Obstáculo perto e o motorista quer ir para a frente: bloqueia
  bool frenteBloqueada =
    acelerando && !dandoRe && classeDistancia == DIST_PERTO;

  if (frenteBloqueada != bloqueadoPorObstaculo) {

    bloqueadoPorObstaculo = frenteBloqueada;

    if (frenteBloqueada) {

      Serial.println("FRENTE BLOQUEADA (obstaculo perto)");

      vibrar(c, 300);

      apitar(300);
    }

    atualizarLuz();
  }

  if (acelerando && dandoRe) {

    // Os dois juntos: freio
    mover('S');

  } else if (frenteBloqueada) {

    mover('S');

  } else if (acelerando) {

    if (esq) {

      mover('L');

    } else if (dir) {

      mover('R');

    } else {

      mover('F');
    }

  } else if (dandoRe) {

    // Em ré, virar para a esquerda leva a traseira para a esquerda
    if (esq) {

      mover('E');

    } else if (dir) {

      mover('D');

    } else {

      mover('B');
    }

  } else {

    // Sem acelerar: parado
    mover('S');
  }
}


// =====================================================
// MOVIMENTOS
// =====================================================

// Só muda quando o comando é diferente do atual
void mover(char comando) {

  if (comando == movimentoAtual) {

    return;
  }

  switch (comando) {

    case 'F':

      frente();

      break;

    case 'B':

      re();

      break;

    case 'L':

      esquerda();

      break;

    case 'R':

      direita();

      break;

    case 'E':

      reEsquerda();

      break;

    case 'D':

      reDireita();

      break;

    case 'S':

      parar();

      break;
  }
}


void frente() {

  movimentoAtual = 'F';

  inicioFrenteMs = millis();

  motorA(1);
  motorB(1);

  atualizarLuz();
}


void re() {

  movimentoAtual = 'B';

  motorA(-1);
  motorB(-1);

  atualizarLuz();
}


void esquerda() {

  movimentoAtual = 'L';

  // Motor A parado, motor B para frente
  motorA(0);
  motorB(1);

  atualizarLuz();
}


void direita() {

  movimentoAtual = 'R';

  // Motor A para frente, motor B parado
  motorA(1);
  motorB(0);

  atualizarLuz();
}


void reEsquerda() {

  movimentoAtual = 'E';

  motorA(0);
  motorB(-1);

  atualizarLuz();
}


void reDireita() {

  movimentoAtual = 'D';

  motorA(-1);
  motorB(0);

  atualizarLuz();
}


// Gira no lugar para a direita (esquerda para frente, direita para trás)
void girar() {

  if (movimentoAtual != 'G') {

    movimentoAtual = 'G';

    atualizarLuz();
  }

  motorA(1);
  motorB(-1);
}


void parar() {

  movimentoAtual = 'S';

  motorA(0);
  motorB(0);

  atualizarLuz();
}


// =====================================================
// RESPOSTA NO CONTROLE (luz e vibração)
// =====================================================

// Manda a cor para a barra de luz, só se for diferente da última
void definirCor(uint8_t r, uint8_t g, uint8_t b) {

  if (!controle) {

    return;
  }

  long codigo = ((long)r << 16) | ((long)g << 8) | b;

  if (codigo == ultimaCor) {

    return;
  }

  ultimaCor = codigo;

  controle->setColorLED(r, g, b);
}


void atualizarLuz() {

  // Enquanto a luz pisca por causa de uma ocorrência, ela manda
  if (luzEventoAte != 0) {

    return;
  }

  bool indoParaFrente =
    movimentoAtual == 'F' ||
    movimentoAtual == 'L' ||
    movimentoAtual == 'R';

  if (tombado || emergenciaAtiva || bloqueadoPorObstaculo) {

    definirCor(255, 0, 0);           // vermelho

  } else if (modoPatrulha) {

    definirCor(0, 200, 255);         // ciano: patrulha

  } else if (movimentoAtual == 'S') {

    definirCor(0, 255, 0);           // verde: parado

  } else if (!indoParaFrente) {

    definirCor(255, 160, 0);         // amarelo: ré ou girando

  } else if (classeDistancia == DIST_MEDIO) {

    definirCor(160, 0, 255);         // roxo: obstáculo médio

  } else {

    definirCor(0, 0, 255);           // azul: andando
  }
}


void vibrar(ControllerPtr c, int duracaoMs) {

  c->playDualRumble(0, duracaoMs, 0xC0, 0xC0);
}


// Escolhe a cor da luz para a ocorrência e começa a piscar
void iniciarLuzEvento(TipoEvento tipo) {

  switch (tipo) {

    case EVENTO_DEGRAU:

      luzEventoR = 255;
      luzEventoG = 0;
      luzEventoB = 255;          // magenta

      break;

    case EVENTO_RAMPA:

      luzEventoR = 255;
      luzEventoG = 110;
      luzEventoB = 0;            // laranja

      break;

    case EVENTO_PISO_IRREGULAR:

      luzEventoR = 255;
      luzEventoG = 255;
      luzEventoB = 0;            // amarelo

      break;

    case EVENTO_BATIDA:

      luzEventoR = 255;
      luzEventoG = 255;
      luzEventoB = 255;          // branco

      break;

    default:

      luzEventoR = 255;
      luzEventoG = 0;
      luzEventoB = 0;            // vermelho (obstáculo e tombo)

      break;
  }

  luzEventoAte = millis() + DURACAO_LUZ_EVENTO_MS;
}


// Pisca a luz do controle (250 ms ligada, 250 ms apagada) e, no
// fim, volta para a cor do estado do carrinho
void atualizarLuzEvento() {

  if (luzEventoAte == 0) {

    return;
  }

  unsigned long agora = millis();

  if (agora >= luzEventoAte) {

    luzEventoAte = 0;

    ultimaCor = -1;

    atualizarLuz();

    return;
  }

  bool ligada = (agora / 250) % 2 == 0;

  if (ligada) {

    definirCor(luzEventoR, luzEventoG, luzEventoB);

  } else {

    definirCor(0, 0, 0);
  }
}


void saudarControle(ControllerPtr c) {

  atualizarLuz();

  c->setPlayerLEDs(1);

  vibrar(c, 200);

  int nivel = c->battery();

  Serial.print("Bateria do controle: ");

  if (nivel == 0) {

    Serial.println("desconhecida");

  } else {

    Serial.print(nivel * 100 / 255);
    Serial.println("%");
  }
}


// Bateria baixa: duas vibrações curtas, no máximo a cada 30 s
void checarBateria(ControllerPtr c) {

  if (millis() - ultimaChecagemBateria < INTERVALO_BATERIA) {

    return;
  }

  ultimaChecagemBateria = millis();

  int nivel = c->battery();

  if (nivel > 0 && nivel < BATERIA_BAIXA) {

    Serial.println("Bateria do controle baixa!");

    c->playDualRumble(0, 150, 0xC0, 0xC0);
    c->playDualRumble(300, 150, 0xC0, 0xC0);
  }
}


// =====================================================
// BUZZER
// =====================================================

// Apito de tempo fixo (sem delay)
void apitar(int duracaoMs) {

  buzzerDesligaEm = millis() + duracaoMs;
}


// Sequência de bipes curtos (sem delay)
void bipar(int quantidade) {

  bipesRestantes = quantidade;

  bipeLigado = false;

  bipeProximaTroca = 0;
}


// Decide se o buzzer deve estar ligado agora
void atualizarBuzzer() {

  unsigned long agora = millis();

  if (bipesRestantes > 0 && agora >= bipeProximaTroca) {

    if (!bipeLigado) {

      bipeLigado = true;

    } else {

      bipeLigado = false;

      bipesRestantes--;
    }

    bipeProximaTroca = agora + 120;
  }

  bool alertaAtivo = buzzerDesligaEm != 0 && agora < buzzerDesligaEm;

  if (buzzerDesligaEm != 0 && !alertaAtivo) {

    buzzerDesligaEm = 0;
  }

  digitalWrite(BUZZER, (buzinaAtiva || alertaAtivo || bipeLigado) ? HIGH : LOW);
}


// =====================================================
// STATUS NA SERIAL
// =====================================================

const char* nomePiso(TipoPiso piso) {

  switch (piso) {

    case PISO_LISO:

      return "LISO";

    case PISO_IRREGULAR:

      return "IRREGULAR";

    case PISO_DEGRAU:

      return "DEGRAU";

    case PISO_PARADO:

      return "PARADO";

    default:

      return "?";
  }
}


void mostrarStatus() {

  unsigned long agora = millis();

  if (agora - ultimoStatusMs < 1000) {

    return;
  }

  ultimoStatusMs = agora;

  const char* nomesDist[3] = { "PERTO", "MEDIO", "LONGE" };

  Serial.printf(
    "modo=%s mov=%c | dist=%.0f cm (%s) | inclinacao=%.1f%% | piso=%s (%.0f%%)%s\n",
    modoPatrulha ? "PATRULHA" : "MANUAL",
    movimentoAtual,
    distanciaMedia,
    nomesDist[classeDistancia],
    inclinacaoPercent,
    nomePiso(pisoAtual),
    confiancaPiso * 100,
    tombado ? " | TOMBADO" : ""
  );
}


// =====================================================
// MOTORES
// =====================================================

// sentido: 1 = frente, -1 = trás, 0 = parado
void motorA(int sentido) {

  digitalWrite(IN1, sentido == 1 ? HIGH : LOW);
  digitalWrite(IN2, sentido == -1 ? HIGH : LOW);
}


void motorB(int sentido) {

  digitalWrite(IN3, sentido == 1 ? HIGH : LOW);
  digitalWrite(IN4, sentido == -1 ? HIGH : LOW);
}
