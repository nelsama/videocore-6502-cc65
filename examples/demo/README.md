# Demo de la biblioteca `vc`

Demo funcional del Core de Vídeo. Ejercita toda la biblioteca:

- Carga de tileset (patrones de fondo de 2 planos) y mundo 64×32.
- Scroll de cámara.
- Sprite animado (3 frames) que rebota.
- Objeto 16×16 (4 sprites de 8×8 a 1×) con cara (marco, ojos, nariz, boca).
- Sprite 8×8 dibujado a **2×** (`VC_SPR_SCALE2X`) → se ve 16×16 en pantalla.
- HUD fijo con **banda de raster superior** (`RASTER_LINE0`), texto en blanco y
  verde, mostrando la posición del jugador y qué tipo de colisión ocurrió.
- **Colisión sprite↔tile sólido por hardware** (`vc_solid_hit`), que rebota al
  jugador contra la plataforma.
- Salida al monitor pulsando **`q`** por la UART.

## Banco de pruebas de `vc_solid_hit()`

Esta demo está montada a propósito para que `vc_solid_hit()` **se pueda probar**:

1. **La zona de juego NO usa split de raster.** El hardware tiene una única
   señal de scroll y el barrido de colisión usa la de la *última banda dibujada*.
   Si hubiera una banda inferior fija, la colisión se calcularía con scroll 0
   mientras el mundo scrollea, y saltarían falsos positivos. Aquí solo se define
   la banda superior (`vc_set_raster(24, 0xFF)`), así que el resto de la pantalla
   usa el scroll normal — el mismo que ve el barrido — y la colisión es exacta.

2. **Solo el jugador puede disparar el flag.** `vc_solid_hit()` es global (dice
   "alguien chocó", no quién). El bloque y el rombo llevan el `COLL_POINT`
   aparcado en la esquina superior y además se mueven por encima de la
   plataforma (`Y_APARCADO_MAX`), de modo que su punto nunca cae sobre un
   ladrillo sólido.

El HUD distingue qué se golpeó (`PLATAFORMA`, `PISO`, `OTRO` o `BOX`), porque el
flag del hardware por sí solo no lo dice.

> **Al probarlo en el emulador**, `/state.txt` publica el scroll, cuál usa la
> colisión y la celda que consulta cada sprite (`celda_colision()`). Si quitas el
> `RASTER_LINE1 = 0xFF` y vuelves a poner una banda inferior fija, verás el
> desfase entre la celda consultada y la columna real del sprite.

## Compilar

Desde esta carpeta:

```bash
make
```

Genera `output/demo.bin`. Requiere que la biblioteca esté construida en la raíz
(`make lib`, que produce `output/vc.lib`).

## Cargar en el monitor

```
SD                 ; inicializar SD
LOAD DEMO          ; cargar (copia demo.bin como DEMO)
R                  ; ejecutar
```

Al pulsar `q` en el terminal UART, el programa sale y vuelve al monitor.

## Notas

- El HUD ocupa las filas 1 y 2, dentro de la banda superior (líneas 0-23). La
  fila 0 se ve desplazada por el pipeline del core, así que no se usa.
- `vc_solid_hit()` llega con **un frame de latencia**: la demo clasifica el
  choque con la posición que tenía el jugador en el frame anterior (`py_ant`).
