# Ejemplo de colisiones

Demuestra la API de **colisión sprite↔sprite** de la biblioteca (núcleo en
ensamblador, `src/collide.s`). El hardware no tiene colisión sprite↔sprite, así
que se hace por software con **cajas AABB**.

## Qué muestra

- **`vc_box_t`** y **`vc_box_from_sprite()`**: construir la caja de un objeto
  desde su posición y tamaño.
- **`vc_box_overlap(a, b)`**: solapamiento de dos cajas. Hay 4 objetos 8×8 que
  convergen hacia el centro y **rebotan al chocar** entre sí. También **rebotan
  al chocar con la nave** (la nave es "pesada": no rebota, solo el objeto).
  El HUD muestra un **contador de choques** (`C:nnn`).
- **`vc_box_contains(b, px, py)`**: test puntual punto-dentro-de-caja. La "nave"
  (16×16) marca los objetos cuyo **centro** cae dentro de ella (indicador `X`).
- **`vc_solid_hit()`**: flag del hardware de colisión contra tiles sólidos (hay
  una franja de suelo sólido para probarlo; la `S` del HUD se enciende).

## Compilar

Desde esta carpeta:

```bash
make
```

Genera `output/collision.bin`. Requiere la biblioteca en la raíz (`make lib`).

## Cargar en el monitor

```
SD                 ; inicializar SD
LOAD COLLISION     ; cargar (copia collision.bin como COLLISION)
R                  ; ejecutar
```

Pulsa **`q`** por el terminal UART para salir al monitor.

## HUD

- Texto superior: título + indicadores `X` (centro-en-nave) y `S` (tile sólido).
- Fila 28: leyenda + contador de choques `C:nnn`.
- Las filas 0 y 29 se evitan (pipeline / overscan).
