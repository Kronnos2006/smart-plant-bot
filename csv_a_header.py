#!/usr/bin/env python3
"""
csv_a_header.py - Smart Plant Bot

Regenera plantas.cpp a partir de datos/plantas.csv.

Uso:
    python3 csv_a_header.py

No tiene dependencias externas: solo biblioteca estandar de Python 3.

Reglas:
  - fuenteVerificada se marca en true UNICAMENTE cuando fuente_url y
    fuente_cita_apa estan ambas no vacias en esa fila (el valor que
    venga escrito en la columna fuenteVerificada del CSV se ignora,
    siempre se recalcula aqui, para que no pueda haber contradiccion
    entre lo declarado y lo realmente respaldado).
  - Se validan los mismos rangos que validarPerfil() en plantas_lib.ino.
  - luxMin/luxMax/tempMinC/tempMaxC pueden dejarse vacios (todavia sin
    dato bibliografico): en ese caso se escriben como 0 en el struct y
    NO se exige luxMin<luxMax ni tempMinC<tempMaxC para esa fila. Estos
    cuatro campos son todo-o-nada SIEMPRE (no se admite una pareja
    completa y la otra vacia, ni ninguna combinacion parcial): asi
    luxMax == 0 sirve como centinela inequivoco de "sin dato ambiental"
    en validarPerfil() del lado C.
  - fuenteVerificada (hidrica) y datos ambientales (luxMin/luxMax/
    tempMinC/tempMaxC) son DOS verificaciones independientes, no una
    sola: una fuente sobre riego/humedad de sustrato no dice nada
    sobre luz ni temperatura, y exigir ambas cosas juntas forzaria a
    inventar datos ambientales cada vez que se carga una fuente
    puramente hidrica. Por eso fuenteVerificada se marca en true
    con solo tener fuente_url + fuente_cita_apa completos, sin
    importar si luxMin/luxMax/tempMinC/tempMaxC estan cargados o no.
  - La columna "referencia_literatura" es solo documentacion (que
    tratamiento/nivel de la fuente se uso de referencia orientativa):
    no se valida ni se escribe en plantas.cpp, existe para que quede
    trazabilidad humana en el propio CSV.
"""

import csv
import os
import sys

RUTA_CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "datos", "plantas.csv")
RUTA_CPP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plantas.cpp")

TAM_NOMBRE_COMUN = 24       # char nombreComun[24]
TAM_NOMBRE_CIENTIFICO = 32  # char nombreCientifico[32]

CATEGORIAS = {
    "SUCULENTA": 0,
    "MEDITERRANEA": 1,
    "TROPICAL": 2,
    "HORTALIZA": 3,
}

# Rangos de validacion, deben coincidir con validarPerfil() en plantas_lib.ino
UMBRAL_MIN, UMBRAL_MAX = 5, 80
DOSIS_MIN, DOSIS_MAX = 500, 15000
ESPERA_MIN, ESPERA_MAX = 1, 336


class ErrorValidacion(Exception):
    """Error de validacion de una fila del CSV, con numero de fila y campo."""
    def __init__(self, num_fila, campo, mensaje):
        self.num_fila = num_fila
        self.campo = campo
        self.mensaje = mensaje
        super().__init__(f"Fila {num_fila}, campo '{campo}': {mensaje}")


def truncar(valor, tam_buffer, nombre_campo, num_fila, avisos):
    """Trunca `valor` para que entre en un char[tam_buffer] (deja lugar al '\\0')."""
    max_chars = tam_buffer - 1
    if len(valor) > max_chars:
        avisos.append(
            f"Fila {num_fila}: '{nombre_campo}' truncado de {len(valor)} a "
            f"{max_chars} caracteres ('{valor}' -> '{valor[:max_chars]}')"
        )
        return valor[:max_chars]
    return valor


def escapar_c(valor):
    """Escapa comillas dobles y backslashes para insertar en un string literal C."""
    return valor.replace("\\", "\\\\").replace('"', '\\"')


def leer_entero(valor, num_fila, campo):
    valor = (valor or "").strip()
    if valor == "":
        return None
    try:
        return int(valor)
    except ValueError:
        raise ErrorValidacion(num_fila, campo, f"'{valor}' no es un entero valido")


