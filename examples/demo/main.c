/**
 * ============================================================================
 * main.c - Demo de la biblioteca del Core de Vídeo (vc)
 * ============================================================================
 * Ejercita las capacidades principales de la biblioteca:
 *   - Sincronización con VBLANK y espera de VIDEO_READY
 *   - Carga de tileset (patrones de fondo de 2 planos)
 *   - Fondo con scroll de camara
 *   - Sprite animado (persona1, 3 frames) que rebota
 *   - Objeto 16x16 compuesto por 4 sprites (a 1x)
 *   - Sprite 8x8 dibujado a 2x (SCALE2X) -> 16x16 en pantalla
 *   - HUD fijo mediante split de raster (bandas)
 *   - Texto (tile = ASCII)
 *   - Colision sprite<->sprite por software (vc_box_overlap)
 *
 * Uso:
 *   make
 *   LOAD GAME 0800   (o el nombre que uses en la SD)
 *   R 0800
 * ============================================================================
 */

#include <stdint.h>
#include "video.h"
#include "romapi.h"

/* ===========================================================================
 * PATRONES DE TILES (2 planos, 8x8).  color = (plano1<<1) | plano0
 * =========================================================================== */

/* Tile 0: vacio (transparente -> se ve BG_COLOR) */
static const uint8_t tile_blank_p0[8] = { 0,0,0,0,0,0,0,0 };
static const uint8_t tile_blank_p1[8] = { 0,0,0,0,0,0,0,0 };

/* Tile 1: suelo solido, color 2 (plano1 = 1) */
static const uint8_t tile_solid_p0[8] = { 0,0,0,0,0,0,0,0 };
static const uint8_t tile_solid_p1[8] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

/* Tile 2: ladrillo, color 1 (plano0 alterno) */
static const uint8_t tile_brick_p0[8] = { 0xFF,0x81,0x81,0xFF,0x99,0x99,0xFF,0xFF };
static const uint8_t tile_brick_p1[8] = { 0,0,0,0,0,0,0,0 };

/* Tile 3: punto de estrella (cielo). Cuadrado 4x4 centrado, color 3.
 * Ambos planos iguales -> cada pixel es color 3. */
static const uint8_t tile_star_p0[8] = { 0x00,0x00,0x3C,0x3C,0x3C,0x3C,0x00,0x00 };
static const uint8_t tile_star_p1[8] = { 0x00,0x00,0x3C,0x3C,0x3C,0x3C,0x00,0x00 };

/* Sprite del "rombo" a 2x (8x8 de patron -> 16x16 en pantalla), color 3. */
static const uint8_t rhom_p0[8] = { 0x18,0x3C,0x7E,0xFF,0xFF,0x7E,0x3C,0x18 };
static const uint8_t rhom_p1[8] = { 0x18,0x24,0x42,0x81,0x81,0x42,0x24,0x18 };

/* Sprite del jugador: persona1, 3 frames (8x8). color 3 (ambos planos). */
static const uint8_t pers0_p0[8] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
static const uint8_t pers0_p1[8] = { 0x3C,0x7E,0xDB,0xFF,0x7E,0x3C,0x18,0x18 };
static const uint8_t pers1_p0[8] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
static const uint8_t pers1_p1[8] = { 0x3C,0x7E,0xDB,0xFF,0x7E,0x18,0x24,0x42 };
static const uint8_t pers2_p0[8] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
static const uint8_t pers2_p1[8] = { 0x3C,0x7E,0xDB,0xFF,0x7E,0x24,0x18,0x18 };

/* Sprites del objeto 16x16 "carita": marco cuadrado + dos ojos, nariz y boca.
 * 4 cuadrantes de 8x8, color 1 (plano0=1, plano1=0).
 *
 * Rejilla 16x16 ('.' transparente, '#' color 1):
 *    ################
 *    #..............#
 *    #.##.##..##..#.#
 *    #.##.##..##..#.#
 *    #..............#
 *    #..............#
 *    #........##....#
 *    #........##....#
 *    #..............#
 *    #.##########...#
 *    #..............#
 *    #..............#
 *    #..............#
 *    #..............#
 *    #..............#
 *    ################
 */
static const uint8_t box_p0[4][8] = {
    /* cuadrante 0: x0-7, y0-7 */
    { 0xFF, 0x80, 0xB3, 0xB3, 0x80, 0x80, 0x80, 0x80 },
    /* cuadrante 1: x8-15, y0-7 */
    { 0xFF, 0x01, 0x32, 0x32, 0x01, 0x01, 0x61, 0x61 },
    /* cuadrante 2: x0-7, y8-15 */
    { 0x80, 0xBF, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF },
    /* cuadrante 3: x8-15, y8-15 */
    { 0x01, 0xF1, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF }
};

