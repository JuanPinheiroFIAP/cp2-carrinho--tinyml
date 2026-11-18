// =====================================================
// TESTE DO MPU-6050 (GY-521) - ESP32
// Só ESP32 + MPU-6050. Sem biblioteca: o sketch fala direto
// com o módulo pelo I2C.
// Monitor Serial: 115200
//
// Ligação:
//   VCC -> 3V3
//   GND -> GND
//   SDA -> GPIO 21
//   SCL -> GPIO 22
//
// Deixe o carrinho PARADO, numa superfície plana, quando ligar:
// no começo o programa calibra o giroscópio (uns 2 segundos).
//
// Como ler os números com o módulo parado e na horizontal:
//   acel X e Y perto de 0,00 g e acel Z perto de 1,00 g
//   giro X, Y e Z perto de 0 graus/s
// =====================================================

#include <Wire.h>


// =====================================================
// PINOS
// =====================================================

const int PINO_SDA = 21;
const int PINO_SCL = 22;

// Pinos da ponte H: mantidos desligados (LOW) para os motores
// não girarem sozinhos enquanto este teste roda.
const int IN1 = 25;
const int IN2 = 26;
const int IN3 = 27;
const int IN4 = 14;


// =====================================================
// REGISTRADORES DO MPU-6050
// =====================================================

const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_WHO_AM_I = 0x75;

// Com a faixa mais sensível (±2 g e ±250 graus/s):
const float ACEL_POR_G = 16384.0;       // 16384 unidades = 1 g
const float GIRO_POR_GRAU_S = 131.0;    // 131 unidades = 1 grau/s


// =====================================================
// ESTADO
// =====================================================

uint8_t enderecoMpu = 0x68;

bool mpuPronto = false;

// Erro do giroscópio parado (calculado na calibração)
float giroOffsetX = 0;
float giroOffsetY = 0;
float giroOffsetZ = 0;

const unsigned long INTERVALO_LEITURA = 200;

unsigned long ultimaLeitura = 0;

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

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.begin(115200);

  Wire.begin(PINO_SDA, PINO_SCL);

  Serial.println();
  Serial.println("================================");
  Serial.println("   TESTE DO MPU-6050 (GY-521)");
  Serial.println("================================");

  mpuPronto = iniciarMpu();
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
    }

    return;
  }

  if (millis() - ultimaLeitura < INTERVALO_LEITURA) {

    return;
  }

  ultimaLeitura = millis();

  lerEMostrar();
}


// =====================================================
// INICIAR O MÓDULO
// =====================================================

bool iniciarMpu() {

  // Procura o módulo: 0x68 (AD0 solto) ou 0x69 (AD0 no 3V3)
  if (existeNoEndereco(0x68)) {

    enderecoMpu = 0x68;

  } else if (existeNoEndereco(0x69)) {

    enderecoMpu = 0x69;

  } else {

    Serial.println("ERRO: MPU-6050 nao encontrado no I2C.");
    Serial.println("Confira: VCC no 3V3, GND, SDA no GPIO 21 e SCL no GPIO 22.");
    Serial.println("(SDA e SCL trocados tambem dao esse erro.)");

    return false;
  }

  Serial.print("Modulo encontrado no endereco 0x");
  Serial.println(enderecoMpu, HEX);

  int quem = lerRegistrador(REG_WHO_AM_I);

  Serial.print("WHO_AM_I = 0x");
  Serial.print(quem, HEX);
  Serial.println(" (o MPU-6050 original responde 0x68; clones podem responder outro valor e ainda funcionar)");

  // Acorda o módulo (ele começa dormindo)
  escreverRegistrador(REG_PWR_MGMT_1, 0x00);

  delay(100);

  // Filtro interno de ruído (cerca de 44 Hz)
  escreverRegistrador(REG_CONFIG, 0x03);

  // Faixas mais sensíveis: ±250 graus/s e ±2 g
  escreverRegistrador(REG_GYRO_CONFIG, 0x00);
  escreverRegistrador(REG_ACCEL_CONFIG, 0x00);

  delay(50);

  calibrarGiroscopio();

  Serial.println("Pronto! Mexa e incline o modulo e veja os numeros mudarem.");
  Serial.println();

  return true;
}


// =====================================================
// CALIBRAR O GIROSCÓPIO
// =====================================================

