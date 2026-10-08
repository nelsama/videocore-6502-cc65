/**
 * ============================================================================
 * examples/animation/main.c - Demo de animacion de sprite
 * ============================================================================
 * Un humanito de 8x8 (3 frames, extraidos del .piskel con piskel2c.py) camina
 * sobre una plataforma. Al chocar con las paredes laterales, hace FLIP para
 * caminar en la otra direccion, y sigue animado.
 *
 *   - Animacion: cambia de frame (tile) cada pocos frames.
 *   - Flip horizontal: VC_SPR_FLIP_X cuando camina hacia la izquierda.
 *   - Colision con las paredes: vc_box_overlap contra dos cajas fijas.
 *
 * Pulsa 'q' por la UART para salir al monitor.
 * ============================================================================
 */

#include <stdint.h>
#include "video.h"
#include "romapi.h"

/* ===========================================================================
 * PATRONES DEL HUMANO (3 frames, 2 planos). Generados por piskel2c.py.
 * Mapeo de color (--map R=2 P=1 Y=3) para la paleta HEART:
 *   R (cuerpo, rojo oscuro)  -> indice 2 (marron)
 *   P (cara, rosa claro)     -> indice 1 (piel)
 *   Y (detalles, amarillo)   -> indice 3 (negro)
 * ========================================================================= */
static const uint8_t f0_p0[8] = {0x00,0x30,0x3C,0x30,0x4E,0x40,0x40,0x00};
static const uint8_t f0_p1[8] = {0x38,0x00,0x00,0x00,0x7C,0x70,0x30,0x38};
static const uint8_t f1_p0[8] = {0x00,0x30,0x3C,0x30,0x4E,0x40,0x40,0x00};
static const uint8_t f1_p1[8] = {0x38,0x00,0x00,0x00,0x7C,0x70,0x2A,0x24};
static const uint8_t f2_p0[8] = {0x00,0x30,0x3C,0x30,0x48,0x86,0x80,0x00};
static const uint8_t f2_p1[8] = {0x38,0x00,0x00,0x00,0x78,0xB4,0x50,0x58};

