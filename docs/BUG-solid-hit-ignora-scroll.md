# Bug: el barrido de colisión sprite↔tile ignora el scroll

**Estado:** abierto — pendiente de analizar en el proyecto del core
**Detectado en:** `fpga-6502-16k` (manual v2.8, `6502_board_v3`)
**Encontrado por:** biblioteca `videocore-6502-cc65` mientras se validaba la demo
en el emulador (`EmuladorFpga6502`).
**Severidad:** media. No corrompe nada, pero hace que `SOLID_HIT` sea
**inutilizable con scroll activo**, que es el caso de cualquier juego con cámara.

---

## Resumen

El **dibujado** aplica el scroll al calcular qué celda del tilemap se pinta en
cada píxel de pantalla. El **barrido de colisión** **no** lo aplica: usa la
posición del sprite como si el scroll fuera 0.

Consecuencia: un sprite puede colisionar con una celda sólida **que no está
debajo de él en pantalla**, y no colisionar con la que sí se ve.

---

## Evidencia en el código del emulador

El emulador (`src/video/video.cpp`) reproduce fielmente el RTL. Las dos rutas
usan fórmulas distintas:

**Dibujado** — sí aplica scroll:

```cpp
int map_x = (px + band_scroll_x) & 0x1FF; // envuelve módulo 512
int map_y = (py + band_scroll_y) & 0xFF;  // envuelve módulo 256
```

**Barrido de colisión** — no lo aplica:

```cpp
const int px = (ox + dx) & 0x1FF;   // X del sprite + COLL_POINT
const int py = (oy + dy) & 0xFF;    // Y del sprite + COLL_POINT
const int celda = ((py >> 3) & 0x1F) * MAP_W + ((px >> 3) & 0x3F);
if (attrib_[celda] & 0x10) hit = true;   // bit 4 = SOLIDO
```

El propio comentario de la función lo reconoce:

> *"NO se aplica scroll: usa la celda directa de pantalla, no la del mapa."*

---

## Cómo reproducirlo

1. Un tilemap con una superficie sólida en una fila concreta (p. ej. fila 20,
   columnas 4-27).
2. Activar scroll horizontal: `vc_set_scroll_x(n)` con `n != 0`.
3. Un sprite con `COLL_POINT` que caiga sobre esa fila.
4. Leer `$D803` (bit 5, `SOLID_HIT`).

**Observado:** el flag se activa **cuando el sprite está encima de una celda del
mapa que es sólida**, no cuando está encima de la superficie *visible*.
Desplazando el scroll, el punto de colisión y la imagen **se desincronizan**.

### Medición real (demo de la biblioteca, emulador)

Con auto-scroll activo, correlacionando el rebote con la posición **visual** de
la plataforma:

```
rebote en (22, 162)  →  plataforma visible en x=232..319   ← desfasado por completo
rebote en (26, 173)  →  plataforma visible en x=0..113
rebote en (12, 153)  →  plataforma visible en x=122..313
```

Con el scroll **desactivado**, los rebotes coinciden (`y≈150-154` sobre una
plataforma en `y=160-167`): 4 rebotes correctos en 20 s.

---

## Impacto

- **`SOLID_HIT` no sirve con scroll.** Todo juego con cámara (el caso normal de
  scroll) ve falseada la colisión sprite↔tile del hardware.
- Coincide con la observación de que **todos los juegos reales** de
  `juegos_6502/` (Arkanoid, Pacman, snake, Space Invaders, Tetris, RiverRaid)
  hacen la colisión sprite↔tile **por software** y **ninguno** usa `vc_solid_hit()`.
- El modo sin scroll (pantalla fija) **funciona bien**: ahí scroll = 0 y ambas
  fórmulas coinciden.

---

## Posibles correcciones (a valorar en el core)

1. **Aplicar el scroll en el barrido**: sumar `scroll_x`/`scroll_y` (por banda,
   si el sprite está en una banda concreta) antes de calcular la celda.
   - Complica el barrido: hay que saber a qué banda pertenece el sprite según su
     `Y` y usar el scroll de esa banda.
2. **Documentar la limitación** y dejarlo como está, indicando que
   `COLL_POINT`/`SOLID_HIT` son solo para pantalla fija.
3. **Ofrecer ambos modos** con un bit de configuración (p. ej. en un registro
   libre): "colisión con scroll / sin scroll".

---

## Documentación a actualizar

El manual (§10.1 y §10.3) describe la falta de recorte a pantalla y el
desbordamiento de `Y`, pero **no menciona que el scroll se ignora**. Habría que
añadir un aviso explícito, independientemente de si se corrige el RTL.

---

## Notas

- **No es un bug del emulador**: reproduce el RTL, y su propia prueba
  (`tools/test_video.cpp`) cubre la colisión sin scroll.
- **No es un bug de la biblioteca**: `vc_solid_hit()` solo expone el registro.
- La biblioteca debería **advertir** en su documentación que no use `SOLID_HIT`
  cuando haya scroll.