static const uint8_t box_p1[4][8] = {
    { 0,0,0,0,0,0,0,0 }, { 0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0 }, { 0,0,0,0,0,0,0,0 }
};

/* Tiles del mapa */
#define TILE_EMPTY    0
#define TILE_SOLID    1
#define TILE_BRICK    2
#define TILE_STAR     3

/* ===========================================================================
 * ESTADO DEL JUEGO
 * =========================================================================== */
#define SPR_PLAYER    0     /* sprite base del jugador (8x8) */
#define SPR_RHOM      3     /* sprite 8x8 dibujado a 2x -> 16x16 */
#define SPR_BLOCK     4     /* sprite base del bloque 16x16 (usa 4..7) */
#define PLAYER_FRAMES 3

static uint16_t px, py;      /* jugador (X 9 bits) */
static int8_t   vx, vy;
static uint8_t  frame;       /* frame de animacion */
static uint8_t  tick;        /* contador para animar */

static uint16_t bx, by;      /* bloque 16x16 */
static int8_t   bvx, bvy;

static uint16_t rx, ry;      /* rombo 8x8 a 2x */
static int8_t   rvx, rvy;

static uint16_t scroll;      /* camara (auto-scroll del fondo) */
static uint8_t  hit;         /* parpadeo al colisionar */

/* ===========================================================================
 * CARGA DE ASSETS
 * =========================================================================== */

static void load_assets(void) {
    /* Tile vacio primero: el fondo "vacio" debe ser transparente (patron 0 en
     * blanco). vc_clear_vram() ya lo deja asi; lo recargamos por claridad. */
    vc_load_bg_pattern(TILE_EMPTY, tile_blank_p0, tile_blank_p1);

    /* Tileset de fondo (formatos planares) */
    vc_load_bg_pattern(TILE_SOLID, tile_solid_p0, tile_solid_p1);
    vc_load_bg_pattern(TILE_BRICK, tile_brick_p0, tile_brick_p1);
    vc_load_bg_pattern(TILE_STAR,  tile_star_p0,  tile_star_p1);

    /* Patrones de sprite: jugador (3 frames) + rombo 2x + bloque 16x16 */
    vc_load_spr_pattern(0, pers0_p0, pers0_p1);
    vc_load_spr_pattern(1, pers1_p0, pers1_p1);
    vc_load_spr_pattern(2, pers2_p0, pers2_p1);
    vc_load_spr_pattern(SPR_RHOM, rhom_p0, rhom_p1);
    vc_load_spr_pattern(4, box_p0[0], box_p1[0]);
    vc_load_spr_pattern(5, box_p0[1], box_p1[1]);
    vc_load_spr_pattern(6, box_p0[2], box_p1[2]);
    vc_load_spr_pattern(7, box_p0[3], box_p1[3]);
}

static void build_world(void) {
    uint8_t x, y;

    /* Mapa 64x32: suelo solido abajo, estrellas dispersas sobre vacio */
    for (y = 0; y < VC_MAP_ROWS; y++) {
        for (x = 0; x < VC_MAP_COLS; x++) {
            uint8_t tile;
            if (y >= VC_MAP_ROWS - 2) {
                tile = TILE_SOLID;               /* suelo */
            } else if (((x * 7 + y * 13) & 0x1F) == 0) {
                tile = TILE_STAR;                /* estrellas dispersas */
            } else {
                tile = TILE_EMPTY;
            }
            vc_put_cell(x, y, tile);
        }
    }

    /* Suelo solido (colision) con paleta de terreno */
    for (y = VC_MAP_ROWS - 2; y < VC_MAP_ROWS; y++) {
        for (x = 0; x < VC_MAP_COLS; x++) {
            vc_set_cell_attr(x, y, VC_BGPAL_1, VC_ATTR_SOLID);
        }
    }

    /* Plataforma de ladrillo (mas ancha: columnas 4-27 = 24 tiles) */
    for (x = 4; x < 28; x++) {
        vc_put_cell(x, 20, TILE_BRICK);
        vc_set_cell_attr(x, 20, VC_BGPAL_1, VC_ATTR_SOLID);
    }
}

/* ===========================================================================
 * HUD (banda superior fija con split de raster)
 * =========================================================================== */

