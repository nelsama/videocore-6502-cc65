/**
 * ============================================================================
 * examples/collision/main.c - Ejemplo de deteccion de colisiones
 * ============================================================================
 * Demuestra la API de colisiones de la biblioteca (nucleo en ensamblador,
 * src/collide.s):
 *
 *   - vc_box_t / vc_box_from_sprite()   construye cajas desde posiciones
 *   - vc_box_overlap(a, b)              solapamiento caja contra caja (AABB)
 *   - vc_box_contains(b, px, py)        test puntual punto-dentro-de-caja
 *   - vc_solid_hit()                    colision sprite<->tile del hardware
 *
 * Hay 4 objetos 8x8 rebotando. Cuando dos chocan, se ponen en rojo y rebotan.
 * Una "nave" 16x16 rebota tambien; si el CENTRO de un objeto cae dentro de la
 * nave (test puntual), el objeto se marca.
 *
 * Pulsa 'q' por la UART para salir al monitor.
 * ============================================================================
 */

#include <stdint.h>
#include "video.h"
#include "romapi.h"

/* ===========================================================================
 * PATRONES (2 planos, 8x8).
 * ========================================================================= */

/* Objeto 8x8: circulo color 3 */
static const uint8_t dot_p0[8] = { 0x3C,0x7E,0xFF,0xFF,0xFF,0xFF,0x7E,0x3C };
static const uint8_t dot_p1[8] = { 0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C };

/* Nave 8x8 (se dibuja a 2x -> 16x16): carita */
static const uint8_t ship_p0[8] = { 0x18,0x3C,0x7E,0x7E,0xFF,0xFF,0x66,0x66 };
static const uint8_t ship_p1[8] = { 0x18,0x3C,0x5A,0x5A,0xFF,0xFF,0x66,0x66 };

/* Tiles de fondo: 0 = vacio, 1 = solido (para vc_solid_hit) */
static const uint8_t tile_empty_p0[8] = { 0,0,0,0,0,0,0,0 };
static const uint8_t tile_empty_p1[8] = { 0,0,0,0,0,0,0,0 };
static const uint8_t tile_solid_p0[8] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static const uint8_t tile_solid_p1[8] = { 0,0,0,0,0,0,0,0 };

/* ===========================================================================
 * ESTADO
 * =========================================================================== */
#define NUM_OBJS   4
#define OBJ_SIZE   8          /* objetos 8x8 (la caja coincide con el sprite) */
#define SHIP_SPR   8           /* slot de OAM de la nave */
#define SHIP_SIZE  16          /* en pantalla (8x8 a 2x) */

static uint16_t ox[NUM_OBJS], oy[NUM_OBJS];
static int8_t   ovx[NUM_OBJS], ovy[NUM_OBJS];
static uint8_t  ocol[NUM_OBJS];   /* 1 = colisionando este frame */

static const uint8_t pal_normal[NUM_OBJS] = {
    VC_SPPAL_1, VC_SPPAL_3, VC_SPPAL_2, VC_SPPAL_0
};

static uint16_t sx, sy;
static int8_t   svx, svy;
static uint8_t  center_in;        /* 1 si el centro de un objeto cae en la nave */
static uint8_t  nchoques;         /* contador de choques objeto-objeto (para HUD) */

/* ===========================================================================
 * ENTRADA / VIDEO
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

    /* Fija el color de fondo global (BG_COLOR) al azul por defecto: el fondo
     * vacio (color 0) es transparente y deja ver BG_COLOR. */
    vc_set_bgcolor(VC_BG_COLOR_DEFAULT);

    vc_load_bg_pattern(0, tile_empty_p0, tile_empty_p1);
    vc_load_bg_pattern(1, tile_solid_p0, tile_solid_p1);

    /* Fondo vacio + una franja solida abajo (para vc_solid_hit) */
    for (y = 0; y < VC_MAP_ROWS; y++) {
        for (x = 0; x < VC_MAP_COLS; x++) {
            vc_put_cell(x, y, 0);
        }
    }
    for (x = 0; x < VC_MAP_COLS; x++) {
        vc_put_cell(x, VC_MAP_ROWS - 2, 1);
        vc_set_cell_attr(x, VC_MAP_ROWS - 2, VC_BGPAL_1, VC_ATTR_SOLID);
    }

    vc_load_spr_pattern(0, dot_p0, dot_p1);
    vc_load_spr_pattern(SHIP_SPR, ship_p0, ship_p1);
}

