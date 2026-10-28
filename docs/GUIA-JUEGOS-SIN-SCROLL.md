# Guía para construir juegos SIN SCROLL en el Core de Vídeo (6502)

**Documento autocontenido para una IA.** Con este archivo basta para diseñar,
programar y depurar un juego nuevo de pantalla fija para el computador 6502 con el
Core de Vídeo `vc`. No requiere consultar nada más, aunque al final hay referencias
opcionales.

Destilado de cinco juegos reales ya funcionando en este hardware:
**Arkanoid** (breakout), **Pac-Man** (laberinto), **Snake** (rejilla),
**Space Invaders** (flota + disparos) y **Tetris** (pozo de piezas).

---

# 0. Reglas de oro (leer primero)

1. **No hay framebuffer.** No dibujas píxeles: escribes tilemap/OAM y el hardware
   compone la imagen. La VRAM es **solo de escritura** (no se lee).
2. **Toda escritura a VRAM/OAM/scroll/paletas va DENTRO del VBLANK.** Fuera de él
   aparecen franjas, sprites partidos y rasgado.
3. **La RAM es el recurso escaso.** El código, los datos y el stack de cc65 salen
   del mismo bloque (~14 KB). Mide en cada paso.
4. **"Patrón" ≠ "tile".** El patrón es el dibujo (2 planos de 8 bytes); el tile es
   el índice. El tilemap guarda índices.
5. **Lo que se mueve libre píxel a píxel → sprites. Lo que es rejilla o son
   muchos → tiles.** Máx. **32 sprites**, **8 por línea**.
6. **El estado del juego vive en RAM** (arrays propios). La VRAM solo lo refleja.
   Nunca "consultes la VRAM" para saber qué hay: guarda tú el estado.
7. **Sin scroll**: deja el scroll a 0 y no uses bandas. Todo el juego cabe en la
   pantalla fija.

---

# 1. El hardware en una página

## 1.1 Pantalla

| Concepto | Valor |
|---|---|
| Pantalla visible | **40 × 30 celdas** = 320 × 240 px lógicos |
| Tamaño de celda | 8 × 8 px |
| Margen | barras a los lados pintadas con `BG_COLOR` (no dibujable) |
| Fila 0 del tilemap | se ve **desplazada** por el pipeline → úsala solo de margen |
| Fila 29 | puede **recortarse** por overscan del monitor |
| **Filas seguras para contenido** | **1 .. 28** |
| Mapa de fondo | 64 × 32 celdas (con scroll; **irrelevante sin scroll**) |

> **Regla práctica sin scroll:** usa las filas **1..28** y las columnas **0..39**.
> La fila 0 es margen/HUD superior si el monitor lo permite.

## 1.2 Paletas

- **4 paletas de fondo** y **4 paletas de sprite**, en **bancos separados**.
- Cada paleta tiene **4 colores** (índices 0-3). El **índice 0 es transparente**.
- Todo es **RGB444** reprogramable: `vc_pal_set_bg(pal, idx, rgb)` /
  `vc_pal_set_spr(pal, idx, rgb)`.
- **`BG_COLOR` = entrada 15 = "paleta 3, color 3"**. Es el color de fondo visible
  (donde hay transparencia) y del margen. Se cambia con `vc_set_bgcolor(rgb)`.
- ⚠️ El auto-incremento del puntero de paleta **satura en 31** (no vuelve a 0);
  recarga el puntero con `vc_pal_ptr()` si quieres empezar de nuevo.
- Los **colores son independientes del patrón**: el mismo dibujo se ve de distinto
  color según la paleta. **Aprovéchalo** para no duplicar patrones.

## 1.3 Tiles y patrones de fondo

- **256 patrones de fondo** (índices `$00`-`$FF`), 8×8 px, **2 bpp** (4 colores).
- **La fuente de texto ocupa `$20`-`$7F`** (el índice = el código ASCII).
  **Tus dibujos propios van en `$01`-`$1F` y `$80`-`$FF`**.
- Patrón = **dos arrays `uint8_t[8]`** (plano 0 y plano 1); cada byte es una fila,
  **bit 7 = píxel izquierdo**.
  ```
  índice de color del píxel = (bit_plano1 << 1) | bit_plano0    (0..3)
  ```
  - Dibujo **a un color (índice 3)**: `plano0 = dibujo`, `plano1 = dibujo`.
  - Dibujo en **índice 1**: `plano0 = dibujo`, `plano1 = 0x00`.
  - Dibujo en **índice 2**: `plano0 = 0x00`, `plano1 = dibujo`.
  - Con **3 tonos** (bisel 3D): usa los índices 1, 2 y 3 repartidos por el dibujo.

## 1.4 Sprites (OAM)

- **32 sprites**, cada uno de **8×8** nativo (o 16×16 con `SCALE2X`).
- **Máximo 8 sprites por línea horizontal** (límite **duro** del chip); si se supera,
  se descartan y el flag `OVERFLOW` se pone a 1. **Planifica el reparto por fila.**
