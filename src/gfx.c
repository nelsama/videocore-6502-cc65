/**
 * ============================================================================
 * gfx.c - Capa de alto nivel de la biblioteca del Core de Vídeo
 * ============================================================================
 * Wrappers cómodos sobre el núcleo en ensamblador (video.s). Aquí viven las
 * funciones pensadas para el código de juego: sprites, objetos 16x16, texto,
 * colisiones por software y utilidades de fondo.
 *
 * Toda escritura a VRAM/OAM debe hacerse en VBLANK para evitar "partir"
 * sprites o tiles a mitad de frame.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * LIMPIEZA DE VRAM
 * =========================================================================== */

/* Limpia la VRAM de trabajo: tilemap a tile 0 (vacio), atributos a 0 y el OAM
 * deshabilitado.
 *
 * NO limpia los patrones de fondo ni de sprite: el manual v2.1 confirma que la
 * fuente de texto ocupa los patrones $20-$7F y el hardware los deja cargados al
 * arrancar. Un "clear de patrones" borraria la fuente y el texto dejaria de
 * verse.
 *
 * Nota: tras VIDEO_READY, el hardware ya deja el tilemap a $20 (espacio) y los
 * atributos a 0. Si vas a sobrescribir todo el mundo igualmente, puedes omitir
 * las partes que no necesites.
 *
 * Debe llamarse tras vc_wait_ready() y dentro del VBLANK. */
void vc_clear_vram(void) {
    vc_fill_tilemap(VC_TILE_BLANK);   /* todas las celdas -> tile 0 */
    vc_clear_attr();                  /* atributos a 0 (paleta 0, sin flags) */
    vc_clear_oam();                   /* todos los sprites deshabilitados */
}

/* ===========================================================================
 * ATRIBUTOS DEL FONDO
 * =========================================================================== */

/* Escribe el atributo de una celda = paleta + flags (VC_ATTR_*). */
void vc_set_cell_attr(uint8_t col, uint8_t row, uint8_t paleta, uint8_t flags) {
    vc_put_attr(col, row, (uint8_t)((flags & 0xF0) | (paleta & 0x0F)));
}

/* Deja la celda vacía: tile 0 y atributo 0. */
void vc_free_cell(uint8_t col, uint8_t row) {
    vc_put_cell(col, row, 0);
    vc_put_attr(col, row, 0);
}

/* ===========================================================================
 * SPRITES
 * ===========================================================================
 * vc_sprite_set() y vc_sprite_move() viven en el núcleo en ensamblador
 * (src/video.s) por rendimiento; sus prototipos están en video.h.
 * ========================================================================= */

/* Deshabilita un sprite (Y fuera de rango). */
void vc_sprite_disable(uint8_t spr) {
    vc_oam_put(spr, VC_OAM_Y, VC_SPR_DISABLED);
}

/*
 * vc_sprite16_set() vive en el núcleo en ensamblador (src/video.s) por
 * rendimiento. Compone un objeto 16x16 con 4 sprites consecutivos:
 *   cuadrante 0 = (x,   y)      tile0+0
 *   cuadrante 1 = (x+8, y)      tile0+1
 *   cuadrante 2 = (x,   y+8)    tile0+2
 *   cuadrante 3 = (x+8, y+8)    tile0+3
 * (ver prototipo en video.h)
 */

/* ===========================================================================
 * COLISIÓN
 * =========================================================================== */

/* Flag global de colisión sprite↔tile sólido de este frame. */
uint8_t vc_solid_hit(void) {
    return (uint8_t)(vc_status() & VC_STATUS_SOLID_HIT);
}

/* Detección AABB genérica (sprite↔sprite, en software). */

/* ===========================================================================
 * COLISIÓN AABB — núcleo en ensamblador (src/collide.s)
 * ===========================================================================
 * Los wrappers cargan la Zero Page del módulo (CL_*) y llaman al núcleo asm,
 * que evita el coste del stack de cc65 (mucho más rápido que C puro).
 * ========================================================================= */

/* Zero Page del módulo de colisiones (definida en collide.s). Se declaran
 * extern y se marcan como zero page con #pragma zpsym para que cc65 use
 * direccionamiento de página cero. */
extern uint16_t CL_AX, CL_AY, CL_AW, CL_AH;
extern uint16_t CL_BX, CL_BY, CL_BW, CL_BH;
#pragma zpsym("CL_AX")
#pragma zpsym("CL_AY")
#pragma zpsym("CL_AW")
#pragma zpsym("CL_AH")
#pragma zpsym("CL_BX")
#pragma zpsym("CL_BY")
#pragma zpsym("CL_BW")
#pragma zpsym("CL_BH")
extern uint8_t  cl_overlap(void);
extern uint8_t  cl_point(void);

/* 1 = la caja A solapa con la caja B. */
uint8_t vc_box_overlap(const vc_box_t *a, const vc_box_t *b) {
    CL_AX = a->x; CL_AY = a->y; CL_AW = a->w; CL_AH = a->h;
    CL_BX = b->x; CL_BY = b->y; CL_BW = b->w; CL_BH = b->h;
    return cl_overlap();
}

