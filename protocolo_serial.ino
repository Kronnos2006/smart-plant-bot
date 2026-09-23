/*
 * protocolo_serial.ino — Smart Plant Bot
 * Firmware principal (punto de entrada unico del sketch: define
 * setup()/loop()). Expone por Serial, a 9600 baudios, un protocolo de
 * texto de una linea por comando para seleccionar perfil, calibrar el
 * sensor de humedad, consultar el estado del sistema y disparar riego
 * manual. Tambien ejecuta el riego automatico: debeRegar() (definida
 * en plantas_lib.ino) es la UNICA puerta de entrada al riego, tanto
 * automatico como manual — ninguna de las dos rutas la evita. Ademas
 * de humedad/tiempo, el riego (manual y automatico) tambien exige
 * calibracion valida y nivel de agua suficiente (sensor de flotador
 * AD12343): ver motivoInhibicion().
 *
 * Este sketch necesita, en la misma carpeta:
 *   - plantas.h / plantas.cpp / plantas_lib.ino   (biblioteca de perfiles)
 *   - config_persistente.h / config_persistente.cpp (persistencia EEPROM)
 * y dos bibliotecas de terceros para los sensores I2C/digital:
 *   - "DHT sensor library" de Adafruit (para el DHT22)
 *   - "BH1750" de Christopher Laws (para el luxometro BH1750, I2C)
 *
 * Sin delay() bloqueante en loop(): todo el timing (duracion del
 * riego, espaciado entre riegos) se resuelve con millis().
 */

#include <EEPROM.h>
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>
#include "plantas.h"
#include "config_persistente.h"

// ---------------------------------------------------------------
// Asignacion de pines. Corresponden al cableado real del equipo,
// verificado con el sketch de prueba en protoboard.
// ---------------------------------------------------------------
#define PIN_HUMEDAD_SUELO A0  // AD69070, salida analogica
#define PIN_BOMBA 7           // gate del MOSFET FQP30N06 (con resistencia serie)
#define PIN_NIVEL_AGUA 3      // flotador AD12343, salida digital
#define PIN_DHT22 2           // pin de datos del DHT22

// El flotador se lee con la resistencia de pull-up interna del
// ATmega328P (ver pinMode en setup): en reposo el pin queda en HIGH y
// el flotador lo lleva a LOW al cerrar contra GND. Con ese cableado
// HIGH significa "hay agua suficiente". Si en un armado distinto da al
// reves, cambiar esta constante a LOW.
#define NIVEL_AGUA_OK HIGH

DHT dht(PIN_DHT22, DHT22);
BH1750 luxometro;

// ---------------------------------------------------------------
// Estado en RAM
// ---------------------------------------------------------------

ConfigPersistente config;

// Momento del ultimo riego efectivo. Vive SOLO en RAM (no forma parte
// de ConfigPersistente, no se pidio persistirlo): al reiniciar el
// equipo vuelve a 0. Como millis() arranca tambien en 0, "millis() -
// ultimoRiegoMs" da un numero chico recien reiniciado, asi que
// debeRegar() bloquea el riego hasta que pase esperaHoras completo
// desde el arranque. Es un comportamiento conservador a proposito:
// un reinicio nunca causa riego de mas, en el peor caso demora uno
// que ya correspondia.
unsigned long ultimoRiegoMs = 0;

bool riegoEnCurso = false;
unsigned long riegoInicioMs = 0;
uint16_t riegoDuracionMs = 0;

// ---------------------------------------------------------------
// Buffer de linea por Serial (sin String de Arduino, solo char[])
// ---------------------------------------------------------------

#define TAM_BUFFER_LINEA 48
char bufferLinea[TAM_BUFFER_LINEA];
uint8_t posBuffer = 0;
bool lineaDescartada = false;

// ---------------------------------------------------------------
// Utilitarios de texto
// ---------------------------------------------------------------