- El campo `TILE` del OAM es el **índice de patrón de SPRITE (0-63)**, un banco
  **aparte** del fondo. No confundir con un tile de fondo.
- ⚠️ **Los patrones 0-2 no arrancan vacíos** (el core graba ahí formas de demo al
  encender). **No están reservados: los 64 patrones (0-63) son utilizables.** Si usas
  0-2, **sobrescríbelos** con tu arte; el setup de VRAM los deja a cero. Igual con
  los **tiles de fondo 0-7** (traen texturas de demo).
- **`SCALE2X` duplica píxeles, no compone 4 patrones**: un 8×8 pasa a 16×16 leyendo
  **un solo** patrón. Para un 16×16 real usa **4 sprites** de 8×8.
- Coordenada **X de 9 bits (0-511)**; la biblioteca gestiona el bit 8.
- `Y ≥ 248` **deshabilita** el sprite.
- Prioridad: **gana el sprite de menor índice** de OAM.
- **Flip**: `VC_SPR_FLIP_X` / `VC_SPR_FLIP_Y` funcionan de forma **fiable**.
  (En tiles de fondo el flip **no** es fiable.)

---

# 2. Arranque del proyecto (copiar y adaptar)

## 2.1 Estructura de directorios (proyecto autónomo)

Cada juego real es **autónomo**: lleva su copia de la librería.

```
MiJuego/
├── makefile
├── game.c              lógica del juego
├── startup.s           arranque cc65 (copiar tal cual)
├── include/
│   ├── video.h         copia de la lib
│   └── romapi.h        ROM API del monitor
├── config/programa.cfg copia de la lib
├── lib/vc.lib          copia de la biblioteca
├── build/              objetos
└── output/game.bin     binario final
```

## 2.2 Makefile mínimo

```make
CC65_HOME ?= D:/cc65
CC  = $(CC65_HOME)/bin/cl65.exe
CA65= $(CC65_HOME)/bin/ca65.exe
LD  = $(CC65_HOME)/bin/ld65.exe

CFLAGS  = -t none -O --cpu 6502 -I include
ASFLAGS = -t none --cpu 6502

all: dirs output/game.bin

dirs:
	@mkdir -p build output

build/game.o: game.c include/video.h include/romapi.h
	$(CC) -c $(CFLAGS) -o $@ $<

build/startup.o: startup.s
	$(CA65) $(ASFLAGS) -o $@ $<

output/game.bin: build/game.o build/startup.o lib/vc.lib
	$(LD) -C config/programa.cfg -m output/game.map -o $@ \
	      build/startup.o build/game.o lib/vc.lib $(CC65_HOME)/lib/none.lib

clean:
	rm -rf build output

.PHONY: all dirs clean
```

> **Compila desde Git Bash / cmd de Windows**, con `make CC65_HOME=D:/cc65`.
> cc65 son `.exe` y no se lanzan desde WSL.

## 2.3 Mapa de memoria (obligatorio conocerlo)

| Rango | Uso |
|---|---|
| `$0000-$00FF` | Zero Page (el monitor usa `$02-$1F`; el juego `$20-$7F`) |
| `$0100-$01FF` | pila hardware del 6502 |
| **`$0800-$3DFF`** | **programa**: código + RODATA + DATA + BSS |
| `$3E00-$3FFF` | pila software de cc65 |
| ⚠️ `$3FFE-$3FFF` | **flag de AUTOBOOT del monitor. No usar.** |
| `$4000-$7FFF` | VRAM |
| `$8000-$BFFF` | ROM (monitor) |
| `$C000-$C0xx` | I/O (GPIO joystick, UART, SPI, I2C, timer) |
| `$D400-$D41F` | SID (sonido) |
| `$D800-$D817` | **registros del Core de Vídeo** |

- Carga en **`$0800`** (`LOAD GAME 0800` / `R 0800`). **No cargues en `$0200`.**
- Comprueba el fin del BSS tras cada compilación:
  ```bash
  grep -A9 "^Segment list:" output/game.map
  ```
  Si se pasa de `$3DFF` → `ld65: Segment 'BSS' overflows`. **Deja margen.**

## 2.4 Esqueleto de `game.c`