def procesar_filas(filas):
    perfiles = []
    avisos = []

    for idx, fila in enumerate(filas):
        num_fila = idx + 2  # +1 por 0-index, +1 por la fila de encabezado

        nombre_comun = (fila.get("nombreComun") or "").strip()
        nombre_cientifico = (fila.get("nombreCientifico") or "").strip()
        categoria_txt = (fila.get("categoria") or "").strip().upper()

        if not nombre_comun:
            raise ErrorValidacion(num_fila, "nombreComun", "no puede estar vacio")
        if not nombre_cientifico:
            raise ErrorValidacion(num_fila, "nombreCientifico", "no puede estar vacio")
        if categoria_txt not in CATEGORIAS:
            raise ErrorValidacion(
                num_fila, "categoria",
                f"'{categoria_txt}' invalida, debe ser una de {list(CATEGORIAS.keys())}"
            )
        categoria = CATEGORIAS[categoria_txt]

        umbral = leer_entero(fila.get("umbralRiego"), num_fila, "umbralRiego")
        dosis = leer_entero(fila.get("dosisMs"), num_fila, "dosisMs")
        espera = leer_entero(fila.get("esperaHoras"), num_fila, "esperaHoras")

        if umbral is None:
            raise ErrorValidacion(num_fila, "umbralRiego", "no puede estar vacio")
        if dosis is None:
            raise ErrorValidacion(num_fila, "dosisMs", "no puede estar vacio")
        if espera is None:
            raise ErrorValidacion(num_fila, "esperaHoras", "no puede estar vacio")

        if not (UMBRAL_MIN <= umbral <= UMBRAL_MAX):
            raise ErrorValidacion(
                num_fila, "umbralRiego",
                f"{umbral} fuera de rango [{UMBRAL_MIN}, {UMBRAL_MAX}]"
            )
        if not (DOSIS_MIN <= dosis <= DOSIS_MAX):
            raise ErrorValidacion(
                num_fila, "dosisMs",
                f"{dosis} fuera de rango [{DOSIS_MIN}, {DOSIS_MAX}]"
            )
        if not (ESPERA_MIN <= espera <= ESPERA_MAX):
            raise ErrorValidacion(
                num_fila, "esperaHoras",
                f"{espera} fuera de rango [{ESPERA_MIN}, {ESPERA_MAX}]"
            )

        lux_min = leer_entero(fila.get("luxMin"), num_fila, "luxMin")
        lux_max = leer_entero(fila.get("luxMax"), num_fila, "luxMax")
        temp_min = leer_entero(fila.get("tempMinC"), num_fila, "tempMinC")
        temp_max = leer_entero(fila.get("tempMaxC"), num_fila, "tempMaxC")

        lux_llenos = [v is not None for v in (lux_min, lux_max)]
        temp_llenos = [v is not None for v in (temp_min, temp_max)]
        ambiental_lleno = lux_llenos + temp_llenos

        # Los cuatro campos ambientales son todo-o-nada, siempre (no solo
        # cuando hay fuente cargada): validarPerfil() del lado C usa
        # luxMax == 0 como centinela de "sin dato ambiental todavia", y
        # ese centinela solo es valido si nunca queda un dato suelto
        # (por ejemplo luxMin/luxMax cargados pero tempMinC/tempMaxC no).
        if any(ambiental_lleno) and not all(ambiental_lleno):
            raise ErrorValidacion(
                num_fila, "luxMin/luxMax/tempMinC/tempMaxC",
                "deben completarse los cuatro juntos (luxMin, luxMax, "
                "tempMinC, tempMaxC), o dejarse los cuatro vacios"
            )

        if all(ambiental_lleno):
            if not (0 <= lux_min <= 65535 and 0 <= lux_max <= 65535):
                raise ErrorValidacion(
                    num_fila, "luxMin/luxMax",
                    "deben ser enteros sin signo de 16 bits, rango [0, 65535]"
                )
            if lux_min >= lux_max:
                raise ErrorValidacion(
                    num_fila, "luxMin/luxMax",
                    f"luxMin ({lux_min}) debe ser menor que luxMax ({lux_max})"
                )
            if not (-128 <= temp_min <= 127 and -128 <= temp_max <= 127):
                raise ErrorValidacion(
                    num_fila, "tempMinC/tempMaxC",
                    "deben ser enteros con signo de 8 bits, rango [-128, 127]"
                )
            if temp_min >= temp_max:
                raise ErrorValidacion(
                    num_fila, "tempMinC/tempMaxC",
                    f"tempMinC ({temp_min}) debe ser menor que tempMaxC ({temp_max})"
                )

        lux_min = lux_min if lux_min is not None else 0
        lux_max = lux_max if lux_max is not None else 0
        temp_min = temp_min if temp_min is not None else 0
        temp_max = temp_max if temp_max is not None else 0

        fuente_url = (fila.get("fuente_url") or "").strip()
        fuente_cita_apa = (fila.get("fuente_cita_apa") or "").strip()
        fuente_verificada = bool(fuente_url) and bool(fuente_cita_apa)

        nombre_comun = truncar(nombre_comun, TAM_NOMBRE_COMUN, "nombreComun", num_fila, avisos)
        nombre_cientifico = truncar(
            nombre_cientifico, TAM_NOMBRE_CIENTIFICO, "nombreCientifico", num_fila, avisos
        )

        perfiles.append({
            "nombreComun": nombre_comun,
            "nombreCientifico": nombre_cientifico,
            "categoria_nombre": categoria_txt,
            "categoria": categoria,
            "umbralRiego": umbral,
            "dosisMs": dosis,
            "esperaHoras": espera,
            "luxMin": lux_min,
            "luxMax": lux_max,
            "tempMinC": temp_min,
            "tempMaxC": temp_max,
            "fuenteVerificada": fuente_verificada,
        })

    return perfiles, avisos


