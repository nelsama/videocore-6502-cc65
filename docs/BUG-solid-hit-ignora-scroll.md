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

## Verificación en emulador

Tras actualizar core y emulador se comprobó el comportamiento con la demo
(`examples/demo`), que es el caso más exigente: **scroll horizontal continuo**
(`vc_set_scroll_x` avanza 1 px por frame) más colisión sprite↔tile por hardware.

### Montaje

1. Emulador con el servidor de depuración:
   ```powershell
   Start-Process -FilePath '.\emulador.exe' `
     -ArgumentList 'rom\monitor.bin','4000','tarjeta.img','--debug=8080' `
     -WorkingDirectory 'D:\Proyectos\EmuladorFpga6502'
   ```
2. Demo en la SD y arranque por el **endpoint HTTP** (no abriendo el socket 4000,
   que reinicia el monitor):
   ```
   curl "http://127.0.0.1:8080/sdcard/remount"
   curl "http://127.0.0.1:8080/uart?cmd=LOAD%20DEMO.BIN%200800"
   curl "http://127.0.0.1:8080/uart?cmd=R%200800"
   ```
3. Sondeo en bucle de `/vram.json` (OAM y texto del HUD) y de `/screen.png`
   (píxeles reales).

### Resultados

- **Medición por estado (~15 min de ejecución):** el flag `SOLID_HIT` solo se
  activa con el jugador en la banda vertical de la plataforma (`y = 150-152`
  posado encima, o `y = 168-169` golpeando por debajo). Con el core anterior se
  activaba con **cualquier** `y`.
- **Capturas sincronizadas:** se revisaron fotograma a fotograma los instantes de
  contacto. **Todos** corresponden a un solapamiento real con la plataforma,
  incluido el caso límite del extremo derecho (el sprite tocaba con **2 px** de
  su borde izquierdo, que a simple vista parece un fallo y no lo es).
- **Sin falsos positivos observados** una vez descartado el caso anterior.

### Precauciones al reproducir la prueba

> ⚠️ **El indicador de la demo se mantiene varios frames** (`HIT_HOLD`). Si
> muestreas el flag suelto y comparas con la posición en ese momento, casi todo
> parece un falso positivo: el jugador **ya se ha desplazado** del punto de
> contacto. Hay que mirar el **primer** frame de cada episodio, no cualquiera.

> ⚠️ **La captura y la lectura de OAM no son atómicas.** A 3.375 MHz el jugador
> cambia de posición entre ambas peticiones, así que cruzarlas puede dar
> discrepancias de varios píxeles. Es preferible decidir sobre los **píxeles de
> `/screen.png`** (jugador y plataforma en la misma imagen) que sobre la OAM.

> ℹ️ **El emulador no expone el scroll.** `/ram.bin` devuelve solo los 16 KB de
> RAM y `/state.txt` no incluye `$D804-$D807`, así que la celda que calcula el
> hardware **no se puede leer directamente**. La comprobación es, por tanto,
> **geométrica sobre la imagen** (que el sprite y la plataforma se toquen), no
> sobre la celda del barrido.

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
| — | Verificado en emulador con la demo (scroll activo): sin falsos positivos |