```c
#include <stdint.h>
#include "video.h"
#include "romapi.h"

#define W 40
#define H 30

static uint8_t quit_flag;

/* --------------------------- arranque de vídeo --------------------------- */
static void setup_video(void) {
    vc_wait_ready();            /* 1. esperar VIDEO_READY   */
    vc_wait_vblank();           /* 2. entrar en VBLANK       */
    vc_clear_vram();            /* 3. limpiar VRAM (setup HW; limpia patrones y
                                   recarga la fuente $20-$7F) */
    vc_set_bgcolor(VC_RGB888(0x000000));  /* 4. fondo global */

    /* 5. paletas propias (opcional: las de defecto ya sirven) */
    /* 6. cargar mis patrones de fondo y de sprite */
    /* 7. dibujar el mundo/UI estático (bordes, marco, HUD) */

    vc_wait_vblank_end();       /* 8. salir de VBLANK       */
}

/* --------------------------- entrada ------------------------------------ */
static void flush_uart(void) {          /* evitar 'q' fantasma de la carga */
    while (rom_uart_rx_ready()) { (void)rom_uart_getc(); }
}

static void read_input(void) {          /* teclado UART (y/o joystick) */
    while (rom_uart_rx_ready()) {
        char c = (char)rom_uart_getc();
        if (c == 'q' || c == 'Q') quit_flag = 1;
        /* ...  movimiento segun la tecla ... */
    }
}

/* --------------------------- bucle principal ---------------------------- */
int main(void) {
    setup_video();
    flush_uart();                       /* IMPORTANTE: antes del bucle */

    while (!quit_flag) {
        vc_wait_vblank();

        read_input();                   /* leer teclas                 */
        update();                       /* lógica del juego            */
        draw();                         /* escribir VRAM/OAM/paletas   */

        vc_wait_vblank_end();
    }

    /* salida limpia */
    vc_clear_oam();
    vc_set_scroll_x(0);
    vc_set_scroll_y(0);
    return 0;                           /* startup.s hace jmp $8000 */
}
```

> **Ciclo por frame:** `vc_wait_vblank()` → leer → actualizar → dibujar →
> `vc_wait_vblank_end()`. **Una vuelta = un frame** (60 fps). Todo el trabajo debe
> caber en el VBLANK o verás ralentización/parpadeo.

---

# 3. El patrón de diseño central: **rejilla en RAM + tiles; actores en sprites**

Todos los juegos analizados siguen la misma división:

| Qué | Dónde se dibuja | Por qué |
|---|---|---|
| Tablero, laberinto, ladrillos, invasores, piezas fijas, puntos, frutas | **tiles** (tilemap) | Son muchos y están en rejilla |
| Jugador, bolas, balas, fantasmas, nave, cápsulas | **sprites** (OAM) | Son pocos y se mueven libre/píxel a píxel |
| HUD, textos | **tiles** (fuente ASCII) | No cambian o cambian poco |

Y **siempre** hay una **copia del estado en RAM** (array/matriz) que es la fuente
de verdad. El tilemap es solo el reflejo.

## 3.1 Ejemplo: matriz de estado (Pac-Man / Arkanoid / Tetris)

```c
#define COLS 40
#define ROWS 30

static uint8_t board[ROWS][COLS];   /* 1200 B. 0 = vacío, otro = tipo */

/* pintar una celda en la VRAM a partir del estado */
static void draw_cell(uint8_t col, uint8_t row, uint8_t tile, uint8_t pal) {
    vc_put_cell(col, row, tile);
    vc_set_cell_attr(col, row, pal, 0);
}
```

> **RAM:** una matriz 40×30 = **1200 bytes**. Si no cabe, usa una rejilla más
> pequeña (Tetris usa solo el pozo 10×20 = 200 B; Pac-Man usa 28×28 = 784 B).

## 3.2 Objetos (actores) con estructura

```c
typedef struct {
    uint16_t x, y;      /* posición en píxeles      */
    int8_t   vx, vy;    /* velocidad (con signo)    */
    uint8_t  tile;      /* patrón de sprite         */
    uint8_t  flags;     /* paleta + flips           */
    uint8_t  alive;
} actor_t;

static actor_t player;
```

> `vc_sprite_t` describe **cómo va al OAM** (x_lo, y, tile, flags, coll). Para el
> juego, define tu propia estructura y **compón** o copia a la OAM al dibujar.

## 3.3 Dibujar un actor (dentro del VBLANK)

```c
static void draw_actor(uint8_t slot, const actor_t *a) {
    if (!a->alive) { vc_sprite_disable(slot); return; }
    vc_sprite_move(slot, a->x, a->y, a->flags);   /* flags = paleta | VC_SPR_* */
    vc_oam_put(slot, VC_OAM_TILE, a->tile);
}
```

---

# 4. Colisiones (los tres tipos que necesitarás)

## 4.1 Sprite ↔ sprite (software, cajas AABB)

```c
static uint8_t choca(actor_t *a, actor_t *b, uint8_t w, uint8_t h) {
    vc_box_t ba, bb;
    vc_box_from_sprite(&ba, a->x, a->y, w);
    vc_box_from_sprite(&bb, b->x, b->y, w);
    return vc_box_overlap(&ba, &bb);
}
```

La caja puede ser más pequeña que el dibujo (la bola de Arkanoid es 6×6; el misil
de Space Invaders es estrecho) para evitar falsos positivos.

## 4.2 Sprite ↔ rejilla (software: convertir a celda)

