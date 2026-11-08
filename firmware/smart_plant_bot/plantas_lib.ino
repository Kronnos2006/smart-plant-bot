/*
 * plantas_lib.ino — Smart Plant Bot
 * Funciones de acceso a la biblioteca de perfiles (PERFILES, en PROGMEM).
 *
 * Todas las lecturas de datos constantes se hacen con memcpy_P /
 * strcpy_P porque PERFILES vive en flash (PROGMEM), no en SRAM.
 * El ATmega328P solo tiene 2 KB de SRAM: no conviene copiar toda
 * la tabla, solo el registro puntual que se necesita en cada momento.
 *
 * NOTA: este archivo NO define setup()/loop(). Antes tenia un demo
 * propio que imprimia los 8 perfiles al arrancar; se quito porque un
 * mismo sketch de Arduino no admite dos definiciones de setup()/loop(),
 * y protocolo_serial.ino ya es el punto de entrada real del firmware
 * (su comando LISTAR cubre exactamente lo que hacia ese demo, y ESTADO
 * lo que hacia falta ademas). Para compilar, este archivo tiene que
 * estar en el mismo sketch que protocolo_serial.ino, plantas.h/.cpp y
 * config_persistente.h/.cpp.
 */

#include "plantas.h"

// Compara dos strings ignorando mayusculas/minusculas (ambas en RAM).
static bool igualIgnorandoMayus(const char *a, const char *b) {
  while (*a && *b) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += ('a' - 'A');
    if (cb >= 'A' && cb <= 'Z') cb += ('a' - 'A');
    if (ca != cb) return false;
    a++;
    b++;
  }
  return *a == *b; // ambos deben terminar en '\0' a la vez
}

uint8_t contarPerfiles() {
  return NUM_PERFILES;
}

bool perfilPorIndice(uint8_t i, PerfilPlanta &destino) {
  if (i >= NUM_PERFILES) return false;
  memcpy_P(&destino, &PERFILES[i], sizeof(PerfilPlanta));
  return true;
}

int8_t buscarPorNombre(const char *nombre) {
  char buffer[24]; // tamano de nombreComun
  for (uint8_t i = 0; i < NUM_PERFILES; i++) {
    strcpy_P(buffer, PERFILES[i].nombreComun);
    if (igualIgnorandoMayus(buffer, nombre)) {
      return (int8_t)i;
    }
  }
  return -1;
}

EstadoValidacion validarPerfil(const PerfilPlanta &p) {
  if (p.umbralRiego < 5 || p.umbralRiego > 80) return INVALIDO;
  if (p.dosisMs < 500 || p.dosisMs > 15000) return INVALIDO;
  if (p.esperaHoras < 1 || p.esperaHoras > 336) return INVALIDO;

  // luxMax == 0 es el centinela de "sin dato ambiental todavia": ningun
  // perfil real tiene como maximo de luz aceptable 0 lux. csv_a_header.py
  // garantiza que luxMin/luxMax/tempMinC/tempMaxC se cargan siempre los
  // cuatro juntos o ninguno, asi que revisar solo luxMax alcanza para
  // saber si tambien hay temperatura cargada.
  if (p.luxMax == 0) return VALIDO_SIN_AMBIENTAL;

  if (p.luxMin >= p.luxMax) return INVALIDO;
  if (p.tempMinC >= p.tempMaxC) return INVALIDO;
  return VALIDO;
}

uint8_t humedadPorcentaje(int crudo, int valorSeco, int valorSaturado) {
  // En el sensor capacitivo AD69070 la lectura BAJA cuando el suelo
  // esta mas humedo, por eso se mapea valorSeco -> 0% y valorSaturado -> 100%.
  long pct = map((long)crudo, (long)valorSeco, (long)valorSaturado, 0L, 100L);
  pct = constrain(pct, 0L, 100L);
  return (uint8_t)pct;
}

bool debeRegar(const PerfilPlanta &p, uint8_t humedadPct, unsigned long ultimoRiegoMs) {
  bool sueloSeco = humedadPct < p.umbralRiego;

  // Resta sin signo: tolera el desborde de millis() (~cada 49 dias)
  // porque la aritmetica modular de unsigned long da el delta correcto
  // aun cuando millis() actual < ultimoRiegoMs.
  unsigned long transcurridoMs = millis() - ultimoRiegoMs;
  unsigned long esperaMs = (unsigned long)p.esperaHoras * 3600000UL;
  bool tiempoCumplido = transcurridoMs >= esperaMs;

  return sueloSeco && tiempoCumplido;
}