/* Tiles de fondo: 0 = vacio, 1 = plataforma solida */
static const uint8_t tile_empty_p0[8] = {0,0,0,0,0,0,0,0};
static const uint8_t tile_empty_p1[8] = {0,0,0,0,0,0,0,0};
static const uint8_t tile_solid_p0[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint8_t tile_solid_p1[8] = {0,0,0,0,0,0,0,0};

/* ===========================================================================
 * ESTADO
 * =========================================================================== */
#define HUMANO_SPR   0
#define NFRAMES      3
#define TILE_FRAME0  8       /* frames en los patrones 8,9,10 */
#define HUMANO_W     8
#define Y_PLATAFORMA 176     /* fila de tiles de la plataforma (176 px) */

static uint16_t hx;          /* posicion X del humano (9 bits) */
static uint8_t  hy;          /* posicion Y */
static int8_t   hvx;         /* velocidad X (+ derecha, - izquierda) */
static uint8_t  hframe;      /* frame actual 0..2 */
static uint8_t  htic;        /* contador para cambiar de frame */

/* Rango de la plataforma (paredes de las que rebotar), en px.
 * Se usa para el push-out tras la colision, no para limitar el movimiento. */
#define PARED_IZQ  48
#define PARED_DER  272       /* el humano 8x8: borde derecho = 280 */

/* ===========================================================================
 * VIDEO
 * =========================================================================== */
static uint8_t check_quit(void) {
    while (rom_uart_rx_ready()) {
        char c = rom_uart_getc();
        if (c == 'q' || c == 'Q') return 1;
    }
    return 0;
}

static void setup_video(void) {
    uint8_t x, y;

    vc_wait_ready();
    vc_wait_vblank();
    vc_clear_vram();
    vc_clear_spr_patterns();   /* redundante tras el setup de vc_clear_vram,
                                * pero explicito por seguridad */

    /* Fija el color de fondo global (BG_COLOR) al azul por defecto: el fondo
     * vacio (color 0) es transparente y deja ver BG_COLOR. */
    vc_set_bgcolor(VC_BG_COLOR_DEFAULT);

    /* Tiles: 0 vacio, 1 plataforma */
    vc_load_bg_pattern(0, tile_empty_p0, tile_empty_p1);
    vc_load_bg_pattern(1, tile_solid_p0, tile_solid_p1);

    /* Fondo vacio con una plataforma: 2 filas de tiles solidos, desde la
     * columna de PARED_IZQ hasta PARED_DER+8 (las paredes). */
    for (y = 0; y < VC_MAP_ROWS; y++) {
        for (x = 0; x < VC_MAP_COLS; x++) {
            vc_put_cell(x, y, 0);
        }
    }
    /* Plataforma (suelo) */
    for (x = 5; x < 35; x++) {
        vc_put_cell(x, 23, 1);
        vc_put_cell(x, 24, 1);
        vc_set_cell_attr(x, 23, VC_BGPAL_1, VC_ATTR_SOLID);
        vc_set_cell_attr(x, 24, VC_BGPAL_1, VC_ATTR_SOLID);
    }
    /* Paredes (bloques solidos a ambos lados de la plataforma) */
    for (y = 21; y < 23; y++) {
        vc_put_cell(5, y, 1);
        vc_set_cell_attr(5, y, VC_BGPAL_1, VC_ATTR_SOLID);
        vc_put_cell(34, y, 1);
        vc_set_cell_attr(34, y, VC_BGPAL_1, VC_ATTR_SOLID);
    }

    /* Patrones de sprite del humano: frames en los patrones 8,9,10.
     * Mapeo (piskel2c.py --map R=2 P=1 Y=3) + paleta HEART:
     * cuerpo=marron, cara=piel, detalles=negro. */
    vc_load_spr_pattern(TILE_FRAME0 + 0, f0_p0, f0_p1);
    vc_load_spr_pattern(TILE_FRAME0 + 1, f1_p0, f1_p1);
    vc_load_spr_pattern(TILE_FRAME0 + 2, f2_p0, f2_p1);
}

static void init_humano(void) {
    vc_sprite_t s;

    hx = 80; hy = (uint8_t)(Y_PLATAFORMA - HUMANO_W);   /* pies sobre la plataforma */
    hvx = 1; hframe = 0; htic = 0;

    s.x_lo  = (uint8_t)hx;
    s.y     = hy;
    s.tile  = TILE_FRAME0;
    s.flags = VC_SPPAL_0;            /* preset: cuerpo marron, cara piel, detalles negro */
    s.coll  = VC_COLL_RIGHT;         /* empieza mirando a la derecha */
    vc_sprite_set(HUMANO_SPR, &s);
}

/* ===========================================================================
 * MOVIMIENTO Y ANIMACION
 * =========================================================================== */
static void update_humano(void) {
    uint16_t prev_x;
    uint8_t  dir_flip;

    /* Avanza segun la direccion */
    prev_x = hx;
    hx = (uint16_t)(hx + hvx);

    /* Colision real contra un tile solido: el punto de colision es el BORDE
     * que avanza; si toca, rebobina la posicion (push-out) y da la vuelta. */
    vc_oam_put(HUMANO_SPR, VC_OAM_COLL, (hvx < 0) ? VC_COLL_LEFT
                                                   : VC_COLL_RIGHT);
    vc_sprite_move(HUMANO_SPR, hx, hy,
                   VC_SPPAL_0 | ((hvx < 0) ? VC_SPR_FLIP_X : 0));
    if (vc_solid_hit()) {
        hx = prev_x;
        hvx = (int8_t)-hvx;
    }

    /* Escribe el sprite con la direccion FINAL (tras un posible rebote).
     * El punto de colision tambien se ajusta al borde que avanza ahora. */
    dir_flip = (hvx < 0) ? VC_SPR_FLIP_X : 0;
    vc_oam_put(HUMANO_SPR, VC_OAM_COLL, (hvx < 0) ? VC_COLL_LEFT
                                                   : VC_COLL_RIGHT);
    vc_sprite_move(HUMANO_SPR, hx, hy, VC_SPPAL_0 | dir_flip);

    /* Animacion: cambia de frame cada 12 frames (5 animaciones/seg a 60fps) */
    if (++htic >= 12) {
        htic = 0;
        hframe = (uint8_t)((hframe + 1) % NFRAMES);
    }
    vc_oam_put(HUMANO_SPR, VC_OAM_TILE, (uint8_t)(TILE_FRAME0 + hframe));
}

/* ===========================================================================
 * HUD
 * =========================================================================== */
static void init_hud(void) {
    vc_set_raster(24, 216);
    vc_set_band2_scroll(0, 0);
    vc_set_band3_scroll(0, 0);

    vc_put_str_pal(1, 1, "DEMO DE ANIMACION (vc)", VC_TINTA(VC_BGPAL_0));
    vc_put_str_pal(1, 28, "camina, flip y rebote (colision HW)  ('q' sale)",
                   VC_TINTA(VC_BGPAL_3));
}

/* ===========================================================================
 * MAIN
 * =========================================================================== */
int main(void) {
    rom_uart_puts("\r\nDemo de animacion (vc). Pulsa 'q' para salir.\r\n");

    setup_video();
    init_hud();
    init_humano();

    while (!check_quit()) {
        vc_wait_vblank();
        update_humano();
        vc_wait_vblank_end();
    }

    vc_wait_vblank();
    vc_clear_oam();
    rom_uart_puts("\r\nSaliendo al monitor...\r\n");
    return 0;
}