Es la técnica de **Pac-Man, Snake y Tetris**.

```c
static uint8_t celda(uint16_t px) { return (uint8_t)(px >> 3); }

/* ¿la celda de destino es muro / está ocupada? */
if (board[celda(y)][celda(x)] == MURO) { /* choque */ }
```

## 4.3 Sprite ↔ tile sólido (hardware, flag **global**)

- Marca celdas sólidas con `vc_set_cell_attr(col, row, pal, VC_ATTR_SOLID)`.
- `vc_solid_hit()` devuelve **1 si algún sprite tocó algo sólido**, pero **no dice
  cuál**. Para saber quién chocó, compara en software (regla 4.2).
- Se evalúa **una vez por VBLANK**; hay 1 frame de latencia.

> ⚠️ **Tres trampas de este mecanismo** (manual §6.1 y §10):
> 1. **`Y ≥ 248` no desactiva la colisión**, solo el dibujado: el barrido recorre los
>    32 sprites. `vc_sprite_disable()` y `vc_clear_oam()` limpian el `COLL_POINT` para
>    evitarlo; si aparcas sprites a mano, hazlo tú.
> 2. **No hay recorte a pantalla:** un sprite en el borde derecho puede colisionar con
>    la celda 40 (fuera de vista pero válida en el mapa).
> 3. **`Y = 255` desborda:** `255 + dy` envuelve a la fila 0. Un objeto que sale por
>    abajo puede “chocar” con algo sólido de arriba.
>
> 4. **Con split de raster no es fiable.** El hardware tiene **un solo** `scroll_x`
>    (y `scroll_y`): la colisión usa el de la **última banda dibujada**. Si tu juego
>    usa bandas con scroll distinto, haz la colisión sprite↔tile **por software**
>    (regla 4.2). Sin split, la colisión con scroll es exacta (verificado en
>    emulador; con split se midió un desfase de 19 columnas).
>
> **Regla derivada:** nunca marques como sólida la **fila 31** (ni la 30 si puedes
> evitarlo). Los sprites aparcados (`Y = 248`) caen en la **fila 31**: si esa fila
> tiene `VC_ATTR_SOLID`, el flag se activa de forma **permanente** por sprites que ni
> siquiera se muestran. Marca sólidas solo las filas ocupables del juego.
>
> `vc_sprite_disable()` deja `X = 0` justo para que ese punto caiga siempre en la
> **fila 31, columna 0**, pero **no puede impedir** que marques la fila 31 como
> sólida: eso lo decide tu juego.
>
> Por estas trampas, **muchos juegos hacen la colisión sprite↔tile en software**
> (regla 4.2) y no usan `vc_solid_hit()`. Es la opción más predecible.

---

# 5. Los cinco arquetipos (elige el más cercano y adáptalo)

## 5.1 Rejilla con movimiento por celdas — **Snake** / **Tetris**

**Cuándo:** el mundo es una cuadrícula y todo se mueve a saltos de celda.

**Técnica clave — dibujo por diferencia:** en vez de redibujar el tablero, solo
tocas las celdas que cambian.

```c
/* mover un segmento: borrar la celda vieja, pintar la nueva */
draw_cell(old_col, old_row, TILE_VACIO, 0);
draw_cell(new_col, new_row, TILE_CUERPO, PAL);
```

- **Snake:** array de celdas `(x,y)`; `snake[0]` = cabeza, `[len-1]` = cola. Cada
  paso toca **2-3 celdas** (nueva cabeza, y la cola si no come).
- **Tetris:** pieza activa = **máscara de bits** por rotación (4 rotaciones,
  `uint16_t`); al mover se **borra la anterior y se pinta la nueva** (4 celdas).
  Al fijar, se copian sus celdas al tablero y se colapsan líneas completas.

**Piezas como bitmask (Tetris):**
```c
/* 4x4 bits por rotación; bit 3 = columna 0; cada fila es un nibble */
static const uint16_t pieza_I[4] = { 0x0F00, 0x2222, 0x00F0, 0x4444 };
```

**Velocidad:** un contador de frames decide cuándo dar el siguiente paso
(p. ej. cada 12 frames). Baja el contador al subir de nivel.

## 5.2 Flota en rejilla + disparos — **Space Invaders**

**Cuándo:** muchos enemigos alineados que avanzan juntos, más proyectiles sueltos.

**Técnica clave — marcha en bloque, por pasos:**
```
cada N frames:
  1. borrar TODAS las celdas actuales de la flota
  2. escribir las celdas desplazadas (col ± 1)
  3. si algún invasor vivo toca el borde: bajar 1 fila y cambiar de dirección
```
- Mantén una **máscara/array de vivos** (`enemy_alive[i]`) para saber qué pintar y
  cuántos quedan.
- **La velocidad sube** según nivel **y** según cuántos invasores quedan (clásico).
- **Colisión bala↔invasor:** convertir la posición de la bala a celda
  (`col = x>>3`, `row = y>>3`) y consultar el array de vivos.
