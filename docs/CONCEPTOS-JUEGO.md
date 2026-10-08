# Conceptos: del 6502 a un juego de 8 bits

Este documento es una **introducción conceptual**. Está pensado para quien ya sabe
programar el 6502 (instrucciones, direccionamiento, zero page) pero **nunca ha hecho
un juego de 8 bits** ni conoce la jerga típica (tile, sprite, tilemap, scroll, VBLANK…).

No enseña la API de la biblioteca `vc_*` ni el detalle de los registros: para eso está
la referencia de la API en [`VIDEO-LIB.md`](VIDEO-LIB.md) y el manual del core en
[`07-MANUAL-PROGRAMACION.md`](07-MANUAL-PROGRAMACION.md). Aquí se explica **qué es cada
cosa**, **por qué existe** y **cómo encajan** para armar un juego.

---

## 1. La idea clave: no dibujas píxeles

En un juego moderno escribirías píxeles en un framebuffer (una zona de memoria que es
la pantalla). Aquí **no**. El core de vídeo de este computador es un **coprocesador
gráfico**: tú no pintas píxeles, **escribes memoria y registros**, y el hardware genera
la imagen sola, en tiempo real, línea a línea.

Ese es el modelo de los chips clásicos (el VIC-II del Commodore 64, el PPU de la NES).
Tiene consecuencias importantes:

- **No hay framebuffer.** No existe una RAM de "la pantalla" que puedas leer/escribir
  píxel a píxel. La VRAM guarda *descripciones* (qué tile va en cada celda, dónde está
  cada sprite), no píxeles.
- **La VRAM es de solo escritura.** No puedes leerla de vuelta; si necesitas recordar
  lo que hay en pantalla, mantienes **tu propia copia en RAM**.
- **Todo se actualiza dentro del VBLANK** (ver §7).

Piénsalo como una imprenta: tú le das al hardware las **planchas** (dibujos) y la
**lista de lo que va impreso y dónde** (posición), y él se encarga de imprimir cada
frame a 60 imágenes por segundo.

---

## 2. El fondo: tiles y tilemap

### Tile

Un **tile** ("baldosa") es un **dibujo pequeño cuadrado**, de **8×8 píxeles**. En vez de
guardar la imagen completa de la pantalla, se divide en una cuadrícula de tiles y se
referencian por índice.

- Cada tile tiene **4 colores** como máximo (es 2 bpp: 2 bits por píxel → valores 0-3).
- Un dibujo de 8×8 se guarda en **2 planos** ("planos 0 y 1"), igual que en la NES:
  cada fila son 8 píxeles; el color de cada píxel sale de combinar el bit del plano 0
  con el del plano 1.
- En este core hay **256 patrones de tile de fondo** (índices 0-255).

> **Patrón vs tile:** en la práctica se usan como sinónimos. "Patrón" es el **dibujo**
> (los bytes de los 2 planos); "tile" es ese dibujo puesto a funcionar en el fondo. El
> tilemap guarda **números**, no dibujos: el dibujo vive en el banco de patrones.

### Tilemap

El **tilemap** es el "plano" del nivel: una cuadrícula donde **cada celda guarda el
número del tile** que va ahí. En este core el mapa es de **64×32 celdas**, aunque la
pantalla solo muestra **40×30** (320×240 píxeles). Es decir, el mapa es **más grande
que la pantalla** — eso es lo que permite hacer *scroll* (§4).

```
  tilemap (64 x 32 celdas)          pantalla (40 x 30 celdas visibles)
  ┌───────────────────────────┐     ┌─────────────────┐
  │  nº  nº  nº  nº  nº ...   │     │                 │
  │  nº  nº  nº  nº  nº ...   │ ──▶ │  se muestra una  │
  │  nº  nº  nº  nº  nº ...   │     │  "ventana" del   │
  │  ...                      │     │  mapa            │
  └───────────────────────────┘     └─────────────────┘
```

### Atributos de celda

Además del número de tile, cada celda tiene un **atributo** (un byte extra) con
propiedades: qué **paleta** usa, si está **volteada** en X/Y, si se dibuja **detrás**
de los sprites, y si es **sólida** (para colisiones, §6). Fondo y `tilemap` + `atributos`
van en tablas paralelas: una guarda el dibujo, la otra las propiedades.

