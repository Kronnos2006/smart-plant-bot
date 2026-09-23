/*
 * plantas.h — Smart Plant Bot
 * Biblioteca de perfiles de plantas para riego automatizado.
 *
 * MECANISMO ANTIPUDRICION (esperaHoras):
 * -----------------------------------------------------------------
 * El campo `esperaHoras` es la protección principal contra el riego
 * excesivo, en particular para cactaceas y suculentas. Aunque el
 * sensor capacitivo reporte suelo por debajo del umbral de humedad
 * (`umbralRiego`), el riego NO se ejecuta si no transcurrio esa
 * cantidad minima de horas desde el ultimo riego efectivo. Esto
 * evita que ruido de lectura, sombra momentanea u otra causa
 * dispare ciclos de bomba demasiado seguidos y pudra la raiz.
 * La funcion `debeRegar()` es quien hace cumplir ambas condiciones
 * (humedad Y tiempo) de forma simultanea.
 *
 * `esperaHoras` es una decision de diseno propia (histeresis temporal
 * para evitar que el control oscile), NO un dato bibliografico:
 * ninguna de las fuentes cargadas en datos/plantas.csv estudia
 * frecuencia minima de riego en maceta.
 * -----------------------------------------------------------------
 *
 * IMPORTANTE: ningun valor numerico de este archivo esta basado en
 * datos de especies individuales "a ojo". Todos derivan del default
 * de la categoria hidrica correspondiente (ver DEFAULTS_CATEGORIA).
 * El campo `fuenteVerificada` indica si esa fila ya fue respaldada
 * con una fuente bibliografica concreta (ver datos/plantas.csv). Esa
 * fuente puede ser puramente hidrica: no implica que tambien haya
 * datos ambientales cargados (luxMin/luxMax/tempMinC/tempMaxC son una
 * verificacion independiente, ver EstadoValidacion mas abajo).
 */

#ifndef PLANTAS_H
#define PLANTAS_H

#include <stdint.h>
#include <avr/pgmspace.h>

// Categorias hidricas: agrupan plantas por requerimiento de agua,
// no por especie. Los valores de riego se heredan de la categoria.
enum CategoriaHidrica {
  SUCULENTA    = 0,
  MEDITERRANEA = 1,
  TROPICAL     = 2,
  HORTALIZA    = 3
};

// Perfil de una planta individual.
// Orden y tipos de campos FIJOS: csv_a_header.py depende de este
// layout exacto para regenerar plantas.cpp desde el CSV.
struct PerfilPlanta {
  char nombreComun[24];
  char nombreCientifico[32];
  uint8_t categoria;        // ver enum CategoriaHidrica
  uint8_t umbralRiego;      // % de humedad de suelo bajo el cual se riega
  uint16_t dosisMs;         // ms de bomba encendida por ciclo
  uint16_t esperaHoras;     // horas minimas obligatorias entre riegos (antipudricion)
  uint16_t luxMin;          // lux minimo aceptable
  uint16_t luxMax;          // lux maximo aceptable
  int8_t tempMinC;          // temperatura minima aceptable, en C
  int8_t tempMaxC;          // temperatura maxima aceptable, en C
  bool fuenteVerificada;    // true solo si hay fuente_url + fuente_cita_apa cargadas
};

// Resultado de validarPerfil(). Distingue tres casos, no dos:
// un perfil puede ser invalido, o valido pero todavia sin datos
// ambientales (lux/temp), o valido y completo. Un perfil
// VALIDO_SIN_AMBIENTAL puede usarse para riego (humedad + tiempo),
// pero el firmware NO debe usarlo para decidir nada en base al
// BH1750 o al DHT22 hasta que pase a VALIDO.
enum EstadoValidacion {
  INVALIDO = 0,
  VALIDO_SIN_AMBIENTAL = 1,
  VALIDO = 2
};

// Valores base por categoria hidrica. Toda planta sin fuente
// verificada usa estos valores como default de su categoria.
struct CategoriaDefault {
  uint8_t umbralRiego;
  uint16_t dosisMs;
  uint16_t esperaHoras;
};

// Defaults por categoria (indexados por CategoriaHidrica):
//   SUCULENTA:    umbral 15%, dosis 2000 ms, espera 168 h (7 dias)
//   MEDITERRANEA: umbral 28%, dosis 4000 ms, espera 48 h
//   TROPICAL:     umbral 45%, dosis 5000 ms, espera 24 h
//   HORTALIZA:    umbral 55%, dosis 6000 ms, espera 12 h
extern const CategoriaDefault DEFAULTS_CATEGORIA[4];

// Cantidad fija de perfiles precargados en PERFILES (ver plantas.cpp).
#define NUM_PERFILES 14

// Tabla de perfiles en PROGMEM (definida en plantas.cpp).
extern const PerfilPlanta PERFILES[NUM_PERFILES] PROGMEM;

// ---------------------------------------------------------------
// Prototipos (implementados en plantas_lib.ino)
// ---------------------------------------------------------------

// Devuelve la cantidad de perfiles disponibles en PERFILES.
uint8_t contarPerfiles();

// Copia el perfil en la posicion i (leyendo desde PROGMEM) a destino.
// Devuelve false si el indice es invalido.
bool perfilPorIndice(uint8_t i, PerfilPlanta &destino);

// Busca un perfil por nombre comun, insensible a mayusculas/minusculas.
// Devuelve el indice encontrado o -1 si no existe.
int8_t buscarPorNombre(const char *nombre);

// Valida que los rangos de un perfil sean fisicamente / operacionalmente
// coherentes (umbral 5-80%, dosis 500-15000 ms, espera 1-336 h). Si
// esos tres pasan pero no hay datos ambientales cargados todavia
// (luxMax == 0, ver csv_a_header.py), devuelve VALIDO_SIN_AMBIENTAL en
// lugar de VALIDO: el riego por humedad ya es confiable, pero lux y
// temperatura son datos pendientes, no un rango real de 0 a 0.
// Si hay datos ambientales, tambien exige luxMin < luxMax y
// tempMinC < tempMaxC. Devuelve INVALIDO si cualquiera de los rangos
// obligatorios (umbral/dosis/espera) esta fuera de rango.
EstadoValidacion validarPerfil(const PerfilPlanta &p);

// Normaliza una lectura cruda de ADC a porcentaje de humedad (0-100).
// En el sensor capacitivo AD69070 la lectura BAJA cuando la humedad SUBE,
// por eso valorSeco > valorSaturado tipicamente.
uint8_t humedadPorcentaje(int crudo, int valorSeco, int valorSaturado);

// Determina si corresponde regar: exige humedad por debajo del umbral
// Y que haya transcurrido al menos esperaHoras desde el ultimo riego.
// Usa resta sin signo sobre millis() para tolerar el desborde (~49 dias).
bool debeRegar(const PerfilPlanta &p, uint8_t humedadPct, unsigned long ultimoRiegoMs);

#endif // PLANTAS_H
