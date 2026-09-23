# Smart Plant Bot — Biblioteca de perfiles de plantas

Biblioteca de perfiles para el sistema de riego automatizado, Arduino Uno
(ATmega328P). Los valores de riego (`umbralRiego`, `dosisMs`,
`esperaHoras`) parten del default de la **categoría hídrica** de la planta
(`SUCULENTA`, `MEDITERRANEA`, `TROPICAL`, `HORTALIZA`), no de datos
inventados por especie. De los 14 perfiles, 8 tienen `fuenteVerificada =
true` (Aloe vera, Nopal, Romero, Lavanda, Tomate, Albahaca, Culantro
coyote y Chile dulce); Potus, Lirio de la paz, Violeta africana, Lengua
de suegra, Helecho de Boston y Orquídea mariposa son negativos
confirmados — se buscó fuente y no existe una válida para el uso que
necesita este proyecto, no una carga pendiente (ver "Fuentes
bibliográficas cargadas y nota metodológica" más abajo).

## Puesta en marcha paso a paso

El Arduino **no detecta** qué planta tiene: el perfil se elige una vez
por Serial y queda guardado en EEPROM, así que sobrevive a un apagón.
Estos son los pasos, en orden. Los pasos 2 y 3 son obligatorios: hasta
que no se hagan, el riego automático se queda inhibido a propósito.

### 0. Cargar el firmware al Arduino

**0.1 — Instalar las dos librerías.** El firmware usa `DHT.h` y
`BH1750.h`, que no vienen con el IDE. En el Arduino IDE, menú
**Herramientas → Administrar bibliotecas** (`Ctrl+Shift+I`), buscar e
instalar:

1. **DHT sensor library**, de Adafruit. Al instalarla el IDE va a
   ofrecer instalar también **Adafruit Unified Sensor**: aceptar, es
   una dependencia obligatoria.
2. **BH1750**, de Christopher Laws.

`EEPROM.h` y `Wire.h` ya vienen incluidas, no hay que instalar nada
para ellas.

**0.2 — Abrir el sketch.** Abrir `biblotecas.ino` con
**Archivo → Abrir**. El IDE carga toda la carpeta de una y muestra una
pestaña por archivo. Si en vez de eso aparece un error de que la
carpeta no es un sketch válido, es porque el nombre de la carpeta y el
del `.ino` no coinciden: tienen que ser iguales (por eso existe
`biblotecas.ino`). Al clonar el repositorio, la carpeta se llama
`smart-plant-bot`, así que hay que renombrarla a `smart_plant_bot`
—con guion bajo, el IDE no acepta guiones medios— y renombrar
`biblotecas.ino` a `smart_plant_bot.ino`.

**0.2b — Verificar el cableado contra los pines del firmware.** Los
`#define` al inicio de `protocolo_serial.ino` tienen que coincidir con
el armado real:

| Señal | Pin | Nota |
|---|---|---|
| Sensor de humedad AD69070 | `A0` | salida analógica |
| Flotador AD12343 | `3` | `INPUT_PULLUP`: en reposo HIGH, el flotador lo lleva a LOW |
| DHT22 | `2` | pin de datos |
| Gate del MOSFET (bomba) | `7` | con resistencia en serie |
| BH1750 | `A4` / `A5` | I2C: SDA y SCL, fijos en el Uno |

Si el flotador queda invertido (marca "sin agua" con el depósito
lleno), cambiar `NIVEL_AGUA_OK` de `HIGH` a `LOW` en vez de recablear.

**0.3 — Elegir placa y puerto.** **Herramientas → Placa → Arduino AVR
Boards → Arduino Uno**, y **Herramientas → Puerto**, el COM que
aparezca al conectar la placa (en Windows suele ser `COM3` o más
alto). Si no aparece ninguno, el cable es de solo carga o falta el
driver CH340 de la placa.

**0.4 — Verificar antes de subir.** Botón ✓ (`Ctrl+R`). Tiene que
terminar en "Compilación completada". Errores típicos:

| Error | Causa |
|---|---|
| `DHT.h: No such file or directory` | Falta el paso 0.1 |
| `BH1750.h: No such file or directory` | Falta instalar BH1750 |
| `redefinition of 'void setup()'` | Se abrió `demo_perfiles.ino` junto al firmware. Son dos sketches distintos, no van en la misma carpeta |

**0.5 — Subir.** Botón → (`Ctrl+U`). Termina en "Subida completa". Si
da `avrdude: ser_open(): can't open device`, el Monitor Serial está
abierto ocupando el puerto: cerralo y reintentá.

**0.6 — Abrir el Monitor Serial.** `Ctrl+Shift+M`. Abajo a la derecha
poner **9600 baudios** y el final de línea en **"Nueva línea"** (`\n`);
si queda en "Sin ajuste de línea" los comandos nunca se ejecutan,
porque el firmware espera un `\n` para dar la línea por terminada.

Alternativa sin Monitor Serial: abrir `plantabot.html` en Chrome o Edge
(no funciona en Firefox, que no tiene Web Serial API) y hacer todo con
botones.

Apenas arranca, si todavía no está calibrado, el firmware avisa solo
con `AVISO;SIN_CALIBRAR`. Todo lo que sigue son comandos que se
escriben en el Monitor Serial, uno por línea, y se mandan con Enter.

### 1. Ver qué plantas hay

```
LISTAR
```

Devuelve una línea `PERFIL;<índice>;...` por especie y termina con
`OK;LISTAR;14`. El **índice** es lo único que hay que anotar:

| Índice | Planta | Índice | Planta |
|---|---|---|---|
| 0 | Aloe vera | 7 | Albahaca |
| 1 | Nopal | 8 | Violeta africana |
| 2 | Romero | 9 | Lengua de suegra |
| 3 | Lavanda | 10 | Helecho de Boston |
| 4 | Potus | 11 | Orquídea mariposa |
| 5 | Lirio de la paz | 12 | Culantro coyote |
| 6 | Tomate | 13 | Chile dulce |

### 2. Calibrar el sensor de humedad (obligatorio)

Hay que hacerlo **una sola vez por sensor**, y hay que hacerlo antes de
seleccionar nada: el umbral de cada perfil está en la escala calibrada,
no en el valor crudo del ADC.

```
CAL;SECO      <- con el sensor limpio, seco y al aire
CAL;AGUA      <- con la punta sumergida en agua, sin mojar la placa
```

Cada uno responde `OK;CAL;SECO;<valor>` / `OK;CAL;AGUA;<valor>` y
guarda en EEPROM. `CAL;RESET` vuelve a fábrica si hay que empezar de
nuevo. Detalle completo en "Calibración de dos puntos" más abajo.

**No confundir estos dos puntos con los umbrales de tierra seca y
tierra húmeda.** `CAL;SECO` va con el sensor **al aire**, no clavado en
tierra seca, y `CAL;AGUA` con la punta **sumergida en agua**, no en
tierra mojada. Son los dos extremos absolutos de la escala, y por eso
quedan mucho más separados que las lecturas que da el sensor dentro de
una maceta. El firmware exige que la diferencia entre ambos sea de al
menos **100 cuentas de ADC**; si es menor, `configEstaCalibrada()`
devuelve `false` y el riego automático nunca actúa. Si la calibración
se hace con el sensor dentro de la tierra en los dos pasos, el rango
sale demasiado angosto y el sistema queda inhibido sin razón aparente.

### 3. Elegir la planta

Con el índice del paso 1:

```
SELECT;8
```

Responde `OK;SELECT;8` y lo guarda en EEPROM. Desde ese momento el
riego automático usa el `umbralRiego`, `dosisMs` y `esperaHoras` de esa
especie. Para cambiar de planta, se manda otro `SELECT` y listo — no
hace falta recompilar nada.

### 4. Verificar que quedó todo bien

```
ESTADO
```

Devuelve `ESTADO;<perfilActivo>;<humedadPct>;<crudo>;<tempC>;<humAmbiente>;<lux>;<nivelAgua>;<calibrada>;<horasDesdeUltimoRiego>;<motivoInhibicion>`.

Lo que hay que mirar:

- `<perfilActivo>` debe ser el índice que elegiste.
- `<calibrada>` debe estar en `1`.
- `<motivoInhibicion>` vacío significa que el riego automático está
  habilitado. Si dice algo, ahí está el problema: `SIN_CALIBRAR`,
  `SIN_AGUA`, `ESPERA_ACTIVA` o `SUELO_HUMEDO`.
- `<humedadPct>` tiene que moverse: probá sacar el sensor de la tierra
  y volver a meterlo. Si queda clavado en 0 o 100, la calibración del
  paso 2 salió mal.

### 5. Probar el riego a mano antes de dejarlo solo

```
RIEGO;MANUAL
```

Enciende la bomba por la dosis del perfil activo y responde
`OK;RIEGO;MANUAL;<dosisMs>`. Si en vez de eso responde `ERR;SIN_AGUA`,
`ERR;SIN_CALIBRAR` o `ERR;ESPERA_ACTIVA`, resolvé eso primero. Con este
comando se comprueba que la bomba, el MOSFET y el flotador están bien
conectados **sin** depender de que el suelo esté seco.

Hecho esto, el sistema ya trabaja solo: revisa humedad y nivel de agua,
y riega cuando el suelo baja del umbral, hay agua en el depósito y pasó
la espera mínima del perfil.

### Notas de manejo de dos especies

- **Violeta africana (8):** el gotero debe descargar en el sustrato, no
  sobre la roseta. El agua sobre la hoja pilosa mancha y favorece
  pudrición.
- **Orquídea mariposa (11):** va en corteza, no en tierra, así que la
  lectura del sensor capacitivo en su sustrato no es comparable con la
  de las demás especies de la tabla.

## Archivos

| Archivo | Rol |
|---|---|
| `plantas.h` | Definiciones (`enum`, `struct`, prototipos). Editable. |
| `plantas.cpp` | Tabla `PERFILES[]` en PROGMEM. **Generado, no editar a mano.** |
| `plantas_lib.ino` | Funciones de acceso a `PERFILES[]`. No define `setup()`/`loop()`. |
| `datos/plantas.csv` | Fuente de verdad de los perfiles. Editar acá. |
| `csv_a_header.py` | Regenera `plantas.cpp` desde el CSV. |
| `config_persistente.h/.cpp` | Persistencia en EEPROM (perfil activo + calibración). |
| `biblotecas.ino` | Archivo ancla, sin código: el IDE exige un `.ino` con el nombre de la carpeta. |
| `protocolo_serial.ino` | **Firmware principal**: único `setup()`/`loop()` del sketch. |
| `puente_serial.py` | Puente HTTP opcional para navegadores sin Web Serial API. |
| `ejemplos/demo_perfiles/` | Sketch aparte, solo para probar la biblioteca de perfiles aislada. |
| `plantabot.html` | Interfaz de configuración y monitoreo (Web Serial API / puente HTTP). Un solo archivo, sin dependencias externas. |

`plantas.h/.cpp`, `plantas_lib.ino`, `config_persistente.h/.cpp` y
`protocolo_serial.ino` van todos en el mismo sketch de Arduino (misma
carpeta): un sketch solo puede tener un `setup()`/`loop()`, y ese lo
pone `protocolo_serial.ino`.

## Cómo agregar o corregir una planta

1. Abrí `datos/plantas.csv` (en el editor de hojas de cálculo que prefieras
   o directamente como texto).
2. Agregá una fila nueva, o editá una existente. Columnas:
   `nombreComun, nombreCientifico, categoria, umbralRiego, dosisMs,
   esperaHoras, luxMin, luxMax, tempMinC, tempMaxC, fuenteVerificada,
   fuente_url, fuente_cita_apa, referencia_literatura`.
   - `categoria` debe ser una de: `SUCULENTA`, `MEDITERRANEA`, `TROPICAL`,
     `HORTALIZA`.
   - `umbralRiego`, `dosisMs`, `esperaHoras` se dejan iguales al default
     de la categoría (ver tabla abajo). La escala del sensor (calibración
     de dos puntos: seco al aire = 0%, saturado = 100%) no equivale a
     capacidad de campo ni a contenido volumétrico de agua, así que un
     valor de la literatura en esos términos no se traslada mecánicamente
     a `umbralRiego` — ver la nota metodológica más abajo.
   - `luxMin`, `luxMax`, `tempMinC`, `tempMaxC` se dejan **vacíos** hasta
     tener una fuente que sí hable específicamente de luz o temperatura.
     No hay default por categoría para estos campos — no hay que
     inventarlos.
   - `fuenteVerificada` en el CSV se ignora: el script la recalcula solo,
     en base a si `fuente_url` y `fuente_cita_apa` están las dos completas.
   - `luxMin`, `luxMax`, `tempMinC` y `tempMaxC` son **todo o nada**: o
     dejás los cuatro vacíos, o completás los cuatro juntos. El script
     rechaza cualquier combinación parcial (por ejemplo lux cargado y
     temperatura vacía). Esto es lo que le permite a `validarPerfil()`
     usar `luxMax == 0` como señal inequívoca de "sin dato ambiental
     todavía" (ver más abajo) — si se admitiera una pareja suelta, esa
     señal dejaría de ser confiable.
   - `fuenteVerificada` (hídrica) y los cuatro campos ambientales son
     **verificaciones independientes**: una fuente sobre riego o humedad
     de sustrato no dice nada sobre luz ni temperatura, así que cargar
     `fuente_url`/`fuente_cita_apa` **no** exige tener también
     luxMin/luxMax/tempMinC/tempMaxC — eso obligaría a inventar datos
     ambientales cada vez que aparece una fuente puramente hídrica, que
     es exactamente lo que no queremos. Un perfil puede perfectamente
     tener `fuenteVerificada = true` y a la vez `estadoValidacion =
     VALIDO_SIN_AMBIENTAL`.
   - `referencia_literatura` es texto libre, solo documentación (por
     ejemplo `"50% CC fue el umbral que mejor resistió..."` o `"sin
     fuente"`): no se valida ni llega a `plantas.cpp`, es trazabilidad
     para quien lea el CSV o el informe.
3. Regenerá `plantas.cpp`:
   ```
   python3 csv_a_header.py
   ```
   El script valida rangos (`umbralRiego` 5–80, `dosisMs` 500–15000 ms,
   `esperaHoras` 1–336 h, `luxMin < luxMax` dentro de `[0, 65535]`,
   `tempMinC < tempMaxC` dentro de `[-128, 127]`), trunca strings que no
   entren en el buffer avisándolo, escapa comillas, y al final imprime
   cuántos perfiles hay, cuántos verificados y cuántos pendientes.
4. Si agregaste o quitaste filas, actualizá a mano `#define NUM_PERFILES`
   en `plantas.h` (el script te avisa si no coincide con la cantidad de
   filas del CSV).

**No edites `plantas.cpp` directamente**: se sobreescribe por completo
en cada corrida del script, y cualquier cambio manual se pierde.

## Defaults por categoría hídrica

| Categoría | umbralRiego | dosisMs | esperaHoras |
|---|---|---|---|
| SUCULENTA | 15% | 2000 ms | 168 h (7 días) |
| MEDITERRANEA | 28% | 4000 ms | 48 h |
| TROPICAL | 45% | 5000 ms | 24 h |
| HORTALIZA | 55% | 6000 ms | 12 h |

`esperaHoras` es la protección antipudrición: aunque el sensor reporte
suelo seco, no se riega si no pasó ese tiempo desde el último riego.
Es la protección principal para las suculentas/cactáceas.

**`esperaHoras` es una decisión de diseño propia, no un dato
bibliográfico.** Es una histéresis temporal para evitar que el control
oscile (regar, secarse el sensor superficialmente, regar de nuevo a los
pocos minutos), elegida por criterio propio de ingeniería según la
categoría hídrica. Ninguna de las fuentes citadas en este proyecto
estudia frecuencia mínima de riego en maceta — serían valores
inventados si se presentaran como respaldados por alguna de ellas, y
no lo están.

## Fuentes bibliográficas cargadas y nota metodológica

De los 14 perfiles, **8 tienen una fuente verificada cargada**
(`fuenteVerificada = true`): Aloe vera, Nopal, Romero, Lavanda, Tomate,
Albahaca, Culantro coyote y Chile dulce. Los 6 restantes — Potus, Lirio
de la paz, Violeta africana, Lengua de suegra, Helecho de Boston y
Orquídea mariposa — quedan
explícitamente marcados como *"configuración inicial sin respaldo
bibliográfico"*, y no por falta de búsqueda: para Potus, la única
referencia disponible (Sawwan, J.S., 1987, tesis doctoral, Univ. of
Illinois) reporta potencial hídrico foliar, no contenido de agua de
sustrato, y su propio autor concluye que la especie no tiene umbrales
de estrés bien definidos; para Lirio de la paz (*Spathiphyllum*), no
existe estudio con umbral hídrico cuantificado — solo guías de
extensión y de cuidado de interiores, que no son fuente académica ni
dan valores convertibles. Ninguna de las dos referencias calificaba
como fuente válida para este uso. Los dos casos sin fuente comparten
una explicación limpia: son ornamentales de interior, y la
investigación en ese nicho estudia luz y nutrición, no umbrales de
riego — no es que faltara buscar, es que la literatura no lo cubre.

Las cuatro especies agregadas después (Violeta africana, Lengua de
suegra, Helecho de Boston y Orquídea mariposa) caen en ese mismo
patrón: son ornamentales de interior y la búsqueda no encontró para
ninguna un estudio con umbral de humedad de sustrato cuantificado —
para *Saintpaulia*, *Nephrolepis* y *Phalaenopsis* solo hay guías de
extensión y de cuidado, y para *Dracaena trifasciata* (ex
*Sansevieria*) la literatura disponible trata remoción de
contaminantes del aire y fitoquímica, no riego. Quedan marcadas como
*"configuración inicial sin respaldo bibliográfico"*, con el default de
su categoría hídrica. Dos de ellas llevan además una precaución de
manejo anotada en `referencia_literatura`, que **no** es un dato de la
tabla: la violeta africana se riega por subirrigación (el agua sobre la
hoja pilosa mancha y favorece pudrición, así que el gotero debe
descargar en el sustrato y no sobre la roseta), y la orquídea se
cultiva en corteza y no en tierra, por lo que la lectura del sensor
capacitivo en su sustrato no es comparable con la de las demás
especies.

Las dos especies agregadas **con** fuente son de literatura
costarricense: Culantro coyote (Soto-Bravo y Rodríguez-Ocampo, 2020,
*Agronomía Costarricense* 44(2)) y Chile dulce, que queda respaldado
por la misma fuente que ya sostenía el perfil de Tomate (Soto-Bravo y
Betancourt-Flores, 2024, *Agronomía Costarricense* 48(1)), porque ese
estudio evalúa las dos especies.

Nota metodológica (para citar en el informe):

> La calibración del sensor capacitivo AD69070 es de dos puntos (0% =
> sensor seco al aire, 100% = sensor saturado en agua), sin
> caracterización gravimétrica del sustrato. Por lo tanto, la lectura
> del sensor no representa contenido volumétrico de agua, y el 100% de
> esta escala no equivale a capacidad de campo. Los regímenes de riego
> reportados en la literatura (expresados en % de capacidad de campo o
> % de agotamiento de humedad volumétrica) se citan aquí como
> referencia orientativa del orden de magnitud del requerimiento hídrico
> de cada especie, y no como un parámetro que se traslada
> matemáticamente a la escala del sensor. El umbral de riego
> (`umbralRiego`) de cada perfil sigue derivándose de su categoría
> hídrica, no de un cálculo de conversión entre ambas escalas.

## `validarPerfil()` y `EstadoValidacion`

`validarPerfil()` no devuelve un `bool`, devuelve uno de tres estados
(`enum EstadoValidacion` en `plantas.h`):

| Estado | Significa |
|---|---|
| `INVALIDO` | `umbralRiego`, `dosisMs` o `esperaHoras` está fuera de rango. No usar este perfil para nada. |
| `VALIDO_SIN_AMBIENTAL` | Los datos de riego son correctos, pero todavía no hay `luxMin/luxMax/tempMinC/tempMaxC` cargados (fila pendiente de fuente). El riego automático por humedad ya es confiable; el firmware **no debe** usar el BH1750 ni el DHT22 para decidir nada sobre este perfil todavía. |
| `VALIDO` | Riego y datos ambientales completos y coherentes. |

Un perfil recién agregado con `luxMin/luxMax/tempMinC/tempMaxC` vacíos
da `VALIDO_SIN_AMBIENTAL`, no `INVALIDO`: los ocho perfiles que trae la
biblioteca están en ese estado hasta que se les cargue una
fuente real. `luxMax == 0` es el centinela que usa `validarPerfil()`
para reconocer "sin dato ambiental" — ningún perfil real tiene 0 lux
como máximo aceptable, así que no hay ambigüedad con un rango real.

## Calibración de dos puntos del sensor de humedad (obligatoria)

`humedadPorcentaje()` convierte la lectura cruda del ADC del sensor
capacitivo AD69070 a un porcentaje 0–100%, usando dos puntos de
referencia: `valorSeco` y `valorSaturado`.

En este sensor la lectura **baja** cuando el suelo está más húmedo (es
capacitivo, no resistivo), así que `valorSeco` suele ser un número más
alto que `valorSaturado`. Estos dos valores varían de un sensor a otro
por tolerancias de fabricación, así que **hay que calibrar cada sensor
individualmente antes de usar el sistema**:

1. **`valorSeco`**: con el sensor completamente seco y al aire (sin
   tocar tierra ni agua), mandá el comando `CAL;SECO` por Serial. El
   firmware toma la lectura cruda actual, la guarda en EEPROM como
   `valorSeco` y responde `OK;CAL;SECO;<valor>`.
2. **`valorSaturado`**: sumergí la punta sensora del sensor en agua
   (hasta la línea marcada, sin mojar la placa electrónica) y mandá
   `CAL;AGUA`. Ídem, guarda `valorSaturado` y responde `OK;CAL;AGUA;<valor>`.

Sin esta calibración, `humedadPorcentaje()` va a devolver porcentajes
incorrectos (por ejemplo, siempre 0% o siempre 100%), y como
`debeRegar()` depende directamente de ese porcentaje, el sistema podría
no regar nunca o regar de forma constante e innecesaria. Por eso el
riego automático (`intentarRiegoAutomatico()` en `protocolo_serial.ino`)
se niega a actuar mientras `configEstaCalibrada()` devuelva `false` —
es decir, mientras sigan los valores de fábrica o el rango sea menor a
100 cuentas de ADC. `CAL;RESET` vuelve a los valores de fábrica
(`620`/`310`) si hace falta empezar de nuevo.

Este bloqueo antes era silencioso: si alguien armaba el equipo y nunca
calibraba, la planta simplemente no se regaba nunca, sin ningún aviso.
Ahora `setup()` avisa por Serial apenas arranca (`AVISO;SIN_CALIBRAR`,
ver tabla de códigos más abajo) y `ESTADO` lo repite en cada consulta
vía el campo `motivoInhibicion`, así que no puede pasar desapercibido.

**Nivel de agua**: antes de encender la bomba (manual o automático), el
firmware también exige `hayAguaSuficiente()` — lectura del flotador
AD12343 — para no arriesgar una marcha en seco que dañe la bomba. Si el
depósito está vacío, el riego automático no actúa y `RIEGO;MANUAL`
responde `ERR;SIN_AGUA`, sin importar qué tan seco esté el suelo ni
cuánto haya pasado desde el último riego.

## Persistencia en EEPROM (`config_persistente.h/.cpp`)

El Arduino Uno tiene 1 KB de EEPROM, con un límite de ~100.000 ciclos
de escritura **por celda**. `ConfigPersistente` guarda ahí, desde la
dirección 0, el perfil activo y la calibración de dos puntos, para que
sobrevivan a un corte de energía:

| Campo | Tipo | Bytes |
|---|---|---|
| `magic` | `uint16_t` | 2 |
| `version` | `uint8_t` | 1 |
| `perfilActivo` | `uint8_t` | 1 |
| `valorSeco` | `uint16_t` | 2 |
| `valorSaturado` | `uint16_t` | 2 |
| `checksum` | `uint8_t` | 1 |
| **Total** | | **9 bytes**, sobre 1024 disponibles (~0.9%) |

`magic = 0x50B7` distingue una EEPROM ya configurada de una virgen (que
en un Arduino nuevo suele leerse como `0xFF` en todos sus bytes).
`checksum` es el XOR de los 8 bytes anteriores, campo por campo (no
memoria cruda del struct), para detectar corrupción. Si `cargarConfig()`
encuentra `magic` o `checksum` inválidos, carga los defaults de fábrica
en memoria **sin escribir nada a EEPROM todavía**.

**Regla de desgaste — dónde se escribe y dónde no:** `guardarConfig()`
recorre el struct byte a byte con `EEPROM.update()` (nunca
`EEPROM.write()`), así que solo gasta un ciclo de escritura por celda
que realmente cambió de valor. Y, más importante, **`guardarConfig()`
nunca se ejecuta en la ruta automática de `loop()`**: `atenderSerial()`,
`actualizarRiego()` e `intentarRiegoAutomatico()` —las tres llamadas
que corren en cada vuelta de `loop()`— no escriben EEPROM por sí
mismas. La única forma de que se llegue a `guardarConfig()` es que
`atenderSerial()` reciba y parsee una línea completa con uno de estos
cuatro comandos, ejecutados explícitamente por una persona: `SELECT`,
`CAL;SECO`, `CAL;AGUA` o `CAL;RESET`. Ninguno de estos corre solo ni
en bucle: cada uno escribe como máximo una vez por invocación humana.

## Protocolo Serial (`protocolo_serial.ino`)

9600 baudios, una línea de texto por comando terminada en `\n` (se
tolera `\r\n`). Buffer de entrada de 48 bytes: una línea más larga se
descarta completa y se responde `ERR;LINEA_LARGA`. Sin `String` de
Arduino — parseo con `strtok` sobre `char[]`.

| Comando | Respuesta |
|---|---|
| `LISTAR` | Una línea `PERFIL;<indice>;<nombreComun>;<nombreCientifico>;<categoria>;<umbralRiego>;<estadoValidacion>;<fuenteVerificada>;<luxMin>;<luxMax>;<tempMinC>;<tempMaxC>` por perfil, seguida de `OK;LISTAR;<total>`. |
| `SELECT;<indice>` | `OK;SELECT;<indice>` o `ERR;INDICE_INVALIDO`. Escribe en EEPROM. |
| `CAL;SECO` | `OK;CAL;SECO;<valor>`. Escribe en EEPROM. |
| `CAL;AGUA` | `OK;CAL;AGUA;<valor>`. Escribe en EEPROM. |
| `CAL;RESET` | `OK;CAL;RESET;<valorSeco>;<valorSaturado>`. Escribe en EEPROM. |
| `ESTADO` | `ESTADO;<perfilActivo>;<humedadPct>;<crudo>;<tempC>;<humAmbiente>;<lux>;<nivelAgua>;<calibrada>;<horasDesdeUltimoRiego>;<motivoInhibicion>` |
| `RIEGO;MANUAL` | `OK;RIEGO;MANUAL;<dosisMs>`, o uno de los `ERR;` de riego de la tabla de abajo. |
| `AYUDA` | Una línea `AYUDA;<comando>` por comando, seguida de `OK;AYUDA`. |
| *(desconocido)* | `ERR;COMANDO_DESCONOCIDO` |

**Cambio de formato de `LISTAR`:** cada línea `PERFIL;` ahora termina
con cuatro campos más — `<luxMin>;<luxMax>;<tempMinC>;<tempMaxC>` —
los mismos valores que ya vivían en `PerfilPlanta` pero que antes no
viajaban por el protocolo. `<luxMax> == 0` es el mismo centinela de
"sin dato ambiental todavía" que usa `validarPerfil()` (un perfil
`VALIDO_SIN_AMBIENTAL` siempre lo trae en 0): quien consuma el
protocolo puede usar ese mismo criterio en vez de fijarse en el texto
de `<estadoValidacion>`.

**Verificación del buffer de salida:** la línea `PERFIL;` más larga
posible (nombre común de 23 caracteres + nombre científico de 31,
`MEDITERRANEA` como categoría, `VALIDO_SIN_AMBIENTAL` como estado, y
los campos numéricos en su peor caso) mide **128 bytes** (129 con el
`\n`). Eso supera el buffer de transmisión de `HardwareSerial` en AVR
(`SERIAL_TX_BUFFER_SIZE` = 64 bytes en el 328P, porque tiene más de
1000 bytes de RAM). No es un problema: `Serial.print()`/`println()`
en AVR **bloquean** (esperan) cuando el buffer está lleno, no
descartan bytes — así que la línea siempre llega completa, solo que
tarda un poco más en transmitirse del todo (a 9600 baudios, ~130 ms
para la línea más larga). Ese bloqueo ocurre únicamente dentro de
`cmdListar()`, disparado por un comando explícito del usuario — nunca
en la ruta automática de `loop()` — así que no compromete ni el
riego automático ni la respuesta a otros comandos en curso.

**Cambio de formato de `ESTADO` — importante para quien consuma el
protocolo (`plantabot.html`, `puente_serial.py`):** se agregó un campo
al final, `<motivoInhibicion>`. Es texto, uno de cinco valores fijos:
`NINGUNO`, `SIN_CALIBRAR`, `SIN_AGUA`, `ESPERA_ACTIVA`, `SUELO_HUMEDO`.
Dice, en este instante, por qué el riego automático **no** actuaría —o
que sí actuaría, si es `NINGUNO`. `<nivelAgua>` también cambió de
significado: antes era la lectura cruda de `digitalRead()` en el pin
del flotador, ahora ya viene interpretada (`1` = hay agua suficiente,
`0` = no) para no obligar al consumidor del protocolo a conocer la
polaridad del cableado.

`motivoInhibicion()`, en `protocolo_serial.ino`, es la única función
que calcula esto — la usan tanto `ESTADO` como `RIEGO;MANUAL` como
`intentarRiegoAutomatico()`, así que las tres rutas siempre están de
acuerdo sobre el motivo. Por dentro llama a `debeRegar()` (la única
puerta de entrada real al riego) y solo repite la comparación de
`esperaHoras` para poder distinguir, cuando `debeRegar()` da `false`,
si el motivo fue el tiempo o el umbral de humedad.

Ni `atenderSerial()`, ni `actualizarRiego()`, ni `intentarRiegoAutomatico()`
usan `delay()`: todo el timing (duración del pulso de bomba, espaciado
entre riegos) se resuelve comparando contra `millis()` en cada vuelta
de `loop()`.

### Códigos `AVISO;` y `ERR;`

`AVISO;` son líneas no solicitadas: el firmware las imprime por su
cuenta, no como respuesta a un comando. Las dos únicas ocurren en
`setup()`, apenas arranca (pueden salir las dos juntas, en un equipo
recién armado):

| Código | Cuándo | Se puede repetir |
|---|---|---|
| `AVISO;EEPROM_VIRGEN` | `cargarConfig()` no encontró una configuración válida (EEPROM nueva, o `magic`/`checksum` no coinciden). Se cargaron los defaults de fábrica en RAM, pero todavía no se escribió nada. | No — solo al arrancar |
| `AVISO;SIN_CALIBRAR` | `configEstaCalibrada()` es `false`: siguen los valores de fábrica, o el rango es menor a 100 cuentas de ADC. | No — solo al arrancar (después, consultar `ESTADO`) |

`ERR;` son siempre respuesta directa a un comando:

| Código | Comando | Significa |
|---|---|---|
| `ERR;INDICE_INVALIDO` | `SELECT` | El índice pedido no existe en `PERFILES[]`. |
| `ERR;LINEA_LARGA` | *(cualquiera)* | La línea recibida superó los 48 bytes del buffer; se descartó entera. |
| `ERR;COMANDO_DESCONOCIDO` | *(cualquiera)* | El comando (o subcomando de `CAL`/`RIEGO`) no existe. |
| `ERR;RIEGO_EN_CURSO` | `RIEGO;MANUAL` | Ya hay un ciclo de bomba en marcha (automático o manual); no se puede superponer otro. |
| `ERR;SIN_CALIBRAR` | `RIEGO;MANUAL` | `configEstaCalibrada()` es `false`. Mismo motivo que el `AVISO` de arranque, pero como rechazo puntual de este comando. |
| `ERR;SIN_AGUA` | `RIEGO;MANUAL` | El flotador AD12343 indica que no hay agua suficiente. No se enciende la bomba en seco. |
| `ERR;ESPERA_ACTIVA;<horasRestantes>` | `RIEGO;MANUAL` | Todavía no pasó `esperaHoras` desde el último riego (protección antipudrición). |
| `ERR;SUELO_HUMEDO;<humedadPct>` | `RIEGO;MANUAL` | Ya pasó `esperaHoras`, pero el suelo no está por debajo del umbral del perfil. No estaba en el enunciado original de `RIEGO;MANUAL` — lo agregué porque `ERR;ESPERA_ACTIVA` solo tiene sentido cuando el motivo es el tiempo; si el motivo real es que el suelo ya está húmedo, informar "horas restantes" sería engañoso. |

Los cuatro últimos (`SIN_CALIBRAR`/`SIN_AGUA`/`ESPERA_ACTIVA`/`SUELO_HUMEDO`)
son exactamente los cuatro valores no-`NINGUNO` de `<motivoInhibicion>`
en `ESTADO` — mismos nombres, misma función (`motivoInhibicion()`)
detrás de los dos.

Campos numéricos de `ESTADO` que fallan la lectura (DHT22 sin
responder, BH1750 desconectado) se imprimen como `NAN` en vez de un
valor falso.

## Puente HTTP opcional (`puente_serial.py`)

Solo hace falta si el navegador **no** soporta Web Serial API (Chrome y
Edge sí; Firefox y Safari, no). Traduce el mismo protocolo de arriba a
HTTP en `127.0.0.1`, para que la interfaz le hable a este puente en vez
de al puerto serie directamente.

```
pip install pyserial
python3 puente_serial.py                 # autodetecta el puerto
python3 puente_serial.py --puerto COM3   # o lo especificás a mano
```

- `GET /puerto` → `{"puerto": "..."}`
- `GET /comando?c=ESTADO` o `POST /comando` (body = texto del comando) →
  `{"comando": "...", "lineas": [...]}`

Autodetección: en Linux/Mac prueba `/dev/ttyACM*` (un Uno real, con
ATmega16U2, enumera así) y `/dev/ttyUSB*` (típico de clones con
CH340/FTDI) como respaldo; en Windows enumera los `COM*` disponibles.
Si hay varios candidatos, usa el primero — pasá `--puerto` si necesitás
elegir otro. Es **opcional**: con un navegador compatible con Web
Serial, la interfaz habla directo con el Arduino y este script no hace
falta correrlo.

## Ejemplo aislado (`ejemplos/demo_perfiles/`)

`protocolo_serial.ino` es el único `setup()`/`loop()` del proyecto
real, así que ya no hay forma de probar la biblioteca de perfiles sola
dentro de ese sketch. `ejemplos/demo_perfiles/demo_perfiles.ino` es un
sketch aparte con ese propósito: recorre los 8 perfiles y los imprime
por Serial a 9600 baudios junto con su `EstadoValidacion` — sin tocar
el protocolo Serial ni la EEPROM.

Para que compile solo, esa carpeta tiene su **propia copia** de
`plantas.h`, `plantas.cpp` y `plantas_lib.ino` (un sketch de Arduino
solo compila los archivos que están en su propia carpeta). Si
regenerás `plantas.cpp` desde el CSV o editás la biblioteca en la raíz,
acordate de copiar los archivos actualizados también ahí — si esto se
vuelve tedioso, la solución de fondo es convertir el proyecto en una
biblioteca de Arduino de verdad (`library.properties` + `src/`), donde
los ejemplos referencian la biblioteca instalada en vez de tener su
propia copia; no lo hice ahora porque no se pidió y es un cambio de
estructura más grande.

## Interfaz web (`plantabot.html`)

Un solo archivo HTML autocontenido (sin CDN, sin frameworks) que habla
el protocolo de arriba. Abrilo directamente en Chrome, Edge u Opera de
escritorio — no hace falta servidor. Arquitectura interna en tres
capas, comentadas dentro del `<script>`:

1. **Transporte**: `crearTransporteWebSerial()` (puerto USB directo,
   Web Serial API) y `crearTransporteHTTP(url)` (habla con
   `puente_serial.py`) implementan la misma interfaz de cuatro
   métodos — `conectar`, `desconectar`, `enviarLinea`, `leerLineas` —
   así que son intercambiables sin tocar el resto del archivo.
2. **Protocolo** (clase `Protocolo`): parsea `PERFIL;`, `ESTADO;`,
   `OK;`, `ERR;`, `AVISO;` a objetos JS.
3. **Interfaz**: solo consume la capa de protocolo, nunca el
   transporte directamente.

Sin Web Serial (Firefox, Safari, móviles), muestra un aviso explicando
por qué y ofrece conectarse al puente HTTP como alternativa, desde el
mismo panel.

El estado "dormida" de la mascota compara la lectura de lux actual
contra el `luxMin` **del perfil activo** (no un umbral genérico), y
solo cuando ese perfil trae `luxMax > 0` — el mismo centinela de "hay
dato ambiental" que usa el firmware. Un perfil `VALIDO_SIN_AMBIENTAL`
nunca entra en "dormida", y sus tarjetas de temperatura/humedad
ambiente/lux aparecen atenuadas con una nota explicando por qué.

**Limitación conocida que sigue en pie:** no hay forma de calcular
"horas restantes de espera" fuera de un intento de `RIEGO;MANUAL` (ese
comando sí devuelve el número en `ERR;ESPERA_ACTIVA`) — el cartel de
`ESTADO` con motivo `ESPERA_ACTIVA` explica la causa pero no muestra
cuántas horas faltan, porque `esperaHoras` del perfil no viaja por
`ESTADO` ni por `LISTAR`. Si se quiere corregir, hay que agregar ese
campo al protocolo (`protocolo_serial.ino`); no es algo resoluble solo
del lado de la interfaz.

## Consumo de memoria (ATmega328P: 2 KB SRAM / 32 KB flash)

`sizeof(PerfilPlanta)` ≈ 69 bytes (24 + 32 de nombres, más 13 de campos
numéricos/bool; AVR-GCC no agrega padding de alineación en estos tipos).

- **Flash**: `PERFILES[]` está en `PROGMEM` → 8 × 69 ≈ **552 bytes de
  flash**, sobre 32 KB disponibles (~1.7%).
- **SRAM**: `DEFAULTS_CATEGORIA[4]` **no** está en `PROGMEM` (no lo pidió
  el enunciado), así que sus 4 × 5 bytes = **20 bytes viven en SRAM** de
  forma permanente. El resto del uso de SRAM es solo temporal: cada vez
  que `perfilPorIndice()` o `buscarPorNombre()` copian un registro desde
  flash, usan ~69 bytes de pila por copia (un `PerfilPlanta` a la vez,
  nunca la tabla completa). En total, uso de SRAM en reposo
  insignificante frente a los 2 KB disponibles.
