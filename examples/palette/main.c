/**
 * ============================================================================
 * examples/palette/main.c - Demo de PALETAS PROGRAMABLES (vc)
 * ============================================================================
 * Muestra como reescribir las paletas del core ($D813-$D815) para cambiar los
 * colores EN VIVO, sin tocar los patrones ni el tilemap.
 *
 * Escena:
 *   - Un fondo de tiles con un patron de prueba (indice 3 = claro).
 *   - Un sprite (rombo) centrado, en paleta de sprite 1.
 *   - Las paletas 0 (fondo) y 1 (sprite) se "recorren" ciclicamente, asi que
 *     los colores del fondo y del rombo cambian solos.
 *   - Tecla 'q' por UART para salir al monitor.
 *
 * Puntos que demuestra:
 *   - vc_pal_set_bg/spr(): fijar un color por (paleta, color).
 *   - vc_pal_load_bg/spr(): cargar los 4 colores de una paleta de golpe.
 *   - vc_pal_ptr()/vc_pal_set(): acceso de bajo nivel.
 *   - VC_RGB444()/VC_RGB888(): construir colores.
 * ============================================================================
 */

#include <stdint.h>
#include "video.h"
#include "romapi.h"

/* --- Patron de tile de prueba: color 3 (el mas claro de la paleta) --- */
static const uint8_t tile_p0[8] = {0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF};
static const uint8_t tile_p1[8] = {0x00,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x00};

/* --- Tile vacio (color 0 = transparente): evita ver basura del arranque --- */
static const uint8_t tile_empty_p0[8] = {0,0,0,0,0,0,0,0};
static const uint8_t tile_empty_p1[8] = {0,0,0,0,0,0,0,0};