static void init_hud(void) {
    /* Layout vertical seguro:
     *   - La fila 0 se ve corrida (margen) por el pipeline del core.
     *   - La fila 29 no se ve en la pantalla por OVERSCAN del monitor (el core
     *     SI genera las 30 filas; el panel recorta el borde inferior).
     *     Por eso la ultima fila util en la practica es la 28.
     *   - El HUD inferior se coloca en la fila 28 (no 29) y no en la 27
     *     (limite de banda, puede solaparse con el scroll de la banda media).
     *
     *   RASTER_LINE0=24 -> lineas 0..23  = filas 0-2 (HUD arriba).
     *   RASTER_LINE1=216 -> lineas 216..239 = filas 27-29 (HUD abajo).
     */
    vc_set_raster(24, 216);
    vc_set_band2_scroll(0, 0);       /* HUD superior fijo */
    vc_set_band3_scroll(0, 0);       /* HUD inferior fijo */

    /* HUD superior (fila 1; la 0 es margen). Paleta 0 (tinta clara por defecto). */
    vc_put_str_pal(1, 1, "CORE DE VIDEO  -  DEMO vc", VC_TINTA(VC_BGPAL_0));

    /* HUD inferior (fila 28): texto a la izquierda en paleta 3,
     * estado a la derecha (update_hud escribe en la columna 28). */
    vc_put_str_pal(1, 28, "SPRITES 16x16 + 8x8@2x", VC_TINTA(VC_BGPAL_3));
}

static void update_hud(void) {
    /* Muestra la posicion del jugador y el flag de colision (columna 28) */
    char buf[9];
    uint16_t v = px;

    buf[0] = 'X';
    buf[1] = ':';
    buf[2] = (char)('0' + ((v / 100) % 10));
    buf[3] = (char)('0' + ((v / 10) % 10));
    buf[4] = (char)('0' + (v % 10));
    buf[5] = ' ';
    buf[6] = hit ? 'H' : ' ';
    buf[7] = hit ? 'T' : ' ';
    buf[8] = 0;
    vc_put_str_pal(28, 28, buf, VC_TINTA(VC_BGPAL_3));
}

/* ===========================================================================
 * ACTUALIZACION POR FRAME (llamar dentro del VBLANK)
 * =========================================================================== */

static void move_player(void) {
    /* px/py son 16 bits; vx/vy son int8_t con signo. Se suma con signo para
     * que un valor negativo RESTE (no sumar el byte sin signo, que daria 255). */
    px = (uint16_t)(px + vx);
    if (px >= 312) { px = 312; vx = (int8_t)-vx; }
    else if (px <= 8) { px = 8; vx = (int8_t)-vx; }

    py = (uint16_t)(py + vy);
    if (py >= 224) { py = 224; vy = (int8_t)-vy; }
    else if (py <= 16)  { py = 16;  vy = (int8_t)-vy; }

    /* Animacion cada 6 frames */
    if (++tick >= 6) {
        tick = 0;
        frame = (uint8_t)((frame + 1) % PLAYER_FRAMES);
    }

    /* Recoloca el sprite del jugador con el frame actual (tile = frame) */
    vc_sprite_move(SPR_PLAYER, px, py, VC_SPPAL_2);
    vc_oam_put(SPR_PLAYER, VC_OAM_TILE, frame);
}

/* Bloque 16x16 (4 sprites de 8x8 a 1x). */
#define BLOCK_W 16
static void move_block(void) {
    /* Suma con signo (bvx/bvy son int8_t). Un valor negativo debe RESTAR. */
    bx = (uint16_t)(bx + bvx);
    if (bx >= (312 - BLOCK_W)) { bx = 312 - BLOCK_W; bvx = (int8_t)-bvx; }
    else if (bx <= 8) { bx = 8; bvx = (int8_t)-bvx; }

    by = (uint16_t)(by + bvy);
    if (by >= (224 - BLOCK_W)) { by = 224 - BLOCK_W; bvy = (int8_t)-bvy; }
    else if (by <= 32) { by = 32; bvy = (int8_t)-bvy; }

    vc_sprite16_set(SPR_BLOCK, bx, by, 4, VC_SPPAL_3);   /* 16x16 a 1x */
}

/* Rombo 8x8 dibujado a 2x -> 16x16 en pantalla. */
#define RHOM_W 16
static void move_rhom(void) {
    rx = (uint16_t)(rx + rvx);
    if (rx >= (312 - RHOM_W)) { rx = 312 - RHOM_W; rvx = (int8_t)-rvx; }
    else if (rx <= 8) { rx = 8; rvx = (int8_t)-rvx; }

    ry = (uint16_t)(ry + rvy);
    if (ry >= (224 - RHOM_W)) { ry = 224 - RHOM_W; rvy = (int8_t)-rvy; }
    else if (ry <= 16) { ry = 16; rvy = (int8_t)-rvy; }

    /* SCALE2X: un solo sprite de 8x8 dibujado al doble. */
    vc_sprite_move(SPR_RHOM, rx, ry, VC_SPPAL_1 | VC_SPR_SCALE2X);
    vc_oam_put(SPR_RHOM, VC_OAM_TILE, SPR_RHOM);
}