static void init_objects(void) {
    uint8_t i;

    nchoques = 0;

    /* Trayectorias que SE CRUZAN por el centro para que choquen seguido:
     * dos objetos van hacia el centro y dos salen de él, con velocidades
     * distintas para que los cruces no sean simétricos. */
    ox[0] = 40;  oy[0] = 40;  ovx[0] = 3;  ovy[0] = 3;   /* hacia abajo-derecha */
    ox[1] = 260; oy[1] = 40;  ovx[1] = -3; ovy[1] = 3;   /* hacia abajo-izquierda */
    ox[2] = 40;  oy[2] = 170; ovx[2] = 3;  ovy[2] = -3;  /* hacia arriba-derecha */
    ox[3] = 260; oy[3] = 170; ovx[3] = -3; ovy[3] = -3;  /* hacia arriba-izquierda */

    /* Todos convergen hacia el centro en el mismo instante -> choque seguro. */

    for (i = 0; i < NUM_OBJS; i++) {
        vc_sprite_t s;
        s.x_lo  = (uint8_t)ox[i];
        s.y     = (uint8_t)oy[i];
        s.tile  = 0;
        s.flags = pal_normal[i];
        s.coll  = VC_COLL_CENTER;
        vc_sprite_set(i, &s);
        ocol[i] = 0;
    }

    sx = 152; sy = 112; svx = 2; svy = 2;
    {
        vc_sprite_t s;
        s.x_lo  = (uint8_t)sx;
        s.y     = (uint8_t)sy;
        s.tile  = SHIP_SPR;
        s.flags = VC_SPPAL_3 | VC_SPR_SCALE2X;   /* 16x16 en pantalla */
        s.coll  = VC_COLL_CENTER;
        vc_sprite_set(SHIP_SPR, &s);
    }
}

/* ===========================================================================
 * MOVIMIENTO (suma CON SIGNO: los int8_t negativos deben restar)
 * =========================================================================== */
static void move_object(uint8_t i) {
    uint16_t nx;
    uint8_t  ny;

    nx = (uint16_t)(ox[i] + ovx[i]);
    ny = (uint8_t)(oy[i] + ovy[i]);

    if (nx >= 312) { nx = 312; ovx[i] = (int8_t)-ovx[i]; }
    else if (nx <= 8) { nx = 8; ovx[i] = (int8_t)-ovx[i]; }

    if (ny >= (224 - OBJ_SIZE)) { ny = 224 - OBJ_SIZE; ovy[i] = (int8_t)-ovy[i]; }
    else if (ny <= 16) { ny = 16; ovy[i] = (int8_t)-ovy[i]; }

    ox[i] = nx;
    oy[i] = ny;
}

static void move_ship(void) {
    sx = (uint16_t)(sx + svx);
    sy = (uint8_t)(sy + svy);

    if (sx >= (312 - SHIP_SIZE)) { sx = 312 - SHIP_SIZE; svx = (int8_t)-svx; }
    else if (sx <= 8) { sx = 8; svx = (int8_t)-svx; }

    if (sy >= (224 - SHIP_SIZE)) { sy = 224 - SHIP_SIZE; svy = (int8_t)-svy; }
    else if (sy <= 16) { sy = 16; svy = (int8_t)-svy; }

    vc_sprite_move(SHIP_SPR, sx, sy, VC_SPPAL_3 | VC_SPR_SCALE2X);
    vc_oam_put(SHIP_SPR, VC_OAM_TILE, SHIP_SPR);
}

/* ===========================================================================
 * UN FRAME
 * =========================================================================== */
