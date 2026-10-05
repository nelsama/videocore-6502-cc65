; ============================================================================
; tests/sprite_test.s - Test de host (sim65) de las rutinas OAM de video.s
; ============================================================================
; Verifica vc_sprite_move y vc_sprite_set con los registros de video
; redirigidos a RAM (definiendo TEST_HOOKS al ensamblar video.s).
;
; Compilar y ejecutar (desde la raiz, con bash):
;   ca65 -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
;   cl65 -t sim6502 -o build/sprite_test.prg tests/sprite_test.s build/video_sim.o
;   sim65 build/sprite_test.prg ; echo "exit=$?"
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
;
; El puerto simulado: VID_ADDR_LO=$F000, VID_ADDR_HI=$F001, VID_DATA=$F002.
;   Escribir VID_DATA "dispara" la escritura, pero como no hay hardware, solo
;   queda el ULTIMO valor en $F002. Por eso cada funcion escribe VARIOS campos:
;   para comprobarlos, el test intercepta guardando (addr,data) en un log.
;
; Como video.s escribe directo a $F002, aqui usamos el truco de declarar que
; $F002 es en realidad un byte en RAM y comprobar el ULTIMO campo escrito.
; Para ver TODOS los campos, este test usa llamadas de UN solo campo via
; vc_oam_put (que ya tenia), y para vc_sprite_move/set comprueba la direccion
; final (campo FLAGS o COLL) y su dato.
; ============================================================================

        .export _main
        .export oam_write          ; requerida por video.s bajo TEST_HOOKS
        .import _vc_sprite_move, _vc_sprite_set, _vc_oam_put, _vc_clear_oam
        .importzp VC_SPRIDX, VC_FIELD, VC_TILE, VC_TMP

; Puerto simulado (coincide con TEST_HOOKS de video.s)
VID_ADDR_LO = $F000
VID_ADDR_HI = $F001
VID_DATA    = $F002
VID_STATUS  = $F003

        .segment "CODE"

; ---------------------------------------------------------------------------
; oam_write: version de produccion, escribiendo al puerto simulado.
;   Entrada: A = dato ; VC_SPRIDX = sprite ; VC_FIELD = campo.
;   byte = VC_SPRIDX*5 + VC_FIELD ; area OAM ($C0).
; ---------------------------------------------------------------------------
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
        rts

        .segment "CODE"

; ---------------------------------------------------------------------------
; vc_sprite_move(spr, x, y, base_flags)
;   Tras la llamada, la ULTIMA escritura es a FLAGS (+3) del sprite.
;   -> VID_ADDR_LO debe ser spr*5 + 3, y VID_DATA los flags con bit8 ajustado.
; ---------------------------------------------------------------------------
_main:
        ; --- caso 1: move con x < 256, flags sin bit8 ---
        ; vc_sprite_move(1, 0x0040, 0x20, 0x01) -> flags = 0x01 (sin bit8)
        lda #1
        jsr pusha
        lda #$40
        ldx #$00
        jsr pushax
        lda #$20
        jsr pusha
        lda #$01
        jsr _vc_sprite_move

        ; direccion esperada = 1*5 + 3 = 8
        lda VID_ADDR_LO
        cmp #8
        beq c1a
        lda #1
        rts
c1a:
        ; dato esperado = 0x01 (base_flags, sin bit8); ADDR_HI = $C0 (OAM)
        lda VID_DATA
        cmp #$01
        beq c1b
        lda #11
        rts
c1b:
        lda VID_ADDR_HI
        cmp #$C0
        beq c1ok
        lda #12
        rts
c1ok:

        ; --- caso 2: move con x >= 256 -> debe poner bit8 ---
        ; vc_sprite_move(2, 0x0130, 0x10, 0x02) -> flags = 0x02 | 0x04 = 0x06
        lda #2
        jsr pusha
        lda #$30
        ldx #$01
        jsr pushax
        lda #$10
        jsr pusha
        lda #$02
        jsr _vc_sprite_move
        ; direccion = 2*5+3 = 13
        lda VID_ADDR_LO
        cmp #13
        beq c2a
        lda #2
        rts
c2a:
        lda VID_DATA
        cmp #$06
        beq c2ok
        lda #21
        rts
c2ok:

        ; --- caso 3: move con x < 256 pero base_flags ya tenia bit8 ->
        ;     debe QUITARLO. vc_sprite_move(0, 0x0050, 0x10, 0x07) -> 0x03
        lda #0
        jsr pusha
        lda #$50
        ldx #$00
        jsr pushax
        lda #$10
        jsr pusha
        lda #$07
        jsr _vc_sprite_move
        lda VID_DATA
        cmp #$03
        beq c3ok
        lda #3
        rts
c3ok:

        ; --- caso 4: sprite_set escribe COLL (+4) al final ---
        ; vc_sprite_set(3, &struct) con struct: x=0x11,y=0x22,tile=0x05,
        ; flags=0x06, coll=0x24. Direccion final = 3*5+4 = 19, dato = 0x24.
        lda #3
        jsr pusha
        lda #<sb
        ldx #>sb
        jsr _vc_sprite_set
        lda VID_ADDR_LO
        cmp #19
        beq c4a
        lda #4
        rts
c4a:
        lda VID_DATA
        cmp #$24
        beq c4ok
        lda #41
        rts
c4ok:

        ; --- caso 5: vc_oam_put escribe DATA, no spr ---
        ; vc_oam_put(0, VC_OAM_TILE, 0x2A): debe escribir 0x2A en el byte
        ;  0*5+2 = 2 del OAM. Este caso detecta el bug en que se escribia
        ;  spr (0) en vez de data (0x2A).
        lda #0
        jsr pusha              ; spr = 0
        lda #2
        jsr pusha              ; field = VC_OAM_TILE (2)
        lda #$2A
        jsr _vc_oam_put        ; data = 0x2A

        lda VID_ADDR_LO
        cmp #2
        beq c5a
        lda #5
        rts
c5a:
        lda VID_DATA
        cmp #$2A
        beq c5b
        lda #51
        rts
c5b:
        lda VID_ADDR_HI
        cmp #$C0
        beq c5ok
        lda #52
        rts
c5ok:

        ; --- caso 6: vc_oam_put con spr != data (spr=4, data=0x2A) ---
        ; byte = 4*5+2 = 22. Verifica que el dato no se mezcla con spr.
        lda #4
        jsr pusha              ; spr = 4
        lda #2
        jsr pusha              ; field = VC_OAM_TILE
        lda #$2A
        jsr _vc_oam_put        ; data = 0x2A

        lda VID_ADDR_LO
        cmp #22
        beq c6a
        lda #6
        rts
c6a:
        lda VID_DATA
        cmp #$2A
        beq c6ok
        lda #61
        rts
c6ok:

        ; todo OK
        lda #0
        ldx #0
        rts

.segment "RODATA"
sb:     .byte $11,$22,$05,$06,$24   ; x_lo,y,tile,flags,coll

; --- helpers de push (del runtime) ---
        .import pusha, pushax
