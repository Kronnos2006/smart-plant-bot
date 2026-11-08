/*
 * config_persistente.cpp — Smart Plant Bot
 * Ver config_persistente.h para la regla critica de desgaste de EEPROM.
 */

#include <EEPROM.h>
#include "config_persistente.h"

// sizeof(ConfigPersistente) debe ser 9 en AVR: 2+1+1+2+2+1, sin padding
// (AVR-GCC usa alineacion de 1 byte para estos tipos). Se verifica en
// tiempo de compilacion porque guardarConfig()/cargarConfig() recorren
// exactamente sizeof(ConfigPersistente) bytes de EEPROM.
static_assert(sizeof(ConfigPersistente) == 9,
              "ConfigPersistente debe pesar 9 bytes en AVR (sin padding)");

uint8_t calcularChecksum(const ConfigPersistente &c) {
  uint8_t chk = 0;
  chk ^= (uint8_t)(c.magic & 0xFF);
  chk ^= (uint8_t)(c.magic >> 8);
  chk ^= c.version;
  chk ^= c.perfilActivo;
  chk ^= (uint8_t)(c.valorSeco & 0xFF);
  chk ^= (uint8_t)(c.valorSeco >> 8);
  chk ^= (uint8_t)(c.valorSaturado & 0xFF);
  chk ^= (uint8_t)(c.valorSaturado >> 8);
  return chk;
}

void configDeFabrica(ConfigPersistente &c) {
  c.magic = CONFIG_MAGIC;
  c.version = CONFIG_VERSION;
  c.perfilActivo = 0;
  c.valorSeco = 620;      // PLACEHOLDER: no es una calibracion real, ver CAL;SECO
  c.valorSaturado = 310;  // PLACEHOLDER: no es una calibracion real, ver CAL;AGUA
  c.checksum = calcularChecksum(c);
}

bool cargarConfig(ConfigPersistente &c) {
  // Lectura byte a byte: EEPROM.read() no gasta ciclos de escritura,
  // asi que no hay problema en usarla en cada arranque.
  uint8_t *bytes = (uint8_t *)&c;
  for (uint16_t i = 0; i < sizeof(ConfigPersistente); i++) {
    bytes[i] = EEPROM.read(DIRECCION_CONFIG + i);
  }

  if (c.magic != CONFIG_MAGIC) {
    configDeFabrica(c);
    return false;
  }
  if (c.checksum != calcularChecksum(c)) {
    configDeFabrica(c);
    return false;
  }
  return true;
}

void guardarConfig(const ConfigPersistente &c) {
  ConfigPersistente copia = c;
  copia.checksum = calcularChecksum(copia);

  const uint8_t *bytes = (const uint8_t *)&copia;
  for (uint16_t i = 0; i < sizeof(ConfigPersistente); i++) {
    // EEPROM.update(): escribe solo si el byte cambio. Nunca EEPROM.write().
    EEPROM.update(DIRECCION_CONFIG + i, bytes[i]);
  }
}

bool configEstaCalibrada(const ConfigPersistente &c) {
  ConfigPersistente fabrica;
  configDeFabrica(fabrica);

  if (c.valorSeco == fabrica.valorSeco && c.valorSaturado == fabrica.valorSaturado) {
    return false;
  }
  if ((int16_t)c.valorSeco - (int16_t)c.valorSaturado < 100) {
    return false;
  }
  return true;
}
