/*
 * modo_manual.ino - Smart Plant Bot
 *
 * Sketch de prueba de hardware: NO riega solo. La bomba se enciende y
 * se apaga a mano desde el Monitor Serial (1 = encender, 0 = apagar),
 * mientras el sketch imprime todos los sensores cada 2 segundos.
 *
 * Sirve para verificar el armado antes de cargar el firmware
 * automatico: comprobar que el MOSFET conmuta, que el flotador cambia
 * de estado, que el DHT22 responde y que el BH1750 entrega lux.
 *
 * El BH1750 se lee por I2C directo con la libreria Wire, sin la
 * libreria BH1750: solo hace falta encender el sensor (0x01), ponerlo
 * en medicion continua de alta resolucion (0x10) y leer dos bytes.
 * Asi el sketch de diagnostico no depende de que esa libreria este
 * instalada.
 *
 * Pines: los mismos del firmware (ver README).
 */

#include <Wire.h>
#include "DHT.h"

// =====================================
// PINES
// =====================================

#define DHTPIN 2
#define DHTTYPE DHT22

const int SENSOR_TIERRA = A0;
const int NIVEL_AGUA = 3;
const int BOMBA = 7;

// BH1750
#define BH1750_ADDR 0x23

DHT dht(DHTPIN, DHTTYPE);

// Estado manual de la bomba
bool bombaEncendida = false;


// =====================================
// INICIAR BH1750
// =====================================

void iniciarBH1750() {

  // Encender sensor
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();

  delay(10);

  // Medicion continua
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();

  delay(200);
}


// =====================================
// LEER BH1750
// =====================================

float leerLuz() {

  Wire.requestFrom(BH1750_ADDR, 2);

  if (Wire.available() == 2) {

    unsigned int valor = Wire.read();

    valor = valor << 8;
    valor |= Wire.read();

    return valor / 1.2;
  }

  return -1;
}


// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(9600);

  pinMode(NIVEL_AGUA, INPUT_PULLUP);
  pinMode(BOMBA, OUTPUT);

  // Siempre inicia apagada
  digitalWrite(BOMBA, LOW);

  dht.begin();

  Wire.begin();

  iniciarBH1750();

  Serial.println("================================");
  Serial.println("SISTEMA DE RIEGO - MODO MANUAL");
  Serial.println("================================");

  Serial.println("1 = ENCENDER BOMBA");
  Serial.println("0 = APAGAR BOMBA");

  delay(1000);
}


// =====================================
// LOOP
// =====================================

void loop() {

  // =====================================
  // CONTROL MANUAL DE LA BOMBA
  // =====================================

  if (Serial.available() > 0) {

    char comando = Serial.read();

    if (comando == '1') {

      bombaEncendida = true;

      digitalWrite(BOMBA, HIGH);

      Serial.println();
      Serial.println(">>> BOMBA ENCENDIDA MANUALMENTE <<<");
    }

    else if (comando == '0') {

      bombaEncendida = false;

      digitalWrite(BOMBA, LOW);

      Serial.println();
      Serial.println(">>> BOMBA APAGADA MANUALMENTE <<<");
    }
  }


  // =====================================
  // LEER SENSORES
  // =====================================

  int tierra = analogRead(SENSOR_TIERRA);

  int nivelAgua = digitalRead(NIVEL_AGUA);

  float temperatura = dht.readTemperature();

  float humedadAmbiente = dht.readHumidity();

  float luz = leerLuz();


  // =====================================
  // MOSTRAR INFORMACION
  // =====================================

  Serial.println("-------------------------");

  // TIERRA
  Serial.print("Humedad tierra: ");
  Serial.println(tierra);


  // NIVEL DE AGUA
  Serial.print("Nivel de agua: ");
  Serial.println(nivelAgua);


  // DHT22
  if (!isnan(temperatura) && !isnan(humedadAmbiente)) {

    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");

    Serial.print("Humedad ambiente: ");
    Serial.print(humedadAmbiente);
    Serial.println(" %");

  } else {

    Serial.println("DHT22: lectura perdida");
  }


  // SENSOR DE LUZ
  if (luz >= 0) {

    Serial.print("Luz: ");
    Serial.print(luz);
    Serial.println(" lux");

  } else {

    Serial.println("BH1750: error de lectura");
  }


  // ESTADO BOMBA
  Serial.print("Bomba: ");

  if (bombaEncendida) {

    Serial.println("ENCENDIDA (MANUAL)");

  } else {

    Serial.println("APAGADA (MANUAL)");
  }


  // INFORMACION DEL NIVEL
  if (nivelAgua == 0) {

    Serial.println("AVISO: Nivel de agua bajo");
  }


  delay(2000);
}