const char *nombreCategoriaStr(uint8_t categoria) {
  switch (categoria) {
    case SUCULENTA:    return "SUCULENTA";
    case MEDITERRANEA: return "MEDITERRANEA";
    case TROPICAL:     return "TROPICAL";
    case HORTALIZA:    return "HORTALIZA";
    default:           return "DESCONOCIDA";
  }
}

const char *nombreEstadoStr(EstadoValidacion e) {
  switch (e) {
    case VALIDO:               return "VALIDO";
    case VALIDO_SIN_AMBIENTAL: return "VALIDO_SIN_AMBIENTAL";
    default:                   return "INVALIDO";
  }
}

// true si el flotador AD12343 indica agua suficiente en el deposito.
bool hayAguaSuficiente() {
  return digitalRead(PIN_NIVEL_AGUA) == NIVEL_AGUA_OK;
}

// ---------------------------------------------------------------
// Riego — debeRegar() sigue siendo la unica puerta de entrada al
// riego (manual y automatico), pero no es la unica condicion: falta
// calibrar o falta agua tambien lo bloquean, y eso debeRegar() no lo
// sabe (no tiene ni deberia tener acceso a hardware). motivoInhibicion()
// combina las cuatro condiciones (calibracion, agua, espera, humedad)
// en un unico lugar: ESTADO, RIEGO;MANUAL e intentarRiegoAutomatico()
// la consultan a ella en vez de reimplementar cada uno su propia
// version de estos chequeos, para que no puedan divergir entre si.
// ---------------------------------------------------------------

void iniciarRiego(uint16_t dosisMs) {
  digitalWrite(PIN_BOMBA, HIGH);
  riegoEnCurso = true;
  riegoInicioMs = millis();
  riegoDuracionMs = dosisMs;
  ultimoRiegoMs = riegoInicioMs;
}

// Se llama en cada vuelta de loop(): apaga la bomba cuando se cumplio
// la dosis. No bloquea, no usa delay().
void actualizarRiego() {
  if (riegoEnCurso && (millis() - riegoInicioMs) >= riegoDuracionMs) {
    digitalWrite(PIN_BOMBA, LOW);
    riegoEnCurso = false;
  }
}

// Cuantas horas faltan (redondeadas hacia arriba) para que se cumpla
// esperaHoras del perfil activo. Devuelve 0 si ya se cumplio.
unsigned long horasRestantesEspera() {
  PerfilPlanta activo;
  if (!perfilPorIndice(config.perfilActivo, activo)) return 0;

  unsigned long transcurridoMs = millis() - ultimoRiegoMs;
  unsigned long esperaMs = (unsigned long)activo.esperaHoras * 3600000UL;
  if (transcurridoMs >= esperaMs) return 0;

  unsigned long faltanMs = esperaMs - transcurridoMs;
  return (faltanMs + 3599999UL) / 3600000UL; // redondeo hacia arriba
}

// Unica fuente de verdad sobre por que el riego automatico NO
// actuaria en este instante (o si actuaria: "NINGUNO"). Llama a
// debeRegar() -- la unica puerta de entrada al riego -- y solo repite
// la comparacion de esperaHoras (una linea) para poder distinguir,
// cuando debeRegar() da false, si el motivo fue el tiempo o el umbral
// de humedad. No reimplementa esa logica, la reutiliza.
const char *motivoInhibicion() {
  if (!configEstaCalibrada(config)) return "SIN_CALIBRAR";
  if (!hayAguaSuficiente()) return "SIN_AGUA";

  PerfilPlanta activo;
  if (!perfilPorIndice(config.perfilActivo, activo)) {
    // Defensivo: perfilActivo guardado en EEPROM quedo fuera de rango
    // (por ejemplo, se regenero la biblioteca con menos perfiles de
    // los que habia cuando se selecciono). No hay un codigo dedicado
    // para esto entre los cinco del protocolo; se trata igual que
    // "sin calibrar" porque en ambos casos no hay con que decidir.
    return "SIN_CALIBRAR";
  }

  int crudo = analogRead(PIN_HUMEDAD_SUELO);
  uint8_t humedadPct = humedadPorcentaje(crudo, config.valorSeco, config.valorSaturado);

  if (debeRegar(activo, humedadPct, ultimoRiegoMs)) return "NINGUNO";

  if (horasRestantesEspera() > 0) return "ESPERA_ACTIVA";
  return "SUELO_HUMEDO";
}