static void update_frame(void) {
    move_player();
    move_block();
    move_rhom();

    /* auto-scroll suave del fondo */
    scroll = (uint16_t)((scroll + 1) & 0x01FF);
    vc_set_scroll_x(scroll);

    /* colision sprite<->sprite por software (jugador 8x8 vs bloque 16x16).
     * Usa la API de cajas (nucleo en asm): construye las dos cajas y solapa. */
    {
        vc_box_t pb, bb;
        vc_box_from_sprite(&pb, px, (uint16_t)py, 8);
        vc_box_from_sprite(&bb, bx, (uint16_t)by, BLOCK_W);
        if (vc_box_overlap(&pb, &bb)) {
            /* rebote mutuo: invierte la direccion de ambos en X e Y */
            vx = -vx; bvx = -bvx;
            vy = -vy; bvy = -bvy;
            hit = 1;
        } else {
            hit = 0;
        }
    }

    /* la colision contra tiles solidos la reporta el hardware */
    if (vc_solid_hit()) {
        hit = 1;
    }

    update_hud();
}

/* ===========================================================================
 * ENTRADA POR UART: salir con 'q'
 * ===========================================================================
 * Lee la UART sin bloquear. Devuelve 1 si el usuario pulso 'q' (o 'Q').
 * Si no, descarta los caracteres que no interesan para no acumularlos.
 * ========================================================================= */
static uint8_t check_quit(void) {
    while (rom_uart_rx_ready()) {
        char c = rom_uart_getc();
        if (c == 'q' || c == 'Q') {
            return 1;
        }
        /* otros caracteres se ignoran */
    }
    return 0;
}

/* ===========================================================================
 * MAIN
 * =========================================================================== */

int main(void) {
    rom_uart_puts("\r\nvc lib demo - Core de Video\r\n");
    rom_uart_puts("Pulsa 'q' para salir al monitor.\r\n");

    vc_wait_ready();                 /* VRAM lista antes de tocar nada */
    vc_wait_vblank();

    vc_clear_vram();                 /* setup HW: limpia VRAM y recarga la fuente */

    /* Fija el color de fondo global (BG_COLOR) al azul por defecto: el fondo
     * vacio (color 0) es transparente y deja ver BG_COLOR. */
    vc_set_bgcolor(VC_BG_COLOR_DEFAULT);

    load_assets();
    build_world();
    init_hud();

    /* Jugador */
    px = 48; py = 48; vx = 2; vy = 1; frame = 0; tick = 0;
    {
        vc_sprite_t s;
        s.x_lo  = (uint8_t)px;
        s.y     = (uint8_t)py;
        s.tile  = 0;
        s.flags = VC_SPPAL_2;
        s.coll  = VC_COLL_FEET;
        vc_sprite_set(SPR_PLAYER, &s);
    }

    /* Bloque 16x16 (4 sprites a 1x) */
    bx = 150; by = 120; bvx = 1; bvy = 2;
    vc_sprite16_set(SPR_BLOCK, bx, by, 4, VC_SPPAL_3);

    /* Rombo 8x8 a 2x (se ve 16x16) */
    rx = 200; ry = 60; rvx = -2; rvy = 1;
    {
        vc_sprite_t s;
        s.x_lo  = (uint8_t)rx;
        s.y     = (uint8_t)ry;
        s.tile  = SPR_RHOM;
        s.flags = VC_SPPAL_1 | VC_SPR_SCALE2X;
        s.coll  = VC_COLL_CENTER;
        vc_sprite_set(SPR_RHOM, &s);
    }

    scroll = 0;

    /* Bucle principal: una iteracion = un frame */
    while (!check_quit()) {
        vc_wait_vblank();
        update_frame();
        vc_wait_vblank_end();
    }

    /* Salida limpia: deshabilitar sprites y scroll, avisar, y devolver el
     * control al monitor. startup.s salta a $8000 (monitor) al retornar main(). */
    vc_wait_vblank();
    vc_clear_oam();          /* apaga los 32 sprites */
    vc_set_scroll_x(0);
    vc_set_scroll_y(0);
    rom_uart_puts("\r\nSaliendo al monitor...\r\n");
    return 0;
}
