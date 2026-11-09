/**
 * ============================================================================
 * gfx_vram.c - Limpieza de VRAM y color de la fuente
 * ============================================================================
 * Modulo separado de gfx.c para que el linker pueda descartarlo si el juego no
 * lo usa. La API (video.h) no cambia: las funciones siguen llamandose igual.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * LIMPIEZA DE VRAM
 * =========================================================================== */

/* Limpia la VRAM usando el SETUP POR HARDWARE del core ($D816/$D817).
 *
 * El core limpia en hardware, en ~120-275 us:
 *   - tilemap completo (2048 celdas -> tile 0)
 *   - atributos a 0
 *   - patrones de fondo (tiles 0-7) y de sprite (0-2): escribe ceros
 *   - RE-EXPANDE la fuente de texto (glifos $20-$7F desde la ROM de fuente)
 *
 * Por eso, a diferencia de la version manual antigua, este clear SI reescribe
 * los patrones: los borra y vuelve a cargar la fuente, de modo que el texto
 * sigue disponible despues de limpiar. Si el juego habia dibujado sus propios
 * tiles en el rango de la fuente ($20-$7F), SE PERDERAN.
 *
 * Tambien borra el arte de demo que el core graba al arrancar en los patrones
 * 0-2 de sprite y en los tiles 0-7 de fondo (manual del core, seccion 6.0).
 *
 * El tile 0 queda apuntando a un patron en blanco (transparente), asi que la
 * pantalla se ve del color de fondo (BG_COLOR).
 *
 * OJO: mientras el setup corre (BUSY=1), las escrituras del CPU a VRAM/OAM se
 * IGNORAN. Por eso esta funcion ESPERA a que termine antes de retornar, para
 * que puedas escribir tu mundo justo despues. No toca paletas, scroll ni
 * bandas.
 *
 * El OAM (posiciones de sprite) NO lo limpia el setup; vc_clear_vram() deshabilita
 * los 32 sprites a continuacion, para que no queden dibujando tras borrar sus
 * patrones.
 *
 * Debe llamarse tras vc_wait_ready() y dentro del VBLANK (evita rasgado). */
void vc_clear_vram(void) {
    VID_SETUP = 1;      /* dispara el setup por hardware */
    vc_wait_setup();    /* espera a BUSY=0 -> la VRAM ya esta lista */
    vc_clear_oam();     /* deshabilita los 32 sprites */
}

/* Color del trazo de la fuente ($D818).
 *
 * Los patrones de la fuente se expanden UNA vez (en el setup) usando una sola
 * entrada de paleta para el trazo; este registro elige cual (0-3). Por eso:
 *   - 0 deja el texto invisible (el color 0 del fondo es transparente);
 *   - 3 es el valor de arranque y el que usan los juegos existentes;
 *   - cambiarlo NO repinta: hace falta el setup, que ademas limpia el tilemap.
 *
 * Aqui solo se escribe el registro; el repintado lo decide quien llama. */
void vc_set_font_color(uint8_t color) {
    VID_FONT_COLOR = (uint8_t)(color & 0x03);
}

/* ===========================================================================
 * DIRECCIONES DE CELDA
 * =========================================================================== */

/* Direccion lineal de una celda del tilemap: row*64 + col.
 *
 * Por que una FUNCION y no solo la macro VC_CELL:
 *   - Con la macro, CADA uso con argumentos variables repite el producto
 *     (shlax4 + shlax2 + suma): ~45 bytes por uso en el binario del juego.
 *   - Con la funcion, ese codigo existe UNA vez (32 bytes) y cada uso es un
 *     jsr de ~25 bytes. Con 8 usos el ahorro medido es de ~144 bytes.
 *
 * Cuando conviene CADA una:
 *   - Ambos argumentos CONSTANTES -> VC_CELL (cc65 la pliega a un inmediato, 0).
 *   - Columna constante          -> VC_CELL (cc65 la reduce a incaxN).
 *   - Bucle sobre una fila       -> VC_CELL una vez y despues incrementar.
 *   - Llamadas sueltas con argumentos variables -> vc_cell().
 *
 * OJO: como VC_MAP_COLS es 64, row*64 equivale a row<<6; escribir uno u otro
 * genera EL MISMO codigo (cc65 ya usa shifts), asi que no es una optimizacion. */
uint16_t vc_cell(uint8_t col, uint8_t row) {
    return (uint16_t)((row) * VC_MAP_COLS + (col));
}
