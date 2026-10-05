# Demo de animación de sprite

Un humanito de 8×8 (3 frames de caminata) camina sobre una plataforma de tiles.
Al chocar con las paredes laterales (colisión real del hardware), **hace flip
horizontal** (`VC_SPR_FLIP_X`), rebota y camina en la dirección contraria, siempre
animado.

## Qué muestra

- **Animación por frames**: los 3 frames del sprite se cargan como patrones
  consecutivos y se alternan cambiando el campo `TILE` del OAM.
- **Flip horizontal**: `VC_SPR_FLIP_X` cuando camina hacia la izquierda.
- **Colisión sprite↔tile del hardware**: el punto de colisión (`COLL_POINT`) se pone
  en el **borde que avanza** (derecho o izquierdo según la dirección) y se consulta
  `vc_solid_hit()`; al tocar un tile sólido, el muñeco rebota (con push-out).

## Los frames

Los 3 frames provienen del archivo `persona1-20261004-230007.piskel` y se
convirtieron a arrays C (formato planar 2bpp) con `piskel2c.py`:

```bash
python piskel2c.py persona1-20261004-230007.piskel --map R=2 P=1 Y=3
```

El script decodifica el PNG base64 que guarda Piskel, extrae cada frame de 8×8,
mapea los colores a índices 0-3 y emite `plano0`/`plano1` listos para
`vc_load_spr_pattern()`. `--map` fija a qué índice va cada color (por la inicial del
color según luminosidad: R=rojo, P=rosa, Y=amarillo). Sin `--map`, el mapeo es por
luminosidad.

> **Paleta:** el muñeco usa 3 colores (cuerpo, cara, detalles). Como el hardware
tiene 4 paletas de sprite, se elige una. Aquí se usa `VC_SPPAL_0` (preset:
piel / marrón / negro): el cuerpo va al marrón, la cara a la piel y los detalles al
negro (mapeo `--map R=2 P=1 Y=3`). Si prefieres otros tonos, cambia la paleta en
`init_humano()`/`update_humano()` **y** el mapeo usado al generar los frames, o
reprograma la paleta.

## Compilar

Desde esta carpeta:

```bash
make
```

Genera `output/animation.bin`. Requiere la biblioteca en la raíz (`make lib`).

## Cargar en el monitor

```
SD                 ; inicializar SD
LOAD ANIMATION     ; cargar (copia animation.bin como ANIMATION)
R                  ; ejecutar
```

Pulsa **`q`** por el terminal UART para salir al monitor.

## Ajustes

- `Y_PLATAFORMA` — altura de la plataforma.
- `PARED_IZQ` / `PARED_DER` — referencia de las paredes para el push-out del rebote.
- El ritmo de animación (cada 12 frames ≈ 5 cambios/seg a 60 fps) se ajusta en
  `update_humano()`.

## Archivos

- `main.c` — la demo (patrones, movimiento, animación, colisión).
- `persona1-20261004-230007.piskel` — el sprite original de Piskel.
- `persona1.c` — salida cruda del `.piskel` (array `uint32_t` por frame), como
  referencia de entrada. **No** se compila; los arrays planares usados están en
  `main.c`.
- `piskel2c.py` — conversor `.piskel` → arrays C planares.