static void update_frame(void) {
    vc_box_t ship_box;
    uint8_t i, j;

    /* Mueve todos los objetos */
    for (i = 0; i < NUM_OBJS; i++) {
        move_object(i);
        ocol[i] = 0;
    }

    /* --- Colision objeto <-> objeto: vc_box_overlap --- */
    for (i = 0; i < NUM_OBJS; i++) {
        vc_box_t a;
        vc_box_from_sprite(&a, ox[i], oy[i], OBJ_SIZE);
        for (j = (uint8_t)(i + 1); j < NUM_OBJS; j++) {
            vc_box_t b;
            vc_box_from_sprite(&b, ox[j], oy[j], OBJ_SIZE);
            if (vc_box_overlap(&a, &b)) {
                ovx[i] = (int8_t)-ovx[i]; ovy[i] = (int8_t)-ovy[i];
                ovx[j] = (int8_t)-ovx[j]; ovy[j] = (int8_t)-ovy[j];
                ocol[i] = 1;
                ocol[j] = 1;
                nchoques++;              /* cuenta el choque para el HUD */
            }
        }
    }

    /* --- Colision objeto <-> nave: vc_box_overlap + rebote del objeto ---
     * La nave NO rebota (es "pesada"); solo rebota el objeto que la golpea.
     * Se aplica push-out para que el objeto no quede pegado a la nave. */
    vc_box_from_sprite(&ship_box, sx, sy, SHIP_SIZE);
    for (i = 0; i < NUM_OBJS; i++) {
        vc_box_t o;
        vc_box_from_sprite(&o, ox[i], oy[i], OBJ_SIZE);
        if (vc_box_overlap(&o, &ship_box)) {
            /* rebote: invierte la direccion del objeto */
            ovx[i] = (int8_t)-ovx[i];
            ovy[i] = (int8_t)-ovy[i];

            /* push-out: separa el objeto de la nave segun de donde venga.
             * Compara centros para decidir el eje dominante (mas simple que
             * separar por el eje de menor penetracion). */
            if (ox[i] < sx) {
                ox[i] = (ox[i] >= 4) ? (uint16_t)(ox[i] - 4) : 8;
            } else {
                ox[i] = (uint16_t)(ox[i] + 4);
            }
            if (oy[i] < sy) {
                oy[i] = (oy[i] >= 20) ? (uint8_t)(oy[i] - 4) : 16;
            } else {
                oy[i] = (uint8_t)(oy[i] + 4);
            }

            ocol[i] = 1;
            nchoques++;
        }
    }

    /* --- Test puntual: centro de un objeto dentro de la nave --- */
    center_in = 0;
    for (i = 0; i < NUM_OBJS; i++) {
        if (vc_box_contains(&ship_box, (uint16_t)(ox[i] + (OBJ_SIZE / 2)),
                                       (uint16_t)(oy[i] + (OBJ_SIZE / 2)))) {
            center_in = 1;
            ocol[i] = 1;
        }
    }

    /* Repinta sprites con su color (rojo si chocan; cada uno su paleta si no) */
    for (i = 0; i < NUM_OBJS; i++) {
        uint8_t flags = ocol[i] ? VC_SPPAL_2 : pal_normal[i];
        vc_sprite_move(i, ox[i], oy[i], flags);
        vc_oam_put(i, VC_OAM_TILE, 0);
    }

    move_ship();

    /* HUD fijo (fila 28; la 0 y la 29 no son seguras por overscan) */
    {
        char buf[8];

        vc_put_str_pal(1, 1, "EJEMPLO DE COLISIONES (vc)", VC_TINTA(VC_BGPAL_0));
        vc_put_str_pal(1, 28, "rojo=choque  X=centro-en-nave  S=solido  C:",
                       VC_TINTA(VC_BGPAL_3));

        /* contador de choques objeto-objeto, en 3 digitos */
        {
            uint8_t n = nchoques;
            buf[0] = (char)('0' + ((n / 100) % 10));
            buf[1] = (char)('0' + ((n / 10) % 10));
            buf[2] = (char)('0' + (n % 10));
            buf[3] = 0;
            vc_put_str_pal(37, 28, buf, VC_TINTA(VC_BGPAL_0));
        }

        /* indicadores X (centro-en-nave) y S (tile solido) */
        buf[0] = center_in ? 'X' : '-';
        buf[1] = vc_solid_hit() ? 'S' : '-';
        buf[2] = 0;
        vc_put_str_pal(30, 1, buf, VC_TINTA(VC_BGPAL_3));
    }
}

/* ===========================================================================
 * MAIN
 * =========================================================================== */
int main(void) {
    rom_uart_puts("\r\nEjemplo de colisiones (vc). Pulsa 'q' para salir.\r\n");

    setup_video();
    init_objects();

    while (!check_quit()) {
        vc_wait_vblank();
        update_frame();
        vc_wait_vblank_end();
    }

    vc_wait_vblank();
    vc_clear_oam();
    rom_uart_puts("\r\nSaliendo al monitor...\r\n");
    return 0;
}
