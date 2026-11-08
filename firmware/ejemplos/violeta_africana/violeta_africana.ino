/*
 * Smart Plant Bot - Perfil: Violeta africana (Saintpaulia ionantha)
 *
 * Basado en el sketch de prueba ya verificado en protoboard.
 * Mismos pines, misma calibracion, misma estructura.
 * Lo unico que cambia: los umbrales salen del perfil de la planta.
 *
 * Categoria hidrica: TROPICAL
 *   umbralRiego  45 %   -> riega cuando la humedad baja de 45 %
 *   dosisMs      5000 ms -> 5 segundos de bomba
 *   esperaHoras  24 h    -> no vuelve a regar antes de 24 h
 *
 * IMPORTANTE para esta especie: el gotero tiene que descargar en el
 * sustrato, NUNCA sobre la roseta. El agua sobre la hoja pilosa mancha
 * y favorece pudricion de corona.
 */

#include <Wire.h>
#include <BH1750.h>
#include "DHT.h"

#define DHTPIN 2
#define DHTTYPE DHT22

const int SENSOR_TIERRA = A0;
const int NIVEL_AGUA = 3;
const int BOMBA = 7;

DHT dht(DHTPIN, DHTTYPE);
BH1750 sensorLuz;

// Calibracion que obtuvimos (lecturas crudas del ADC)
const int TIERRA_SECA = 350;    // 0 % de humedad
const int TIERRA_HUMEDA = 280;  // 100 % de humedad

// ---- Perfil de la Violeta africana ----
const int UMBRAL_RIEGO = 45;                          // %
const unsigned long TIEMPO_RIEGO = 5000;              // 5 s
const unsigned long TIEMPO_ESPERA = 24UL * 60UL * 60UL * 1000UL;  // 24 h

// Tope duro de seguridad: la bomba no puede estar encendida mas que
// esto en un mismo riego, pase lo que pase con el resto de la logica.
const unsigned long DOSIS_MAXIMA = 10000;  // 10 s

unsigned long ultimoRiego = 0;
bool yaRego = false;  // false = todavia no rego nunca, puede regar ya

// Convierte la lectura cruda del ADC a porcentaje 0-100.
// El sensor es capacitivo: cuanto mas humedo, MENOR la lectura.
int humedadPorcentaje(int crudo) {
  long pct = map(crudo, TIERRA_SECA, TIERRA_HUMEDA, 0, 100);
  return constrain(pct, 0, 100);
}

// Espera sin bloquear el puerto serie: sigue imprimiendo lecturas
// mientras pasa el tiempo, en vez de quedarse mudo 24 horas.
bool esperaCumplida() {
  if (!yaRego) return true;
  return (millis() - ultimoRiego) >= TIEMPO_ESPERA;
}

// Riega en pasos cortos revisando el flotador entre uno y otro, y con
// un tope duro de tiempo. Asi la bomba no puede quedarse encendida:
// si el deposito se vacia a mitad del riego, corta al instante en vez
// de seguir girando en seco, que es lo que la quema.
void regar(unsigned long dosisMs) {
  if (dosisMs > DOSIS_MAXIMA) dosisMs = DOSIS_MAXIMA;

  unsigned long inicio = millis();

  digitalWrite(BOMBA, HIGH);

  while (millis() - inicio < dosisMs) {
    if (digitalRead(NIVEL_AGUA) == 0) {
      digitalWrite(BOMBA, LOW);
      Serial.println("CORTE - se acabo el agua durante el riego");
      return;
    }
    delay(50);
  }

  digitalWrite(BOMBA, LOW);
}

void setup() {
  Serial.begin(9600);

  pinMode(NIVEL_AGUA, INPUT_PULLUP);

  // El orden importa: primero se fija el valor bajo y recien despues
  // se declara el pin como salida. Al reves, el pin queda un instante
  // como salida en estado indefinido y la bomba puede dar un tiron en
  // el arranque.
  digitalWrite(BOMBA, LOW);
  pinMode(BOMBA, OUTPUT);
  digitalWrite(BOMBA, LOW);

  dht.begin();
  Wire.begin();
  sensorLuz.begin();

  Serial.println("Sistema iniciado");
  Serial.println("Planta: Violeta africana (Saintpaulia ionantha)");
  Serial.print("Riega bajo ");
  Serial.print(UMBRAL_RIEGO);
  Serial.print(" % | dosis ");
  Serial.print(TIEMPO_RIEGO / 1000);
  Serial.println(" s | espera 24 h");
}

void loop() {
  // Seguro de arranque de ciclo: fuera de la funcion regar() la bomba
  // siempre tiene que estar apagada. Si algo la dejo encendida, aca se
  // corta antes de hacer cualquier otra cosa.
  digitalWrite(BOMBA, LOW);

  int tierra = analogRead(SENSOR_TIERRA);
  int humedadTierra = humedadPorcentaje(tierra);
  int nivelAgua = digitalRead(NIVEL_AGUA);

  float temperatura = dht.readTemperature();
  float humedadAmbiente = dht.readHumidity();
  float luz = sensorLuz.readLightLevel();

  Serial.println("--------------------");

  Serial.print("Tierra: ");
  Serial.print(tierra);
  Serial.print(" (");
  Serial.print(humedadTierra);
  Serial.println(" %)");

  Serial.print("Nivel agua: ");
  Serial.println(nivelAgua);

  Serial.print("Luz: ");
  Serial.print(luz);
  Serial.println(" lux");

  if (!isnan(temperatura) && !isnan(humedadAmbiente)) {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");

    Serial.print("Humedad ambiente: ");
    Serial.print(humedadAmbiente);
    Serial.println(" %");
  }

  // SIN AGUA
  if (nivelAgua == 0) {
    digitalWrite(BOMBA, LOW);
    Serial.println("BOMBA APAGADA - No hay agua");

    delay(2000);
    return;
  }

  // TIERRA SECA
  if (humedadTierra <= UMBRAL_RIEGO) {

    if (!esperaCumplida()) {
      unsigned long restanMin = (TIEMPO_ESPERA - (millis() - ultimoRiego)) / 60000UL;
      Serial.print("Tierra seca pero en espera - faltan ");
      Serial.print(restanMin);
      Serial.println(" min");

      delay(5000);
      return;
    }

    Serial.println("Tierra seca - RIEGO");

    regar(TIEMPO_RIEGO);

    ultimoRiego = millis();
    yaRego = true;

    Serial.println("Bomba apagada");
    Serial.println("Esperando que el agua penetre...");

    delay(5000);
  }

  // TIERRA CON SUFICIENTE HUMEDAD
  else {
    digitalWrite(BOMBA, LOW);

    Serial.println("Humedad suficiente - No necesita agua");

    delay(2000);
  }
}
