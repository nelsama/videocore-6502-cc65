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

### Confirmación medida con el depurador (emulador `e6fa5ca`)

Desde el commit `e6fa5ca` el emulador publica en `/state.txt` el scroll en vigor,
cuál usa la colisión y **la celda que consulta cada sprite**, calculada con la
misma función que el barrido (`celda_colision()`). Eso permite comprobarlo sin
cruzar capturas.

La demo **sí configura split** (`raster0 = 24`, `raster1 = 216`), con la banda
media scrolleando y las bandas superior e inferior fijas. Medido en ejecución:

```
  scroll: x=357 y=0
  bandas: superior hasta linea 24, x=0 y=0
          inferior desde linea 216, x=0 y=0
  scroll de la colision: x=0 y=0  (ultima banda dibujada: la inferior)
```

Y los episodios de `SOLID_HIT` disparados por el rombo (`SCALE2X`):

| Sprite | `X` (OAM) | Celda consultada | Columna real en pantalla (`(X+scroll)/8`) |
|--------|-----------|------------------|-------------------------------------------|
| 3 | 190 | fila 20, **col 23** | fila 20, **col 4** |

El barrido mira la **columna 23** mientras el sprite está en la **columna 4**: un
desfase de 19 columnas (152 px). La celda 23 es sólida (la plataforma ocupa las
columnas 4-27) y el flag se dispara aunque el sprite **no toque nada** en
pantalla. En la captura se ve el efecto: la plataforma a la izquierda y el
rombo lejos, con `HIT SOLID` encendido.

Esto **no invalida** la corrección de `cc645d3`: con scroll único (sin split) la
celda se calcula con el scroll correcto y coincide. Es la limitación estructural
ya descrita, ahora **medida**.

> ⚠️ **Aviso para quien use la demo como banco de pruebas:** la demo usa split
> para el HUD, así que su `vc_solid_hit()` **no es fiable** por diseño (el
> comentario del código avisa de que el jugador se valida además por software).
> Para probar la colisión del hardware con scroll, usa una demo **sin bandas**.

### Prueba de control: la misma demo sin split

Para aislar la causa (split) del efecto (desfase), se compiló la demo con
`vc_set_raster(0xFF, 0xFF)` y el resto idéntico. Resultado:

```
  scroll: x=117 y=0
  bandas: sin split
  scroll de la colision: x=117 y=0
  SOLID_HIT: 0
```

Ahora la colisión **usa el scroll real**. Midiendo durante 90 s cada vez que
`SOLID_HIT` se activaba, comparando la celda que consulta el emulador con la
columna que ocupa el sprite en pantalla (`(X + scroll) >> 3`):

| Resultado | Muestras |
|---|---|
| Celda coincide con la posición real | **86** |
| Desfase | 3 (lecturas no atómicas, no repetible) |

La columna coincide **exactamente** en todos los casos estables, tanto para el
rombo (`SCALE2X`) como para los cuatro cuadrantes del bloque 16×16.

**Conclusión:** la corrección de `cc645d3` es correcta. El desfase solo aparece
con **split de raster**, y es la limitación estructural descrita arriba, no un
fallo de la corrección.

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

> ⚠️ **Prueba superada, pero con una lectura equivocada por mi parte en la
> primera pasada.** En esta medición vi `SOLID_HIT` con el jugador tocando la
> plataforma y lo interpreté como «sin falsos positivos». Al exponer el emulador
> el scroll y la celda consultada (`e6fa5ca`) quedó claro que el flag **también
> se disparaba con el rombo**, que estaba lejos. Lo medible es lo de abajo; lo
> que concluí de más está corregido arriba, y la prueba de control
> **sin split** confirma la corrección del core.

- **Medición por estado (~15 min de ejecución):** el flag `SOLID_HIT` se activa
  con los sprites a la altura de la plataforma (`y = 150-152` posado encima, o
  `y = 168-169` golpeando por debajo). Con el core anterior se activaba con
  **cualquier** `y`.
- **Capturas sincronizadas:** los instantes de contacto del **jugador**
  corresponden a un solapamiento real, incluido el caso límite del extremo
  derecho (tocaba con **2 px** de su borde izquierdo, que a simple vista parece
  un fallo y no lo es).
- **Falsos positivos por split:** existen y están medidos en la sección
  anterior (los provoca el rombo, con un desfase de 19 columnas).

### Precauciones al reproducir la prueba

> ⚠️ **El indicador de la demo se mantiene varios frames** (`HIT_HOLD`). Si
> muestreas el flag suelto y comparas con la posición en ese momento, casi todo
> parece un falso positivo: el jugador **ya se ha desplazado** del punto de
> contacto. Hay que mirar el **primer** frame de cada episodio, no cualquiera.

> ⚠️ **`SOLID_HIT` es global y lo puede disparar cualquier sprite.** La demo
> aparca el `COLL` del bloque, pero el **rombo** colisiona de verdad. Al depurar,
> identifica **qué** sprite disparó el flag antes de sacar conclusiones sobre
> otro.

> ⚠️ **La captura y la lectura de OAM no son atómicas.** A 3.375 MHz los sprites
> cambian de posición entre ambas peticiones, así que cruzarlas puede dar
> discrepancias de varios píxeles.

> ℹ️ **Usa `/state.txt` del emulador, no las capturas.** Desde `e6fa5ca` publica
> el scroll, el de colisión y la celda de cada sprite. Antes de ese commit había
> que deducir el scroll de la imagen, que es donde se colaron los errores.

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
| `e6fa5ca` | Emulador publica scroll y celda de colisión; se **mide** el desfase con split |
| — | Prueba de control sin split: la celda coincide en el 100% de las muestras estables |
