# Biblioteca del Core de Vídeo (`vc`)

Librería en **C** con núcleo optimizado en **ensamblador** para programar juegos
sobre el Core de Vídeo de un computador 6502 (Sipeed Tang Nano 9K / Gowin GW1NR-9).

Cubre todas las capacidades del core: **tiles + tilemap, sprites (OAM), scroll,
split de raster (bandas/HUD), modo texto, colisión sprite↔tile y paletas
programables** (incluido el color de fondo global).

> **Referencia de hardware:** el comportamiento del core (registros, semántica,
paletas, límites) se documenta en el *Manual de Programación del Core de Vídeo*,
(v2.7, hardware `6502_board_v3`), incluido en este repositorio como
[`docs/07-MANUAL-PROGRAMACION.md`](docs/07-MANUAL-PROGRAMACION.md). La librería
lo refleja y encapsula; no se necesita conocer el VHDL.

## Estructura

```
.
├── src/
│   ├── video.h         # API pública: registros, constantes y prototipos
│   ├── video.s         # Núcleo en ensamblador (puerto indirecto, VBLANK, sprites)
│   ├── collide.s       # Núcleo de colisiones AABB en ensamblador
│   └── gfx.c           # Alto nivel en C (texto, helpers de fondo, utilidades)
├── config/
│   └── programa.cfg    # Configuración del linker
├── docs/
│   ├── VIDEO-LIB.md    # Documentación de la biblioteca
│   └── CHANGELOG.md    # Historial de cambios y correcciones
├── examples/
│   ├── demo/           # Demo que ejercita toda la biblioteca
│   │   ├── include/romapi.h    # ROM API del monitor (UART, SD, timer)
│   │   ├── main.c
│   │   └── startup.s   # Runtime de arranque (C sobre el monitor)
│   ├── collision/      # Ejemplo de detección de colisiones
│   │   ├── include/romapi.h
│   │   ├── main.c
│   │   └── startup.s
│   ├── animation/      # Demo de animación de sprite (camina + flip)
│   │   ├── include/romapi.h
│   │   ├── main.c
│   │   ├── startup.s
│   │   ├── piskel2c.py         # convierte .piskel a arrays C
│   │   └── *.piskel
│   └── palette/        # Demo de paletas programables (colores en vivo)
│       ├── include/romapi.h
│       ├── main.c
│       ├── startup.s
│       └── makefile
├── tests/              # Tests unitarios de los núcleos asm (sim65)
│   ├── run.sh
│   ├── collide_test.s
│   ├── sprite_test.s
│   ├── sprite16_test.s
│   ├── pat_base_test.s
│   ├── spr_load_test.s
│   ├── pal_test.s
│   └── humano_test.s
├── makefile            # Compila la biblioteca (output/vc.lib)
└── README.md
```

> **Nota:** `romapi.h` **no** es parte de la biblioteca de vídeo. Es el header de
> la **ROM API del monitor** (jump table en `$BF00`: UART, SD, timer, SPI, I2C).
> La demo lo usa para leer el teclado por UART y saludar; la biblioteca de vídeo
> (todo lo `vc_*`) **no depende** de él.

## Compilación de la biblioteca

```bash
make          # genera output/vc.lib
make test     # ejecuta los tests (sim65)
make clean
```

Ajusta `CC65_HOME` si tu cc65 no está en `D:/cc65`:

```bash
make CC65_HOME=C:/ruta/a/cc65
```

## Uso en un programa nuevo

```c
#include <stdint.h>
#include "video.h"
#include "romapi.h"      /* ROM API del monitor (opcional) */

int main(void) {
    vc_wait_ready();          /* 1. esperar inicialización de VRAM */
    vc_wait_vblank();
    vc_clear_vram();          /* 2. limpiar tilemap + atributos + OAM */

    while (1) {
        vc_wait_vblank();     /* 3. actualizar SOLO en VBLANK */
        /* ... mover sprites, scroll, HUD ... */
        vc_wait_vblank_end();
    }
    return 0;                 /* al retornar, el monitor recupera el control */
}
```

Enlaza con `output/vc.lib` y `config/programa.cfg`.

## Ejemplos

Hay cuatro ejemplos en `examples/`, cada uno con su propio makefile:

- **`examples/demo/`** — demo que usa la biblioteca completa: carga de tileset,
  mundo con scroll, sprite animado, objeto 16×16 (con `SCALE2X`), HUD con split
  de raster, texto en color y colisiones.
- **`examples/collision/`** — ejemplo centrado en la detección de colisiones
  (`vc_box_overlap`, `vc_box_contains`, `vc_solid_hit`).
- **`examples/animation/`** — animación de sprite: un humanito de 8×8 (3 frames)
  que camina, hace flip en las paredes y sigue animado. Incluye `piskel2c.py`
  para convertir archivos `.piskel` a arrays C.
- **`examples/palette/`** — paletas programables: cambia los colores en vivo
  reescribiendo las entradas de paleta (`vc_pal_set_*`, `vc_pal_load_*`).

## Documentación

La referencia completa de la API está en
[`docs/VIDEO-LIB.md`](docs/VIDEO-LIB.md): sincronización, limpieza de VRAM,
fondo, sprites, colisión, scroll, texto, entrada por UART y la convención de
llamada cc65 para el núcleo en ensamblador.

El historial de cambios y correcciones está en [`docs/CHANGELOG.md`](docs/CHANGELOG.md).

## Tests

Los núcleos en ensamblador tienen tests unitarios que se ejecutan con `sim65`
(sin hardware):

```sh
make test      # o: sh tests/run.sh
```

Ver [`tests/README.md`](tests/README.md).

## Requisitos

- **CC65** (probado con 2.19)
- **Monitor 6502** con ROM API en `$BF00` (jump table)
- **SD Card** para transferir el programa (o XMODEM)

## Licencia

Este proyecto está licenciado bajo la **GNU General Public License v3.0**.
