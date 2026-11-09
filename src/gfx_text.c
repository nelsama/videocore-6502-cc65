/**
 * ============================================================================
 * gfx_text.c - Modo texto (tile = ASCII)
 * ============================================================================
 * Modulo separado de gfx.c para que el linker pueda descartarlo si el juego no
 * escribe texto. La API (video.h) no cambia.
 *
 * Dependencias: solo el nucleo en asm (video.s). Se llama a vc_put_attr()
 * directamente en vez de vc_set_cell_attr() para NO arrastrar gfx_tiles.o solo
 * por escribir texto con color.
 * ============================================================================
 */

#include "video.h"

/* ===========================================================================
 * TEXTO
 * =========================================================================== */

/* Limpia la pantalla visible (40x30) con espacios. La fila 0 se reserva por
 * el desfase del pipeline; se rellena tambien para mayor comodidad. */
void vc_text_init(void) {
    uint8_t col, row;

    for (row = 0; row < VC_SCREEN_ROWS; row++) {
        for (col = 0; col < VC_SCREEN_COLS; col++) {
            vc_put_cell(col, row, VC_CHAR_SPACE);      /* espacio */
        }
    }
}

/* Escribe un caracter en la posicion (col,row). El tile es el propio ASCII. */
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
        /* vc_put_attr (asm) en vez de vc_set_cell_attr: evita depender de
         * gfx_tiles.o, que el linker arrastraria entero. Mismo resultado: el
         * atributo es paleta en bits 1:0 y sin SOLIDO. */
        vc_put_attr(col, row, (uint8_t)(paleta & 0x03));
        col++;
        if (col >= VC_SCREEN_COLS) {
            col = 0;
            row++;
        }
        s++;
    }
}
