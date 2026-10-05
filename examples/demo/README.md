# Demo de la biblioteca `vc`

Demo funcional del Core de Vídeo. Ejercita toda la biblioteca:

- Carga de tileset (patrones de fondo de 2 planos) y mundo 64×32.
- Scroll de cámara.
- Sprite animado (3 frames) que rebota.
- Objeto 16×16 (4 sprites de 8×8 a 1×) con cara (marco, ojos, nariz, boca).
- Sprite 8×8 dibujado a **2×** (`VC_SPR_SCALE2X`) → se ve 16×16 en pantalla.
- HUD fijo con split de raster (bandas arriba/abajo), texto en **blanco** y
  **verde**, mostrando la posición del jugador.
- Salida al monitor pulsando **`q`** por la UART.

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

- El HUD usa las filas 1 y 28 (la 0 se ve desplazada por el pipeline y la 29
  puede recortarse por overscan del monitor).
- Todos los textos salen en las dos paletas de tinta disponibles
  (`VC_TINTA(VC_BGPAL_0)` y `VC_TINTA(VC_BGPAL_3)`).
