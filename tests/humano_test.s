; Test de diagnostico: reproduce init_humano + update y devuelve en A el
; numero de escrituras al OAM, o comprobaciones concretas.
;
;   ca65 -D TEST_HOOKS ... video_sim.o
;   cl65 ... este test + video_sim.o -> sim65

        .export _main
        .export oam_write
        .import _vc_sprite_set, _vc_sprite_move, _vc_oam_put
        .importzp VC_SPRIDX, VC_FIELD

        .segment "ZEROPAGE"
logcnt: .res 1
tdata:  .res 1
taddr:  .res 1

        .segment "BSS"
last_addr: .res 1
last_data: .res 1
tile_addr: .res 1
tile_data: .res 1

        .segment "CODE"

oam_write:
        sta tdata
        lda VC_SPRIDX
        sta taddr
        asl a
        asl a
        clc
        adc taddr
        clc
        adc VC_FIELD
        sta last_addr
        lda tdata
        sta last_data
        ; si el campo es TILE (VC_FIELD==2), guardalo aparte
        lda VC_FIELD
        cmp #2
        bne @no
        lda last_addr
        sta tile_addr
        lda last_data
        sta tile_data
@no:
        inc logcnt
        rts

        .segment "RODATA"
spr0:   .byte $50,$A8,$08,$F2,$3C

        .segment "CODE"
_main:
        lda #0
        sta logcnt

        ; vc_sprite_set(0, &spr0)  -> escribe 5 campos (tile=8 en addr 2)
        lda #0
        jsr pusha
        lda #<spr0
        ldx #>spr0
        jsr _vc_sprite_set

        ; Verifica: tile_addr == 2 (sprite 0, campo TILE) y tile_data == 8.
        lda tile_addr
        cmp #2
        bne @bad1
        lda tile_data
        cmp #8
        bne @bad2
        ; tambien comprueba que se escribieron 5 campos
        lda logcnt
        cmp #5
        bne @bad3
        lda #0                 ; OK
        ldx #0
        rts
@bad1:
        lda #1
        ldx #0
        rts
@bad2:
        lda #2
        ldx #0
        rts
@bad3:
        lda #3
        ldx #0
        rts

        .import pusha, pushax
