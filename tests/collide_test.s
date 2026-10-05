; ============================================================================
; tests/collide_test.s - Test de host (sim65) para src/collide.s
; ============================================================================
; Ejecuta cl_overlap y cl_point con casos conocidos. Devuelve en A el numero
; del primer caso que falla (0 = todos OK). Sin salida de texto, sin hardware.
;
; Compilar y ejecutar (desde la raiz del repo, con bash):
;   ca65 -t sim6502 --cpu 6502 -o build/collide_sim.o src/collide.s
;   cl65 -t sim6502 -o build/collide_test.prg tests/collide_test.s build/collide_sim.o
;   sim65 build/collide_test.prg ; echo "exit=$?"
; ============================================================================

        .export _main
        .import _cl_overlap, _cl_point
        .importzp _CL_AX, _CL_AY, _CL_AW, _CL_AH
        .importzp _CL_BX, _CL_BY, _CL_BW, _CL_BH

        .segment "CODE"

; --- carga la caja A desde box_as (X = offset en bytes, multiplo de 8) ---
setA:
        lda box_as+0,x
        sta _CL_AX
        lda box_as+1,x
        sta _CL_AX+1
        lda box_as+2,x
        sta _CL_AY
        lda box_as+3,x
        sta _CL_AY+1
        lda box_as+4,x
        sta _CL_AW
        lda box_as+5,x
        sta _CL_AW+1
        lda box_as+6,x
        sta _CL_AH
        lda box_as+7,x
        sta _CL_AH+1
        rts

; --- carga la caja B desde box_bs ---
setB:
        lda box_bs+0,x
        sta _CL_BX
        lda box_bs+1,x
        sta _CL_BX+1
        lda box_bs+2,x
        sta _CL_BY
        lda box_bs+3,x
        sta _CL_BY+1
        lda box_bs+4,x
        sta _CL_BW
        lda box_bs+5,x
        sta _CL_BW+1
        lda box_bs+6,x
        sta _CL_BH
        lda box_bs+7,x
        sta _CL_BH+1
        rts

; --- casos: 4 words (x,y,w,h) por entrada ---
box_as:
        .word 100,100,8,8    ; 1: A lejos
        .word 100,100,8,8    ; 2: A solapa
        .word 100,100,8,8    ; 3: A borde-x
        .word 100,100,8,8    ; 4: A igual
        .word 104,104,0,0    ; 5: punto dentro
        .word 108,104,0,0    ; 6: punto fuera der
        .word 99,104,0,0     ; 7: punto fuera izq

box_bs:
        .word 200,100,8,8    ; 1: lejos -> 0
        .word 104,104,8,8    ; 2: solapa -> 1
        .word 108,100,8,8    ; 3: borde exacto -> 0
        .word 100,100,8,8    ; 4: igual -> 1
        .word 100,100,8,8    ; 5: punto dentro -> 1
        .word 100,100,8,8    ; 6: punto fuera -> 0
        .word 100,100,8,8    ; 7: punto fuera -> 0

_main:
        ; --- caso 1: overlap lejos -> 0 ---
        ldx #0
        jsr setA
        ldx #0
        jsr setB
        jsr _cl_overlap
        cmp #0
        beq c1ok
        lda #1
        rts
c1ok:

        ; --- caso 2: overlap solapa -> 1 ---
        ldx #8
        jsr setA
        ldx #8
        jsr setB
        jsr _cl_overlap
        cmp #1
        beq c2ok
        lda #2
        rts
c2ok:

        ; --- caso 3: borde exacto -> 0 ---
        ldx #16
        jsr setA
        ldx #16
        jsr setB
        jsr _cl_overlap
        cmp #0
        beq c3ok
        lda #3
        rts
c3ok:

        ; --- caso 4: igual -> 1 ---
        ldx #24
        jsr setA
        ldx #24
        jsr setB
        jsr _cl_overlap
        cmp #1
        beq c4ok
        lda #4
        rts
c4ok:

        ; --- caso 5: punto dentro -> 1 ---
        ldx #32
        jsr setA
        ldx #32
        jsr setB
        jsr _cl_point
        cmp #1
        beq c5ok
        lda #5
        rts
c5ok:

        ; --- caso 6: punto fuera der -> 0 ---
        ldx #40
        jsr setA
        ldx #40
        jsr setB
        jsr _cl_point
        cmp #0
        beq c6ok
        lda #6
        rts
c6ok:

        ; --- caso 7: punto fuera izq -> 0 ---
        ldx #48
        jsr setA
        ldx #48
        jsr setB
        jsr _cl_point
        cmp #0
        beq c7ok
        lda #7
        rts
c7ok:

        lda #0
        ldx #0
        rts