// Com o módulo parado, o giroscópio devia ler zero, mas lê um
// pequeno erro. Medimos esse erro aqui e descontamos depois.
void calibrarGiroscopio() {

  Serial.println("Calibrando o giroscopio: deixe o modulo PARADO...");

  const int AMOSTRAS = 300;

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

    Serial.println("Nao consegui ler o modulo durante a calibracao.");

    return;
  }

  giroOffsetX = somaX / lidas / GIRO_POR_GRAU_S;
  giroOffsetY = somaY / lidas / GIRO_POR_GRAU_S;
  giroOffsetZ = somaZ / lidas / GIRO_POR_GRAU_S;

  Serial.print("Erro do giroscopio parado (graus/s): X=");
  Serial.print(giroOffsetX, 2);
  Serial.print(" Y=");
  Serial.print(giroOffsetY, 2);
  Serial.print(" Z=");
  Serial.println(giroOffsetZ, 2);
}


// =====================================================
// LER E MOSTRAR
// =====================================================

void lerEMostrar() {

  uint8_t dados[14];

  if (!lerBytes(REG_ACCEL_XOUT_H, dados, 14)) {

    Serial.println("Falha ao ler o modulo (fio solto?)");

    return;
  }

  // Cada valor ocupa 2 bytes (o primeiro é a parte alta)
  int16_t acelXRaw = (int16_t)((dados[0] << 8) | dados[1]);
  int16_t acelYRaw = (int16_t)((dados[2] << 8) | dados[3]);
  int16_t acelZRaw = (int16_t)((dados[4] << 8) | dados[5]);

  int16_t tempRaw = (int16_t)((dados[6] << 8) | dados[7]);

  int16_t giroXRaw = (int16_t)((dados[8] << 8) | dados[9]);
  int16_t giroYRaw = (int16_t)((dados[10] << 8) | dados[11]);
  int16_t giroZRaw = (int16_t)((dados[12] << 8) | dados[13]);

  // Aceleração em g (1 g é a gravidade da Terra)
  float acelX = acelXRaw / ACEL_POR_G;
  float acelY = acelYRaw / ACEL_POR_G;
  float acelZ = acelZRaw / ACEL_POR_G;

  // Giro em graus por segundo, já sem o erro da calibração
  float giroX = giroXRaw / GIRO_POR_GRAU_S - giroOffsetX;
  float giroY = giroYRaw / GIRO_POR_GRAU_S - giroOffsetY;
  float giroZ = giroZRaw / GIRO_POR_GRAU_S - giroOffsetZ;

  // Temperatura do chip em graus Celsius
  float temperatura = tempRaw / 340.0 + 36.53;

  // Inclinação, calculada a partir de onde a gravidade "aponta"
  // roll = inclinação para os lados, pitch = para frente / trás
  float roll = atan2(acelY, acelZ) * 180.0 / PI;
  float pitch = atan2(-acelX, sqrt(acelY * acelY + acelZ * acelZ)) * 180.0 / PI;

  Serial.printf(
    "acel(g) X:%6.2f Y:%6.2f Z:%6.2f | giro(graus/s) X:%7.1f Y:%7.1f Z:%7.1f | roll:%6.1f pitch:%6.1f | temp:%5.1f C\n",
    acelX, acelY, acelZ,
    giroX, giroY, giroZ,
    roll, pitch,
    temperatura
  );
}


// =====================================================
// I2C
// =====================================================

bool existeNoEndereco(uint8_t endereco) {

  Wire.beginTransmission(endereco);

  return Wire.endTransmission() == 0;
}


bool escreverRegistrador(uint8_t registrador, uint8_t valor) {

  Wire.beginTransmission(enderecoMpu);

  Wire.write(registrador);
  Wire.write(valor);

  return Wire.endTransmission() == 0;
}


// Devolve o valor do registrador, ou -1 se falhar
int lerRegistrador(uint8_t registrador) {

  uint8_t valor;

  if (!lerBytes(registrador, &valor, 1)) {

    return -1;
  }

  return valor;
}


bool lerBytes(uint8_t registrador, uint8_t* destino, size_t quantidade) {

  Wire.beginTransmission(enderecoMpu);

  Wire.write(registrador);

  if (Wire.endTransmission(false) != 0) {

    return false;
  }

  size_t recebidos = Wire.requestFrom((int)enderecoMpu, (int)quantidade, (int)true);

  if (recebidos != quantidade) {

    return false;
  }

  for (size_t i = 0; i < quantidade; i++) {

    destino[i] = Wire.read();
  }

  return true;
}
