/**
 * ============================================================================
 * gfx.c - Capa de alto nivel de la biblioteca del Core de Video
 * ============================================================================
 * Wrappers comodos sobre el nucleo en ensamblador (video.s). Aqui viven los
 * sprites, las colisiones por software y las paletas.
 *
 * El resto de la capa alta se ha separado en modulos para que el linker pueda
 * DESCARTAR lo que un juego no use (el descarte de cc65 es por archivo .o, no
 * por funcion):
 *   - gfx_vram.c   limpieza de VRAM, color de la fuente, vc_cell()
 *   - gfx_tiles.c  atributos de celda y carga de tiles
 *   - gfx_text.c   modo texto
 *
 * La API completa sigue en video.h: para el usuario no cambia nada.
 *
 * Toda escritura a VRAM/OAM debe hacerse en VBLANK para evitar "partir"
 * sprites o tiles a mitad de frame.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * SPRITES
 * ===========================================================================
 * vc_sprite_set() y vc_sprite_move() viven en el nucleo en ensamblador
 * (src/video.s) por rendimiento; sus prototipos estan en video.h.
 * ========================================================================= */

/* Deshabilita un sprite: deja de dibujarse Y deja de colisionar.
 *
 * Poner Y >= 248 solo desactiva el DIBUJADO. El barrido de colision del
 * hardware recorre los 32 sprites SIN filtrar por Y, asi que un sprite
 * "aparcado" sigue activando SOLID_HIT si su COLL_POINT cae sobre una celda
 * solida. Ver manual del core, secciones 6.1 y 10.1.
 *
 * Por eso se dejan los CUATRO campos que deciden el punto de colision:
 *   - Y = 248 y COLL = (0,0): el punto cae en la fila 31;
 *   - X = 0 y FLAGS = 0 (sin bit 8): cae en la COLUMNA 0.
 *
 * Lo de X importa: si se dejara la X donde murio el sprite, el punto se
 * quedaria en su columna, y con scroll_y != 0 en la fila 0, que SI suele ser
 * solida. Dejar X=0 ata el punto a la fila 31, que esta fuera de pantalla.
 *
 * La OAM es de solo escritura (no se puede leer), asi que no hay forma de
 * conservar la paleta ni SCALE2X al apagar: FLAGS se pone a 0. Para volver a
 * dibujarlo, usa vc_sprite_set() o reescribe los campos con vc_oam_put().
 *
 * QUEDA UNA RESPONSABILIDAD DEL USUARIO: no marcar como solida la fila 31.
 * El hardware no la excluye y la libreria no puede impedir que se marque. */
void vc_sprite_disable(uint8_t spr) {
    vc_oam_put(spr, VC_OAM_X, 0);
    vc_oam_put(spr, VC_OAM_Y, VC_SPR_DISABLED);
    vc_oam_put(spr, VC_OAM_FLAGS, 0);
    vc_oam_put(spr, VC_OAM_COLL, VC_COLL_POINT(0, 0));
}

/*
 * vc_sprite16_set() vive en el nucleo en ensamblador (src/video.s) por
 * rendimiento. Compone un objeto 16x16 con 4 sprites consecutivos:
 *   cuadrante 0 = (x,   y)      tile0+0
 *   cuadrante 1 = (x+8, y)      tile0+1
 *   cuadrante 2 = (x,   y+8)    tile0+2
 *   cuadrante 3 = (x+8, y+8)    tile0+3
 * OJO: fija COLL = centro ($24) en los 4 cuadrantes.
 * (ver prototipo en video.h)
 */

/* ===========================================================================
 * COLISION
 * =========================================================================== */

/* Flag global de colision sprite<->tile solido de este frame.
 *
 * Devuelve 0 o 1 (no la mascara 0x20): el bit vive en $D803, pero exponerlo
 * asi evita el error clasico de comparar con `== 1` y no saber por que nunca
 * entra. Si necesitas el byte completo de STATUS, usa vc_status().
 *
 * Llega con 1 frame de latencia: refleja el VBLANK anterior. */
uint8_t vc_solid_hit(void) {
    return (uint8_t)((vc_status() & VC_STATUS_SOLID_HIT) ? 1 : 0);
}

/* ===========================================================================
 * COLISION AABB - nucleo en ensamblador (src/collide.s)
 * ===========================================================================
 * Los wrappers cargan la Zero Page del modulo (CL_*) y llaman al nucleo asm,
 * que evita el coste del stack de cc65. Se declaran extern y se marcan como
 * zero page con #pragma zpsym para que cc65 use direccionamiento de pagina
 * cero.
 *
 * OJO al dividir este archivo: estas declaraciones deben estar en UN SOLO
 * modulo .c. Si se declaran tambien en otro, el linker falla por simbolos
 * duplicados. */
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

/* 1 = las cajas A y B se solapan. */
uint8_t vc_box_overlap(const vc_box_t *a, const vc_box_t *b) {
    CL_AX = a->x; CL_AY = a->y; CL_AW = a->w; CL_AH = a->h;
    CL_BX = b->x; CL_BY = b->y; CL_BW = b->w; CL_BH = b->h;
    return cl_overlap();
}

/* 1 = el punto (px,py) cae dentro de la caja b. */
uint8_t vc_box_contains(const vc_box_t *b, uint16_t px, uint16_t py) {
    CL_AX = px; CL_AY = py; CL_AW = 0; CL_AH = 0;
    CL_BX = b->x; CL_BY = b->y; CL_BW = b->w; CL_BH = b->h;
    return cl_point();
}

/* Construye una caja desde un sprite de size x size en (x,y). */
void vc_box_from_sprite(vc_box_t *box, uint16_t x, uint16_t y, uint8_t size) {
    box->x = x;
    box->y = y;
    box->w = size;
    box->h = size;
}
