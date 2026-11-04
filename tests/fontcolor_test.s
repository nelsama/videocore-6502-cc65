; ============================================================================
; tests/fontcolor_test.s - Regresion de vc_set_font_color()
; ============================================================================
; El color del trazo de la fuente vive en $D818 y es de 2 bits: solo 0-3 son
; validos. Valores mayores deben quedar recortados (no deben escribir basura
; en bits que el hardware reserva).
;
; Compilar y ejecutar (desde la raiz, con bash):
;   sh tests/run.sh
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
; ============================================================================

        .export _main
        .export oam_write
        .import _vc_set_font_color
        .import pusha
        .importzp VC_SPRIDX, VC_FIELD, VC_TILE, VC_TMP

; Registros simulados (TEST_HOOKS redirige el puerto indirecto aqui).
VID_ADDR_LO = $F000
VID_ADDR_HI = $F001
VID_DATA    = $F002
VID_STATUS  = $F003
VID_SETUP   = $F010
VID_SETUP_ST= $F011
VID_FONT_COL= $F018

; oam_write del puerto indirecto (no lo usa este test, pero video.s lo exige).
oam_write:
        rts

        .segment "CODE"

_main:
        ; caso 1: escribir 2 deja 2
        lda #2
        jsr pusha
        jsr _vc_set_font_color
        lda VID_FONT_COL
        cmp #2
        beq c2
        lda #1
        ldx #0
        rts

c2:     ; caso 2: escribir 3 deja 3
        lda #3
        jsr pusha
        jsr _vc_set_font_color
        lda VID_FONT_COL
        cmp #3
        beq c3
        lda #2
        ldx #0
        rts

c3:     ; caso 3: escribir 0 deja 0 (texto invisible, caso valido)
        lda #0
        jsr pusha
        jsr _vc_set_font_color
        lda VID_FONT_COL
        cmp #0
        beq c4
        lda #3
        ldx #0
        rts

c4:     ; caso 4: un valor fuera de rango (7) se recorta a 3
        lda #7
        jsr pusha
        jsr _vc_set_font_color
        lda VID_FONT_COL
        cmp #3
        beq ok
        lda #4
        ldx #0
        rts

ok:     lda #0
        ldx #0
        rts