- **Colisión bala↔jugador/UFO:** AABB (sprite-sprite).
- **Sprites a usar:** nave (1), balas (2-3), explosiones (2), UFO (2). Respeta
  **≤8 por línea**: limita los pools (p. ej. 2 misiles y 2 explosiones).

## 5.3 Laberinto con movimiento píxel a píxel — **Pac-Man**

**Cuándo:** personajes que se mueven suave pero **solo giran en el centro de la
celda** (el caso más pulido).

**Técnica clave — giro en el centro de celda:**
```c
/* cada frame: avanzar en la dirección actual */
x += dir_x[dir];  y += dir_y[dir];

/* solo permite cambiar de dirección cuando está alineado con el centro */
if ((x & 7) == 0 && (y & 7) == 0) {
    uint8_t next = next_dir;                 /* dirección pedida */
    if (!es_pared(celda(x + dir_x[next] * 8), celda(y + dir_y[next] * 8)))
        dir = next;
    if (es_pared(celda(x + dir_x[dir] * 8), celda(y + dir_y[dir] * 8)))
        dir = NINGUNA;                        /* choca: parar */
}
```
- **Laberinto 28×28**, centrado (columnas 6..33), HUD en filas 0-1.
- **Paredes finas** con **auto-tiling**: para cada celda de muro mirar los 4
  vecinos y dibujar una línea fina solo en los lados que dan a pasillo.
  ```
  mascara: bit0=arriba bit1=abajo bit2=izq bit3=der
  tile = TILE_PARED_BASE + mascara     (16 combinaciones)
  ```
- **Túnel:** la fila central envuelve: `x = (x + W*8) % (W*8)`.
- **Personajes 8×8** (1 sprite por personaje). Con 5 personajes ocupas **5 sprites
  por línea** (< 8): **sin parpadeo**. No uses 16×16 (necesitarías 10 → flicker).
- **IA de fantasmas (la parte cara):** en cada cruce elegir la dirección que
  **minimice la distancia** al objetivo (sin invertir). Modos: *scatter* (esquina),
  *chase* (persecución con objetivo propio), *frightened* (huir/opuesto),
  *eaten* (volver a casa). Si es costosa, **pásala a ASM**.

## 5.4 Física con rebote — **Arkanoid** (breakout)

**Cuándo:** una o varias bolas que rebotan y rompen bloques.

**Técnica clave — posición y velocidad fijas (Q8.8), por eje:**
```c
/* posición Q8.8 (8 bits enteros + 8 fracción) y velocidad por eje, con signo */
static uint16_t ball_x, ball_y;     /* .8 fijo */
static int16_t  ball_vx, ball_vy;   /* .8 fijo, signo = dirección */

/* cada frame: avanzar en pasos de 1 px, comprobando el BORDE delantero */
int pasos = max(abs(ball_vx), abs(ball_vy)) >> 8;
while (pasos--) {
    if (ball_vx > 0) { ball_x += 256; if (borde_derecho()) { ball_vx = -ball_vx; ... } }
    ...
}
```
- **Dirección por eje independiente**: permite ángulos **que no sean 45°** (25°,
  45°, recto). El rebote en la pala según la **zona golpeada** cambia solo `vx`.
- **Prueba el borde delantero, no el centro**, o una bola pequeña se cuela en el
  muro.
- ⚠️ **Trampa Q8.8:** `uint16_t` solo llega a `0xFFxx` (255.996). `256 << 8 = 0`
  **desborda a 0**. Deja el tope derecho en **250** (no 256) para evitar el salto.
- **Ladrillos → tiles** (2 celdas cada uno), **pala y bola → sprites**.
- **Pala de ancho variable:** compuesta de trozos `[L][MID]...[R]`; la punta
  derecha es la izquierda con `VC_SPR_FLIP_X` (ahorra un patrón).
- **Estado en RAM:** `brick[filas][ladrillos]` (5×13). Romper = borrar sus celdas.

## 5.5 Piezas cayendo en un pozo — **Tetris**

**Cuándo:** piezas que caen y encajan en una rejilla.

- **Todo son tiles** (no se usan sprites): pozo de 10×20, bloques y pieza activa.
- **Pieza activa como 4 máscaras** (una por rotación), `uint16_t` cada una.
- **Dibujo por diferencia** al mover/rotar/gravedad.
- **Colisión:** probar las 4 celdas de la pieza contra el tablero y los muros.
- **Borrar líneas:** detectar fila completa → animación de parpadeo → colapsar
  filas y redibujar.
- **Lock delay:** al apoyarse, dar unos frames antes de fijar, reiniciando el
  contador si el jugador mueve/rota (permite deslizarla).
- **Siguiente pieza:** bolsa de 7 barajada (cada pieza una vez por ciclo).

---