> **Para construir un mundo** rellenas el tilemap (qué tile en cada celda) y su
> atributo. Para *ver* algo, primero tiene que existir en el tilemap, aunque el
> tilemap sea más grande que la pantalla.

---

## 3. Los sprites

Un **sprite** es un dibujo pequeño (8×8) que se **coloca en una posición libre** de la
pantalla, sin estar atado a la cuadrícula del fondo. Se usan para lo que se mueve: el
jugador, enemigos, balas, objetos.

Diferencias con los tiles:

| | Tile (fondo) | Sprite |
|---|---|---|
| Posición | fija a la cuadrícula (columna/fila) | libre, en píxeles (X, Y) |
| Cuántos | hasta 256 patrones, 2048 celdas | **32 sprites** a la vez |
| Colores | 4 (según paleta de la celda) | 4 (según paleta del sprite) |
| Por línea | ilimitados | **máx. 8 por línea horizontal** |
| Tamaño | 8×8 | 8×8 (o 16×16 al doble) |

### OAM

Los sprites vivos en pantalla se describen en la **OAM** ("Object Attribute Memory"),
una tabla de **32 entradas**. En este core cada entrada son **5 bytes**:

```
  +0  X        (posición horizontal, 9 bits; el bit 8 va en los flags)
  +1  Y        (posición vertical; un valor alto "apaga" el sprite)
  +2  TILE     (qué dibujo de sprite usar, 0-63)
  +3  FLAGS    (paleta, volteos, prioridad, escala 2x, bit 8 de X)
  +4  COLL     (punto de colisión con el fondo, §6)
```

> ⚠️ **Ojo con la palabra `TILE` dentro de la OAM.** NO es un tile de fondo. Los sprites
> tienen su **propio banco de dibujos**, separado del de fondo, con **64 patrones
> (0-63)**. El campo se llama `TILE` por tradición del hardware, pero significa
> "**índice de patrón de sprite**". Un patrón de sprite y uno de fondo con el mismo
> número son **dibujos distintos** que viven en bancos distintos.

### Detalles que sorprenden

- **El sprite no existe hasta que lo pones en la OAM.** Dibujar un patrón no lo
  muestra; hay que escribir su entrada en la OAM (X, Y, TILE, FLAGS, COLL).
- **Y alto = sprite apagado.** No hay un "renderizar sí/no": pones su Y fuera de rango
  (≥ 248) y desaparece. Así se "desactivan" los sprites no usados.
- **Prioridad:** si dos sprites se solapan, gana el de **menor índice** de OAM. Si un
  sprite debe tapar a otro, colócalo antes (índice menor).
- **Transparencia:** el color 0 en un sprite es transparente (deja ver lo de detrás).
- **8 sprites por línea:** si en una misma fila horizontal coinciden más de 8, el
  hardware dibuja los primeros y **descarta el resto** de esa línea (afecta al flag
  `OVERFLOW`).
- **X es de 9 bits (0-511)** para cubrir todo el ancho; el bit 8 va aparte, dentro de
  los flags. La biblioteca lo gestiona sola al mover el sprite.

---

## 4. Scroll: mover la cámara sobre el mundo

Como el mapa (64×32) es más grande que la pantalla (40×30), puedes **desplazar la
ventana** sobre él: eso es el **scroll**. Es lo que hace que el mundo parezca moverse
cuando el personaje avanza.

- El scroll se mide en **píxeles**, no en celdas.
- Es **continuo**: puedes desplazar 1 píxel, no solo una celda entera.
- **Envuelve:** al salir por un borde, el mapa reaparece por el otro (módulo del tamaño
  del mapa). Por eso conviene rellenar **todo** el mapa, o verás basura en las zonas
  sin contenido.

> **Importante:** el scroll mueve el **fondo**, no los sprites. Los sprites tienen
> posiciones absolutas de pantalla. Si quieres que un sprite "pertenezca al mundo",
> tendrás que restarle la posición de la cámara al dibujarlo (esa cuenta la haces tú).

---

## 5. Bandas (split de raster): HUD fijo con juego que scrollea

Si el fondo hace scroll, **todo** el fondo se mueve, incluido el marcador. Pero un HUD
(puntos, vidas) debe quedarse **quieto**. La solución clásica es el **split de raster**:
dividir la pantalla en **bandas verticales** y dar a cada una su propio scroll.

