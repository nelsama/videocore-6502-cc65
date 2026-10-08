# Changelog

Historial de cambios y correcciones de la biblioteca `vc`.

El formato sigue, de forma ligera, [Keep a Changelog](https://keepachangelog.com/).
La documentación de uso está en [`VIDEO-LIB.md`](VIDEO-LIB.md); este archivo es
solo el registro de cambios.

---

## [Sin publicar]

> **Alineado con:** Manual de Programación del Core de Vídeo **v2.8**
> (hardware de referencia `6502_board_v3`, con **setup de VRAM por hardware**). Ver
> [`07-MANUAL-PROGRAMACION.md`](07-MANUAL-PROGRAMACION.md). Al recompilar el core,
> revisa esa versión antes de dar por buena esta de la librería.

### Añadido

- **Setup de VRAM por hardware (`$D816`/`$D817`).** El core puede limpiar la VRAM y
  recargar la fuente en hardware (~120-275 µs). Nueva función:
  - `vc_wait_setup()` — espera a que termine el setup (`BUSY=0`).
  - Macros `VID_SETUP` / `VID_SETUP_ST` y constante `VC_SETUP_BUSY` en `video.h`.

- **Guía de conceptos (`docs/CONCEPTOS-JUEGO.md`).** Documento introductorio para quien
  sabe 6502 pero no conoce los gráficos de 8 bits: tile, patrón, tilemap, atributos,
  sprite, OAM, scroll, bandas (split de raster), paletas, VBLANK, colisiones, modo
  texto y el esqueleto de un juego. Enlazado desde el `README.md`. `VIDEO-LIB.md` queda
  como referencia de API.

- **Color de fondo global (`BG_COLOR`).** El manual del core lo define como la entrada
  15 del banco de fondo, sin registro propio; hasta ahora solo se podía tocar de forma
  indirecta (`vc_pal_set_bg(3, 3, ...)`). Nuevo helper dedicado:
  - `vc_set_bgcolor(rgb444)` (`gfx.c`) — fija el color de fondo y del margen.
  - Constantes `VC_PAL_BGCOLOR` (=15) y `VC_BG_COLOR_DEFAULT` (=`$48C`) en `video.h`.
  - Los ejemplos (`demo`, `collision`, `animation`, `palette`) fijan ahora el fondo
    a `VC_BG_COLOR_DEFAULT` en su arranque, en vez de depender del valor que deje el
    hardware al inicializar la VRAM.

### Cambiado

- **`vc_clear_vram()` usa ahora el setup de VRAM por hardware.** Antes recorría las
  2048 celdas del tilemap desde el CPU (~ms); ahora dispara el setup del core
  (`$D816`) y espera a que termine (~120-275 µs). **Cambio de comportamiento:**
  - Además de tilemap/atributos/OAM, ahora **borra los patrones** de fondo y de sprite;
    el setup **recarga la fuente de texto** (`$20-$7F`) automáticamente.
  - Si el juego había dibujado tiles propios en `$20-$7F`, **se pierden** (ese rango
    vuelve a ser la fuente).
  - Mientras corre el setup, las escrituras de CPU a VRAM/OAM se ignoran;
    `vc_clear_vram()` espera a que acabe antes de retornar.
  - Los cuatro ejemplos actualizados; sus comentarios ya no asumen que los patrones
    sobreviven al clear.

- **Constantes de paleta renombradas a índices neutros (cambio incompatible).** Los
  nombres anteriores describían el color *por defecto* de cada paleta, lo que engañaba
  ahora que las paletas son reprogramables (`$D813-$D815`). Renombrados:
  - `VC_BGPAL_TEXT/TERRAIN/VEGETATION/TEXTGREEN` -> `VC_BGPAL_0/1/2/3`.
  - `VC_SPPAL_HEART/BLUE/MAGENTA/GREEN` -> `VC_SPPAL_0/1/2/3`.
  - `VC_TINTA_BLANCA`/`VC_TINTA_VERDE` -> `VC_TINTA(paleta)` (p. ej.
    `VC_TINTA(VC_BGPAL_0)`).
  - `VC_COLOR0..3` sin cambios (ya eran neutros).

  Los colores de cada paleta siguen siendo los mismos **presets por defecto**; lo que
  cambia es que el nombre ya no afirma un color concreto. Migrados los cuatro ejemplos
  y toda la documentación.

### Corregido

- **Documentación alineada con el manual del core (`6502_board_v3`).**
  - `README.md`: corregido el árbol de `examples/` (`palette/` estaba mal anidado) y
    actualizada la cabecera (plataforma Tang Nano 9K / Gowin GW1NR-9, mención a
    paletas programables).
  - `docs/VIDEO-LIB.md`: corregida la referencia a la demo (`examples/demo/main.c`,
    no `src/main.c`); el manual **sí** se incluye en el repositorio
    (`docs/07-MANUAL-PROGRAMACION.md`); añadidos `vc_set_bgcolor`, `VC_PAL_BGCOLOR`
    y `VC_BG_COLOR_DEFAULT` a la referencia rápida; añadido `src/collide.s` a la
    tabla de archivos.
  - `docs/VIDEO-LIB.md`: aclarado que el banco de paleta de **sprite** es aparte
    (16-31, manual §4.2), por lo que `BG_COLOR` (entrada 15) **no** afecta a los sprites.

- **`vc_oam_put` escribía el índice del sprite en vez del dato.**
  `vc_oam_put(spr, field, data)` guardaba `data` en `VC_TILE`, pero los dos `popa`
  posteriores (para `field` y `spr`) dejaban `A = spr`. `oam_write` volvía a hacer
  `sta VC_TILE`, **pisando el dato con `spr`**. El campo se escribía con el índice
  del sprite, no con `data`.

  - **Síntoma:** para el sprite 0, cualquier `vc_oam_put(0, VC_OAM_TILE, n)` dejaba
    `TILE = 0` → el sprite dibujaba siempre el **patrón 0**. La animación por cambio
    de `TILE` parecía no funcionar, y engañaba como si el **hardware ignorase** el
    `TILE` (se llegó a sospechar un bug del core de vídeo, que resultó **no existir**).
  - **Corrección:** restaurar `A = data` antes de saltar a `oam_write`:

    ```asm
    _vc_oam_put:
            sta VC_TILE            ; data
            jsr popa
            sta VC_FIELD
            jsr popa
            sta VC_SPRIDX
            lda VC_TILE            ; restaurar A = data
            jmp oam_write
    ```

  - **Alcance:** solo `vc_oam_put` (y quienes lo usan, p. ej. `vc_sprite_disable`).
    `vc_sprite_set` y `vc_sprite_move` **no** estaban afectados (cargan `A` con el
    dato justo antes de llamar a `oam_write`).
  - **Test de regresión:** `tests/sprite_test.s` incluye ahora casos que llaman
    `vc_oam_put(0, VC_OAM_TILE, 0x2A)` y `vc_oam_put(4, VC_OAM_TILE, 0x2A)` y
    verifican que se escribe `0x2A` y no `spr`. (Con el bug, el test falla con
    `exit=51`.)

  > **Lección:** al depurar "el hardware ignora X", comprobar **primero** que la
  > librería escribe el valor correcto. Un byte mal pasado puede parecer un bug de
  > hardware.

## [Inicial]

### Corregido

- **Convención de llamada cc65 en `video.s`.** La primera versión asumía "todo por
  el stack" y leía todos los argumentos con `popa`. `vc_fill_tilemap(0)` leía basura
  del stack (el `0` iba en A y el stack estaba vacío) y el tilemap se llenaba de
  **patrones aleatorios**. Se corrigió cada función según la convención real de cc65
  (el último argumento va en `A` / `A+X`, no en el stack). Ver `VIDEO-LIB.md` →
  "Convención de llamada cc65".

- **Dirección de patrón para tiles ≥ 32.** `vc_load_bg_pattern` calculaba
  `dir = tile*8 + fila` con solo 3 `asl` sobre el byte del tile, recogiendo un único
  bit de acarreo. Para `tile ≥ 32`, `tile*8 ≥ 256` (hacen falta 11 bits) y se
  perdían los bits 9 y 10. Ahora se calcula en 16 bits (`VC_PATHI:VC_PATDIR`) y
  `calc_pat_dir` propaga el acarreo. Cubierto por `tests/pat_base_test.s`.

### Añadido

- Tests unitarios en ensamblador con `sim65` (`tests/`): colisiones AABB, sprites
  8×8 y 16×16, dirección de patrón, y `vc_sprite_set`.
- Núcleo de colisiones AABB en ensamblador (`src/collide.s`).
- Ejemplos: `examples/demo`, `examples/collision`, `examples/animation`.
