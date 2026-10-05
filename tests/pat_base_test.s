; ============================================================================
; tests/pat_base_test.s - Test de host (sim65) de la dirección de patrón
; ============================================================================
; Verifica vc_load_bg_pattern con tiles >= 32, que fue un bug real: el cálculo
; de dir = tile*8 + fila necesita 11 bits (tile 0..255, fila 0..7 -> 0..2047),
; y la primera versión perdía los bits 9 y 10.
;
; Compila video.s con -D TEST_HOOKS (los registros van a $F000-$F002). Tras
; cargar un patrón, el ÚLTIMO valor en VID_ADDR_LO/HI es el del plano 1 de la
; fila 7 -> revela la parte alta de la dirección.
;
; Compilar y ejecutar (desde la raiz, con bash):
;   ca65 -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
;   cl65 -t sim6502 -o build/pat_test.prg tests/pat_base_test.s build/video_sim.o
;   sim65 build/pat_test.prg ; echo "exit=$?"
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
; ============================================================================

        .export _main
        .export oam_write          ; requerida por video.s bajo TEST_HOOKS (no-op)
        .import _vc_load_bg_pattern
        .importzp VC_TMP

VID_ADDR_LO = $F000
VID_ADDR_HI = $F001
VID_DATA    = $F002

        .segment "CODE"

; oam_write no se usa en este test, pero video.s la importa bajo TEST_HOOKS.
oam_write:
        rts

; ---------------------------------------------------------------------------
; check: carga el patrón del tile en A y comprueba la dir final esperada
;   (lo en B, hi en C). Si falla, devuelve el codigo en A.
;   Simplificado: se hace inline por caso.
; ---------------------------------------------------------------------------
_main:
        ; --- caso 1: tile=0 -> dir = 0*8+7 = 7 -> lo=$07, hi=$80 ($80|0) ---
        lda #0
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$07
        bne f1
        lda VID_ADDR_HI
        cmp #$A0
        bne f2
        jmp caso2
f1:     lda #1
        rts
f2:     lda #2
        rts

caso2:
        ; --- caso 2: tile=31 -> dir = 31*8+7 = 255 -> lo=$FF, hi=$80|0=$80 ---
        lda #31
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$FF
        bne f3
        lda VID_ADDR_HI
        cmp #$A0
        bne f4
        jmp caso3
f3:     lda #3
        rts
f4:     lda #4
        rts

caso3:
        ; --- caso 3: tile=32 -> dir = 32*8+7 = 263 = $0107 -> lo=$07, hi=$80|1=$81
        lda #32
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$07
        bne f5
        lda VID_ADDR_HI
        cmp #$A1
        bne f6
        jmp caso4
f5:     lda #5
        rts
f6:     lda #6
        rts

caso4:
        ; --- caso 4: tile=64 -> dir = 64*8+7 = 519 = $0207 -> lo=$07, hi=$80|2=$82
        lda #64
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$07
        bne f7
        lda VID_ADDR_HI
        cmp #$A2
        bne f8
        jmp caso5
f7:     lda #7
        rts
f8:     lda #8
        rts

caso5:
        ; --- caso 5: tile=128 -> dir = 128*8+7 = 1031 = $0407 -> lo=$07, hi=$84
        lda #128
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$07
        bne f9
        lda VID_ADDR_HI
        cmp #$A4
        bne f10
        jmp caso6
f9:     lda #9
        rts
f10:    lda #10
        rts

caso6:
        ; --- caso 6: tile=255 -> dir = 255*8+7 = 2047 = $07FF -> lo=$FF, hi=$87
        lda #255
        jsr loadtile
        lda VID_ADDR_LO
        cmp #$FF
        bne f11
        lda VID_ADDR_HI
        cmp #$A7
        bne f12
        jmp allok
f11:    lda #11
        rts
f12:    lda #12
        rts

allok:
        lda #0
        ldx #0
        rts

; ---------------------------------------------------------------------------
; loadtile: carga el patrón del tile en A usando pat0/pat1 (8 bytes de ceros).
;   Convencion: popa = tile ; popax = pat0 ; A/X = pat1
; ---------------------------------------------------------------------------
loadtile:
        jsr pusha              ; tile
        lda #<pat0
        ldx #>pat0
        jsr pushax
        lda #<pat1
        ldx #>pat1
        jmp _vc_load_bg_pattern

        .segment "RODATA"
pat0:   .byte 0,0,0,0,0,0,0,0
pat1:   .byte 0,0,0,0,0,0,0,0

        .import pusha, pushax