En este core hay **hasta 3 bandas**:

```
   ┌───────────────────────┐
   │  BANDA SUPERIOR       │  scroll propio  -> HUD fijo (puntos, vidas)
   ├───────────────────────┤  ← línea de corte
   │  BANDA MEDIA          │  scroll del juego -> el mundo que se mueve
   ├───────────────────────┤  ← línea de corte
   │  BANDA INFERIOR       │  scroll propio  -> otro HUD u otro nivel
   └───────────────────────┘
```

- Las **líneas de corte** se indican en **píxeles de pantalla**.
- La **banda media** es la principal y usa el scroll "normal"; las bandas superior e
  inferior tienen su scroll **independiente**.
- Un valor `0xFF` **desactiva** una banda (esa zona usa el scroll normal).
- El corte conviene ponerlo en **múltiplo de 8**, para que coincida con una fila de tiles.

Es el mismo truco que los juegos de NES/C64 usaban para poner el marcador arriba sin
que se moviese con el escenario.

---

## 6. Colisiones: cómo sabe el juego que "algo choca"

"Colisión" es simplemente detectar que dos cosas se solapan. En este core hay **dos
tipos**, y conviene no confundirlos:

### Colisión sprite ↔ fondo (la hace el hardware)

Cada sprite tiene un **punto de colisión** (`COLL`, el byte +4 de su OAM): un píxel
concreto dentro del sprite (centro, pies, cabeza, borde…). El hardware comprueba, una
vez por frame, si ese punto cae sobre una celda del fondo **marcada como sólida**, y
levanta un flag **global** ("algún sprite tocó algo sólido").

- El flag es **global**: dice *que alguien chocó*, **no quién**. Si tienes varios
  objetos, deduces cuál fue comparando tú su posición con la pared (software).
- Es lo que usarías para que el personaje no atraviese el suelo o las paredes.

### Colisión sprite ↔ sprite (la hace el software)

El hardware **no** compara sprites entre sí. Eso lo haces tú en el juego: comparas las
**cajas** (posición + tamaño) de los objetos y decides si se solapan. Es el mismo
"sí el rectángulo A toca el rectángulo B → explota" de cualquier juego.

> **Regla mental:** el hardware avisa "**alguien chocó con el fondo**"; todo lo demás
> (quién, y sprite-contra-sprite) lo decide tu código.

---

## 7. El frame y el VBLANK: por qué importa *cuándo* escribes

El core dibuja la pantalla **60 veces por segundo**. Mientras dibuja la zona visible,
está **leyendo** la VRAM/OAM para saber qué pintar. Si tú **escribieras** en esos datos
justo entonces, el hardware unas veces leería el valor viejo y otras el nuevo: verías
un sprite **"partido"** a mitad, o rasgado (*tearing*).

La solución es el **VBLANK**: el breve intervalo entre frames, cuando el hardware **no
está dibujando**. Ahí es seguro escribir.

- **Regla de oro:** mueve sprites, escribe OAM/VRAM y scroll **dentro del VBLANK**.
- El `STATUS` (`$D803`) te da un bit que dice si estás en VBLANK; el bucle del juego
  espera a que empiece, actualiza todo, y espera a que termine.
- Por eso **un "frame" de juego = una pasada del bucle principal**, sincronizada con
  el VBLANK. Es lo que fija la cadencia de movimiento: si mueves 1 px por frame, a 60
  frames/segundo son 60 px/segundo.

---

## 8. Paletas e índices de color

El core **no usa RGB directo** en los dibujos. Igual que la NES, el color se resuelve
en **dos capas**:

1. **El patrón guarda índices** (0-3 por píxel): no dice "rojo", dice "color nº 2".
2. **La paleta traduce ese índice a un color real.** La celda (fondo) o los flags
   (sprite) eligen **cuál** de las 4 paletas se usa.

Esto permite un truco potentísimo: **cambiar todos los colores de un dibujo sin
redibujarlo**, solo eligiendo otra paleta.

- Hay **4 paletas de fondo** y **4 paletas de sprite**, en **bancos separados** (una
  paleta de fondo nº 1 y una de sprite nº 1 son independientes).
