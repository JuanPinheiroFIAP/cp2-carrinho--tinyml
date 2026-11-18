// =====================================================
// GS CITIZENTECH - COLETA DE DADOS DO PISO (MPU-6050)
//
// O carrinho anda reto por alguns segundos e manda pela serial
// (cabo USB) as leituras do MPU-6050 (acelerômetro e giroscópio),
// 200 vezes por segundo. O script dados/gs-piso/capturar_piso.py
// guarda tudo em arquivos CSV no formato do Edge Impulse.
//
// Quem comanda é o script do PC. Comandos (uma linha cada):
//   P                        -> responde "OK GS-COLETA"
//   R,<duracao_ms>,<andar>   -> apita 3 vezes e começa a coletar
//                               andar = 2: anda para a frente gravando e
//                                          depois VOLTA de ré até o começo
//                                          (a volta não é gravada)
//                               andar = 1: só anda para a frente
//                               andar = 0: fica parado
//   S                        -> para tudo na hora
//
// Segurança: no máximo 10 s por coleta; qualquer "S" para os motores.
//
// Ligação:
//   MPU-6050: VCC -> 3V3, GND -> GND, SDA -> GPIO 21, SCL -> GPIO 22
//   Buzzer:   (+) -> GPIO 4, (-) -> GND
//   Ponte H:  IN1 -> GPIO 25, IN2 -> GPIO 26, IN3 -> GPIO 27, IN4 -> GPIO 14
//
// Placa na IDE: DOIT ESP32 DEVKIT V1 (pacote ESP32).
// Monitor Serial (se for olhar): 460800.
// =====================================================

#include <Wire.h>


// =====================================================
// PINOS
// =====================================================

const int PINO_SDA = 21;
const int PINO_SCL = 22;

const int IN1 = 25;
const int IN2 = 26;
const int IN3 = 27;
const int IN4 = 14;

const int BUZZER = 4;


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

// Faixas usadas: ±8 g (batidas passam de 2 g) e ±500 graus/s
const float ACEL_POR_G = 4096.0;
const float GIRO_POR_GRAU_S = 65.5;

// 200 leituras por segundo
const unsigned long PERIODO_US = 5000;

const unsigned long DURACAO_MAXIMA_MS = 10000;


// =====================================================
// ESTADO
// =====================================================

bool mpuPronto = false;

// Erro do giroscópio parado (calculado na calibração)
float giroOffsetX = 0;
float giroOffsetY = 0;
float giroOffsetZ = 0;

String linhaRecebida = "";

unsigned long ultimaTentativa = 0;


// =====================================================
// SETUP
// =====================================================

void setup() {

  // Motores desligados
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  parar();

  pinMode(BUZZER, OUTPUT);

  digitalWrite(BUZZER, LOW);

  Serial.begin(460800);

  Wire.begin(PINO_SDA, PINO_SCL);

  // 400 kHz: leitura rápida o bastante para 200 vezes por segundo
  Wire.setClock(400000);

  Serial.println();
  Serial.println("================================");
  Serial.println("  GS - COLETA DE DADOS DO PISO");
  Serial.println("================================");

  mpuPronto = iniciarMpu();

  if (mpuPronto) {

    Serial.println("#PRONTO GS-COLETA");
  }
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // Se o módulo não foi achado, tenta de novo a cada 3 segundos
  if (!mpuPronto) {

    if (millis() - ultimaTentativa >= 3000) {

      ultimaTentativa = millis();

      mpuPronto = iniciarMpu();

      if (mpuPronto) {

        Serial.println("#PRONTO GS-COLETA");
      }
    }

    return;
  }

  lerComando();
}


// =====================================================
// COMANDOS DO PC
// =====================================================

void lerComando() {

  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\r') {

      continue;
    }

    if (c != '\n') {

      linhaRecebida += c;

      // Linha muito grande: descarta
      if (linhaRecebida.length() > 40) {

        linhaRecebida = "";
      }

      continue;
    }

    String linha = linhaRecebida;

    linhaRecebida = "";

    executarComando(linha);
  }
}


void executarComando(String linha) {

  linha.trim();

  if (linha == "P") {

    Serial.println("OK GS-COLETA");

    return;
  }

  if (linha == "S") {

    parar();

    return;
  }

  if (linha.startsWith("R,")) {

    int duracao = 0;
    int andar = 0;

    if (sscanf(linha.c_str(), "R,%d,%d", &duracao, &andar) == 2) {

      coletar((unsigned long)max(duracao, 0), andar);

    } else {

      Serial.println("ERRO comando R invalido");
    }
  }
}


// =====================================================
// COLETA
// =====================================================

