/*
 * plantas.cpp - Smart Plant Bot
 *
 * ATENCION: ARCHIVO GENERADO. VALORES DE CONFIGURACION INICIAL
 * SUJETOS A CALIBRACION EXPERIMENTAL, SIN RESPALDO BIBLIOGRAFICO
 * VERIFICADO SALVO QUE fuenteVerificada = true. NO EDITAR ESTE
 * ARCHIVO A MANO: SE REGENERA A PARTIR DE datos/plantas.csv CON
 * csv_a_header.py. CUALQUIER EDICION MANUAL SE PIERDE EN LA
 * PROXIMA REGENERACION.
 */

#include "plantas.h"

const CategoriaDefault DEFAULTS_CATEGORIA[4] = {
  { 15, 2000, 168 },  // SUCULENTA
  { 28, 4000,  48 },  // MEDITERRANEA
  { 45, 5000,  24 },  // TROPICAL
  { 55, 6000,  12 },  // HORTALIZA
};

const PerfilPlanta PERFILES[NUM_PERFILES] PROGMEM = {
  { "Aloe vera", "Aloe vera", SUCULENTA, 15, 2000, 168, 0, 0, 0, 0, true },
  { "Nopal", "Opuntia ficus-indica", SUCULENTA, 15, 2000, 168, 0, 0, 0, 0, true },
  { "Romero", "Rosmarinus officinalis", MEDITERRANEA, 28, 4000, 48, 0, 0, 0, 0, true },
  { "Lavanda", "Lavandula angustifolia", MEDITERRANEA, 28, 4000, 48, 0, 0, 0, 0, true },
  { "Potus", "Epipremnum aureum", TROPICAL, 45, 5000, 24, 0, 0, 0, 0, false },
  { "Lirio de la paz", "Spathiphyllum wallisii", TROPICAL, 45, 5000, 24, 0, 0, 0, 0, false },
  { "Tomate", "Solanum lycopersicum", HORTALIZA, 55, 6000, 12, 0, 0, 0, 0, true },
  { "Albahaca", "Ocimum basilicum", HORTALIZA, 55, 6000, 12, 0, 0, 0, 0, true },
  { "Violeta africana", "Saintpaulia ionantha", TROPICAL, 45, 5000, 24, 0, 0, 0, 0, false },
  { "Lengua de suegra", "Dracaena trifasciata", SUCULENTA, 15, 2000, 168, 0, 0, 0, 0, false },
  { "Helecho de Boston", "Nephrolepis exaltata", TROPICAL, 45, 5000, 24, 0, 0, 0, 0, false },
  { "Orquidea mariposa", "Phalaenopsis amabilis", TROPICAL, 45, 5000, 24, 0, 0, 0, 0, false },
  { "Culantro coyote", "Eryngium foetidum", HORTALIZA, 55, 6000, 12, 0, 0, 0, 0, true },
  { "Chile dulce", "Capsicum annuum", HORTALIZA, 55, 6000, 12, 0, 0, 0, 0, true },
};