// Riego automatico: usa motivoInhibicion() como unico criterio (que a
// su vez llama a debeRegar()), asi nunca puede quedar en desacuerdo
// con lo que informa ESTADO o con lo que decide RIEGO;MANUAL.
void intentarRiegoAutomatico() {
  if (riegoEnCurso) return;
  if (strcmp(motivoInhibicion(), "NINGUNO") != 0) return;

  PerfilPlanta activo;
  if (!perfilPorIndice(config.perfilActivo, activo)) return; // defensivo, ver motivoInhibicion()

  iniciarRiego(activo.dosisMs);
}

// ---------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------

void cmdListar() {
  for (uint8_t i = 0; i < contarPerfiles(); i++) {
    PerfilPlanta p;
    if (!perfilPorIndice(i, p)) continue;
    EstadoValidacion estado = validarPerfil(p);

    Serial.print(F("PERFIL;"));
    Serial.print(i);
    Serial.print(';');
    Serial.print(p.nombreComun);
    Serial.print(';');
    Serial.print(p.nombreCientifico);
    Serial.print(';');
    Serial.print(nombreCategoriaStr(p.categoria));
    Serial.print(';');
    Serial.print(p.umbralRiego);
    Serial.print(';');
    Serial.print(nombreEstadoStr(estado));
    Serial.print(';');
    Serial.print(p.fuenteVerificada ? 1 : 0);
    Serial.print(';');
    // Datos ambientales al final: luxMax == 0 sigue siendo el
    // centinela de "sin dato ambiental todavia" (ver validarPerfil()
    // en plantas_lib.ino) — plantabot.html se apoya en el mismo
    // centinela para decidir si puede confiar en luxMin.
    Serial.print(p.luxMin);
    Serial.print(';');
    Serial.print(p.luxMax);
    Serial.print(';');
    Serial.print(p.tempMinC);
    Serial.print(';');
    Serial.println(p.tempMaxC);
  }
  // Linea final (registro OK): indica fin de listado y cuenta total,
  // asi un lector automatico (puente_serial.py, plantabot.html) sabe
  // cuando dejar de esperar mas lineas PERFIL;...
  Serial.print(F("OK;LISTAR;"));
  Serial.println(contarPerfiles());
}

void cmdSelect(const char *indiceTxt) {
  if (indiceTxt == NULL || *indiceTxt == '\0') {
    Serial.println(F("ERR;INDICE_INVALIDO"));
    return;
  }
  long indice = atol(indiceTxt);
  if (indice < 0 || indice >= (long)contarPerfiles()) {
    Serial.println(F("ERR;INDICE_INVALIDO"));
    return;
  }
  config.perfilActivo = (uint8_t)indice;
  guardarConfig(config);  // escritura puntual disparada por el usuario (SELECT)
  Serial.print(F("OK;SELECT;"));
  Serial.println(indice);
}

void cmdCalSeco() {
  int crudo = analogRead(PIN_HUMEDAD_SUELO);
  config.valorSeco = (uint16_t)crudo;
  guardarConfig(config);  // escritura puntual disparada por el usuario (CAL;SECO)
  Serial.print(F("OK;CAL;SECO;"));
  Serial.println(crudo);
}

void cmdCalAgua() {
  int crudo = analogRead(PIN_HUMEDAD_SUELO);
  config.valorSaturado = (uint16_t)crudo;
  guardarConfig(config);  // escritura puntual disparada por el usuario (CAL;AGUA)
  Serial.print(F("OK;CAL;AGUA;"));
  Serial.println(crudo);
}

void cmdCalReset() {
  configDeFabrica(config);
  guardarConfig(config);  // escritura puntual disparada por el usuario (CAL;RESET)
  Serial.print(F("OK;CAL;RESET;"));
  Serial.print(config.valorSeco);
  Serial.print(';');
  Serial.println(config.valorSaturado);
}