# 6. Patrones reutilizables (copiar tal cual)

## 6.1 Cargar un patrón de tile

```c
/* patron a 1 color (indice 3) */
static const uint8_t solido_p0[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint8_t solido_p1[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
vc_load_bg_pattern(1, solido_p0, solido_p1);

/* patron con bisel (tres tonos): relleno idx2, borde idx3, sombra idx1 */
static const uint8_t blk_p0[8] = {0xFF,0x80,0x80,0x80,0x80,0x80,0x80,0x80};
static const uint8_t blk_p1[8] = {0xFF,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0};
vc_load_bg_pattern(2, blk_p0, blk_p1);
```

## 6.2 Texto (fuente ASCII, recortando al borde)

La fuente pinta **siempre el color 3** de la paleta de la celda. Para cambiar el
color del texto, cambia la **paleta** de la celda.

```c
static void texto(uint8_t col, uint8_t row, const char *s, uint8_t pal) {
    while (*s && col < VC_SCREEN_COLS) {
        vc_put_cell(col, row, (uint8_t)*s);
        vc_set_cell_attr(col, row, pal, 0);
        col++; s++;
    }
}
/* o con el helper de la lib (no recorta): */
vc_put_str_pal(col, row, "HOLA", VC_TINTA(VC_BGPAL_0));
```

- **Centrar:** `col = (40 - len) / 2`.
- **Recortar siempre** al llegar a la columna 40; si envuelves, descuadras todo.
- **Filas seguras 1..28.**

## 6.3 Números (sin `sprintf`, sin división si puedes)

cc65 arrastra `div`/`mod` (`,` más ROM). Para puntuaciones:

```c
/* imprime un u16 alineado a la derecha en 'digits' posiciones, sin div/mod */
static void numero(uint8_t col, uint8_t row, uint16_t v, uint8_t digits,
                   uint8_t pal) {
    char buf[6];
    int8_t i;
    for (i = digits - 1; i >= 0; i--) { buf[i] = (char)('0' + (v % 10)); v /= 10; }
    /* ... escribir buf con texto(col, row, buf, pal) ... */
}
```
Si la RAM/ROM aprieta, sustituye `/` y `%` por **restas repetidas** (o `u16_div_u8`
en ASM, como en Space Invaders).

## 6.4 Aleatoriedad (nunca con semilla fija)

```c
static uint32_t rng;
static void rng_seed(void) {
    /* $D41B = oscilador de la voz 3 del SID: ruido real */
    rng = ((uint32_t)(*(volatile uint8_t *)0xD41B) << 16)
        ^ rom_get_micros() ^ 0x5A5A;
}
static uint8_t rnd(void) {
    rng = rng * 1103515245u + 12345u;
    return (uint8_t)(rng >> 16);
}
```

## 6.5 Pantallas y estados (TÍTULO / JUEGO / GAME OVER)

```c
enum { ST_TITULO, ST_JUEGO, ST_OVER };
static uint8_t estado = ST_TITULO;

while (!quit_flag) {
    vc_wait_vblank();
    read_input();
    switch (estado) {
        case ST_TITULO: if (pulsa_fuego()) { reset_juego(); estado = ST_JUEGO; } break;
        case ST_JUEGO:  update(); if (sin_vidas()) estado = ST_OVER;    break;
        case ST_OVER:   if (pulsa_fuego()) estado = ST_TITULO;          break;
    }
    draw();
    vc_wait_vblank_end();
}
```

> Al **cambiar de pantalla**, limpia: `vc_clear_oam()`, y redibuja el fondo con
> `vc_clear_vram()` (rápido, ~120-275 µs) + cargar patrones + dibujar.

## 6.6 Sonido (SID, opcional)

- Registros `$D400-$D41F`, 3 voces. `$D418 = 0x0F` (volumen, filtro off).
- **Reparto de voces planificado** desde el principio (van justas).
- Efectos percutivos: `GATE=1` y bajarlo tras un contador de frames. **No bloquees.**
- **Un sonido continuo debe pararse explícitamente al cambiar de estado** (si no,
  se queda pegado al terminar la partida).
- Volumen a 0 al salir (`snd_silence()`).

## 6.7 Salida limpia

```c
vc_clear_oam();        /* apagar todos los sprites */
vc_set_scroll_x(0);    /* scroll a 0               */
vc_set_scroll_y(0);
/* silenciar SID             */
return 0;              /* startup.s hace jmp $8000 */
```

---

# 7. Rendimiento: qué cabe en un frame

- El bucle debe completarse en el **VBLANK**. Si te pasas, hay parpadeo/ralentí.
- **Costoso:** redibujar toda la pantalla, bucles con `div`/`mod` de 16 bits, muchas
  llamadas C por objeto.
- **Barato:** dibujo por diferencia (tocar solo lo que cambia), tablas en ROM,
  bucles de 8 bits.
