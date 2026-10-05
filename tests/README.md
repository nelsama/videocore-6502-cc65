# Tests unitarios de los núcleos en ensamblador

Tests que validan el comportamiento de las rutinas escritas en ensamblador
(`src/video.s`, `src/collide.s`) **sin hardware**, usando `sim65` (el simulador
de 6502 que viene con cc65).

## Por qué

La demo funcionando **no basta** como prueba: solo ejercita los caminos comunes.
Los tests cubren los **casos de borde** que un juego real puede encontrar pero
que la demo rara vez toca:

- X de sprite ≥ 256 (el **bit 8**), y el caso inverso (quitarlo).
- Bordes exactos en la colisión AABB (cajas adyacentes NO colisionan).
- Que `vc_sprite16_set` componga los 4 cuadrantes con las posiciones, tiles,
  flags y `COLL_POINT` correctos.
- Que `vc_oam_put(spr, field, data)` escriba **`data`** (no `spr`) — regresión del
  bug en que el campo `TILE` quedaba siempre a 0.

## Ejecutar

```sh
sh tests/run.sh
```

Salida esperada: cada test imprime `OK` (devuelve 0) o `FAIL` (devuelve el número
del caso que falla).

## Cómo funcionan

- **`collide_test.s`** — carga cajas y llama a `cl_overlap`/`cl_point`,
  comprobando 7 casos conocidos.
- **`sprite_test.s`** y **`sprite16_test.s`** — ensamblan `video.s` con
  `-D TEST_HOOKS`, que **redirige los registros del puerto indirecto**
  (`$D800-$D812`) a RAM (`$F000+`) y hace que `oam_write` lo aporte el test. Así
  el test puede leer lo que la rutina escribió. `sprite_test.s` incluye además
  los casos que verifican que `vc_oam_put` escribe **el dato** y no el sprite.
- **`pat_base_test.s`** — verifica la dirección de patrón `tile*8+fila` para
  tiles 0, 31, 32, 64, 128 y 255 (cubre el cruce de los bits 8, 9 y 10).
- **`spr_load_test.s`** — comprueba que `vc_load_spr_pattern(spr, ...)` escribe en
  la dirección correcta del banco (`$C8`/`$E8`, `spr*8+fila`).
- **`humano_test.s`** — comprueba que `vc_sprite_set` escribe los 5 campos y que
  el campo `TILE` recibe el valor correcto (dirección `spr*5 + 2`).

## Añadir un test nuevo

1. Escribe `tests/mi_test.s` con `_main` que devuelva 0 si pasa, o N (caso que
   falla) en `A`.
2. Añádelo a `tests/run.sh`.
3. Si prueba rutinas que escriben al puerto indirecto, usa `-D TEST_HOOKS` al
   ensamblar `video.s` y aporta tu propio `oam_write`/registro.
