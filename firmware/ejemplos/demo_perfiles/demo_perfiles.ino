/*
 * demo_perfiles.ino — Smart Plant Bot (ejemplo)
 * Prueba autonoma de la biblioteca de perfiles: recorre los 8
 * perfiles de PERFILES[] y los imprime por Serial a 9600 baudios,
 * junto con su EstadoValidacion. No usa el protocolo Serial del
 * firmware principal ni la EEPROM — es el mismo demo que tenia
 * plantas_lib.ino antes de que protocolo_serial.ino se convirtiera
 * en el punto de entrada real del proyecto.
 *
 * plantas.h, plantas.cpp y plantas_lib.ino en esta carpeta son una
 * COPIA de los que estan en la raiz del proyecto — un sketch de
 * Arduino solo compila los archivos que estan en su propia carpeta,
 * asi que este ejemplo necesita su propia copia para compilar solo,
 * sin arrastrar protocolo_serial.ino ni config_persistente.*. Si
 * editas la biblioteca en la raiz (por ejemplo regenerando
 * plantas.cpp desde el CSV con csv_a_header.py), acordate de volver
 * a copiar los archivos actualizados tambien aca.
 */

#include "plantas.h"

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // espera a que el puerto serie este listo (placas con USB nativo)
  }

  Serial.println(F("=== Smart Plant Bot: demo de perfiles ==="));
  Serial.print(F("Total de perfiles: "));
  Serial.println(contarPerfiles());
  Serial.println();

  for (uint8_t i = 0; i < contarPerfiles(); i++) {
    PerfilPlanta p;
    if (!perfilPorIndice(i, p)) continue;

    Serial.print(i);
    Serial.print(F(") "));
    Serial.print(p.nombreComun);
    Serial.print(F(" ("));
    Serial.print(p.nombreCientifico);
    Serial.println(F(")"));

    Serial.print(F("   categoria="));
    Serial.print(p.categoria);
    Serial.print(F(" umbralRiego="));
    Serial.print(p.umbralRiego);
    Serial.print(F("% dosisMs="));
    Serial.print(p.dosisMs);
    Serial.print(F(" esperaHoras="));
    Serial.println(p.esperaHoras);

    Serial.print(F("   luxMin="));
    Serial.print(p.luxMin);
    Serial.print(F(" luxMax="));
    Serial.print(p.luxMax);
    Serial.print(F(" tempMinC="));
    Serial.print(p.tempMinC);
    Serial.print(F(" tempMaxC="));
    Serial.println(p.tempMaxC);

    Serial.print(F("   fuenteVerificada="));
    Serial.print(p.fuenteVerificada ? F("SI") : F("NO (pendiente)"));
    Serial.print(F(" validarPerfil()="));
    switch (validarPerfil(p)) {
      case VALIDO:
        Serial.println(F("VALIDO"));
        break;
      case VALIDO_SIN_AMBIENTAL:
        Serial.println(F("VALIDO_SIN_AMBIENTAL (falta lux/temp)"));
        break;
      default:
        Serial.println(F("INVALIDO"));
        break;
    }

    Serial.println();
  }
}

void loop() {
  // Demo estatica: no hay nada que repetir.
}
