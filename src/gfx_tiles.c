/**
 * ============================================================================
 * gfx_tiles.c - Atributos de celda y carga de tiles de fondo
 * ============================================================================
 * Modulo separado de gfx.c para que el linker pueda descartarlo si el juego no
 * lo usa. La API (video.h) no cambia.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * ATRIBUTOS DEL FONDO
 * =========================================================================== */

/* Escribe el atributo de una celda = paleta + flags (VC_ATTR_*).
 *
 * Recuerda que el hardware solo usa la paleta (bits 1:0) y SOLIDO (bit 4); el
 * resto de flags se descartan. Ver la nota de VC_ATTR_SOLID en video.h. */
void vc_set_cell_attr(uint8_t col, uint8_t row, uint8_t paleta, uint8_t flags) {
    vc_put_attr(col, row, (uint8_t)((flags & 0xF0) | (paleta & 0x0F)));
}

/* Deja la celda vacia: tile 0 y atributo 0. */
void vc_free_cell(uint8_t col, uint8_t row) {
    vc_put_cell(col, row, 0);
    vc_put_attr(col, row, 0);
}

/* ===========================================================================
 * FONDO: CARGAR TILESET Y MAPA COMPLETO
 * =========================================================================== */

/*
 * Carga N patrones de fondo consecutivos desde dos tablas planares.
 * pat0[i*8+fila] = plano 0 del patron (tile_base+i)
 * pat1[i*8+fila] = plano 1 del patron (tile_base+i)
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
