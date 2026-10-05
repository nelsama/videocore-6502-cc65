; ============================================================================
; collide.s - Nucleo de deteccion de colisiones en ensamblador
; ============================================================================
; Colision sprite<->sprite por software (el hardware NO la tiene).
; Trabaja con cajas AABB en pixeles logicos (320x240).
;
; CONVENCION DE LLAMADA: los wrappers de gfx.c cargan la Zero Page del modulo
; (CL_AX..CL_BH, prefijo '_' en asm) y llaman a estas rutinas, que devuelven el
; resultado en A (1 = colision, 0 = no). No usan el stack de argumentos.
;
; Todos los valores son de 16 bits SIN signo. No hay aritmetica con signo, asi
; que no se repite el bug de restar mal un int8_t negativo.
;
; IDEA (AABB): dos cajas NO se solapan si se cumple alguna de:
;     AX >= BX + BW      (A a la derecha de B)
;     BX >= AX + AW      (A a la izquierda de B)
;     AY >= BY + BH      (A debajo de B)
;     BY >= AY + AH      (A encima de B)
;   => solapan = 1 si NINGUNA se cumple.
;
; Se calcula "caja contra caja" con sumas de 16 bits y comparaciones; en cuanto
; una condicion de "no solape" se da, se sale con A=0.
; ============================================================================

        .export _cl_overlap
        .export _cl_point
        .exportzp _CL_AX, _CL_AY, _CL_AW, _CL_AH
        .exportzp _CL_BX, _CL_BY, _CL_BW, _CL_BH

; ============================================================================
; Zero Page del modulo de colisiones
;   (los nombres llevan prefijo '_' porque C los referencia con _CL_AX, etc.;
;    cc65 antepone '_' a los simbolos C)
; ============================================================================
        .segment "ZEROPAGE"
_CL_AX: .res 2      ; caja A
_CL_AY: .res 2
_CL_AW: .res 2
_CL_AH: .res 2
_CL_BX: .res 2      ; caja B
_CL_BY: .res 2
_CL_BW: .res 2
_CL_BH: .res 2
_CL_T:  .res 2      ; temporal interno

        .segment "CODE"

; ----------------------------------------------------------------------------
; macro: add16 (op1 + op2 -> _CL_T, 16 bits)
; ----------------------------------------------------------------------------
.macro add16 op1, op2
        lda op1
        clc
        adc op2
        sta _CL_T
        lda op1+1
        adc op2+1
        sta _CL_T+1
.endmacro

; ============================================================================
; uint8_t cl_overlap(void)
;   1 si la caja A solapa con la caja B.
; ============================================================================
_cl_overlap:
        ; --- if (AX >= BX + BW) return 0; ---
        add16 _CL_BX, _CL_BW          ; _CL_T = BX + BW
        lda _CL_AX
        cmp _CL_T
        lda _CL_AX+1
        sbc _CL_T+1
        bcs @no                     ; AX >= BX + BW

        ; --- if (BX >= AX + AW) return 0; ---
        add16 _CL_AX, _CL_AW          ; _CL_T = AX + AW
        lda _CL_BX
        cmp _CL_T
        lda _CL_BX+1
        sbc _CL_T+1
        bcs @no                     ; BX >= AX + AW

        ; --- if (AY >= BY + BH) return 0; ---
        add16 _CL_BY, _CL_BH          ; _CL_T = BY + BH
        lda _CL_AY
        cmp _CL_T
        lda _CL_AY+1
        sbc _CL_T+1
        bcs @no                     ; AY >= BY + BH

        ; --- if (BY >= AY + AH) return 0; ---
        add16 _CL_AY, _CL_AH          ; _CL_T = AY + AH
        lda _CL_BY
        cmp _CL_T
        lda _CL_BY+1
        sbc _CL_T+1
        bcs @no                     ; BY >= AY + AH

        ; ninguna condicion de no-solape -> hay colision
        lda #1
        rts
@no:
        lda #0
        rts

; ============================================================================
; uint8_t cl_point(void)
;   1 si el punto (_CL_AX, _CL_AY) esta dentro de la caja B
;   (_CL_BX, _CL_BY, _CL_BW, _CL_BH).
; ============================================================================
_cl_point:
        ; px >= bx ?
        lda _CL_AX
        cmp _CL_BX
        lda _CL_AX+1
        sbc _CL_BX+1
        bcc @fuera                  ; px < bx

        ; px < bx + bw ?
        add16 _CL_BX, _CL_BW          ; _CL_T = bx + bw
        lda _CL_AX
        cmp _CL_T
        lda _CL_AX+1
        sbc _CL_T+1
        bcs @fuera                  ; px >= bx + bw

        ; py >= by ?
        lda _CL_AY
        cmp _CL_BY
        lda _CL_AY+1
        sbc _CL_BY+1
        bcc @fuera                  ; py < by

        ; py < by + bh ?
        add16 _CL_BY, _CL_BH          ; _CL_T = by + bh
        lda _CL_AY
        cmp _CL_T
        lda _CL_AY+1
        sbc _CL_T+1
        bcs @fuera                  ; py >= by + bh

        lda #1
        rts
@fuera:
        lda #0
        rts