def generar_cpp(perfiles):
    lineas = []
    lineas.append("/*")
    lineas.append(" * plantas.cpp - Smart Plant Bot")
    lineas.append(" *")
    lineas.append(" * ATENCION: ARCHIVO GENERADO. VALORES DE CONFIGURACION INICIAL")
    lineas.append(" * SUJETOS A CALIBRACION EXPERIMENTAL, SIN RESPALDO BIBLIOGRAFICO")
    lineas.append(" * VERIFICADO SALVO QUE fuenteVerificada = true. NO EDITAR ESTE")
    lineas.append(" * ARCHIVO A MANO: SE REGENERA A PARTIR DE datos/plantas.csv CON")
    lineas.append(" * csv_a_header.py. CUALQUIER EDICION MANUAL SE PIERDE EN LA")
    lineas.append(" * PROXIMA REGENERACION.")
    lineas.append(" */")
    lineas.append("")
    lineas.append('#include "plantas.h"')
    lineas.append("")
    lineas.append("const CategoriaDefault DEFAULTS_CATEGORIA[4] = {")
    lineas.append("  { 15, 2000, 168 },  // SUCULENTA")
    lineas.append("  { 28, 4000,  48 },  // MEDITERRANEA")
    lineas.append("  { 45, 5000,  24 },  // TROPICAL")
    lineas.append("  { 55, 6000,  12 },  // HORTALIZA")
    lineas.append("};")
    lineas.append("")
    lineas.append(f"const PerfilPlanta PERFILES[NUM_PERFILES] PROGMEM = {{")
    for p in perfiles:
        nombre_c = escapar_c(p["nombreComun"])
        cientifico_c = escapar_c(p["nombreCientifico"])
        verificada_c = "true" if p["fuenteVerificada"] else "false"
        lineas.append(
            f'  {{ "{nombre_c}", "{cientifico_c}", {p["categoria_nombre"]}, '
            f'{p["umbralRiego"]}, {p["dosisMs"]}, {p["esperaHoras"]}, '
            f'{p["luxMin"]}, {p["luxMax"]}, {p["tempMinC"]}, {p["tempMaxC"]}, '
            f'{verificada_c} }},'
        )
    lineas.append("};")
    lineas.append("")
    return "\n".join(lineas) + "\n"


def main():
    if not os.path.isfile(RUTA_CSV):
        print(f"ERROR: no se encontro el CSV en {RUTA_CSV}", file=sys.stderr)
        sys.exit(1)

    with open(RUTA_CSV, "r", encoding="utf-8-sig", newline="") as f:
        lector = csv.DictReader(f)
        filas = list(lector)

    if len(filas) == 0:
        print("ERROR: el CSV no tiene filas de datos.", file=sys.stderr)
        sys.exit(1)

    if len(filas) > 255:
        print(
            f"ERROR: el CSV tiene {len(filas)} filas; PerfilPlanta usa indices "
            "uint8_t (maximo 255 perfiles).",
            file=sys.stderr,
        )
        sys.exit(1)

    try:
        perfiles, avisos = procesar_filas(filas)
    except ErrorValidacion as e:
        print(f"ERROR DE VALIDACION: {e}", file=sys.stderr)
        sys.exit(1)

    for aviso in avisos:
        print(f"AVISO: {aviso}")

    contenido_cpp = generar_cpp(perfiles)

    # Asegura que NUM_PERFILES en plantas.h coincida con la cantidad real.
    ruta_h = os.path.join(os.path.dirname(RUTA_CPP), "plantas.h")
    if os.path.isfile(ruta_h):
        with open(ruta_h, "r", encoding="utf-8") as f:
            contenido_h = f.read()
        import re
        m = re.search(r"#define\s+NUM_PERFILES\s+(\d+)", contenido_h)
        if m and int(m.group(1)) != len(perfiles):
            print(
                f"AVISO: NUM_PERFILES en plantas.h es {m.group(1)} pero el CSV "
                f"tiene {len(perfiles)} filas. Actualiza plantas.h manualmente "
                "(#define NUM_PERFILES) antes de compilar."
            )

    with open(RUTA_CPP, "w", encoding="utf-8", newline="\n") as f:
        f.write(contenido_cpp)

    verificados = sum(1 for p in perfiles if p["fuenteVerificada"])
    pendientes = len(perfiles) - verificados

    print()
    print("=== Resumen ===")
    print(f"Total de perfiles:      {len(perfiles)}")
    print(f"Con fuente verificada:  {verificados}")
    print(f"Pendientes de fuente:   {pendientes}")
    print(f"Archivo regenerado: {RUTA_CPP}")


if __name__ == "__main__":
    main()