- **Plan por frame:** leer entrada → mover pocos objetos → escribir unas decenas de
  celdas/OAM. Evita redibujos masivos salvo en transiciones (fuera del VBLANK).

> Si el juego va lento, **mide** con el listado:
> ```bash
> D:/cc65/bin/cl65.exe -c -t none -O --cpu 6502 -I include \
>     -l build/game.lst -o build/game.o game.c
> ```
> y pasa a **ASM** la función más pesada.

---

# 8. Presupuesto de recursos (planifícalo antes)

| Recurso | Límite | Cómo gastarlo |
|---|---|---|
| Sprites (OAM) | 32 | jugador 1 + enemigos pocos + proyectiles limitados |
| Sprites **por línea** | **8** | ⚠️ el más fácil de romper |
| Patrones de fondo | 256 (fuente `$20-$7F`) | tus dibujos `$01-$1F`, `$80-$FF` |
| Patrones de sprite | 64 | 1 por orientación/frame (flip ahorra) |
| RAM | ~14 KB (`$0800-$3DFF`) | matriz de estado + objetos + strings |
| Paletas de fondo | 4 (una es `BG_COLOR`) | reparte tonos entre 0/1/2 |
| Paletas de sprite | 4 | colores por sprite vía índice del patrón |

**Truco de colores:** un **mismo patrón** con distintos índices (1, 2, 3) se ve de
distinto color en la **misma** paleta → 3 colores por paleta sin más patrones.

---

# 9. Depuración: orden de sospechas

| Síntoma | Causa probable | Solución |
|---|---|---|
| **No se ve un tile** | Índice fuera de rango; dibujado en `$20-$7F` (fuente); índice 0 (transparente); atributo con paleta cuyo color es el fondo | Revisa tile, rango y paleta |
| **No se ve un sprite** | OAM escrita fuera del VBLANK; `Y ≥ 248`; >8 en la línea; `TILE` ≥ 64 | VBLANK, Y válido, reparto de línea |
| **Sprite partido / franjas** | Escritura fuera del VBLANK | Mueve todo dentro de `vc_wait_vblank()`…`vc_wait_vblank_end()` |
| **Nada reacciona** | `.lib` o **bitstream del FPGA** desfasados | Recompara/regenera artefactos |
| **El programa vuelve al monitor solo** | Basura en el UART con `'q'` | `flush_uart()` antes del bucle |
| **Texto descuadrado** | Envuelve en vez de recortar; filas 0/29 | Recorta al borde; usa filas 1..28 |
| **Queda algo al cambiar de pantalla** | Sprite sin apagar / VRAM sin limpiar | `vc_clear_oam()` + `vc_clear_vram()` |
| **Sonido pegado** | Efecto continuo sin `stop` | Páralo al salir del estado |
| **Aleatorio repetido** | Semilla fija | Siembra con `$D41B` + temporizador |
| **`ld65: BSS overflows`** | RAM agotada | Reduce stack, cadenas, arrays; pasa a ASM |
| **Edité y no cambia nada** | El `.cfg` no es dependencia → no relink | `rm -f output/game.bin && make` |

---

# 10. Checklist antes de dar un juego por terminado

- [ ] Compila con `make CC65_HOME=D:/cc65` desde Git Bash/cmd.
- [ ] BSS **no** llega a `$3E00` (hay margen en `output/game.map`).
- [ ] `setup_video()`: `wait_ready` → `vblank` → `clear_vram` → `bgcolor` → paletas → patrones.
- [ ] **Todas** las escrituras de vídeo dentro del VBLANK.
- [ ] Tiles propios fuera de `$20-$7F`.
- [ ] Reparto de sprites **≤8 por línea** (revisado línea a línea).
- [ ] Texto con **recorte** y centrado por cálculo; filas 1..28.
- [ ] Entrada: `flush_uart()` antes del bucle; salida con `q`.
- [ ] Colisiones coherentes con lo visible (hitbox ajustada).
- [ ] Estado del juego **en RAM**; nunca se lee la VRAM.
- [ ] Al cambiar de pantalla: limpiar OAM y VRAM.
- [ ] Sonido: continuo parado al cambiar de estado; silencio al salir.
- [ ] RNG con semilla no determinista.
- [ ] Salida limpia (OAM off, scroll 0, SID mudo).
- [ ] Probado en **hardware real** pronto, no solo al final.

---

# 11. Referencia rápida de la API `vc`

