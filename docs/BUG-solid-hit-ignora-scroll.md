# Bug (CORREGIDO): el barrido de colisión sprite↔tile ignoraba el scroll

**Estado:** ✅ **corregido en el core** (`cc645d3`, core v2.9) y reflejado en el
emulador.
**Detectado en:** `fpga-6502-16k` (manual v2.8, `6502_board_v3`)
**Encontrado por:** biblioteca `videocore-6502-cc65` al validar la demo en el
emulador (`EmuladorFpga6502`).

---

## Resumen

El **dibujado** aplicaba el scroll al calcular qué celda del tilemap se pinta en
cada píxel de pantalla. El **barrido de colisión** **no** lo aplicaba: usaba la
posición del sprite como si el scroll fuera 0.

Consecuencia: un sprite podía colisionar con una celda sólida **que no estaba
debajo de él en pantalla**, y no colisionar con la que sí se veía.

---

## Corrección

**Core** (`cc645d3`, "Aplica el scroll en la colision sprite-tile"):

> *"El barrido de colisión usaba la posición de pantalla del sprite como si fuera
> de mundo, así que con scroll activo leía una celda distinta de la que se ve bajo
> el sprite. Ahora suma `scroll_x`/`scroll_y` antes de calcular la celda, igual
> que el dibujado. Coste: +4 CLS, 0 registros nuevos."*

**Emulador** (`2182323` y siguientes): aplica el mismo cálculo.

```cpp
// antes:  celda = (px >> 3) ...
// ahora:  wx = (px + scx) & 0x7FF;
//         wy = (py + scy) & 0x7FF;
//         celda = ((wy >> 3) & 0x1F) * MAP_W + ((wx >> 3) & 0x3F);
```

El manual del core (v2.9, §10.1) ahora lo documenta explícitamente.

---

## Limitación que permanece (no es un bug)

**Con split de raster (§8), la colisión usa el scroll de una sola banda.**

El hardware tiene **un único** par `scroll_x`/`scroll_y`, que se reescribe en cada
transición de banda. El barrido corre en el VBLANK, en un único instante, y usa el
valor que quedó tras dibujar la **última banda** (la INFERIOR si hay split).

Como los 32 sprites pueden estar repartidos entre tres bandas con scroll distinto,
**no existe un scroll válido para todos a la vez**. Con split activo, solo los
sprites de la última banda colisionan correctamente.

Arreglarlo de verdad requeriría guardar la banda por sprite en el line buffer
(cambio de core).

**Recomendación:** no combines `SOLID_HIT` con split de raster.

---

## Lecciones aplicables a la biblioteca

- La biblioteca **no** necesita cambios: `vc_solid_hit()` solo expone el registro.
- `VIDEO-LIB.md` y la guía advierten ahora de la limitación con split de raster.
- Sigue siendo válido que, en juegos con cámara compleja o bandas, la colisión
  sprite↔tile **por software** es más predecible (es lo que hacen los juegos de
  `juegos_6502/`).

---

## Historial del hallazgo

| Fecha | Hecho |
|---|---|
| — | Detectado en la demo: la plataforma "rebotaba" con el jugador lejos |
| — | Confirmado en el emulador: barrido sin `scroll_x`/`scroll_y` |
| — | Documentado aquí y reportado |
| `cc645d3` | **Corregido en el core** (+4 CLS) |
| `2182323`+ | Emulador actualizado; manual v2.9 con el aviso del split |