void cmdEstado() {
  int crudo = analogRead(PIN_HUMEDAD_SUELO);
  uint8_t humedadPct = humedadPorcentaje(crudo, config.valorSeco, config.valorSaturado);

  float tempC = dht.readTemperature();
  float humAmbiente = dht.readHumidity();
  float lux = luxometro.readLightLevel();

  unsigned long transcurridoMs = millis() - ultimoRiegoMs;
  unsigned long horasTranscurridas = transcurridoMs / 3600000UL;

  Serial.print(F("ESTADO;"));
  Serial.print(config.perfilActivo);
  Serial.print(';');
  Serial.print(humedadPct);
  Serial.print(';');
  Serial.print(crudo);
  Serial.print(';');
  if (isnan(tempC)) Serial.print(F("NAN")); else Serial.print(tempC, 1);
  Serial.print(';');
  if (isnan(humAmbiente)) Serial.print(F("NAN")); else Serial.print(humAmbiente, 1);
  Serial.print(';');
  if (isnan(lux) || lux < 0) Serial.print(F("NAN")); else Serial.print(lux, 0);
  Serial.print(';');
  Serial.print(hayAguaSuficiente() ? 1 : 0);
  Serial.print(';');
  Serial.print(configEstaCalibrada(config) ? 1 : 0);
  Serial.print(';');
  Serial.print(horasTranscurridas);
  Serial.print(';');
  Serial.println(motivoInhibicion());
}

void cmdRiegoManual() {
  if (riegoEnCurso) {
    Serial.println(F("ERR;RIEGO_EN_CURSO"));
    return;
  }

  const char *motivo = motivoInhibicion();

  if (strcmp(motivo, "NINGUNO") == 0) {
    PerfilPlanta activo;
    if (!perfilPorIndice(config.perfilActivo, activo)) {
      Serial.println(F("ERR;PERFIL_INVALIDO")); // no deberia pasar, ver motivoInhibicion()
      return;
    }
    iniciarRiego(activo.dosisMs);
    Serial.print(F("OK;RIEGO;MANUAL;"));
    Serial.println(activo.dosisMs);
    return;
  }

  if (strcmp(motivo, "SIN_CALIBRAR") == 0) {
    Serial.println(F("ERR;SIN_CALIBRAR"));
  } else if (strcmp(motivo, "SIN_AGUA") == 0) {
    Serial.println(F("ERR;SIN_AGUA"));
  } else if (strcmp(motivo, "ESPERA_ACTIVA") == 0) {
    Serial.print(F("ERR;ESPERA_ACTIVA;"));
    Serial.println(horasRestantesEspera());
  } else { // "SUELO_HUMEDO"
    int crudo = analogRead(PIN_HUMEDAD_SUELO);
    uint8_t humedadPct = humedadPorcentaje(crudo, config.valorSeco, config.valorSaturado);
    Serial.print(F("ERR;SUELO_HUMEDO;"));
    Serial.println(humedadPct);
  }
}

void cmdAyuda() {
  Serial.println(F("AYUDA;LISTAR"));
  Serial.println(F("AYUDA;SELECT;<indice>"));
  Serial.println(F("AYUDA;CAL;SECO"));
  Serial.println(F("AYUDA;CAL;AGUA"));
  Serial.println(F("AYUDA;CAL;RESET"));
  Serial.println(F("AYUDA;ESTADO"));
  Serial.println(F("AYUDA;RIEGO;MANUAL"));
  Serial.println(F("AYUDA;AYUDA"));
  Serial.println(F("OK;AYUDA"));
}

// ---------------------------------------------------------------
// Parseo de linea (strtok sobre char[], sin String de Arduino)
// ---------------------------------------------------------------

