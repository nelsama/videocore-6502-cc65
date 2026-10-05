# Manual de Programación del Core de Vídeo

**Plataforma:** computador 6502 sobre Sipeed Tang Nano 9K (Gowin GW1NR‑9)
**Salida:** HDMI, 320×240 lógicos (720×480 físicos, escalado 2×), 60 Hz
**Modelo:** coprocesador gráfico estilo NES/VIC‑II (tiles + sprites, **sin framebuffer**)
**Público:** programadores y asistentes de IA que escriban juegos en ensamblador 6502.

> Este manual es **autocontenido**: describe el comportamiento del vídeo como una
> **caja negra** (registros y memoria). No necesitas conocer cómo está hecho por
> dentro. Los ejemplos están en ensamblador (ca65 / cc65).

**Versión del manual:** 2.7
**Hardware de referencia:** `6502_board_v3` (módulo de vídeo cerrado: tiles + sprites
+ texto + scroll + split de raster + colisión sprite↔tile).
Si recompilas el hardware, anota aquí la versión del manual correspondiente.

---

## Índice

1. [Visión general y modelo mental](#1-visión-general-y-modelo-mental)
2. [Mapa de memoria](#2-mapa-de-memoria)
3. [Resolución, coordenadas y unidades](#3-resolución-coordenadas-y-unidades)
4. [Colores y paletas](#4-colores-y-paletas)
5. [El fondo: tiles y tilemap](#5-el-fondo-tiles-y-tilemap)
6. [Sprites (OAM)](#6-sprites-oam)
7. [Scroll](#7-scroll)
8. [Split de raster (bandas / HUD)](#8-split-de-raster-bandas--hud)
9. [Modo texto](#9-modo-texto)
10. [Colisión sprite↔tile (sólidos)](#10-colisión-spritetile-sólidos)
11. [STATUS: VBLANK y sincronización](#11-status-vblank-y-sincronización)
12. [Recetas completas](#12-recetas-completas)
13. [Limitaciones y buenas prácticas](#13-limitaciones-y-buenas-prácticas)
14. [Esqueleto de juego](#14-esqueleto-de-juego)
15. [Apéndice A — Chuletas de referencia](#apéndice-a--chuletas-de-referencia)

---

## 1. Visión general y modelo mental

El vídeo funciona como un **coprocesador gráfico**. Tú (el programa) **no dibujas
píxeles**: escribes **memoria de vídeo** (VRAM) y **registros**, y el vídeo se
encarga de generar la imagen. Para ti es una caja negra: solo importan sus registros
en `$D800`-`$D812` y la memoria de vídeo que llenas a través de ellos.

Tres capas, de atrás a delante:

```
   ┌─────────────────────────────────────────┐
   │ MARGEN (barra de color `BG_COLOR`)      │  ← no dibujable
   ├─────────────────────────────────────────┤
   │ FONDO  (tiles 8×8 desde el tilemap)     │  ← capa 1
   ├─────────────────────────────────────────┤
   │ SPRITES (8×8, con prioridad/transparencia) │ ← capa 2
   └─────────────────────────────────────────┘
```

- **Pantalla visible:** 40×30 tiles = 320×240 píxeles lógicos.
- **Mapa de fondo:** 64×32 celdas (más grande que la pantalla → scroll).
- **Tiles:** 256 patrones de 8×8, 2 bpp (4 colores c/u).
- **Sprite:** 8×8 píxeles, con hasta **64 dibujos (patrones)** distintos, flips, escala
  2× y prioridad. Se controlan con la **OAM** (32 sprites).
- **Color 0 de sprite** = transparente. **Color 0 del fondo** = transparente
  (se ve `BG_COLOR` o un sprite detrás).

Todo se controla por **un puerto indirecto de 3 registros** (`$D800/$D801/$D802`)
para memoria, y **registros directos** para scroll/status/bandas.

---

## 2. Mapa de memoria

### 2.1 Registros de vídeo (escritura/lectura directa)

| Dir | Nombre | R/W | Descripción |
|-----|--------|-----|-------------|
| `$D800` | `VID_ADDR_LO` | W | Dirección de VRAM, byte bajo (8 bits) |
| `$D801` | `VID_ADDR_HI` | W | Área + dirección alta (ver §2.2) |
| `$D802` | `VID_DATA` | W | Dato; **escribirlo dispara la escritura** |
| `$D803` | `STATUS` | R | VBLANK / OVERFLOW / SOLID_HIT / VIDEO_READY |
| `$D804` | `SCROLL_X_LO` | W | Scroll X (banda media), byte bajo |
| `$D805` | `SCROLL_X_HI` | W | Scroll X, bits 2:0 |
| `$D806` | `SCROLL_Y_LO` | W | Scroll Y (banda media), byte bajo |
| `$D807` | `SCROLL_Y_HI` | W | Scroll Y, bits 2:0 |
| `$D808` | `MAP_STRIDE` | W | **Reservado.** El hardware usa stride 64 fijo; no escribir |
| `$D809` | `RASTER_LINE0` | W | Fin de banda superior. `$FF` = sin banda |
| `$D80A/$D80B` | `BAND2_X` lo/hi | W | Scroll X de la banda superior |
| `$D80C/$D80D` | `BAND2_Y` lo/hi | W | Scroll Y de la banda superior |
| `$D80E` | `RASTER_LINE1` | W | Fin de banda media. `$FF` = sin banda |
| `$D80F/$D810` | `BAND3_X` lo/hi | W | Scroll X de la banda inferior |
| `$D811/$D812` | `BAND3_Y` lo/hi | W | Scroll Y de la banda inferior |
| `$D813` | `PAL_PTR` | W | Puntero de paleta (0-15 fondo, 16-31 sprite). Ver §4.4 |
| `$D814` | `PAL_LO` | W | Color de paleta, bits 7:0 (RGB444) |
| `$D815` | `PAL_HI` | W | Color de paleta, bits 11:8 → **escribe la entrada y auto-incrementa `PAL_PTR`** |

### 2.2 Encoding de `$D801` (área + dirección alta)

`$D801` = `area(7:6)` + `pat_hi(5)` + `bit3` + `dir_alta(2:0)`

| bits 7:6 | bit 5 | bit 3 | Destino | Dirección |
|----------|-------|-------|---------|-----------|
| `00` | – | – | **tilemap** | `0..2047` |
| `01` | – | – | **atributos** (del fondo) | `0..2047` |
| `10` | 0/1 | – | **patrón de fondo** (plano 0/1) | `tile*8+fila` |
| `11` | – | 0 | **OAM** (byte de sprite) | `0..159` |
| `11` | 0/1 | 1 | **patrón de sprite** (plano 0/1) | `patron*8+fila` |

**Patrón de fondo/sprite:** cada tile/patrón tiene **2 planos** (`pat_hi=0` → plano 0,
`pat_hi=1` → plano 1). Se escriben por separado.

Cada **fila** del patrón son **2 bytes**: el plano 0 y el plano 1. El color de cada
píxel es `(bit_plano1 << 1) | bit_plano0`.

> **Cómo se almacena en memoria de vídeo:** cada fila es una **palabra de 16 bits**,
> con el **plano 0 en el byte bajo** y el **plano 1 en el byte alto**:
> `palabra = (plano1 << 8) | plano0`. El hardware **retiene el byte del plano 0**
> cuando escribes el plano 0 y, **al escribir el plano 1, guarda la palabra completa**
> de esa fila. (En la terminología de los registros: `$D801` bit 5 = 0 → plano 0,
> bit 5 = 1 → plano 1.)

- Puedes escribir **plano 0 y plano 1 en cualquier orden**. El color visible no
  cambia hasta que se escribe el **plano 1** de esa fila; mientras tanto se usa el
  plano 0 anterior.
- **No difieras** plano 0 y plano 1 de la misma fila entre frames distintos: si
  cargas un tileset "todo el plano 0 y luego todo el plano 1", durante esa carga se
  verán colores intermedios. Para evitar parpadeo, escribe ambos planos de la fila
  seguidos (o hazlo entero dentro del mismo VBLANK).

### 2.3 Reparto de la dirección alta (bits 2:0 de `$D801`)

Sea `D` la dirección dentro del área (para el tilemap `D = fila*64+col`; para un
patrón `D = tile*8+fila`). Entonces:

```
$D800 = D & $FF
$D801 = area | ((D >> 8) & $07)      ; bits 2:0 = bits 10:8 de D
```

Ejemplo (tilemap, celda 1300 = `$0514`): `$D800 = $14`, `$D801 = $05` (área `00`).
Para un patrón de fondo (`area=10`, `tile=200`, `fila=3`): `D = 200*8+3 = 1603 = $0643`
→ `$D800 = $43`, `$D801 = $86` (área `10` | `$06`).

> Para patrones de fondo, `tile=0..255` y `fila=0..7` dan `D=0..2047` (11 bits);
> no hay desbordamiento y `dir_alta` usa sus 3 bits cuando `tile >= 32`.

### 2.4 Mapa de memoria completo de la VRAM

La memoria de vídeo se organiza en **áreas independientes**, seleccionadas por los
bits del `$D801`. Cada una tiene su propio espacio de direcciones (empieza en 0):

| Área | `$D801` (bits 7:5 / bit3) | Tamaño | Unidad | Dirección | Uso |
|------|---------------------------|--------|--------|-----------|-----|
| **Tilemap** | `$00` | 2048 | bytes | `fila*64 + col` | índice de patrón por celda (mundo 64×32) |
| **Atributos** | `$40` | 2048 | bytes | `fila*64 + col` | paleta/flips/PRIO/SÓLIDO por celda |
| **Patrones de fondo** | `$80`/`$A0` (plano 0/1) | 2048 | **palabras** | `tile*8 + fila` | 256 patrones de 8×8, 2bpp |
| **OAM** | `$C0` (bit3=0) | 160 | bytes | `sprite*5 + campo` | 32 sprites × 5 campos |
| **Patrones de sprite** | `$C8`/`$E8` (plano 0/1) | 512 | **palabras** | `patron*8 + fila` | 64 patrones de 8×8, 2bpp (§6.0) |

**Aclaraciones:**

- Cada área **empieza en dirección 0**; el `$D801` lo que hace es *seleccionar el área*
  y aportar los bits altos de la dirección.
- **Tilemap y atributos:** aunque el mundo es 64×32 = 2048 celdas, la pantalla visible
  usa 40 columnas × 30 filas. Los bits 11 de dirección cubren las 2048 celdas.
- **Patrones de fondo:** 2048 palabras de 16 bits ÷ 8 filas = **256 patrones** (0..255).
- **Patrones de sprite:** 512 palabras ÷ 8 filas = **64 patrones** (0..63). Ver §6.0.
- El banco de **fuente** (los glifos del modo texto) es una **memoria aparte**, no
  direccionable por este puerto desde el CPU; se copia a los patrones de fondo en el
  arranque (ver §9).
- El OAM **no tiene “dirección de patrón”: son registros sueltos**, no un array 2D;
  su dirección es simplemente el índice de byte (0..159).

---

## 3. Resolución, coordenadas y unidades

- **Píxel lógico** = 2×2 píxeles físicos. Trabajas siempre en **320×240**.
- **Tile/celda** = 8×8 píxeles lógicos.
- **Pantalla visible:** 40 tiles de ancho × 30 de alto.
- **Mapa de fondo:** 64 tiles de ancho × 32 de alto.

```
  Eje X: 0..319 (píxeles)   |  0..39 (tiles visibles)  |  0..63 (mapa)
  Eje Y: 0..239 (píxeles)   |  0..29 (tiles generados) |  0..31 (mapa)
```

> **Filas realmente utilizables:** aunque el hardware genera 30 filas (0..29),
> las filas **0 y 29** pueden quedar fuera de pantalla por overscan del monitor.
> Usa como contenido las filas **1..28**. Ver el aviso de overscan más abajo.

> **Sprites:** X es de **9 bits** (0-511) para cubrir los 320 px de ancho; Y es de
> 8 bits (0-255, sobra para 240). El bit 8 de X va en FLAGS(2). Ver §6.2.

**Dirección de celda en el tilemap:** `celda = (y_tile * 64) + x_tile`

> ⚠️ **Margen:** el margen son **80 px físicos en total (40 px físicos por lado = 20 px
> lógicos por lado)**, que se pintan con `BG_COLOR` y no muestran tiles. La pantalla
> útil de tiles es **40×30 = 320×240 píxeles lógicos**, exactamente las 40 columnas
> visibles; el margen está **fuera** de esos 320 px lógicos.

> ⚠️ **Desfase de la fila 0:** la **fila 0 del mapa** tiende a verse parcialmente
> corrida. Es una peculiaridad del hardware que el programador no controla. Por eso
> se reserva la **fila 0 como margen** y el contenido empieza en la **fila 1**. Esto
> afecta **solo al fondo** (no a los sprites).

> ⚠️ **Recorte del monitor (overscan):** las filas **0 y 29** (los 8 píxeles lógicos
> superiores e inferiores, es decir los **bordes absolutos** de la imagen) pueden
> **quedar fuera** de la pantalla según el monitor/TV, que suele recortar el overscan.
> El hardware dibuja las 30 filas pegadas al límite de la zona visible, así que no deja
> colchón vertical (a diferencia del eje X, que sí tiene margen).
>
> **Recomendación:** no coloques información crítica en las filas **0 y 29**; úsalas
> como margen y pon tu contenido entre las filas **1 y 28**. Ver §13.

---

## 4. Colores y paletas

Cada píxel es **2 bpp** → índice de color **0-3** dentro de la paleta de su celda/sprite.

### 4.1 Paletas de FONDO (4 paletas × 4 colores)

> Los colores de esta tabla son los **valores por defecto** al arrancar. El CPU
> puede reescribirlos (ver §4.4).

El **atributo de la celda usa los bits 1:0** para seleccionar una de las 4 paletas
(los bits 3:2 del atributo están reservados / no se usan). El índice final (0-15) se
forma con la **paleta en los bits altos** y el **color en los bajos**:

```
índice = (paleta << 2) | color          ; paleta 0-3, color 0-3
```

| Paleta | color0 | color1 | color2 | color3 | Uso |
|--------|--------|--------|--------|--------|-----|
| 0 | (transparente) | azul (`$00A`) | cian (`$0CF`) | blanco | texto / cielo |
| 1 | (transparente) | marrón (`$A62`) | gris (`$AAA`) | blanco | terreno |
| 2 | (transparente) | verde (`$0A0`) | verde oscuro (`$060`) | verde | vegetación |
| 3 | (transparente) | gris (`$888`) | marrón (`$840`) | **BG_COLOR** | entrada 15 = fondo |

> **El color 0 del fondo es SIEMPRE transparente**: donde el patrón tiene color 0 no
> se pinta fondo y se ve `BG_COLOR` (o un sprite que esté detrás). Por eso la columna
> "color0" es transparente, no negro. El **color 3** es el que usa la fuente de texto.
>
> ⚠️ **La entrada 15 (paleta 3, color 3) coincide con `BG_COLOR`** (§4.3). Si un tile
> usa este color, se pinta con el **color del fondo** (y cambia con él). No es un
> color independiente para tiles: úsalo si quieres que un tile tenga "fondo del color
> de BG" (sólido, a diferencia del color 0 que es transparente).

### 4.2 Paletas de SPRITE (4 paletas × 4 colores, banco aparte)

> Los colores de esta tabla son los **valores por defecto** al arrancar. El CPU
> puede reescribirlos (§4.4).

Colores **RGB exactos** (24 bits, formato `#RRGGBB`). El valor es un nibble por canal
(0-F) expandido a 8 bits, igual que en el fondo:

| Paleta | color0 | color1 | color2 | color3 |
|--------|--------|--------|--------|--------|
| 0 | **transparente** | naranja/piel `#FF8800` | marrón `#884400` | negro `#000000` |
| 1 | **transparente** | azul `#0000FF` | cian `#00FFFF` | blanco `#FFFFFF` |
| 2 | **transparente** | magenta `#FF00FF` | rojo `#FF0000` | blanco `#FFFFFF` |
| 3 | **transparente** | verde `#00FF00` | naranja `#FF8800` | blanco `#FFFFFF` |

> **Nota sobre el valor crudo:** el hardware guarda 12 bits (nibble por canal) y los
expande a 24 bits duplicando el nibble. Según eso, el valor `$F80` de la paleta 0
es `#FF8800` (R=F, G=8, B=0 → `FF 88 00`), no un naranja genérico.

Se selecciona con los **bits 1:0** de FLAGS del sprite (4 paletas; los bits 3:2 de
FLAGS están reservados). El índice se forma igual que en el fondo:
`índice = (paleta << 2) | color`. Recuerda que el **color 0 del sprite también es
transparente** (nunca se ve la columna "transparente"): donde el patrón del sprite
tiene color 0, se ve lo que haya detrás (fondo o `BG_COLOR`).

### 4.3 Color de fondo global (`BG_COLOR`)

Es lo que se ve en el **margen** (barras) y en el **fondo vacío** (donde el tile de
fondo tiene color 0 y no hay sprite).

**BG_COLOR = la entrada 15 de la paleta de FONDO** (`pal_bg(15)`). No tiene registro
propio: se cambia escribiendo esa entrada con el puerto de paleta:

```asm
    LDA #15  : STA $D813     ; entrada 15 = BG_COLOR
    LDA #$8C : STA $D814     ; bits 7:0
    LDA #$04 : STA $D815     ; bits 11:8  -> aplica BG = $48C (azul cielo)
```

- Valor por defecto: `$48C` = azul cielo (`#4488CC`).
- Formato **RGB444**, igual que el resto de colores.
- **La entrada 15 se comparte con el color 3 de la paleta 3 del fondo.** Un tile que
  use "paleta 3, color 3" se pintará con el **mismo color que BG_COLOR** (y cambiará
  junto con él). Es decir: **sí puedes usarla en tiles**, pero su color es **el del
  fondo**, no uno independiente.

  > ⚠️ **Los sprites no se ven afectados.** Las paletas de sprite son un **banco
  > aparte** (entradas 16-31), así que un sprite con "paleta X, color 3" usa la
  > entrada `16 + X*4 + 3` (19/23/27/31), nunca la 15. Cambiar `BG_COLOR` **no**
  > altera ningún color de sprite.

  > **Truco útil:** como el color 15 siempre coincide con el fondo, un tile puede
  > usarlo como "fondo opaco del color de BG": a diferencia del **color 0** (que es
  > transparente y deja ver sprites detrás), el **color 15** pinta el color del fondo
  > de forma **sólida** (tapa lo de abajo).
- Es **global**: un solo color para toda la pantalla. Para "fondos por zona", coloca
  **tiles** en las celdas deseadas (las letras transparentes dejarán ver ese tile).

### 4.4 Paleta ESCRIBIBLE por el CPU (`$D813-$D815`)

Las tablas de §4.1 y §4.2 son los valores **por defecto** (al arrancar). El CPU puede
**reescribir cualquier entrada de paleta** en cualquier momento mediante un puerto
indirecto con **auto-incremento**. Esto permite crear tu propia paleta, hacer fundidos,
parpadeos, paletas por nivel, etc.

**Registros:**

| Dir | Nombre | R/W | Función |
|-----|--------|-----|---------|
| `$D813` | `PAL_PTR` | W | Puntero de entrada: **0-15 = fondo**, **16-31 = sprite** |
| `$D814` | `PAL_LO` | W | `color[7:0]` |
| `$D815` | `PAL_HI` | W | `color[11:8]` (bits 3:0) → **escribe la entrada y `PAL_PTR++`** |

- Hay **32 entradas de 12 bits (RGB444)**: 16 de fondo + 16 de sprite.
- La entrada se calcula como `paleta*4 + color` (dentro de su banco).
  - Fondo: entradas 0-15 → paleta 0 = 0-3, paleta 1 = 4-7, paleta 2 = 8-11, paleta 3 = 12-15.
  - Sprite: entradas 16-31 → paleta 0 = 16-19, paleta 1 = 20-23, etc.
- **`PAL_PTR` avanza solo** al escribir `$D815`, así que para cargar una paleta
  completa pones el puntero **una vez** y encadenas escrituras.
- Formato del color: **RGB444**, un nibble por canal. Ej.: `$F80` = rojo F, verde 8,
  azul 0 = naranja `#FF8800`.

**Ejemplo: definir los 4 colores de la paleta de fondo 0**

```asm
; entrada 0 del fondo (paleta 0, color 0)
    LDA #0   : STA $D813      ; puntero = entrada 0
    LDA #$00 : STA $D814      ; color[7:0]
    LDA #$00 : STA $D815      ; color[11:8] -> escribe, ptr=1
; entrada 1 (paleta 0, color 1)
    LDA #$0A : STA $D814
    LDA #$00 : STA $D815      ; color=00A (azul), ptr=2
; entrada 2 (paleta 0, color 2)
    LDA #$CF : STA $D814
    LDA #$00 : STA $D815      ; color=0CF (cian), ptr=3
; entrada 3 (paleta 0, color 3)
    LDA #$FF : STA $D814
    LDA #$0F : STA $D815      ; color=FFF (blanco), ptr=4
```

**Ejemplo: un color de SPRITE (paleta 1, color 2 = entrada 22)**

```asm
    LDA #22  : STA $D813      ; entrada 22 (sprite)
    LDA #$F0 : STA $D814
    LDA #$0F : STA $D815      ; color=FF0 (amarillo)
```

> **Al arrancar**, las paletas valen lo de §4.1/§4.2 (hardware). Si no escribes nada,
> todo funciona como siempre.

> ⚠️ **Escribe la paleta en VBLANK** si cambias muchos colores: aunque el motor aplica
> el color al vuelo, hacerlo a mitad de frame puede mostrar una franja con el color
> viejo y otra con el nuevo.

---

## 5. El fondo: tiles y tilemap

### 5.1 Concepto

- **Patrón de tile:** los 8 bytes (×2 planos) que definen el dibujo de un tile 8×8.
- **Tilemap:** 64×32 celdas; cada celda guarda el **índice de patrón** (0-255).
- **Atributos:** 64×32 bytes; cada uno = paleta + flips + PRIO + SÓLIDO de esa celda.

El fondo se genera así: por cada celda se toma el **índice de patrón** guardado en el
tilemap y con él se busca el **dibujo (patrón)** correspondiente, que es lo que se
pinta en pantalla. El programador nunca ve ese proceso: solo escribe tilemap y patrones.

### 5.2 Escribir una celda (tilemap)

```asm
; Escribe TILE en la celda (col, fila).  celda = fila*64 + col (16 bits)
; Entrada: TILE (índice), COL, ROW
put_cell:
    JSR calc_cell          ; CUR_LO/CUR_HI = fila*64+col
    LDA CUR_LO
    STA $D800              ; addr lo
    LDA CUR_HI
    STA $D801              ; area 00 = tilemap
    LDA TILE
    STA $D802              ; dispara la escritura
    RTS

; calc_cell: CUR = ROW*64 + COL  (stride potencia de 2 -> shifts)
calc_cell:
    LDA ROW
    LSR A
    LSR A
    STA CUR_HI             ; CUR_HI = ROW >> 2
    LDA ROW
    AND #$03
    ASL A : ASL A : ASL A : ASL A : ASL A : ASL A
    STA CUR_LO             ; (ROW & 3) << 6
    LDA CUR_LO
    CLC
    ADC COL
    STA CUR_LO
    BCC cc_done
    INC CUR_HI
cc_done:
    RTS
```

### 5.3 Escribir el atributo de una celda

```asm
; put_attr: escribe el atributo de la celda ya calculada.
; Entrada: A = valor del atributo
;          CUR_LO/CUR_HI = celda (fila*64+col), p. ej. tras JSR calc_cell
; Atributo: bit7 PRIO | bit6 FLIP_Y | bit5 FLIP_X | bit4 SOLIDO | bits1:0 PALETA
put_attr:
    PHA
    LDA CUR_LO
    STA $D800
    LDA CUR_HI
    ORA #$40               ; area 01 = atributos
    STA $D801
    PLA
    STA $D802
    RTS
```

> **Ojo:** `put_attr` **no** calcula la celda. Debes llamar antes a `calc_cell`
> (o tener `CUR_LO/CUR_HI` ya puestos) para la celda correcta.

### 5.4 Cargar un patrón de tile (2 planos)

Un patrón ocupa `tile*8` a `tile*8+7`; se escriben **plano 0** y **plano 1**.

```asm
; Carga 8 filas del patrón TILE.  pat0/pat1 = tablas en ROM (8 bytes c/u)
load_tile:
    LDY #0
lt_loop:
    ; plano 0: area 10, pat_hi=0
    LDA #$80               ; area 10
    STA $D801
    LDA tile_dir_lo,Y      ; = (tile*8+fila) & $FF
    STA $D800
    LDA pat0,Y
    STA $D802
    ; plano 1: area 10, pat_hi=1
    LDA #$A0               ; area 10 | pat_hi
    STA $D801
    LDA tile_dir_lo,Y
    STA $D800
    LDA pat1,Y
    STA $D802
    INY
    CPY #8
    BNE lt_loop
    RTS
```

> **2bpp planar:** el color de cada píxel = `(bit del plano1, bit del plano0)`.
> Ej.: sólo plano 0 = color 1; sólo plano 1 = color 2; ambos = color 3; ninguno = 0.

---

## 6. Sprites (OAM)

### 6.0 Banco de patrones de sprite

Los sprites usan un **banco aparte** del de fondo. Su tamaño y direccionamiento:

| Aspecto | Valor |
|---------|-------|
| Patrones de sprite | **64** (índices 0..63) |
| Formato | 2bpp planar, igual que el fondo (8×8 = 8 filas × 2 planos) |
| Palabras de 16 bits | **512** (0..511) |
| Dirección de una fila | `patron*8 + fila` → 0..511 |

> **Terminología (importante):** distinguir dos cosas que suenan parecido:
> - **índice de patrón de sprite**: qué dibujo usar (0..63). Es el valor que se
>   escribe en el campo **`TILE`** del OAM (offset `+2`, ver §6.1).
> - **campo `TILE` del OAM**: el byte que guarda ese índice. Rango **0..63**.
>
> En esta sección, `patron` = índice de patrón (0..63), equivalente al valor del
> campo `TILE` del OAM.

**Cómo se direcciona `patron*8+fila` en el banco:**

- `$D800` = `(patron*8 + fila) & $FF` (bits 7:0)
- `$D801` = área `11` + bit3=1 + `pat_hi` + `dir_alta` (`$C8` = plano 0, `$E8` = plano 1;
  ver §2.2 y Apéndice A.1).

> **Dirección con `dir_alta`:** el banco tiene 512 palabras, así que `patron*8+fila`
> llega hasta 511 (9 bits). Para `patron*8+fila <= 255` basta `$D800`; para valores
> mayores (índice de patrón ≥ 32) los bits altos van en los bits 2:0 de `$D801`:
> `$D800 = dir & $FF` y `$D801 = $C8/$E8 | (dir >> 8)`. Ejemplos:
> - patrón 5, fila 3 → dir 43 → `$D800=$2B`, `$D801=$C8` (plano 0)
> - patrón 40, fila 0 → dir 320 → `$D800=$40`, `$D801=$C9` (plano 0, dir_alta=1)

> ⚠️ **Rango del campo `TILE` del OAM:** el byte `TILE` (+2) es de **6 bits (0..63)**
> y el banco tiene **64 patrones**. Usa **TILE = 0..63** para seleccionar el patrón.

### 6.1 Estructura (5 bytes por sprite)

| Offset | Campo |
|--------|-------|
| +0 | `X` (bits 0-7 de la X de 9 bits) |
| +1 | `Y` (0-255). **Y ≥ 248 = deshabilitado** |
| +2 | `TILE` (**0-63**; 6 bits, ver §6.0) |
| +3 | `FLAGS` (incluye el bit 8 de X) |
| +4 | `COLL_POINT` |

**FLAGS:** `bit7 FLIP_Y | bit6 FLIP_X | bit5 PRIO | bit4 SCALE2X | bit2 X_bit8 | bits1:0 PALETA`

- `PRIO=1` → sprite **detrás** del fondo (sólo se ve en huecos del fondo).
- `PRIO=0` → sprite **delante** del fondo.
- `SCALE2X=1` → sprite dibujado al doble (16×16 en pantalla).
- **`X_bit8` (bit 2)** = bit 8 de la coordenada X → permite X de **0 a 511**
  (toda la pantalla, 0-319). Ver §6.2.
- Prioridad entre sprites: **menor índice de OAM gana**.

**Dirección del byte en el OAM:** `sprite*5 + offset` (sprite 0..31 → byte 0..159).

### 6.2 Coordenada X de 9 bits

La pantalla tiene 320 px de ancho, pero el byte X del OAM es de 8 bits (0-255).
Para que un sprite llegue a la **mitad derecha** (X 256-319), la X es de **9 bits**:

- **Bits 7:0** → en el byte `X` (+0) del OAM.
- **Bit 8** → en el **bit 2 de FLAGS**.

Ejemplos:

| X deseada | byte X (+0) | FLAGS bit 2 |
|-----------|-------------|-------------|
| 4 | `$04` | 0 |
| 200 | `$C8` | 0 |
| 255 | `$FF` | 0 |
| 256 | `$00` | 1 |
| 312 | `$38` | 1 |

(El bit 2 de FLAGS se combina con la paleta y demás flags; p. ej. paleta 0 + X>255 → FLAGS = `$04`.)

```asm
; mover un sprite 1 px a la derecha con X de 9 bits (XLO = byte X, XHI = bit 8)
    INC XLO
    BNE .lim
    INC XHI           ; acarreo 255->256 -> bit 8
.lim:
    ; escribir byte X y FLAGS (bit2 = XHI)
    LDA XLO : STA $D802   ; (tras poner addr al byte X)
    ; FLAGS = (XHI<<2) | flags/paleta
```

### 6.3 Escribir un campo del OAM

El byte de un campo del sprite `SPR` está en `SPR*5 + FIELD`. Para multiplicar por 5
sin instrucción de multiplicar, mantén una tabla `SPR_BASE` en ROM con
`[0, 5, 10, 15, ..., 155]` y usa `LDA SPR_BASE,X` (una entrada por sprite).

```asm
; oam_put: escribe DATA en el campo FIELD del sprite SPR.
; Entrada: A = DATA, X = SPR (0-31), Y = FIELD (0-4)
; Requiere: tabla SPR_BASE con SPR*5 para cada sprite.
oam_put:
    PHA                    ; guardar DATA
    LDA SPR_BASE,X         ; byte base = SPR*5
    STA BIDX
    TYA                    ; FIELD
    CLC
    ADC BIDX               ; byte = SPR*5 + FIELD
    STA BIDX
    LDA BIDX
    STA $D800              ; direccion baja = byte del OAM
    LDA #$C0               ; area 11, bit3=0 -> OAM; dir alta = 0
    STA $D801
    PLA                    ; recuperar DATA
    STA $D802              ; dispara la escritura
    RTS
```

> El OAM tiene 160 bytes (32 sprites × 5). Todos caben en `$D800` con la dirección
> alta en 0, por eso `$D801` es siempre `$C0`.

### 6.4 Definir un sprite completo (helper de juego)

```asm
; sprite_put: escribe los 5 campos del sprite SPR de una vez.
; Entrada: XSPR, YSPR, TILE, FLAGS, COLL = valores de los 5 campos
;          SPR = indice del sprite (0-31)
; Usar dentro del VBLANK.
sprite_put:
    LDX SPR                ; indice de sprite para oam_put
    LDA XSPR
    LDY #0                 ; campo 0 = X
    JSR oam_put
    LDA YSPR
    LDY #1                 ; campo 1 = Y
    JSR oam_put
    LDA TILE
    LDY #2                 ; campo 2 = TILE
    JSR oam_put
    LDA FLAGS
    LDY #3                 ; campo 3 = FLAGS (paleta, flips, PRIO, X_bit8)
    JSR oam_put
    LDA COLL
    LDY #4                 ; campo 4 = COLL_POINT
    JSR oam_put
    RTS
```

> `oam_put` espera: **A = dato**, **X = sprite**, **Y = campo**. `sprite_put` toma
> los datos de sus variables y los pasa uno a uno. El sprite queda **completo**
> (los 5 campos); los sprites no usados deben llevar **Y ≥ 248** para deshabilitarse.

---

## 7. Scroll

El **scroll** desplaza la cámara sobre el mapa 64×32. Es global (por banda, ver §8).

### 7.1 Registros

- `$D804/$D805` = scroll X, en **píxeles lógicos**: 0..511 (bits 2:0 en `$D805`).
- `$D806/$D807` = scroll Y, en **píxeles lógicos**: 0..255 (bits 2:0 en `$D807`).

> Los registros almacenan 11 bits (0..2047), pero el mapa solo mide 64×32 tiles =
> 512×256 px. **X envuelve módulo 512 y Y módulo 256**; los bits por encima de esos
> rangos se ignoran.

### 7.2 Mover la cámara

```asm
; Avanzar la cámara 1 px a la derecha (en VBLANK)
    INC SCX_LO
    BNE sc_ok
    INC SCX_HI
sc_ok:
    LDA SCX_LO
    STA $D804
    LDA SCX_HI
    STA $D805
```

- **Envoltura:** el mapa envuelve en X **módulo 512 px (64 tiles)** y en Y
  **módulo 256 px (32 tiles)**. Al salir por un borde, reaparece por el otro.
- **Velocidad:** cambia `scroll` una vez por frame (en VBLANK) para 1 px/frame máx.

> ⚠️ **Rellena todo el mundo:** las celdas que el scroll pueda alcanzar
> (columnas 0-63, filas 0-31) deben tener contenido, o se verá basura.

---

## 8. Split de raster (bandas / HUD)

Divide la pantalla en **hasta 3 bandas verticales** con scroll independiente.
Ideal para **HUD fijo** arriba y/o abajo.

### 8.1 Registros

| Dir | Registro | Significado |
|-----|----------|-------------|
| `$D809` | `RASTER_LINE0` | **Primera línea (en píxeles) NO incluida en la banda superior.** `$FF` = sin banda superior |
| `$D80A/$D80B` | `BAND2_X` lo/hi | Scroll X de la banda superior |
| `$D80C/$D80D` | `BAND2_Y` lo/hi | Scroll Y de la banda superior |
| `$D80E` | `RASTER_LINE1` | **Primera línea (en píxeles) NO incluida en la banda media.** `$FF` = sin banda inferior |
| `$D80F/$D810` | `BAND3_X` lo/hi | Scroll X de la banda inferior |
| `$D811/$D812` | `BAND3_Y` lo/hi | Scroll Y de la banda inferior |

- `RASTER_LINE0`/`RASTER_LINE1` están en **píxeles** (0-239), no en filas de tile.
- **Banda superior** = líneas `0 .. RASTER_LINE0-1` → usa `BAND2_*`.
- **Banda media** = líneas `RASTER_LINE0 .. RASTER_LINE1-1` → usa scroll normal (`$D804`).
- **Banda inferior** = líneas `RASTER_LINE1 .. 239` → usa `BAND3_*`.

**`BAND2_X`/`BAND3_X`/`_Y` funcionan igual que el scroll global** `$D804/$D805`
(bits 2:0 en el registro `_HI`), pero recuerda que **BAND2_X lo/hi ocupan `$D80A`/`$D80B`**
y **BAND3_X lo/hi ocupan `$D80F`/`$D810`** (ver la tabla de registros de §2.1).

**¿Qué pasa si desactivas una banda (`$FF`)?** Hay 4 casos:

| `RASTER_LINE0` | `RASTER_LINE1` | Resultado |
|----------------|----------------|-----------|
| `$FF` | `$FF` | Toda la pantalla usa el **scroll global** (`$D804`). Nada de bandas |
| valor | `$FF` | Arriba `BAND2_*`, el resto **scroll global** (no hay banda inferior) |
| `$FF` | valor | Arriba+medio **scroll global**, abajo `BAND3_*` (no hay banda superior) |
| valor | valor | Arriba `BAND2_*`, medio **scroll global**, abajo `BAND3_*` |

> Es decir: la **banda media siempre existe** y usa el scroll global. Si desactivas
> la superior, la media empieza desde la línea 0. Si desactivas la inferior, la media
> llega hasta abajo. `BAND2_*` solo se usa si `RASTER_LINE0` no es `$FF`.

> El corte entre bandas ocurre en un **límite de fila de tile** (el valor de
> `RASTER_LINE` se redondea hacia abajo a la fila de 8 píxeles correspondiente).
> Por eso conviene fijar `RASTER_LINE` en un **múltiplo de 8**. `$FF` (`255`) = banda
> desactivada.

### 8.2 Ejemplo: HUD fijo arriba y abajo, juego al medio

```asm
; En el arranque:
    LDA #24                ; banda superior = 3 filas de tiles (0 margen + 2 HUD)
    STA $D809
    LDA #216               ; banda inferior empieza en linea 216 (filas 27..28 utiles)
    STA $D80E
    ; HUD fijo: scroll 0 en ambas bandas
    LDA #0
    STA $D80A : STA $D80B : STA $D80C : STA $D80D
    STA $D80F : STA $D810 : STA $D811 : STA $D812
```

- La fila de tilemap `n` empieza en la línea de píxeles `n*8`.
- Con `RASTER_LINE0=24`, la banda superior cubre las **líneas 0..23 = filas 0..2**
  (fila 0 = margen; usa filas 1-2 para el HUD).
- Con `RASTER_LINE1=216`, la banda inferior cubre las **líneas 216..239 = filas 27..29**
  (la fila 27 del mapa se solapa también con la banda media). Recuerda que la **fila 29
  puede recortarse** por overscan: para el HUD inferior usa las **filas 27-28**.

---

## 9. Modo texto

No es un modo aparte: es **el mismo sistema de tiles, con una fuente cargada como
dibujos**. Escribes el **código ASCII** como índice de tile en el tilemap y aparece
el carácter correspondiente.

- Fuente: charset del C64 (96 caracteres, `$20`-`$7F`), 8×8, 1bpp expandido a 2bpp.
- **`tile = ASCII`** → aparece el glifo. No tienes que dibujar las letras: basta
  con escribir el código ASCII como índice de patrón.
- Usa la **paleta 0** del fondo (color 3 = tinta = blanco).
- **Rango útil:** `$20` (espacio) a `$7F`. Mayúsculas `$41-$5A`, minúsculas `$61-$7A`
  (están remapeadas internamente), dígitos `$30-$39`, signos `$21-$3F`.

> **Reserva de tiles:** la fuente ocupa los patrones `$20`-`$7F`, que el hardware
> **ya deja cargados al arrancar**. Estos patrones **no son un rango vacío: contienen
> la fuente**.
>
> ⚠️ **No borres ni sobrescribas `$20`-`$7F`** si quieres usar texto: un "clear de
> patrones" que escriba 0 en todo el banco **borra la fuente y el texto deja de
> verse**. Si necesitas ese rango para gráficos, tendrás que recargar los glifos
> después (o renunciar al texto). Los tiles `$00`-`$1F` y `$80`-`$FF` están libres.

### 9.1 La rejilla de texto

El texto vive en el **tilemap** (64×32 celdas). Una pantalla de texto usa las
40 columnas visibles × **29 filas útiles** (filas 1..28; la 0 y la 29 se reservan
por el desfase y por overscan). La **celda del cursor** se calcula igual que
cualquier celda:

```
celda = fila * 64 + columna
```

### 9.2 Variables de la consola (zero page sugeridas)

```
CX = $40        ; columna del cursor (0..39)
CY = $41        ; fila del cursor (0..28)   ; la fila 29 puede recortarse (overscan)
CTMP = $42      ; temporal
```

### 9.3 `put_xy`: escribir un carácter en una posición

```asm
; A = caracter ASCII, CX = columna, CY = fila
put_xy:
    STA CTMP
    ; celda = CY*64 + CX  (stride 64 = shift)
    LDA CY
    LSR A : LSR A : STA $18      ; celda_hi = CY >> 2
    LDA CY
    AND #$03
    ASL A : ASL A : ASL A : ASL A : ASL A : ASL A
    ORA CX                        ; | CX (0..39, cabe en 6 bits)
    STA $17                       ; celda_lo
    ; escribir en el tilemap (area 00)
    LDA $17 : STA $D800
    LDA $18 : STA $D801
    LDA CTMP : STA $D802          ; tile = ASCII -> glifo
    RTS
```

### 9.4 `put_char`: carácter en el cursor y avanzar

```asm
; A = caracter ASCII. Avanza el cursor; al final de linea, salta.
; Usa CX/CY como posicion del cursor (ver §9.2).
put_char:
    CMP #$0D               ; CR = fin de linea
    BEQ .cr
    JSR put_xy             ; dibuja A en (CX,CY); put_xy conserva A
    ; avanzar columna
    INC CX
    LDA CX
    CMP #40                ; 40 columnas
    BCC .fin
    LDA #0 : STA CX        ; nueva linea
    INC CY
.fin:
    RTS
.cr:
    LDA #0 : STA CX
    INC CY
    RTS
```

### 9.5 `put_str`: imprimir una cadena

```asm
; X = indice en la cadena (terminada en 0)
put_str:
    LDA cadena,X
    BEQ .fin
    JSR put_char
    INX
    JMP put_str
.fin:
    RTS
```

### 9.6 Borrar pantalla (llenar con espacios)

La pantalla visible son 40 columnas × 29 filas útiles (filas 1..28), pero el mapa
mide 64 de ancho. Por eso NO vale con rellenar celdas seguidas: hay que recorrer
**fila por fila** (40 celdas en cada una) saltando el resto de la línea del mapa.

```asm
; Rellena las 40x29 celdas utiles (filas 1..28) con espacio ($20).
;   celda(fila,col) = fila*64 + col
;   $D800 = celda_lo = ((fila & 3) << 6) | col
;   $D801 = celda_hi = fila >> 2          (area 00)
; FROW = fila actual (empieza en 1).  Y = columna actual (0..39)
clear_screen:
    LDA #1
    STA FROW              ; fila actual = 1
    LDX #28               ; 28 filas (1..28)
clr_row:
    LDY #0                ; columna visible 0..39
clr_col:
    ; $D801 = fila >> 2  (calcular desde FROW)
    LDA FROW
    LSR A : LSR A
    STA $D801             ; area 00 | (fila >> 2)
    ; $D800 = ((fila & 3) << 6) | col   (col = Y)
    LDA FROW
    AND #$03
    ASL A : ASL A : ASL A : ASL A : ASL A : ASL A
    STY CTMP              ; guardar Y (TYA borraria el resultado)
    ORA CTMP              ; ORA col
    STA $D800
    LDA #$20              ; espacio
    STA $D802             ; dispara la escritura
    INY
    CPY #40
    BNE clr_col
    INC FROW              ; siguiente fila
    DEX
    BNE clr_row
    RTS
```

> **Cálculo de la dirección de celda:** la fila `f` empieza en `celda = f*64`.
> El byte bajo para col 0 es `((f & 3) << 6)` y el bit alto (`f >> 2`) va en `$D801`.
> Esta versión recalcula ambos bytes desde `f` y `col` en cada celda (sin llevar
> acarreo), igual que `put_xy` (§9.3). Requiere las variables `FROW` y un temporal.

> **Nota:** el puerto indirecto no auto-incrementa; hay que reescribir `$D800/$D801`
> antes de cada `$D802`. El ejemplo lo hace en cada iteración.

### 9.7 Scroll de texto (subir una línea)

Cuando el cursor pasa de la última fila, desplaza todo el texto una fila hacia
arriba y deja la última línea en blanco. Se hace **copiando el tilemap** desde RAM
(o releyendo si tuvieras lectura de VRAM; hoy se mantiene **una copia en RAM**):

```asm
; Idea: el juego mantiene el texto en una RAM de 40x29 (1160 bytes) para las filas
; utiles 1..28. scroll_text: copia fila N+1 sobre fila N, y limpia la fila 28.
; Luego reescribe las celdas de VRAM que cambiaron (o toda la pantalla).
```

> **Recomendación:** la memoria de vídeo es de **solo escritura** (no se puede leer
> de vuelta), así que la consola mantiene su propia **copia del texto en RAM**
> (1160 B para 29 filas) y la vuelca a la VRAM por bloques. Es lo estándar.

### 9.8 HUD de texto con split de raster

Para un marcador **fijo** (score, vidas) sobre el juego, usa el **split de raster**
(§8): reserva una banda (p. ej. la superior) con **scroll 0** y escribe el texto en
las filas del tilemap que caen en esa banda. El texto queda fijo mientras el juego
scrollea.

```asm
; banda superior de 3 filas (raster_line0 = 24): filas 1-2 para el HUD
; escribir "SCORE 000000" en la fila 1, columnas 2..
```

### 9.9 Ejemplo completo: pantalla de título

```asm
start_text:
    JSR clear_screen
    LDA #10 : STA CX       ; columna 10
    LDA #5  : STA CY       ; fila 5
    LDX #0
    JSR put_str_title
    RTS

title:  .byte "MI JUEGO", $0D
        .byte "PULSA FIRE", 0
```

### 9.10 Colores del texto

El color lo da la **paleta de la celda** (bits 1:0 del atributo de esa celda), no el
tilemap. Para cambiar el color de un texto, escribe el **atributo** de la celda:

```asm
; poner el texto de la celda (CX,CY) en paleta P (0..3)
; (usar la misma celda; area 01 = atributos)
    ; celda = CY*64 + CX  -> $17/$18
    LDA $17 : STA $D800
    LDA $18 : STA $D801    ; OJO: aqui area 00; para atributo, ORA #$40 en $18
    ; ... ver put_attr del manual §5.3
```

### 9.11 Resumen de la fuente

| Rango ASCII | Contenido |
|-------------|-----------|
| `$20` | espacio |
| `$21`-`$2F` | signos (`!"#$%&'()*+,-./`) |
| `$30`-`$39` | dígitos `0`-`9` |
| `$3A`-`$40` | signos (`:;<=>?@`) |
| `$41`-`$5A` | mayúsculas `A`-`Z` |
| `$5B`-`$60` | signos (`[\]^_` + backtick) |
| `$61`-`$7A` | minúsculas `a`-`z` |
| `$7B`-`$7F` | `{|}~` (algunos en blanco) |

> `CR` (`$0D`) se usa como **fin de línea** en las rutinas de consola (no es un glifo).

---

## 10. Colisión sprite↔tile (sólidos)

### 10.1 Concepto

- Marca celdas del fondo como **sólidas**: bit 4 del **atributo** de la celda.
- El hardware comprueba el **COLL_POINT** de cada sprite **una vez por frame,
  al entrar en VBLANK** (barrido completo del OAM), y pone el flag `SOLID_HIT`
  (bit 5 de `$D803`) si **algún** sprite toca una celda sólida.
- `SOLID_HIT` **no es un evento pegajoso**: refleja la situación del **frame en
  curso**. Se vuelve a evaluar cada frame (con los `COLL_POINT` y el OAM del momento)
  y el resultado se mantiene hasta el siguiente frame. Si quieres construir una copia
  en RAM del OAM, **escríbelo en el mismo VBLANK**.
- **Leer `$D803` no limpia `SOLID_HIT`**; el bit refleja el frame en curso hasta el
  siguiente VBLANK.

### 10.2 Marcar una celda sólida

```asm
; Marcar la celda (ROW,COL) como solida (bit 4 del atributo)
    JSR calc_cell
    LDA CUR_LO
    STA $D800
    LDA CUR_HI
    ORA #$40               ; area 01 = atributos
    STA $D801
    LDA #$10               ; bit 4 = SOLIDO
    STA $D802
```

### 10.3 Punto de colisión (COLL_POINT)

- `COLL_POINT`: `bits 2:0 = dx`, `bits 5:3 = dy` (**offset libre 0-7**).
- **Auto-escala (en hardware):** **siempre escribes dx/dy en escala 0-7** (como si el
  sprite fuera 1×). Si el sprite tiene `SCALE2X`, el hardware **escala el punto
  automáticamente**: `dx 0..6 → dx*2` y `dx=7 → 15`. El `7 → 15` (en vez de `14`) es
  para que el valor máximo cubra el **borde exterior** del sprite 2× (que ocupa 16 px,
  índices 0-15). La misma regla aplica a `dy`. Así **no tienes que saber la escala**:
  los mismos valores dan los mismos puntos lógicos.

| Punto | Byte | 1× alcanza (píxel) | 2× alcanza (píxel) |
|-------|------|--------------------|--------------------|
| centro | `$24` | 4 | 8 |
| pie | `$3C` | 7 | 15 |
| cabeza | `$04` | 0 | 0 |
| borde izq | `$20` | 0 | 0 |
| borde der | `$27` | 7 | 15 |

> **En 1×** los índices son **píxeles** dentro del sprite 8×8: `dx=0` es el primer
> píxel (columna 0) y `dx=7` es el **último píxel** (columna 7), no un punto fuera del
> sprite. El rectángulo del sprite ocupa los píxeles 0-7.
>
> **En 2×** el sprite ocupa 16 px (0-15). Por eso `dx=7 → 15` es el **último píxel**
> del sprite ampliado, y los valores intermedios dan posiciones **pares** (`dx*2`).
> Al escribir siempre dx/dy en escala 0-7 obtienes el mismo punto lógico en ambas
> escalas; el hardware hace la conversión.

### 10.4 Patrón de uso (varios sprites: deducción en software)

El flag `SOLID_HIT` es **global** (no dice **qué** sprite chocó). Si tienes varios
objetos, cada uno con su posición en RAM, **deduces cuál chocó** comparando su borde
(el que avanza) contra la columna/pared:

```asm
; cada frame, en VBLANK:
;   1) ajustar COLL_POINT segun direccion (borde que avanza)
;   2) LEER SOLID_HIT; si activo, deducir QUE sprite choco comparando su borde
;      con la posicion de la pared, y rebotar SOLO ese.
    LDA $D803
    AND #$20               ; SOLID_HIT
    BEQ no_hay
    ; --- deducir sprite 0 ---
    LDA DIR0
    BNE s0_izq
    LDA X0 : CLC : ADC #7  ; borde derecho
    CMP PARED_X_IZQ        ; ¿llego a la pared?
    BCC s0_no
    LDA DIR0 : EOR #$01 : STA DIR0
s0_no:
    ; (repetir para el sprite 1 con su borde = X+15 si es 2x)
no_hay:
```

> **Regla:** el hardware dice **"alguien chocó"**; el software decide **"quién"**.
> Para 1 solo objeto colisionador, la bandera basta sin deducción.

> **Latencia:** 1 frame. El barrido de colisión se evalúa al **entrar en VBLANK**;
> si el juego mueve el sprite y reescribe el OAM en *el mismo* VBLANK, el `SOLID_HIT`
> que leas corresponde a la posición que tenía el OAM **al final del frame anterior**.
> En la práctica basta con mover, reescribir OAM, leer `SOLID_HIT` y rebotar en el
> mismo VBLANK. Al chocar el sprite puede "pasarse" ~1 px; compénsalo con push-out
> si lo necesitas.

### 10.5 Colisión sprite↔sprite → SOFTWARE

**No hay colisión sprite↔sprite en hardware.** Se hace en software comparando las
cajas (AABB) de los objetos, cuyas posiciones ya tienes en RAM:

```asm
; ¿colisionan el sprite A (xa,ya) y el B (xb,yb)?  (8x8)
    LDA xa : SEC : SBC xb
    ; |dx| < 8 ?  y  |dy| < 8 ?  -> colision
```

---

## 11. STATUS: VBLANK y sincronización

`$D803` (lectura):

| Bit | Nombre | Significado |
|-----|--------|-------------|
| 7 | `VBLANK` | 1 = fuera de la zona visible (seguro escribir VRAM/OAM) |
| 6 | `OVERFLOW` | 1 = hubo más de 8 sprites en alguna línea del último frame. No se limpia al leer |
| 5 | `SOLID_HIT` | 1 = algún sprite tocó un tile sólido este frame. No se limpia al leer |
| 4 | `VIDEO_READY` | 1 = inicialización de VRAM terminada (esperar al arrancar) |

> **`OVERFLOW` (detalle):** el hardware puede dibujar como máximo **8 sprites por
> línea**. Si en una línea hay más de 8, se dibujan los primeros y **se descartan los
> demás de esa línea**; el flag se pone a 1. No indica *cuál* se descartó.

> ⚠️ **Leer `$D803` no limpia `OVERFLOW` ni `SOLID_HIT`.** Ambos reflejan el estado
> del frame en curso:
> - **`SOLID_HIT`**: se vuelve a evaluar al comenzar cada VBLANK y **se mantiene
>   durante todo el frame**, hasta el siguiente VBLANK.
> - **`OVERFLOW`**: refleja si hubo desbordamiento en el frame.
>
> Por tanto, simplemente revisa el bit que te interesa. Como son varios bits en un
> solo registro, **guarda el byte en RAM una vez por frame** si necesitas consultar
> más de uno.

### 11.1 Esperar VIDEO_READY al arrancar

```asm
wait_ready:
    LDA $D803
    AND #$10
    BEQ wait_ready
```

### 11.2 Sincronizar con el frame (VBLANK)

**Regla de oro:** mueve sprites, escribe OAM y scroll **durante el VBLANK**.

El bucle de espera del VBLANK lee `$D803` repetidamente; como esos flags **no se
limpian al leer**, no se pierden. Aun así, para `SOLID_HIT` recuerda que su valor
corresponde al **barrido de colisión de ese VBLANK** (mira §10.1).

```asm
; una iteración = un frame
frame:
wait_vb:
    LDA $D803
    AND #$80
    BEQ wait_vb            ; espera entrar en VBLANK
    ; ---- aquí actualizas todo (OAM, scroll, paletas) ----
    JSR update_game
wait_vb_end:
    LDA $D803
    AND #$80
    BNE wait_vb_end        ; espera salir del VBLANK
    JMP frame
```

> Escribir el OAM a mitad del frame visible **parte el sprite** (unas líneas con
> el valor viejo y otras con el nuevo). Hazlo siempre en VBLANK.

**¿Qué se puede escribir fuera de VBLANK?**

| Registro | ¿Fuera de VBLANK? | Nota |
|----------|-------------------|------|
| `$D800/$D801/$D802` (VRAM, OAM, patrones) | ❌ **No** | Puede "partir" sprites y producir rasgado. Solo en VBLANK |
| `$D804-$D807` (scroll) | ⚠️ Idealmente no | Cambia a mitad de frame = salto de cámara |
| `$D809-$D812` (bandas/raster) | ⚠️ Idealmente no | Igual; normalmente se fijan una vez al arrancar |
| `$D808` (`MAP_STRIDE`) | — | Reservado; no escribir |
| `$D803` | solo lectura | `VBLANK`/`VIDEO_READY` combinacionales; `OVERFLOW`/`SOLID_HIT` del frame |

---

## 12. Recetas completas

### 12.1 Dibujar un fondo de un color (lleno)

> ⚠️ **El puerto indirecto NO auto-incrementa.** Cada escritura a `$D802` usa la
dirección que haya en `$D800`/`$D801`. Para recorrer varias celdas hay que
**reescribir `$D800`/`$D801` antes de cada `$D802`**.

```asm
; Rellena las 64*32=2048 celdas del mapa con el tile TILE
;   CUR_LO/CUR_HI = celda actual (0..2047)
    LDA #0 : STA CUR_LO : STA CUR_HI
    LDY #8                 ; 8 bloques de 256 celdas = 2048
blk:
    LDX #0
inner:
    LDA CUR_LO : STA $D800
    LDA CUR_HI : STA $D801   ; area 00 -> tilemap
    LDA #TILE  : STA $D802   ; dato (dispara la escritura)
    INC CUR_LO
    BNE inner_n
    INC CUR_HI
inner_n:
    DEX
    BNE inner
    DEY
    BNE blk
```

> **Nota de rendimiento:** el acceso a la memoria de vídeo es secuencial (un dato
> por escritura, sin auto-incremento). Si necesitas velocidad, mantén los búferes en
> RAM y vuelca solo lo que cambie cada frame.

> **Estado inicial del vídeo:** tras `VIDEO_READY`, la memoria de vídeo ya viene
> inicializada: **tilemap = `$20` (espacio)**, **atributos = 0**, los primeros
> patrones en blanco y la **fuente ya cargada** en `$20`-`$7F`. Escribir tu propio
> mundo desde cero es posible, pero recuerda que **`$20`-`$7F` contiene la fuente**
> (no la sobrescribas si usas texto).

### 12.2 Sprite que se mueve y rebota en los bordes

```asm
; en VBLANK: mover X, rebotar en 0 y 312
    LDA SPDIR
    BNE .left
    INC SPX
    LDA SPX
    CMP #232
    BCC .wr
    LDA #1
    STA SPDIR
    JMP .wr
.left:
    DEC SPX
    LDA SPX
    CMP #8
    BCS .wr
    LDA #0
    STA SPDIR
.wr:
    LDX #0                 ; sprite 0
    LDA SPX
    LDY #0                 ; campo X
    JSR oam_put
```

### 12.3 Un objeto 16×16 (4 sprites) a 1×

```asm
; cuadrantes A=0,B=1,C=2,D=3; posicion base (BX,BY)
;  A (BX, BY)     B (BX+8, BY)
;  C (BX, BY+8)   D (BX+8, BY+8)
    ; sprite 0 = A
    ... oam_put(sprites 0..3, X=BX/BX+8, Y=BY/BY+8, TILE=0..3, FLAGS=paleta)
```

### 12.4 Un objeto 16×16 a 2× (32×32)

Igual que 12.3 pero cada sprite con `FLAGS |= $10` (SCALE2X) y **paso +16** entre
cuadrantes (no +8).

### 12.5 Escribir texto

Ver la **sección 9** para la librería de consola completa (`put_xy`, `put_char`,
`put_str`, `clear_screen`, scroll). Resumen mínimo:

```asm
; Imprime "HOLA" en la fila CY, col CX (tile = ASCII)
    LDX #0
txt_loop:
    LDA msg,X
    BEQ txt_done
    JSR put_char           ; usa CX/CY; ver §9.4
    INX
    JMP txt_loop
txt_done:
    RTS
msg:
    .byte "HOLA", 0
```

---

## 13. Limitaciones y buenas prácticas

| Limitación | Valor | Nota |
|------------|-------|------|
| Sprites | 32 en OAM | Más requeriría otro hardware |
| Coordenada X | **9 bits (0-511)** | bit 8 en FLAGS(2) → cubre toda la pantalla |
| Coordenada Y | 8 bits (0-255) | pantalla 240 → sobra |
| Sprites por línea | **8** | Más → `OVERFLOW` y se pierde alguno |
| Sprites (patrones) | **64** (0..63) | 8×8, 2bpp; el campo TILE es de 6 bits (§6.0) |
| Patrones de fondo | 256 | comparte rango con la fuente (`$20`-`$7F`) |
| Paletas de fondo / sprite | 4 / 4 | cada una de 4 colores; **escribibles** por el CPU (§4.4) |
| Colores en pantalla | hasta 32 | 16 de fondo (4 paletas × 4) + 16 de sprite |
| Mapa | 64×32 | scroll con envoltura |
| Framebuffer | **no hay** | los píxeles se generan al vuelo |
| Colisión sprite↔sprite | **no hay** | hacer por software (comparar X/Y) |
| Colisión sprite↔tile | flag **global** | no dice qué sprite; deducir por software |
| COLL_POINT | dx, dy **0-7** | auto-escala a 0-15 si el sprite es 2× |
| Rotación de sprites | **no hay** | usar sprites pre-rotados |
| Overscan del monitor | variable | las filas 0 y 29 pueden recortarse según el monitor |

**Buenas prácticas:**

1. **Actualiza todo en VBLANK** (OAM, scroll, paletas). Nunca a mitad del frame.
2. **Rellena el mundo** completo (64×32) antes de hacer scroll.
3. **Espera VIDEO_READY** al arrancar antes de tocar la VRAM.
4. **No superes 8 sprites por línea** (o usa `OVERFLOW` para detectarlo).
5. **Colisión de juego = software**: guarda X/Y de los objetos y compáralas.
6. **HUD arriba:** reserva la fila 0 del tilemap (margen) y usa las filas 1+.
7. **No uses las filas 0 ni 29 para información crítica:** dependiendo del monitor
   pueden quedar fuera de pantalla (overscan). Mantén el contenido entre las filas
   **1 y 28**.
8. **Personajes grandes:** usa varios sprites (16×16 = 4) o tiles (para muchos).
9. **Muchos objetos en pantalla** (Space Invaders, etc.): usa **tiles** en el
   tilemap, no sprites.

---

## 14. Esqueleto de juego

```asm
; ============================================
; Plantilla de juego minimo
; ============================================
    .setcpu "6502"

VID_LO = $D800
VID_HI = $D801
VID_DT = $D802
VID_ST = $D803

; --- zero page ---
CUR_LO = $10
CUR_HI = $11
SPX    = $12
SPY    = $13
DIR    = $14

    .segment "CODE"
    .org $8000

reset:
    SEI
    CLD
    LDX #$FF
    TXS

    ; 1) esperar VIDEO_READY
wr:
    LDA VID_ST
    AND #$10
    BEQ wr

    ; 2) inicializar: tiles, tilemap, atributos, sprites, bandas

    ; 3) bucle principal (1 iteracion = 1 frame)
main:
wvb:
    LDA VID_ST
    AND #$80
    BEQ wvb                ; entrar en VBLANK
    JSR update             ; mover objetos, escribir OAM, scroll
wvbe:
    LDA VID_ST
    AND #$80
    BNE wvbe               ; salir de VBLANK
    JMP main

; --- actualizacion por frame ---
update:
    ; mover jugador (leer joy/teclado), mover balas, colisiones,
    ; animar tiles, actualizar HUD, scroll
    RTS

    .org $BFFA
    .word $8000
    .word reset
    .word $8000
```

---

## Apéndice A — Chuletas de referencia

### A.1 Áreas de `$D801`

| `$D801` | Escribe en |
|---------|-----------|
| `$00` | tilemap |
| `$40` | atributos |
| `$80` | patrón fondo plano 0 |
| `$A0` | patrón fondo plano 1 |
| `$C0` | OAM |
| `$C8` | patrón sprite plano 0 |
| `$E8` | patrón sprite plano 1 |

(`$C8`/`$E8` = `$C0`/`$E0` con bit 3 = 1; la dirección alta del patrón va en bits 2:0.)

### A.2 Bits del atributo del fondo y FLAGS del sprite

**Atributo del fondo** (un byte por celda, área `01`):

```
bit7 PRIO | bit6 FLIP_Y | bit5 FLIP_X | bit4 SOLIDO | bit3 rsv | bits1:0 PALETA
```

**FLAGS del sprite** (byte +3 del OAM):

```
bit7 FLIP_Y | bit6 FLIP_X | bit5 PRIO | bit4 SCALE2X | bit3 rsv | bit2 X_bit8 | bits1:0 PALETA
```

### A.3 Índice de color

```
indice = (paleta << 2) | color           (paleta 0-3, color 0-3 -> 0-15)
```

### A.4 Fórmulas útiles

```
celda      = y_tile*64 + x_tile          (tilemap/atributo)
$D800      = celda & $FF
$D801      = area | ((celda >> 8) & $07)
dir_patron = tile*8 + fila               (patrón de fondo)
dir_spr    = patron*8 + fila             (patrón de sprite)
byte_oam   = sprite*5 + campo            (campo 0..4)
```

---

*Fin del manual.*
