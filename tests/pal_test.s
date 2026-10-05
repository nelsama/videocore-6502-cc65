; ============================================================================
; tests/pal_test.s - Test de host (sim65) de las rutinas de paleta (video.s)
; ============================================================================
; Verifica vc_pal_ptr, vc_pal_set y vc_pal_load con los registros de paleta
; redirigidos a RAM ($F013-$F015, definidos en video.s bajo TEST_HOOKS).
;
; Compilar y ejecutar (desde la raiz, con bash):
;   ca65 -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
;   cl65 -t sim6502 -o build/test_pal.prg tests/pal_test.s build/video_sim.o
;   sim65 build/test_pal.prg ; echo "exit=$?"
;
; Devuelve en A el numero de caso que falla (0 = todos OK).
; ============================================================================

        .export _main
        .export oam_write            ; requerida por video.s bajo TEST_HOOKS
        .import _vc_pal_ptr, _vc_pal_set, _vc_pal_load

; Registros simulados (coinciden con TEST_HOOKS de video.s)
PAL_PTR = $F013
PAL_LO  = $F014
PAL_HI  = $F015

        .segment "CODE"

oam_write:                           ; no se usa en este test
        rts

_main:
        ; --- caso 1: vc_pal_ptr(20) -> PAL_PTR = 20 ---
        lda #20
        jsr _vc_pal_ptr
        lda PAL_PTR
        cmp #20
        beq c1ok
        lda #1
        rts
c1ok:

        ; --- caso 2: vc_pal_set(22, 0x0FF0)
        ;   -> PAL_PTR = 22 (se fija primero), luego PAL_LO=$F0, PAL_HI=$0F.
        ;   Tras escribir $D815, el puntero REAL auto-incrementa, pero en el
        ;   simulador solo queda el valor del ultimo byte escrito. Verificamos
        ;   el puntero (que se fijo antes) y el par lo/hi.
        ;   Convencion: popa = entrada, A = lo, X = hi.
        lda #22
        jsr pusha
        lda #$F0
        ldx #$0F
        jsr _vc_pal_set
        lda PAL_PTR
        cmp #22
        beq c2a
        lda #2
        rts
c2a:
        lda PAL_LO
        cmp #$F0
        beq c2b
        lda #21
        rts
c2b:
        lda PAL_HI
        cmp #$0F
        beq c2ok
        lda #22
        rts
c2ok:

        ; --- caso 3: vc_pal_load(0, colores, 4)
        ;   colores = 4 palabras RGB444. Al terminar, PAL_PTR apunta al inicio (0)
        ;   y el ultimo lo/hi escrito es el del 4o color.
        lda #0
        jsr pusha                    ; entrada
        lda #<colores
        ldx #>colores
        jsr pushax                   ; puntero
        lda #4
        jsr _vc_pal_load             ; count = 4
        lda PAL_PTR
        cmp #0
        beq c3a
        lda #3
        rts
c3a:
        ; ultimo color cargado = colores[3] = $0ABC -> lo=BC, hi=0A
        lda PAL_LO
        cmp #$BC
        beq c3b
        lda #31
        rts
c3b:
        lda PAL_HI
        cmp #$0A
        beq c3ok
        lda #32
        rts
c3ok:

        ; --- caso 4: vc_pal_load con count=2 -> solo carga 2 colores ---
        lda #16
        jsr pusha
        lda #<colores
        ldx #>colores
        jsr pushax
        lda #2
        jsr _vc_pal_load
        ; ultimo color cargado = colores[1] = $0123 -> lo=23, hi=01
        lda PAL_LO
        cmp #$23
        beq c4a
        lda #4
        rts
c4a:
        lda PAL_HI
        cmp #$01
        beq c4ok
        lda #41
        rts
c4ok:

        ; todo OK
        lda #0
        ldx #0
        rts

        .segment "RODATA"
; 4 colores RGB444 (lo, hi por color en little-endian)
colores:
        .word $0000                  ; entrada 0
        .word $0123                  ; entrada 1
        .word $0456                  ; entrada 2
        .word $0ABC                  ; entrada 3

        .import pusha, pushax
