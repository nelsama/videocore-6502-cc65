; ============================================================================
; tests/disable_test.s - Regresion: que escribe realmente vc_sprite_disable()
; ============================================================================
; vc_sprite_disable(spr) debe dejar el sprite INERTE, y "inerte" incluye la
; colision: el barrido del hardware recorre los 32 sprites sin filtrar por Y,
; asi que si el COLL_POINT queda sobre una celda solida, SOLID_HIT se dispara.
;
; El COLL_POINT es (X + dx, Y + dy) + scroll. Si la libreria solo pone Y=248 y
; COLL=(0,0) pero NO toca X, el punto cae en la COLUMNA donde murio el sprite.
; Este test fija X antes de deshabilitar y comprueba que X y el bit 8 se ponen
; a 0, ademas de Y y COLL.
;
; Compilar y ejecutar (desde la raiz, con bash):
;   ca65 -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
;   cl65 -t sim6502 -o build/disable_test.prg tests/disable_test.s build/video_sim.o
;   sim65 build/disable_test.prg ; echo "exit=$?"
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
; ============================================================================

        .export _main
        .export oam_write
        .import _vc_oam_put, _vc_sprite_disable
        .import pusha
        .importzp VC_SPRIDX, VC_FIELD, VC_TILE, VC_TMP

VID_ADDR_LO = $F000
VID_ADDR_HI = $F001
VID_DATA    = $F002
VID_STATUS  = $F003

; Log de escrituras: hasta 16 pares (addr_lo, dato).
LOG         = $0400
LOG_N       = $03FF

        .segment "CODE"

; oam_write: redirige al puerto simulado y registra (addr_lo, dato) en LOG.
oam_write:
        sta VC_TILE
        lda VC_SPRIDX
        sta VC_TMP
        asl a
        asl a
        clc
        adc VC_TMP
        clc
        adc VC_FIELD
        sta VID_ADDR_LO
        lda #$C0
        sta VID_ADDR_HI
        lda VC_TILE
        sta VID_DATA

        ldx LOG_N
        cpx #32               ; log lleno (16 pares)
        bcs @nolog
        lda VID_ADDR_LO
        sta LOG,x
        inx
        lda VC_TILE
        sta LOG,x
        inx
        stx LOG_N
@nolog:
        rts

        .segment "CODE"

; busca en el log el dato escrito para la direccion A (= addr_lo).
; Devuelve en A el dato, o $EE si esa direccion no se escribio.
_busca:
        sta VC_TMP
        ldx #0
@b:
        cpx LOG_N
        beq @no
        lda LOG,x
        cmp VC_TMP
        beq @si
        inx
        inx
        jmp @b
@si:
        inx
        lda LOG,x
        rts
@no:
        lda #$EE
        rts

; --- main ------------------------------------------------------------------
_main:
        ; --- preparar sprite 3: X=200, Y=100, COLL=$3C ---
        lda #3
        jsr pusha              ; spr
        lda #0
        jsr pusha              ; field = X
        lda #$C8               ; 200
        jsr _vc_oam_put

        lda #3
        jsr pusha
        lda #1
        jsr pusha              ; field = Y
        lda #100
        jsr _vc_oam_put

        lda #3
        jsr pusha
        lda #4
        jsr pusha              ; field = COLL
        lda #$3C               ; pie
        jsr _vc_oam_put

        ; solo interesan las escrituras de disable
        lda #0
        sta LOG_N

        lda #3
        jsr pusha
        jsr _vc_sprite_disable

        ; caso 1: Y = 248 -> addr 3*5+1 = 16
        lda #16
        jsr _busca
        cmp #248
        bne @f1

        ; caso 2: COLL = 0 -> addr 3*5+4 = 19
        lda #19
        jsr _busca
        cmp #0
        bne @f2

        ; caso 3: X = 0 -> addr 3*5+0 = 15. Si no se escribe, _busca da $EE.
        lda #15
        jsr _busca
        cmp #0
        bne @f3

        ; caso 4: FLAGS sin bit 8 -> addr 3*5+3 = 18
        lda #18
        jsr _busca
        and #$04
        bne @f4

        lda #0
        ldx #0
        rts

@f1:    lda #1
        ldx #0
        rts
@f2:    lda #2
        ldx #0
        rts
@f3:    lda #3
        ldx #0
        rts
@f4:    lda #4
        ldx #0
        rts

