; Test rapido: comprueba que vc_load_spr_pattern(8,...) escribe en la direccion
; correcta del area de patrones de sprite ($C8 + spr*8 + fila).
;
;   ca65 -D TEST_HOOKS ... video_sim.o
;   cl65 ... este test + video_sim.o -> sim65
;
; Devuelve 0 en A si OK, o el numero del caso que falla.

        .export _main
        .export oam_write
        .import _vc_load_spr_pattern

VID_ADDR_LO = $F000
VID_ADDR_HI = $F001
VID_DATA    = $F002

        .segment "CODE"

oam_write:
        rts

_main:
        ; vc_load_spr_pattern(spr=8, pat0, pat1)  -> ultima escritura: plano1 fila7
        ; dir = 8*8 + 7 = 71 = $47 ; area plano1 = $E8 | (71>>8=0) = $E8
        lda #8
        jsr pusha
        lda #<pat0
        ldx #>pat0
        jsr pushax
        lda #<pat1
        ldx #>pat1
        jsr _vc_load_spr_pattern

        lda VID_ADDR_LO
        cmp #$47
        bne bad1
        lda VID_ADDR_HI
        cmp #$E8                ; area patron sprite plano1
        bne bad2
        lda VID_DATA
        cmp #$AA                ; ultimo byte de pat1
        bne bad3
        lda #0
        ldx #0
        rts
bad1:   lda #1
        rts
bad2:   lda #2
        rts
bad3:   lda #3
        rts

        .segment "RODATA"
pat0:   .byte 1,2,3,4,5,6,7,8
pat1:   .byte 9,10,11,12,13,14,15,$AA

        .import pusha, pushax