/* 1 = el punto (px,py) cae dentro de la caja b. */
uint8_t vc_box_contains(const vc_box_t *b, uint16_t px, uint16_t py) {
    CL_AX = px; CL_AY = py;                 /* el punto va en la "caja A" */
    CL_BX = b->x; CL_BY = b->y; CL_BW = b->w; CL_BH = b->h;
    return cl_point();
}

/* Construye una caja cuadrada desde un sprite en (x,y) de tamaño `size` (8/16/32). */
void vc_box_from_sprite(vc_box_t *box, uint16_t x, uint16_t y, uint8_t size) {
    box->x = x;
    box->y = y;
    box->w = size;
    box->h = size;
}

/* ===========================================================================
 * TEXTO
 * =========================================================================== */

/* Limpia la pantalla visible (40x30) con espacios. La fila 0 se reserva por
 * el desfase del pipeline; se rellena también para mayor comodidad. */
void vc_text_init(void) {
    uint8_t col, row;

    for (row = 0; row < VC_SCREEN_ROWS; row++) {
        for (col = 0; col < VC_SCREEN_COLS; col++) {
            vc_put_cell(col, row, VC_CHAR_SPACE);      /* espacio */
        }
    }
}

/* Escribe un carácter en la posición (col,row). El tile es el propio ASCII. */
void vc_put_char(uint8_t col, uint8_t row, char c) {
    vc_put_cell(col, row, (uint8_t)c);
}

/* Escribe una cadena desde (col,row); se detiene en el terminador 0.
 * Usa la paleta de texto por defecto (paleta 0). */
void vc_put_str(uint8_t col, uint8_t row, const char *s) {
    while (*s) {
        vc_put_cell(col, row, (uint8_t)*s);
        col++;
        if (col >= VC_SCREEN_COLS) {
            col = 0;
            row++;
        }
        s++;
    }
}

/* Igual que vc_put_str pero fija la paleta del texto en cada celda. */
void vc_put_str_pal(uint8_t col, uint8_t row, const char *s, uint8_t paleta) {
    while (*s) {
        vc_put_cell(col, row, (uint8_t)*s);
        vc_set_cell_attr(col, row, paleta, 0);
        col++;
        if (col >= VC_SCREEN_COLS) {
            col = 0;
            row++;
        }
        s++;
    }
}

/* ===========================================================================
 * FONDO: CARGAR TILESET Y MAPA COMPLETO
 * =========================================================================== */

/*
 * Carga N patrones de fondo consecutivos desde dos tablas planares.
 * pat0[i*8+fila] = plano 0 del patrón (tile_base+i)
 * pat1[i*8+fila] = plano 1 del patrón (tile_base+i)
 */
void vc_load_tiles(uint8_t tile_base, uint8_t count,
                   const uint8_t *pat0, const uint8_t *pat1) {
    uint8_t i;

    for (i = 0; i < count; i++) {
        vc_load_bg_pattern((uint8_t)(tile_base + i), pat0, pat1);
        pat0 += VC_TILE_SIZE;
        pat1 += VC_TILE_SIZE;
    }
}

/* Copia un bloque del tilemap visible a VRAM, fila a fila.
 * src debe contener 40*30 = 1200 bytes (formato de pantalla, no de mapa 64). */
void vc_blit_screen(const uint8_t *src) {
    uint8_t col, row;

    for (row = 0; row < VC_SCREEN_ROWS; row++) {
        for (col = 0; col < VC_SCREEN_COLS; col++) {
            vc_put_cell(col, row, *src++);
        }
    }
}

/* ===========================================================================
 * PALETAS PROGRAMABLES
 * ===========================================================================
 * El hardware permite reescribir las entradas de paleta en cualquier momento
 * (RGB444, $D813-$D815). Al arrancar valen los valores por defecto del manual.
 * ESCRIBIR EN VBLANK si cambias muchos colores a la vez.
 */

/* Fija UN color de la paleta de FONDO (paleta 0-3, color 0-3). */
void vc_pal_set_bg(uint8_t paleta, uint8_t color, uint16_t rgb444) {
    vc_pal_set(VC_PAL_BG(paleta, color), rgb444);
}

/* Fija UN color de la paleta de SPRITE (paleta 0-3, color 0-3). */
void vc_pal_set_spr(uint8_t paleta, uint8_t color, uint16_t rgb444) {
    vc_pal_set(VC_PAL_SPR(paleta, color), rgb444);
}

/* Carga los 4 colores de una paleta de FONDO desde un array RGB444[4]. */
void vc_pal_load_bg(uint8_t paleta, const uint16_t *colores4) {
    vc_pal_load(VC_PAL_BG(paleta, 0), colores4, 4);
}

/* Carga los 4 colores de una paleta de SPRITE desde un array RGB444[4]. */
void vc_pal_load_spr(uint8_t paleta, const uint16_t *colores4) {
    vc_pal_load(VC_PAL_SPR(paleta, 0), colores4, 4);
}

/* Fija el color de fondo global (BG_COLOR) con un RGB444.
 *
 * BG_COLOR es la entrada 15 del banco de fondo (= paleta 3, color 3); no hay
 * registro propio. Afecta al margen de la pantalla y al fondo vacio (color 0
 * del fondo). Escribelo en VBLANK para evitar una franja con el color viejo. */
void vc_set_bgcolor(uint16_t rgb444) {
    vc_pal_set(VC_PAL_BGCOLOR, rgb444);
}
