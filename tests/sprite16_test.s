; ============================================================================
; tests/sprite16_test.s - Test de host (sim65) de vc_sprite16_set (video.s)
; ============================================================================
; Compila video.s con -D TEST_HOOKS: alli NO se define oam_write y la importa;
; este test aporta su propia oam_write que registra cada (campo, dato) en un log.
; Asi se verifica TODO lo que escribe vc_sprite16_set.
;
; Compilar y ejecutar (desde la raiz, con bash):
;   ca65 -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
;   cl65 -t sim6502 -o build/sprite16_test.prg tests/sprite16_test.s build/video_sim.o
;   sim65 build/sprite16_test.prg ; echo "exit=$?"
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
; ============================================================================

        .export _main
        .export oam_write          ; sustituye al de video.s bajo TEST_HOOKS
        .import _vc_sprite16_set
        .importzp VC_SPRIDX, VC_FIELD

        .segment "ZEROPAGE"
logcnt:  .res 1
tdata:   .res 1

        .segment "BSS"
; log de 40 entradas x 2 bytes (campo, dato) = 80 bytes
log:     .res 80

        .segment "CODE"

; ---------------------------------------------------------------------------
; oam_write: SUSTITUTO del de video.s bajo TEST_HOOKS.
;   Entrada: A = dato ; VC_SPRIDX = sprite ; VC_FIELD = campo.
;   Registra (campo, dato) en log[cnt*2].
; ---------------------------------------------------------------------------
oam_write:
        sta tdata
        lda logcnt
        asl a                  ; indice = cnt*2
        tay
        lda VC_FIELD
        sta log,y              ; log[n]   = campo (0..4)
        lda tdata
        sta log+1,y            ; log[n+1] = dato
        inc logcnt
        rts

; ---------------------------------------------------------------------------
_main:
        lda #0
        sta logcnt

        ; vc_sprite16_set(first=0, x=100, y=50, tile0=4, flags=0x12)
        lda #0
        jsr pusha              ; first
        lda #100
        ldx #0
        jsr pushax             ; x = 100
        lda #50
        jsr pusha              ; y
        lda #4
        jsr pusha              ; tile0
        lda #$12
        jsr _vc_sprite16_set

        ; Esperado: 20 escrituras (4 cuadrantes x 5 campos)
        lda logcnt
        cmp #20
        beq c1
        lda #1
        rts
c1:
        ; --- cuadrante 0: X,100 Y,50 TILE,4 FLAGS,0x12 COLL,0x24 ---
        lda log+0
        cmp #0
        bne bad0
        lda log+1
        cmp #100
        bne bad0
        lda log+2
        cmp #1
        bne bad0
        lda log+3
        cmp #50
        bne bad0
        lda log+4
        cmp #2
        bne bad0
        lda log+5
        cmp #4
        bne bad0
        lda log+6
        cmp #3
        bne bad0
        lda log+7
        cmp #$12
        bne bad0
        lda log+8
        cmp #4
        bne bad0
        lda log+9
        cmp #$24
        bne bad0
        jmp c2
bad0:
        lda #10
        rts
c2:
        ; --- cuadrante 1: x=108, y=50, tile=5 ---
        lda log+11
        cmp #108
        bne bad1
        lda log+13
        cmp #50
        bne bad1
        lda log+15
        cmp #5
        bne bad1
        jmp c3
bad1:
        lda #11
        rts
c3:
        ; --- cuadrante 2: x=100, y=58, tile=6 ---
        lda log+21
        cmp #100
        bne bad2
        lda log+23
        cmp #58
        bne bad2
        lda log+25
        cmp #6
        bne bad2
        jmp c4
bad2:
        lda #12
        rts
c4:
        ; --- cuadrante 3: x=108, y=58, tile=7 ---
        lda log+31
        cmp #108
        bne bad3
        lda log+33
        cmp #58
        bne bad3
        lda log+35
        cmp #7
        bne bad3
        jmp c5
bad3:
        lda #13
        rts

c5:
        ; --- caso 5: X=300 (>=256) en los pares -> FLAGS con bit8 ---
        lda #0
        sta logcnt
        lda #0
        jsr pusha              ; first
        lda #<300
        ldx #>300
        jsr pushax             ; x = 300
        lda #10
        jsr pusha              ; y
        lda #0
        jsr pusha              ; tile0
        lda #$00
        jsr _vc_sprite16_set
        ; cuadrante 0 (x=300): FLAGS en log+7 = 0x04
        lda log+7
        cmp #$04
        beq c5b
        lda #5
        rts
c5b:
        ; cuadrante 2 (x=300): FLAGS en log+27 = 0x04
        lda log+27
        cmp #$04
        beq c5c
        lda #51
        rts
c5c:
        ; cuadrante 1 (x=308): FLAGS en log+17 = 0x04
        lda log+17
        cmp #$04
        beq c5d
        lda #52
        rts
c5d:
        ; cuadrante 3 (x=308): FLAGS en log+37 = 0x04
        lda log+37
        cmp #$04
        beq allok
        lda #53
        rts

allok:
        lda #0
        ldx #0
        rts

        .import pusha, pushax