// andar: 0 = parado, 1 = só frente, 2 = frente e volta de ré
void coletar(unsigned long duracaoMs, int andar) {

  if (duracaoMs > DURACAO_MAXIMA_MS) {

    duracaoMs = DURACAO_MAXIMA_MS;
  }

  if (duracaoMs < 500) {

    duracaoMs = 500;
  }

  // Aviso: 3 apitos para dar tempo de soltar o carrinho no chão
  for (int i = 0; i < 3; i++) {

    digitalWrite(BUZZER, HIGH);

    delay(150);

    digitalWrite(BUZZER, LOW);

    delay(250);
  }

  // Descarta o que chegou durante os apitos
  while (Serial.available()) {

    Serial.read();
  }

  Serial.println("#INICIO");

  if (andar >= 1) {

    frente();
  }

  unsigned long inicioMs = millis();

  bool interrompido = false;

  unsigned long totalAmostras = duracaoMs * 1000UL / PERIODO_US;

  unsigned long amostras = 0;

  unsigned long falhas = 0;

  // Se uma leitura falhar, repete a anterior para não abrir
  // um buraco no tempo
  float acel[3] = { 0, 0, 1 };
  float giro[3] = { 0, 0, 0 };

  unsigned long proxima = micros();

  while (amostras < totalAmostras) {

    // "S" no meio da coleta: para tudo
    if (Serial.available()) {

      char c = Serial.read();

      if (c == 'S' || c == 's') {

        interrompido = true;

        break;
      }
    }

    // Ainda não é a hora da próxima leitura
    if ((long)(micros() - proxima) < 0) {

      continue;
    }

    proxima += PERIODO_US;

    if (!lerMpu(acel, giro)) {

      falhas++;
    }

    // Tempo em ms desde o começo (as leituras são a cada 5 ms)
    Serial.printf(
      "%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f\n",
      amostras * (PERIODO_US / 1000),
      acel[0], acel[1], acel[2],
      giro[0], giro[1], giro[2]
    );

    amostras++;
  }

  unsigned long tempoAndadoMs = millis() - inicioMs;

  parar();

  Serial.printf("#FIM amostras=%lu falhas=%lu\n", amostras, falhas);

  // Volta de ré o mesmo tempo que andou, para o carrinho chegar
  // perto do começo sem ninguém precisar pegar ele
  if (andar == 2 && !interrompido) {

    voltarAoInicio(tempoAndadoMs);

    Serial.println("#VOLTOU");
  }
}


// Dá ré pelo mesmo tempo da ida (esta volta não é gravada)
void voltarAoInicio(unsigned long duracaoMs) {

  // Pausa antes de inverter os motores, para não forçar a ponte H
  delay(400);

  re();

  unsigned long inicio = millis();

  while (millis() - inicio < duracaoMs) {

    if (Serial.available()) {

      char c = Serial.read();

      if (c == 'S' || c == 's') {

        break;
      }
    }
  }

  parar();
}


// =====================================================
// MPU-6050
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

  // 1000 Hz / (1 + 4) = 200 leituras por segundo
  escreverRegistrador(REG_SMPLRT_DIV, 0x04);

  // Filtro interno de ruído: deixa passar até uns 94 Hz
  escreverRegistrador(REG_CONFIG, 0x02);

  // ±500 graus/s e ±8 g
  escreverRegistrador(REG_GYRO_CONFIG, 0x08);
  escreverRegistrador(REG_ACCEL_CONFIG, 0x10);

  delay(50);

  calibrarGiroscopio();

  return true;
}


// Com o módulo parado o giroscópio devia ler zero, mas lê um
// pequeno erro. Medimos esse erro aqui e descontamos depois.
void calibrarGiroscopio() {

  Serial.println("Calibrando o giroscopio: deixe o carrinho PARADO...");

  const int AMOSTRAS = 200;

  float somaX = 0;
  float somaY = 0;
  float somaZ = 0;

  int lidas = 0;

  for (int i = 0; i < AMOSTRAS; i++) {

    uint8_t dados[14];

    if (lerBytes(REG_ACCEL_XOUT_H, dados, 14)) {

      somaX += (int16_t)((dados[8] << 8) | dados[9]);
      somaY += (int16_t)((dados[10] << 8) | dados[11]);
      somaZ += (int16_t)((dados[12] << 8) | dados[13]);

      lidas++;
    }

    delay(5);
  }

  if (lidas == 0) {

    return;
  }

  giroOffsetX = somaX / lidas / GIRO_POR_GRAU_S;
  giroOffsetY = somaY / lidas / GIRO_POR_GRAU_S;
  giroOffsetZ = somaZ / lidas / GIRO_POR_GRAU_S;
}


// Lê aceleração (em g) e giro (em graus/s). Devolve false se falhar.
bool lerMpu(float* acel, float* giro) {

  uint8_t dados[14];

  if (!lerBytes(REG_ACCEL_XOUT_H, dados, 14)) {

    return false;
  }

  acel[0] = (int16_t)((dados[0] << 8) | dados[1]) / ACEL_POR_G;
  acel[1] = (int16_t)((dados[2] << 8) | dados[3]) / ACEL_POR_G;
  acel[2] = (int16_t)((dados[4] << 8) | dados[5]) / ACEL_POR_G;

  giro[0] = (int16_t)((dados[8] << 8) | dados[9]) / GIRO_POR_GRAU_S - giroOffsetX;
  giro[1] = (int16_t)((dados[10] << 8) | dados[11]) / GIRO_POR_GRAU_S - giroOffsetY;
  giro[2] = (int16_t)((dados[12] << 8) | dados[13]) / GIRO_POR_GRAU_S - giroOffsetZ;

  return true;
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
// MOTORES
// =====================================================

void frente() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}


void re() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}


void parar() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
