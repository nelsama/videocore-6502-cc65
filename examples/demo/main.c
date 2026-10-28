/**
 * ============================================================================
 * main.c - Demo de la biblioteca del Core de Video (vc)
 * ============================================================================
 * Ejercita las capacidades principales de la biblioteca:
 *   - Sincronizacion con VBLANK y espera de VIDEO_READY
 *   - Carga de tileset (patrones de fondo de 2 planos)
 *   - Fondo con scroll de camara
 *   - Sprite animado (persona1, 3 frames) que rebota
 *   - Objeto 16x16 compuesto por 4 sprites (a 1x)
 *   - Sprite 8x8 dibujado a 2x (SCALE2X) -> 16x16 en pantalla
 *   - HUD fijo mediante banda de raster superior (RASTER_LINE0)
 *   - Texto (tile = ASCII)
 *   - Colision sprite<->sprite por software (vc_box_overlap)
 *   - Colision sprite<->tile solido POR HARDWARE (vc_solid_hit)
 *
 * Ojo al combinar SOLID_HIT con bandas de scroll: el hardware tiene UNA sola
 * senal de scroll y el barrido usa la de la ultima banda dibujada. Esta demo
 * deja la zona de juego SIN banda precisamente para que la colision sea exacta
 * y sirva de banco de pruebas de vc_solid_hit().
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
/* Slots de OAM (posiciones de sprite en pantalla) */
#define SPR_PLAYER    0     /* sprite base del jugador (8x8) */
#define SPR_RHOM      3     /* sprite 8x8 dibujado a 2x -> 16x16 */
#define SPR_BLOCK     4     /* sprite base del bloque 16x16 (usa 4..7) */
#define PLAYER_FRAMES 3
/* Frames que el aviso de colision permanece encendido tras el contacto: el
 * hardware solo lo levanta 1-2 frames y a simple vista no se veria. */
#define HIT_HOLD      30

/* Plataforma de ladrillo: fila 20 del tilemap (8 px por celda). */
#define PLATAFORMA_FILA  20
#define PLATAFORMA_PY   (PLATAFORMA_FILA * 8)   /* 160 px */
#define PLATAFORMA_COL0  4                      /* primera columna */
#define PLATAFORMA_COL1  28                     /* fin (exclusivo)  */
/* Frames de bloqueo tras un rebote contra el solido, para no invertir varias
 * veces con el mismo flag (dura el frame entero). */
#define SOLID_LOCK_FRAMES 8

/* Suelo del mapa: las filas 30-31 son solidas (ver build_world). */
#define SUELO_PY  (30 * 8)   /* 240: primera fila del suelo */

/* Que toco al activarse SOLID_HIT (el flag del hardware es global, asi que el
 * dato solo se obtiene mirando donde esta el jugador). */
#define HIT_NINGUNO    0
#define HIT_PLATAFORMA 1
#define HIT_SUELO      2
#define HIT_OTRO       3   /* ninguno de los dos: sprite ajeno colisionando */

/* vc_solid_hit() es GLOBAL: dice "alguien choco", no quien. Para que solo el
 * JUGADOR pueda activarlo, el bloque y el rombo se "aparcan":
 *
 *   - COLL_POINT en la esquina superior izquierda (0,0), y
 *   - se les impide bajar hasta la fila de la plataforma (fila 20 = y 160).
 *
 * Asi su punto de colision nunca cae sobre un ladrillo solido.
 *
 * Trampas que NO bastan por si solas:
 *   - Dejar el COLL que pone vc_sprite16_set (centro): cae 4 px mas abajo.
 *   - Solo poner (0,0) sin limitar el movimiento: al cruzar y=160 el punto
 *     entra en la fila solida y dispara el flag.
 *   - Confiar en Y >= 248: el barrido NO filtra por Y, recorre los 32.
 */
#define COLL_APARCADO  VC_COLL_POINT(0, 0)

/* Techo de movimiento para los objetos que no deben colisionar: se quedan por
 * encima de la plataforma (y=160), con margen.
 *
 * CONSECUENCIA VISIBLE Y ACEPTADA: el bloque y el rombo solo recorren la mitad
 * superior de la pantalla (y = 16..144), asi que parecen rebotar "en
 * horizontal" a la altura de la plataforma. Es deliberado: es el precio de que
 * su COLL_POINT nunca caiga en la fila 20 y el flag siga siendo fiable.
 *
 * Alternativa si algun dia se quiere movimiento completo: quitar VC_ATTR_SOLID
 * de la fila 20 y validar la plataforma solo por software. Se descarto porque
 * entonces la demo dejaria de probar vc_solid_hit() contra la plataforma. */