- Las paletas **vienen con colores por defecto**, pero el juego puede **reescribir
  cualquiera** en caliente: fundidos, parpadeos, paletas por nivel, efectos…
- El **color 0 es transparente** (tanto en fondo como en sprite): deja ver lo de detrás.
- El **color de fondo global** (el que se ve en los bordes y detrás de lo transparente)
  se cambia aparte.

---

## 9. El modo texto

No es un modo distinto: es **el motor de tiles con una fuente cargada como dibujos**.
Escribes el **código ASCII** de una letra como número de tile y aparece el glifo. Sirve
para marcadores, menús y pantallas de título.

- Mayúsculas, minúsculas, dígitos y signos, en 8×8.
- El color de la letra lo da la **paleta** de la celda (como cualquier tile).
- **Ojo:** la fuente ocupa un rango de patrones del banco de fondo. No lo sobrescribas
  con otros dibujos si vas a usar texto.

---

## 10. Cómo se arma un juego: el esqueleto

Con lo anterior, un juego mínimo tiene esta forma:

1. **Arrancar:** esperar a que el vídeo esté listo.
2. **Preparar el mundo (una vez):**
   - cargar los **patrones** (dibujos) de fondo y de sprite;
   - rellenar el **tilemap** y los **atributos** (qué tile y qué propiedades en cada celda);
   - colocar los **sprites** iniciales en la OAM;
   - configurar **bandas/scroll** y **paletas**.
3. **Bucle principal (una vuelta = un frame):**
   - **esperar el VBLANK**;
   - leer **entrada** (teclado/joystick);
   - **mover** los objetos (actualizar sus variables en RAM);
   - **escribir** sus nuevas posiciones en la OAM y el scroll;
   - resolver **colisiones** y reaccionar (rebotar, perder vida…);
   - dibujar el **HUD**;
   - esperar a salir del VBLANK y repetir.

Ese bucle es el corazón del juego. Todo lo demás (niveles, enemigos, sonido) son
variaciones sobre este esqueleto.

---

## 11. Chuleta de términos

| Término | Qué es |
|---------|--------|
| **Tile** | dibujo de 8×8 del fondo, guardado como patrón (2 planos, 4 colores) |
| **Patrón** | el dibujo en sí (los bytes). El fondo tiene 256; los sprites, 64 (banco aparte) |
| **Tilemap** | cuadrícula 64×32 que dice qué tile va en cada celda (más grande que la pantalla) |
| **Atributo** | byte por celda: paleta, volteos, prioridad, sólido |
| **Sprite** | dibujo libre de 8×8 colocado en una posición X,Y (hasta 32, en la OAM) |
| **OAM** | tabla de los 32 sprites: X, Y, TILE, FLAGS, COLL |
| **`TILE` (del OAM)** | índice de **patrón de sprite** (0-63) — NO es un tile de fondo |
| **VRAM** | memoria de vídeo (solo escritura) donde viven tilemap, atributos y patrones |
| **Paleta** | traduce índice (0-3) a color; 4 de fondo + 4 de sprite, reescribibles |
| **BG_COLOR** | color de fondo global (bordes y zonas transparentes) |
| **Scroll** | desplazar la ventana sobre el mapa, en píxeles |
| **Banda / split de raster** | dividir la pantalla en zonas con scroll independiente (HUD fijo) |
| **VBLANK** | intervalo entre frames en que es seguro escribir VRAM/OAM |
| **Setup de VRAM** | limpieza de toda la VRAM (+ recarga de la fuente) hecha por el hardware, sin recorrer celdas |
| **COLL_POINT** | píxel del sprite que el hardware comprueba contra el fondo sólido |
| **OVERFLOW** | flag: hubo más de 8 sprites en una línea y se descartaron algunos |
| **Overscan** | el monitor recorta los bordes; las filas extremas pueden no verse |

---

**¿Y ahora qué?** Con estos conceptos, sigue por:

- [`VIDEO-LIB.md`](VIDEO-LIB.md) — la **API** de la biblioteca (funciones `vc_*`).
- [`07-MANUAL-PROGRAMACION.md`](07-MANUAL-PROGRAMACION.md) — el **manual del core**
  (registros, direcciones, detalles de hardware).
- [`../examples/demo/`](../examples/demo/) — una demo funcional que usa todo esto.
