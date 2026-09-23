/*
 * config_persistente.h / config_persistente.cpp — Smart Plant Bot
 *
 * Persiste en EEPROM (desde la direccion 0) el perfil activo y la
 * calibracion de dos puntos del sensor de humedad de suelo, para que
 * sobrevivan a un corte de energia sin tener que recalibrar.
 *
 * REGLA CRITICA DE DESGASTE: la EEPROM del ATmega328P soporta del
 * orden de 100.000 ciclos de escritura POR CELDA. Por eso
 * guardarConfig() NUNCA se llama desde loop() en la ruta normal de
 * ejecucion (lectura de sensores, decision de riego, impresion de
 * ESTADO). Solo se escribe en eventos puntuales y poco frecuentes,
 * disparados por el usuario a traves del protocolo Serial: SELECT
 * (cambio de perfil activo) y CAL;SECO / CAL;AGUA / CAL;RESET
 * (calibracion). Ver protocolo_serial.ino.
 */

#ifndef CONFIG_PERSISTENTE_H
#define CONFIG_PERSISTENTE_H

#include <stdint.h>

#define CONFIG_MAGIC 0x50B7
#define CONFIG_VERSION 1
#define DIRECCION_CONFIG 0

// Struct guardado tal cual (byte a byte) en EEPROM desde DIRECCION_CONFIG.
// Orden y tipos de campos FIJOS: guardarConfig()/cargarConfig() asumen
// que sizeof(ConfigPersistente) == 9 en AVR (sin padding, ver .cpp).
struct ConfigPersistente {
  uint16_t magic;          // 0x50B7 fijo: distingue EEPROM configurada de virgen
  uint8_t version;         // empieza en 1, para migraciones futuras
  uint8_t perfilActivo;    // indice en PERFILES[]
  uint16_t valorSeco;      // lectura cruda de ADC, sensor al aire
  uint16_t valorSaturado;  // lectura cruda de ADC, sensor sumergido en agua
  uint8_t checksum;        // XOR de los 8 bytes anteriores (ver calcularChecksum)
};

// Calcula el XOR de todos los campos salvo checksum, campo por campo
// (no lee memoria cruda del struct): asi el resultado no depende de
// como el compilador haya alineado u ordenado los bytes en memoria.
uint8_t calcularChecksum(const ConfigPersistente &c);

// Lee la EEPROM desde DIRECCION_CONFIG y llena c. Si magic no coincide
// con CONFIG_MAGIC o el checksum no valida (EEPROM virgen o corrupta),
// carga los defaults de fabrica en c y devuelve false SIN escribir
// nada a EEPROM todavia: la escritura queda para cuando el usuario
// confirme una calibracion o seleccione un perfil (ver arriba).
bool cargarConfig(ConfigPersistente &c);

// Recalcula el checksum de c y escribe el resultado en EEPROM byte a
// byte con EEPROM.update() — nunca con EEPROM.write() — para no
// gastar ciclos de borrado/escritura en celdas cuyo valor no cambio.
void guardarConfig(const ConfigPersistente &c);

// Defaults de fabrica: perfilActivo=0, valorSeco=620, valorSaturado=310.
// Son valores de referencia tipicos de un sensor capacitivo generico,
// NO una calibracion real de este sensor en particular. Sirven solo
// para que el sistema arranque de forma segura antes de la primera
// calibracion (ver README: procedimiento de dos puntos, CAL;SECO/AGUA).
void configDeFabrica(ConfigPersistente &c);

// Devuelve false si c todavia tiene los valores de fabrica, o si el
// rango (valorSeco - valorSaturado) es menor a 100 cuentas de ADC: un
// rango tan angosto no distingue de forma confiable "seco" de
// "saturado" y probablemente sea ruido, no una calibracion real hecha
// por el usuario.
bool configEstaCalibrada(const ConfigPersistente &c);

#endif // CONFIG_PERSISTENTE_H