#define Y_APARCADO_MAX   144

/* Indices de PATRON de sprite (0-63).
 * Los patrones 0-2 arrancan con arte de demo del core (y el setup los deja a
 * cero); no estan reservados, pero el arte propio empieza en el 3 para no
 * depender de sobrescribirlos. */
#define PAT_PLAYER    3     /* jugador: frames 3, 4 y 5            */
#define PAT_RHOM      6     /* rombo 2x                            */
#define PAT_BLOCK     7     /* bloque 16x16: 7..10 (4 cuadrantes)  */

static uint16_t px, py;      /* jugador 8x8 */
static uint16_t py_ant;      /* py en el frame anterior (SOLID_HIT llega con 1 frame de latencia) */
static int8_t   vx, vy;
static uint8_t  frame;       /* frame de animacion */
static uint8_t  tick;        /* contador para animar */

static uint16_t bx, by;      /* bloque 16x16 */
static int8_t   bvx, bvy;

static uint16_t rx, ry;      /* rombo 8x8 a 2x */
static int8_t   rvx, rvy;

static uint16_t scroll;      /* camara (auto-scroll del fondo) */
static uint8_t  hit;         /* colision sprite<->sprite (cajas, software) */
static uint8_t  hit_solid;   /* SOLID_HIT + donde ocurrio (HIT_*)          */
static uint8_t  hit_hold;    /* frames visibles del aviso HIT SOLID */
static uint8_t  hit_tipo;    /* HIT_* que provoco el ultimo aviso */
static uint8_t  box_hold;    /* frames visibles del aviso HIT BOX   */
static uint8_t  solid_lock;  /* antirrebote del choque con tile solido */

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
    vc_load_spr_pattern(PAT_PLAYER + 0, pers0_p0, pers0_p1);
    vc_load_spr_pattern(PAT_PLAYER + 1, pers1_p0, pers1_p1);
    vc_load_spr_pattern(PAT_PLAYER + 2, pers2_p0, pers2_p1);
    vc_load_spr_pattern(PAT_RHOM, rhom_p0, rhom_p1);
    vc_load_spr_pattern(PAT_BLOCK + 0, box_p0[0], box_p1[0]);
    vc_load_spr_pattern(PAT_BLOCK + 1, box_p0[1], box_p1[1]);
    vc_load_spr_pattern(PAT_BLOCK + 2, box_p0[2], box_p1[2]);
    vc_load_spr_pattern(PAT_BLOCK + 3, box_p0[3], box_p1[3]);
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

    /* Suelo solido (colision) con paleta de terreno.
     * Solo las filas VISIBLES (30). La fila 31 queda fuera de pantalla y es
     * donde caen los sprites aparcados (Y = 248 -> fila 31): el barrido de
     * colision del hardware NO filtra por Y, asi que marcarla solida haria que
     * un sprite deshabilitado activara SOLID_HIT de forma permanente. */
    for (y = VC_MAP_ROWS - 2; y < VC_MAP_ROWS - 1; y++) {
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
 * HUD (banda superior fija)
 * ===========================================================================
 * IMPORTANTE para que esta demo sirva de banco de pruebas de vc_solid_hit():
 *
 *   Con SOLID_HIT y SPLIT de raster, el barrido usa el scroll de la ULTIMA
 *   banda dibujada, que el hardware reescribe en una unica senal. Si la banda
 *   inferior estuviera fija, la colision se haria con scroll 0 mientras el
 *   mundo scrollea, y saltarian falsos positivos.
 *
 *   Por eso aqui solo se define la banda SUPERIOR (RASTER_LINE0): el HUD de
 *   arriba queda fijo, el resto de la pantalla usa el scroll normal (el mismo
 *   que ve el barrido) y la colision es exacta. RASTER_LINE1 = 0xFF (sin banda
 *   inferior).
 *
 *   Consecuencia: el HUD inferior ya no puede ser fijo. Se dibuja en la banda
 *   media, asi que el auto-scroll del mundo se lo lleva. Por eso los avisos de
 *   colision se muestran ARRIBA, dentro de la banda fija.
 */

#define HUD_FILA_MENU    1   /* fila 1: titulo + X del jugador */
#define HUD_FILA_ESTADO  2   /* fila 2: avisos de colision */

static void init_hud(void) {
    /*   RASTER_LINE0 = 24 -> lineas 0..23 = filas 0-2 (HUD arriba, fijo).
     *   RASTER_LINE1 = 0xFF -> sin banda inferior: la colision usa el scroll
     *                         normal, que es el que ve el jugador. */
    vc_set_raster(24, 0xFF);
    vc_set_band2_scroll(0, 0);       /* HUD superior fijo */

    /* HUD superior (fila 1; la 0 se ve corrida por el pipeline del core).
     * Titulo en la fila 1; en la fila 2 va X del jugador + estado de colision. */
    vc_put_str_pal(1, HUD_FILA_MENU, "CORE DE VIDEO - DEMO vc", VC_TINTA(VC_BGPAL_0));
}

static void update_hud(void) {
    /* Todo el HUD va en la banda superior, que NO scrollea. */
    char buf[9];
    uint16_t v = px;

    buf[0] = 'X';
    buf[1] = ':';
    buf[2] = (char)('0' + ((v / 100) % 10));
    buf[3] = (char)('0' + ((v / 10) % 10));
    buf[4] = (char)('0' + (v % 10));
    buf[5] = 0;
    vc_put_str_pal(1, HUD_FILA_ESTADO, buf, VC_TINTA(VC_BGPAL_0));

    /* Indicador de colision sprite<->tile SOLIDO (vc_solid_hit).
     * El hardware lo levanta solo 1-2 frames y a simple vista no se veria, asi
     * que se mantiene unos frames (HIT_HOLD). */
    if (hit_solid) {
        hit_hold = HIT_HOLD;
        hit_tipo = hit_solid;
    } else if (hit_hold) {
        hit_hold--;
    }

    /* El aviso distingue QUE se golpeo: el flag del hardware no lo dice. */
    if (hit_hold) {
        const char *msg;
        if (hit_tipo == HIT_PLATAFORMA)      msg = "PLATAFORMA";
        else if (hit_tipo == HIT_SUELO)       msg = "PISO      ";
        else if (hit_tipo == HIT_OTRO)        msg = "OTRO!!    ";
        else                                 msg = "SOLID     ";
        /* 10 columnas fijas: se rellena siempre para borrar el aviso previo.
         * Empieza en la 12 para no pisar la X del jugador (columnas 1-5). */
        vc_put_str_pal(16, HUD_FILA_ESTADO, msg, VC_TINTA(VC_BGPAL_2));
    } else {
        vc_put_str_pal(16, HUD_FILA_ESTADO, "          ", VC_TINTA(VC_BGPAL_2));
    }

    /* Indicador de colision sprite<->sprite (cajas, SOFTWARE: vc_box_overlap).
     * Es la del bloque 16x16; distinta de la del tile solido. */
    if (hit) {
        box_hold = HIT_HOLD;
    } else if (box_hold) {
        box_hold--;
    }

    if (box_hold) {
        vc_put_str_pal(30, HUD_FILA_ESTADO, "BOX ", VC_TINTA(VC_BGPAL_1));
    } else {
        vc_put_str_pal(30, HUD_FILA_ESTADO, "    ", VC_TINTA(VC_BGPAL_1));
    }
}

/* ===========================================================================
 * ACTUALIZACION POR FRAME (llamar dentro del VBLANK)
 * =========================================================================== */

static void move_player(void) {
    /* px/py son 16 bits; vx/vy son int8_t con signo. Se suma con signo para
     * que un valor negativo RESTE (no sumar el byte sin signo, que daria 255). */
    py_ant = py;     /* posicion con la que el hardware calculo SOLID_HIT */
    px = (uint16_t)(px + vx);
    if (px >= 312) { px = 312; vx = (int8_t)-vx; }
    else if (px <= 8) { px = 8; vx = (int8_t)-vx; }

    py = (uint16_t)(py + vy);
    /* Rebote vertical: techo = borde superior de la plataforma, suelo = borde
     * inferior de la pantalla. Al tocar la plataforma (tile solido) el rebote
     * lo resuelve vc_solid_hit() en update_frame; estos limites son la red de
     * seguridad para que el jugador no salga de pantalla. */
    if (py >= 224) { py = 224; vy = (int8_t)-vy; }
    else if (py <= 8) { py = 8; vy = (int8_t)-vy; }

    /* Animacion cada 6 frames */
    if (++tick >= 6) {
        tick = 0;
        frame = (uint8_t)((frame + 1) % PLAYER_FRAMES);
    }

    /* Recoloca el sprite del jugador con el frame actual (tile = frame) */
    vc_sprite_move(SPR_PLAYER, px, py, VC_SPPAL_2);
    vc_oam_put(SPR_PLAYER, VC_OAM_TILE, (uint8_t)(PAT_PLAYER + frame));
}

/* Bloque 16x16 (4 sprites de 8x8 a 1x). */
#define BLOCK_W 16
static void move_block(void) {
    uint8_t i;

    /* Suma con signo (bvx/bvy son int8_t). Un valor negativo debe RESTAR. */
    bx = (uint16_t)(bx + bvx);
    if (bx >= (312 - BLOCK_W)) { bx = 312 - BLOCK_W; bvx = (int8_t)-bvx; }
    else if (bx <= 8) { bx = 8; bvx = (int8_t)-bvx; }

    by = (uint16_t)(by + bvy);
    if (by >= Y_APARCADO_MAX) { by = Y_APARCADO_MAX; bvy = (int8_t)-bvy; }
    else if (by <= 32) { by = 32; bvy = (int8_t)-bvy; }

    vc_sprite16_set(SPR_BLOCK, bx, by, PAT_BLOCK, VC_SPPAL_3);   /* 16x16 a 1x */

    /* vc_sprite16_set deja COLL=centro en los 4 cuadrantes. Se aparca (esquina
     * superior) para que el bloque NO pueda disparar SOLID_HIT: el flag es
     * global y el jugador rebotaria sin tocar nada. Ver COLL_APARCADO. */
    for (i = 0; i < 4; i++) {
        vc_oam_put((uint8_t)(SPR_BLOCK + i), VC_OAM_COLL, COLL_APARCADO);
    }
}

/* Rombo 8x8 dibujado a 2x -> 16x16 en pantalla. */
#define RHOM_W 16
static void move_rhom(void) {
    rx = (uint16_t)(rx + rvx);
    if (rx >= (312 - RHOM_W)) { rx = 312 - RHOM_W; rvx = (int8_t)-rvx; }
    else if (rx <= 8) { rx = 8; rvx = (int8_t)-rvx; }

    ry = (uint16_t)(ry + rvy);
    if (ry >= Y_APARCADO_MAX) { ry = Y_APARCADO_MAX; rvy = (int8_t)-rvy; }
    else if (ry <= 16) { ry = 16; rvy = (int8_t)-rvy; }

    /* SCALE2X: un solo sprite de 8x8 dibujado al doble. */
    vc_sprite_move(SPR_RHOM, rx, ry, VC_SPPAL_1 | VC_SPR_SCALE2X);
    vc_oam_put(SPR_RHOM, VC_OAM_TILE, PAT_RHOM);
}

/* La plataforma del tilemap ocupa las columnas 4..27 de la fila 20.
 *
 * OJO: el fondo tiene auto-scroll en X, asi que en PANTALLA la plataforma NO
 * esta en x=32..223: aparece desplazada. El barrido de colision del hardware
 * usa la celda de pantalla (no la del mapa), de modo que aqui hay que aplicar
 * el mismo scroll para que la comprobacion coincida con lo que se ve.
 *
 *   x_pantalla = col*8 - scroll   (el mapa envuelve cada 512 px)
 *
 * Devuelve 1 si el jugador (8x8) esta tocando la plataforma EN PANTALLA. */
static uint8_t jugador_sobre_plataforma(void) {
    uint16_t sx0, sx1;   /* extremos de la plataforma en pantalla */

    /* offset en pantalla de la columna 4 (inicio) y 28 (fin exclusivo),
     * con la misma envoltura que hace el hardware (modulo 512). */
    sx0 = (uint16_t)((uint16_t)(PLATAFORMA_COL0 * 8 + 512 - scroll) & 0x1FF);
    sx1 = (uint16_t)((uint16_t)(PLATAFORMA_COL1 * 8 + 512 - scroll) & 0x1FF);

    /* contacto vertical: tocando la cara superior o la inferior */
    if (!(py + 8 >= PLATAFORMA_PY && py + 8 <= PLATAFORMA_PY + 8) &&
        !(py >= PLATAFORMA_PY && py <= PLATAFORMA_PY + 8)) {
        return 0;
    }

    /* contacto horizontal, teniendo en cuenta que la plataforma puede estar
     * partida por la envoltura (sx1 < sx0) */
    if (sx1 > sx0) {
        return (uint8_t)(!(px + 8 <= sx0 || px >= sx1));
    }
    /* envuelta: dos tramos [sx0,512) y [0,sx1) */
    return (uint8_t)((px + 8 > sx0) || (px < sx1));
}

static void update_frame(void) {
    hit_solid = 0;      /* se recalcula cada frame */

    move_player();
    move_block();
    move_rhom();

    /* Auto-scroll suave del fondo: la plataforma se mueve en pantalla.
     * La colision contra el tile solido tiene en cuenta este scroll
     * (ver jugador_sobre_plataforma), para que el rebote coincida con lo que
     * se ve. */
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

    /* Colision sprite<->tile SOLIDO: el hardware la detecta (vc_solid_hit),
     * pero el flag es GLOBAL: dice "alguien choco", no quien. En esta demo solo
     * el jugador puede dispararlo (el bloque y el rombo llevan el COLL aparcado
     * y se mueven por encima de la plataforma), pero el jugador puede tocar DOS
     * cosas solidas: la plataforma (fila 20) y el suelo (fila 30).
     *
     * Para que el aviso diga cual, se combina:
     *   1. el flag del hardware, y
     *   2. una comprobacion en SOFTWARE de donde esta el jugador.
     * solid_lock evita invertir varias veces con el mismo flag. */
    {
        uint8_t hw = vc_solid_hit();   /* flag del hardware (global) */

        if (hw) {
            /* El flag lo calculo el hardware en el VBLANK ANTERIOR, cuando el
             * jugador estaba en py_ant. Por eso se clasifica con la posicion
             * previa, no con la actual: si no, el aviso sale con un frame de
             * retraso y cae en el caso "OTRO" por error. */
            uint16_t py_save = py;
            py = py_ant;
            if (jugador_sobre_plataforma()) {
                hit_solid = HIT_PLATAFORMA;
                if (!solid_lock) {
                    if (vy < 0) {
                        /* subiendo: choca con la cara INFERIOR -> lo empuja abajo */
                        py = (uint16_t)(PLATAFORMA_PY + 8);
                        vy = 1;
                    } else {
                        /* bajando: choca con la cara SUPERIOR -> lo posa encima */
                        py = (uint16_t)(PLATAFORMA_PY - 8);
                        vy = -1;
                    }
                    solid_lock = SOLID_LOCK_FRAMES;
                }
                py_save = py;
            } else if (py_ant + 8 >= SUELO_PY) {
                /* Toco el suelo (fila 30). El rebote ya lo hace move_player. */
                hit_solid = HIT_SUELO;
            } else {
                hit_solid = HIT_OTRO;
            }
            py = py_save;
        }

        if (solid_lock && !hw) {
            solid_lock--;
        }
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
        s.tile  = PAT_PLAYER;
        s.flags = VC_SPPAL_2;
        s.coll  = VC_COLL_FEET;
        vc_sprite_set(SPR_PLAYER, &s);
    }

    /* Bloque 16x16 (4 sprites a 1x) */
    bx = 150; by = 120; bvx = 1; bvy = 2;
    vc_sprite16_set(SPR_BLOCK, bx, by, PAT_BLOCK, VC_SPPAL_3);

    /* Rombo 8x8 a 2x (se ve 16x16) */
    rx = 200; ry = 60; rvx = -2; rvy = 1;
    {
        vc_sprite_t s;
        s.x_lo  = (uint8_t)rx;
        s.y     = (uint8_t)ry;
        s.tile  = PAT_RHOM;
        s.flags = VC_SPPAL_1 | VC_SPR_SCALE2X;
        /* COLL aparcado: el rombo NO debe disparar SOLID_HIT (el flag es
         * global y el jugador rebotaria sin tocar la plataforma). */
        s.coll  = COLL_APARCADO;
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