void procesarLinea(char *linea) {
  char *comando = strtok(linea, ";");
  if (comando == NULL) {
    Serial.println(F("ERR;COMANDO_DESCONOCIDO"));
    return;
  }

  if (strcmp(comando, "LISTAR") == 0) {
    cmdListar();
  } else if (strcmp(comando, "SELECT") == 0) {
    cmdSelect(strtok(NULL, ";"));
  } else if (strcmp(comando, "CAL") == 0) {
    char *sub = strtok(NULL, ";");
    if (sub == NULL) {
      Serial.println(F("ERR;COMANDO_DESCONOCIDO"));
    } else if (strcmp(sub, "SECO") == 0) {
      cmdCalSeco();
    } else if (strcmp(sub, "AGUA") == 0) {
      cmdCalAgua();
    } else if (strcmp(sub, "RESET") == 0) {
      cmdCalReset();
    } else {
      Serial.println(F("ERR;COMANDO_DESCONOCIDO"));
    }
  } else if (strcmp(comando, "ESTADO") == 0) {
    cmdEstado();
  } else if (strcmp(comando, "RIEGO") == 0) {
    char *sub = strtok(NULL, ";");
    if (sub != NULL && strcmp(sub, "MANUAL") == 0) {
      cmdRiegoManual();
    } else {
      Serial.println(F("ERR;COMANDO_DESCONOCIDO"));
    }
  } else if (strcmp(comando, "AYUDA") == 0) {
    cmdAyuda();
  } else {
    Serial.println(F("ERR;COMANDO_DESCONOCIDO"));
  }
}

// Lee los bytes disponibles del Serial sin bloquear (nunca espera: si
// no hay datos, sale enseguida) y arma lineas terminadas en '\n'.
// Buffer fijo de TAM_BUFFER_LINEA bytes: si una linea lo excede, se
// descarta completa y se responde ERR;LINEA_LARGA una sola vez, al
// llegar el '\n' que la cierra.
void atenderSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') continue; // tolera CRLF ademas de LF

    if (c == '\n') {
      if (lineaDescartada) {
        Serial.println(F("ERR;LINEA_LARGA"));
      } else if (posBuffer > 0) {
        bufferLinea[posBuffer] = '\0';
        procesarLinea(bufferLinea);
      }
      posBuffer = 0;
      lineaDescartada = false;
      continue;
    }

    if (lineaDescartada) continue; // ya se supero el buffer, se descarta hasta el '\n'

    if (posBuffer >= TAM_BUFFER_LINEA - 1) {
      // No entra ni el '\0' final: se marca la linea para descartar.
      lineaDescartada = true;
      posBuffer = 0;
      continue;
    }

    bufferLinea[posBuffer++] = c;
  }
}

// ---------------------------------------------------------------
// setup() / loop()
// ---------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BOMBA, OUTPUT);
  digitalWrite(PIN_BOMBA, LOW);
  pinMode(PIN_NIVEL_AGUA, INPUT_PULLUP);

  dht.begin();
  Wire.begin();
  luxometro.begin();

  // cargarConfig() ya aplica configDeFabrica() internamente si la
  // EEPROM esta virgen o corrupta; en ese caso NO escribe nada todavia
  // (ver config_persistente.cpp) — se espera a que el usuario calibre
  // o seleccione perfil de forma explicita. Son dos avisos distintos
  // porque son dos causas distintas: EEPROM_VIRGEN es que no habia
  // nada guardado (o estaba corrupto); SIN_CALIBRAR es que, haya o no
  // configuracion guardada, los valores de humedad siguen siendo los
  // de fabrica (o un rango invalido) y el riego automatico no va a
  // actuar hasta que se calibre.
  if (!cargarConfig(config)) {
    Serial.println(F("AVISO;EEPROM_VIRGEN"));
  }
  if (!configEstaCalibrada(config)) {
    Serial.println(F("AVISO;SIN_CALIBRAR"));
  }

  Serial.println(F("OK;INICIO"));
}

void loop() {
  atenderSerial();       // nunca bloquea: procesa solo lo que ya llego
  actualizarRiego();     // apaga la bomba si se cumplio la dosis (sin delay)
  intentarRiegoAutomatico(); // misma puerta de entrada que RIEGO;MANUAL: debeRegar()
}
