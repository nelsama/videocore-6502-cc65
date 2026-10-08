# Demo de paletas programables

Muestra cómo **reescribir las paletas** del core (`$D813-$D815`) para cambiar los
colores **en vivo**, sin tocar los patrones ni el tilemap.

## Qué muestra

- **Ciclo de paleta de fondo**: los 4 colores de la paleta de fondo 1 se
  interpolan entre un juego "A" (azules) y un juego "B" (marrones), canal a canal.
- **Cambio de paleta de sprite**: los 4 colores de la paleta de sprite 1 alternan
  entre "A" (cian) y "B" (amarillo/magenta).
- **Texto** fijo, usando la paleta 0 (`VC_TINTA(VC_BGPAL_0)`) y la 3
  (`VC_TINTA(VC_BGPAL_3)`) para las dos filas de HUD.

La escena es un marco de tiles (patrón de prueba, aparece el índice 3 = más
claro) más un rombo de sprite en el centro. Todo cambia de color porque solo se
reescriben **las paletas**, no los dibujos.

## API que usa

```c
vc_pal_set_bg(paleta, color, rgb444);   /* un color de fondo */
vc_pal_set_spr(paleta, color, rgb444);  /* un color de sprite */
vc_pal_load_bg(paleta, arr4);           /* los 4 colores de fondo de golpe */
vc_pal_load_spr(paleta, arr4);          /* los 4 colores de sprite de golpe */
vc_pal_ptr(entrada);                     /* bajo nivel: fija el puntero */
vc_pal_set(entrada, rgb444);            /* bajo nivel: escribe y auto-incrementa */

VC_RGB444(r,g,b);                        /* color desde componentes 0-15 */
VC_RGB888(0xRRGGBB);                     /* color desde RGB888 */
```

## Cómo funciona el hardware

- Hay **32 entradas** de paleta de 12 bits (**RGB444**): **0-15 fondo**, **16-31 sprite**.
- Entrada = `paleta*4 + color` (dentro de su banco).
- El puntero **auto-incrementa** al escribir, así que cargar 4 colores seguidos es
  encadenar escrituras.
- Los colores por defecto son los del manual (§4); aquí los sobreescribimos.
- **Escribe en VBLANK** si cambias muchos colores a la vez.

## Compilar

Desde esta carpeta:

```bash
make
```

Genera `output/palette.bin`. Requiere la biblioteca en la raíz (`make`).

## Cargar en el monitor

```
SD                 ; inicializar SD
LOAD PALETTE       ; cargar (copia palette.bin como PALETTE)
R                  ; ejecutar
```

Pulsa **`q`** por el terminal UART para salir al monitor.

## Ajustes

- `bg_palA`/`bg_palB` y `spr_palA`/`spr_palB` — los colores de cada juego.
- La velocidad del ciclo (cada 4 frames) en `main()`.

## Notas de diseño

- **Carga el patrón del tile 0 como vacío** (`tile_empty_p0/p1`): `vc_clear_vram()`
  ya deja el patrón 0 en blanco (su setup por hardware limpia los patrones), pero
  el ejemplo lo recarga explícitamente para no depender de ese detalle.
- **Evita que el sprite use el mismo color que el `BG_COLOR`** (azul cielo): un
  rombo azul sobre fondo azul se camufla y parece desaparecer. El ejemplo usa solo
  colores cálidos (rojo/magenta/naranja/amarillo) para el sprite.
- **Fija el `BG_COLOR` explícitamente** con `vc_set_bgcolor(VC_BG_COLOR_DEFAULT)`:
  el fondo vacío es transparente y deja ver `BG_COLOR`, así que no conviene depender
  del valor de arranque del hardware. El resto de ejemplos hacen lo mismo.