/* --- Rombo de sprite: color 3 con contorno color 1 --- */
static const uint8_t spr_p0[8] = {0x18,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0x18};
static const uint8_t spr_p1[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

/* ===========================================================================
 * PALETAS: dos juegos (el "A" y el "B") para paleta de fondo 1 y sprite 1.
 * ========================================================================= */

/* Fondo: 4 colores RGB444. color0 es transparente (no se usa). */
static const uint16_t bg_palA[4] = {
    VC_RGB888(0x002040),   /* 0: no se ve (fondo transparente) */
    VC_RGB888(0x004080),   /* 1: azul oscuro */
    VC_RGB888(0x00A0E0),   /* 2: azul claro */
    VC_RGB888(0xE0F0FF)    /* 3: casi blanco */
};
static const uint16_t bg_palB[4] = {
    VC_RGB888(0x201000),
    VC_RGB888(0x804000),   /* 1: marron */
    VC_RGB888(0xE08040),   /* 2: naranja */
    VC_RGB888(0xFFF0C0)    /* 3: crema */
};

/* Sprite: 4 colores RGB444. color0 transparente.
 * Se EVITAN los azules: el fondo (BG_COLOR) es azul cielo, y un rombo azul
 * se camufla. Se usan colores calidos que siempre contrastan. */
static const uint16_t spr_palA[4] = {
    VC_RGB444(0,0,0),
    VC_RGB444(0xF,0,0),    /* 1: rojo */
    VC_RGB444(0xF,0x6,0),  /* 2: naranja */
    VC_RGB444(0xF,0xF,0)   /* 3: amarillo */
};
static const uint16_t spr_palB[4] = {
    VC_RGB444(0,0,0),
    VC_RGB444(0xF,0,0xF),  /* 1: magenta */
    VC_RGB444(0xF,0,0),    /* 2: rojo */
    VC_RGB444(0xF,0xF,0xF) /* 3: blanco */
};

/* ===========================================================================
 * UART
 * =========================================================================== */
static uint8_t check_quit(void) {
    while (rom_uart_rx_ready()) {
        char c = rom_uart_getc();
        if (c == 'q' || c == 'Q') return 1;
    }
    return 0;
}

/* ===========================================================================
 * SETUP
 * =========================================================================== */
static void setup_video(void) {
    uint8_t x, y;

    vc_wait_ready();
    vc_wait_vblank();
    vc_clear_vram();

    /* Patrones de fondo: 0 = vacio (transparente), 1 = bloque de prueba.
     * Cargar el 0 es IMPRESCINDIBLE: si no, el fondo vacio muestra el
     * patron 0, que trae basura del arranque. */
    vc_load_bg_pattern(0, tile_empty_p0, tile_empty_p1);
    vc_load_bg_pattern(1, tile_p0, tile_p1);

    /* Fija el color de fondo global (BG_COLOR) al azul por defecto: el fondo
     * vacio (color 0) es transparente y deja ver BG_COLOR. No dependemos del
     * valor de arranque del hardware. */
    vc_set_bgcolor(VC_BG_COLOR_DEFAULT);

    /* Fondo: marco de tiles del patron 1, en paleta de fondo 1 */
    for (x = 0; x < VC_MAP_COLS; x++) {
        for (y = 0; y < VC_MAP_ROWS; y++) {
            vc_put_cell(x, y, 0);
        }
    }
    for (x = 2; x < 38; x++) {
        vc_put_cell(x, 4, 1);
        vc_set_cell_attr(x, 4, 1, 0);   /* paleta de fondo 1 */
        vc_put_cell(x, 24, 1);
        vc_set_cell_attr(x, 24, 1, 0);
    }
    for (y = 4; y <= 24; y++) {
        vc_put_cell(2, y, 1);
        vc_set_cell_attr(2, y, 1, 0);
        vc_put_cell(37, y, 1);
        vc_set_cell_attr(37, y, 1, 0);
    }

    /* Sprite rombo, paleta de sprite 1 */
    vc_load_spr_pattern(0, spr_p0, spr_p1);
    {
        vc_sprite_t s;
        s.x_lo  = 100;
        s.y     = 96;
        s.tile  = 0;
        s.flags = VC_SPPAL_1 | VC_SPR_SCALE2X;   /* paleta sprite 1 */
        s.coll  = VC_COLL_CENTER;
        vc_sprite_set(0, &s);
    }
}

/* ===========================================================================
 * TEXTO
 * =========================================================================== */
static void init_text(void) {
    vc_set_raster(24, 216);
    vc_set_band2_scroll(0, 0);
    vc_set_band3_scroll(0, 0);

    vc_put_str_pal(1, 1, "PALETAS PROGRAMABLES (vc)", VC_TINTA(VC_BGPAL_0));
    vc_put_str_pal(1, 28, "colores en vivo  ('q' sale)", VC_TINTA(VC_BGPAL_3));
}

/* ===========================================================================
 * CICLO DE PALETAS
 * ===========================================================================
 * Interpola los 4 colores de la paleta de fondo 1 y de la de sprite 1 entre el
 * juego A y el B, canal a canal. El sprite cambia de color de forma continua
 * (no parpadea).
 */
static void update_palettes(uint8_t t) {
    uint8_t i;
    uint8_t m;

    /* Factor de mezcla 0..16 (t de 0..31 -> 0..16..0) */
    m = (t < 16) ? t : (uint8_t)(31 - t);

    /* Mezcla canal a canal de dos colores RGB444. */
    #define MIX(a, b, mask) \
        (uint16_t)((((a) & (mask)) * (16 - m) + ((b) & (mask)) * m) >> 4)

    /* Fondo: color 0 es transparente, no se toca. */
    for (i = 1; i < 4; i++) {
        vc_pal_set_bg(1, i, (uint16_t)(MIX(bg_palA[i], bg_palB[i], 0xF00) |
                                       MIX(bg_palA[i], bg_palB[i], 0x0F0) |
                                       MIX(bg_palA[i], bg_palB[i], 0x00F)));
    }

    /* Sprite: mismo tratamiento, color 0 transparente. */
    for (i = 1; i < 4; i++) {
        vc_pal_set_spr(1, i, (uint16_t)(MIX(spr_palA[i], spr_palB[i], 0xF00) |
                                        MIX(spr_palA[i], spr_palB[i], 0x0F0) |
                                        MIX(spr_palA[i], spr_palB[i], 0x00F)));
    }

    #undef MIX
}

/* ===========================================================================
 * MAIN
 * =========================================================================== */
int main(void) {
    uint8_t tick = 0;
    uint8_t ctr = 0;

    rom_uart_puts("\r\nDemo de paletas (vc). Pulsa 'q' para salir.\r\n");

    setup_video();
    init_text();

    while (!check_quit()) {
        vc_wait_vblank();

        /* Cambia las paletas unas 15 veces/seg (pulso de la animacion). */
        if (++ctr >= 4) {
            ctr = 0;
            update_palettes(tick);
            tick = (uint8_t)((tick + 1) & 31);
        }

        vc_wait_vblank_end();
    }

    /* Restaura las paletas por defecto del hardware (opcional) antes de salir. */
    vc_pal_load_bg(1, bg_palA);
    vc_pal_load_spr(1, spr_palA);

    vc_wait_vblank();
    vc_clear_oam();
    rom_uart_puts("\r\nSaliendo al monitor...\r\n");
    return 0;
}
