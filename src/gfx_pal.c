/**
 * ============================================================================
 * gfx_pal.c - Paletas programables
 * ============================================================================
 * Modulo separado de gfx.c para que el linker pueda descartarlo si el juego no
 * reprograma colores. La API (video.h) no cambia.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * PALETAS PROGRAMABLES
 * ===========================================================================
 * El hardware permite reescribir las entradas de paleta en cualquier momento
 * (RGB444, $D813-$D815). Al arrancar valen los valores por defecto del manual.
 * ESCRIBIR EN VBLANK si cambias muchos colores a la vez.
 *
 * OJO: una entrada de paleta es COMPARTIDA. Cambiarla afecta a todo lo que la
 * use: los tiles graficos Y el texto de esa paleta. Truco: reserva una paleta
 * entera para el texto y no la uses para graficos.
 */

/* Fija UN color de la paleta de FONDO (paleta 0-3, color 0-3). */
void vc_pal_set_bg(uint8_t paleta, uint8_t color, uint16_t rgb444) {
    vc_pal_set(VC_PAL_BG(paleta, color), rgb444);
}

/* Fija UN color de la paleta de SPRITE (paleta 0-3, color 0-3). */
void vc_pal_set_spr(uint8_t paleta, uint8_t color, uint16_t rgb444) {
    vc_pal_set(VC_PAL_SPR(paleta, color), rgb444);
}

/* Carga los 4 colores de una paleta de fondo desde un array RGB444. */
void vc_pal_load_bg(uint8_t paleta, const uint16_t *colores4) {
    vc_pal_load((uint8_t)VC_PAL_BG(paleta, 0), colores4, 4);
}

/* Carga los 4 colores de una paleta de sprite desde un array RGB444. */
void vc_pal_load_spr(uint8_t paleta, const uint16_t *colores4) {
    vc_pal_load((uint8_t)VC_PAL_SPR(paleta, 0), colores4, 4);
}

/* Fija el color de fondo global (BG_COLOR = entrada 15 = paleta 3, color 3).
 * Afecta al margen y a las zonas transparentes, no solo al texto de la
 * paleta 3. Los sprites NO se ven afectados (usan su propio banco). */
void vc_set_bgcolor(uint16_t rgb444) {
    vc_pal_set(VC_PAL_BGCOLOR, rgb444);
}