```c
/* Sincronización */
vc_wait_ready();  vc_wait_vblank();  vc_wait_vblank_end();  vc_status();

/* VRAM */
vc_put_cell(col,row,tile);         vc_put_attr(col,row,attr);
vc_set_cell_attr(col,row,pal,flags);
vc_free_cell(col,row);
vc_load_bg_pattern(tile,p0,p1);    vc_load_spr_pattern(pat,p0,p1);
vc_fill_tilemap(tile);             vc_clear_attr();
vc_copy_tilemap(src);              vc_copy_attr(src);
vc_clear_vram();                   /* setup HW: limpia y recarga la fuente */
vc_clear_oam();                    vc_clear_bg_patterns();  vc_clear_spr_patterns();

/* Sprites */
vc_sprite_set(slot,&s);  vc_sprite_move(slot,x,y,flags);
vc_sprite_disable(slot); vc_sprite16_set(first,x,y,tile0,flags);
vc_oam_put(slot, field, data);   /* OJO: X solo 8 bits, ver abajo */

/* Colisión */
vc_solid_hit();
vc_box_overlap(&a,&b);   vc_box_contains(&b,px,py);   vc_box_from_sprite(&box,x,y,size);

/* Paletas y color */
vc_pal_set_bg(pal,idx,rgb);   vc_pal_set_spr(pal,idx,rgb);
vc_pal_load_bg(pal,arr4);     vc_pal_load_spr(pal,arr4);
vc_set_bgcolor(rgb);
VC_RGB444(r,g,b);   VC_RGB888(0xRRGGBB);

/* Texto */
vc_text_init();
vc_put_char(col,row,c);  vc_put_str(col,row,s);  vc_put_str_pal(col,row,s,pal);

/* Sin scroll: */
vc_set_scroll_x(0);  vc_set_scroll_y(0);   /* y no llames a vc_set_raster */
```

> ⚠️ **La X de un sprite son 9 bits y `vc_oam_put` solo escribe 8.** El bit 8 vive
> en el `FLAGS` (bit 2, `VC_SPR_XBIT8`). Si mueves a mano cruzando 255→256, el
> sprite **salta a la mitad izquierda**:
>
> ```c
> vc_oam_put(slot, VC_OAM_X, (uint8_t)x);   /* MAL: en x=256 reaparece en x=0 */
> vc_sprite_move(slot, x, y, flags);        /* BIEN: gestiona el bit 8       */
> ```
>
> Usa **`vc_sprite_move()`** para mover. `vc_oam_put` reservalo para `TILE`,
> `COLL` y los demás campos de 8 bits.

**Constantes útiles**

```c
VC_SCREEN_COLS (40)   VC_SCREEN_ROWS (30)
VC_TILE_BLANK (0)     VC_CHAR_SPACE (0x20)
VC_BGPAL_0..3         VC_TINTA(pal)
VC_SPPAL_0..3
VC_SPR_FLIP_X  VC_SPR_FLIP_Y  VC_SPR_PRIO  VC_SPR_SCALE2X
VC_ATTR_PRIO  VC_ATTR_FLIP_X  VC_ATTR_FLIP_Y  VC_ATTR_SOLID
VC_COLL_CENTER  VC_COLL_FEET  VC_COLL_HEAD  VC_COLL_LEFT  VC_COLL_RIGHT
VC_OAM_X  VC_OAM_Y  VC_OAM_TILE  VC_OAM_FLAGS  VC_OAM_COLL
VC_SPR_DISABLED (248)
```

---

# 12. Plan de trabajo recomendado para una IA

1. **Definir el arquetipo** (§5) más cercano y anotar su tabla.
2. **Presupuesto:** sprites (y por línea), patrones, RAM de estado, paletas.
3. **Mapa de pantalla en texto** (40×30) antes de programar: filas de HUD, área de
   juego, márgenes.
4. **Diseñar el estado en RAM** (estructuras y matrices).
5. **Constantes de pantalla** (`#define`) y tabla de tiles.
6. **`setup_video()`** + dibujo del mundo estático.
7. **Bucle** → entrada → update → draw.
8. **Un sistema cada vez**, probando en hardware: movimiento → colisiones → puntuación
   → estados → sonido.
9. **Medir RAM y tiempo** en cada paso.
10. **Pulir**: transiciones limpias, salida limpia, dificultad/niveles.

---

# 13. Errores que NO debes cometer (resumen)

- Leer la VRAM (no se puede) o usar registros de hardware como contadores.
- Escribir OAM/VRAM/tiles fuera del VBLANK.
- Dibujar tiles propios dentro de `$20-$7F` (destruye la fuente).
- Creer que un índice alto de OAM tiene prioridad (gana el **menor**).
- Usar flip en tiles de fondo (no es fiable; usa sprites o 4 patrones).
- Superponer >8 sprites en una línea horizontal.
- Volver `main` sin limpiar (deja sprites/scroll/sonido).
- Semilla de RNG fija (todo sale igual).
- Olvidar `flush_uart()` (parece "reinicio").
- Meter 16×16 en personajes que comparten línea (flicker).
- Dar por hecho que la `.lib`/bitstream están al día.

---

*Fin. Este documento es autosuficiente: con §1 (hardware), §2 (arranque), §3
(patrón de diseño), §4 (colisiones), §5 (arquetipos) y §6 (snippets) se puede
construir un juego completo sin scroll.*
