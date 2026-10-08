; ============================================================================
; video.s - Núcleo en ensamblador de la biblioteca del Core de Vídeo
; ============================================================================
; CONVENCIÓN DE LLAMADA (cc65, sin __fastcall__):
;
;   El ÚLTIMO parámetro llega en A (8 bits) o A/X (16 bits, lo en A, hi en X).
;   Los parámetros ANTERIORES llegan por el stack software, de izquierda a
;   derecha (el primero se empuja primero, queda más abajo; se extrae el más
;   cercano al tope primero con popa/popax).
;
;   Ejemplos (firma -> cómo leer):
;     f(uint8_t a)                  -> A = a
;     f(uint8_t a, uint8_t b)       -> popa = a ; A = b
;     f(uint8_t a, uint8_t b, u8 c) -> popa = a ; popa = b ; A = c
;     f(uint16_t d)                 -> A = lo(d) ; X = hi(d)
;     f(uint16_t x, uint16_t y)     -> popax = x ; A = lo(y) ; X = hi(y)
;
;   Retorno en A (o A/X para 16 bits).
;
; El puerto indirecto (VRAM/OAM) NO auto-incrementa: cada escritura requiere
; fijar $D800 (dir baja) y $D801 (área + dir alta) antes de tocar $D802.
; ============================================================================

        .export _vc_wait_ready
        .export _vc_wait_vblank
        .export _vc_wait_vblank_end
        .export _vc_status
        .export _vc_setup_busy
        .export _vc_wait_setup
        .export _vc_write
        .export _vc_put_cell
        .export _vc_put_attr
        .export _vc_load_bg_pattern
        .export _vc_load_spr_pattern
        .export _vc_oam_put
        .export _vc_sprite_move
        .export _vc_sprite_set
        .export _vc_sprite16_set
        .export _vc_fill_tilemap
        .export _vc_clear_attr
        .export _vc_clear_bg_patterns
        .export _vc_clear_spr_patterns
        .export _vc_copy_tilemap
        .export _vc_copy_attr
        .export _vc_clear_oam
        .export _vc_set_scroll_x
        .export _vc_set_scroll_y
        .export _vc_set_raster
        .export _vc_set_band2_scroll
        .export _vc_set_band3_scroll
        .export _vc_pal_ptr
        .export _vc_pal_set
        .export _vc_pal_load

        .import popa, popax

; ============================================================================
; Zero Page propia del driver (definida antes de su uso para que ca65
; aplique direccionamiento de página cero).
; ============================================================================
        .segment "ZEROPAGE"
        .exportzp ptr, sptr0, sptr1
        .exportzp VC_COL, VC_ROW, VC_TILE, VC_FIELD, VC_SPRIDX
        .exportzp VC_PATDIR, VC_PATHI, VC_TMP, VC_PAGE
ptr:        .res 2
sptr0:      .res 2
sptr1:      .res 2
VC_COL:     .res 1
VC_ROW:     .res 1
VC_TILE:    .res 1
VC_FIELD:   .res 1
VC_SPRIDX:  .res 1
VC_PATDIR:  .res 1
VC_PATHI:   .res 1
VC_TMP:     .res 1
VC_PAGE:    .res 1
; --- temporales para vc_sprite16_set ---
VC_XLO:     .res 1
VC_XHI:     .res 1
VC_SY:      .res 1
VC_STILE:   .res 1
VC_SFLAGS:  .res 1

; --- Registros del core ---
; Para tests de host (sim65) se pueden redirigir a RAM definiendo TEST_HOOKS
; al ensamblar: los registros pasan a ser variables en $F000 (dirección de RAM
; libre en el mapa del test), de modo que el test puede leer lo escrito.
.ifdef TEST_HOOKS
VID_ADDR_LO     = $F000
VID_ADDR_HI     = $F001
VID_DATA        = $F002
VID_STATUS      = $F003
VID_SCROLL_X_LO = $F004
VID_SCROLL_X_HI = $F005
VID_SCROLL_Y_LO = $F006
VID_SCROLL_Y_HI = $F007
VID_RASTER_L0   = $F009
VID_BAND2_X_LO  = $F00A
VID_BAND2_X_HI  = $F00B
VID_BAND2_Y_LO  = $F00C
VID_BAND2_Y_HI  = $F00D
VID_RASTER_L1   = $F00E
VID_BAND3_X_LO  = $F00F
VID_BAND3_X_HI  = $F010
VID_BAND3_Y_LO  = $F011
VID_BAND3_Y_HI  = $F012
VID_PAL_PTR     = $F013
VID_PAL_LO      = $F014
VID_PAL_HI      = $F015
VID_SETUP       = $F016
VID_SETUP_ST    = $F017
.else
VID_ADDR_LO     = $D800
VID_ADDR_HI     = $D801
VID_DATA        = $D802
VID_STATUS      = $D803
VID_SCROLL_X_LO = $D804
VID_SCROLL_X_HI = $D805
VID_SCROLL_Y_LO = $D806
VID_SCROLL_Y_HI = $D807
VID_RASTER_L0   = $D809
VID_BAND2_X_LO  = $D80A
VID_BAND2_X_HI  = $D80B
VID_BAND2_Y_LO  = $D80C
VID_BAND2_Y_HI  = $D80D
VID_RASTER_L1   = $D80E
VID_BAND3_X_LO  = $D80F
VID_BAND3_X_HI  = $D810
VID_BAND3_Y_LO  = $D811
VID_BAND3_Y_HI  = $D812
VID_PAL_PTR     = $D813
VID_PAL_LO      = $D814
VID_PAL_HI      = $D815
VID_SETUP       = $D816
VID_SETUP_ST    = $D817
.endif

; --- Áreas del puerto indirecto ---
AREA_TILEMAP   = $00
AREA_ATTR      = $40
AREA_PAT_BG0   = $80
AREA_PAT_BG1   = $A0
AREA_OAM       = $C0
AREA_PAT_SPR0  = $C8
AREA_PAT_SPR1  = $E8

; --- Campos del OAM (offsets dentro del sprite) ---
VC_OAM_X       = 0
VC_OAM_Y       = 1
VC_OAM_TILE    = 2
VC_OAM_FLAGS   = 3
VC_OAM_COLL    = 4

; --- Flags de sprite ---
VC_SPR_XBIT8   = $04          ; bit 8 de la coordenada X

; --- Bits de status ---
ST_VBLANK      = $80
ST_READY       = $10

; --- Bit de estado del setup ($D817) ---
ST_SETUP_BUSY  = $01

; --- Tamaño del tilemap/atributos ---
MAP_BYTES_HI   = 8              ; 8 * 256 = 2048

; ============================================================================
        .segment "CODE"
; ============================================================================

; ----------------------------------------------------------------------------
; void vc_wait_ready(void) / vc_wait_vblank(void) / vc_wait_vblank_end(void)
; ----------------------------------------------------------------------------
_vc_wait_ready:
        lda VID_STATUS
        and #ST_READY
        beq _vc_wait_ready
        rts

_vc_wait_vblank:
        lda VID_STATUS
        and #ST_VBLANK
        beq _vc_wait_vblank
        rts

_vc_wait_vblank_end:
        lda VID_STATUS
        and #ST_VBLANK
        bne _vc_wait_vblank_end
        rts

; ----------------------------------------------------------------------------
; uint8_t vc_status(void)
; ----------------------------------------------------------------------------
_vc_status:
        lda VID_STATUS
        ldx #0
        rts

; ----------------------------------------------------------------------------
; uint8_t vc_setup_busy(void)
;   1 si hay un setup de VRAM en curso (BIT0 de $D817).
; ----------------------------------------------------------------------------
_vc_setup_busy:
        lda VID_SETUP_ST
        and #ST_SETUP_BUSY
        ldx #0
        rts

; ----------------------------------------------------------------------------
; void vc_wait_setup(void)
;   Espera a que termine el setup de VRAM (BUSY=0).
; ----------------------------------------------------------------------------
_vc_wait_setup:
        lda VID_SETUP_ST
        and #ST_SETUP_BUSY
        bne _vc_wait_setup
        rts

; ----------------------------------------------------------------------------
; void vc_write(uint8_t area_dir_hi, uint16_t addr, uint8_t data)
;   area_dir_hi ya viene pre-formado (area(7:6) | dir_alta(2:0)).
;   Convención: popa = area_dir_hi ; popax = addr ; A = data
; ----------------------------------------------------------------------------
_vc_write:
        sta VC_TILE            ; data (último param en A)
        jsr popax              ; addr
        sta VC_ROW             ; addr lo
        stx VC_COL             ; addr hi
        jsr popa               ; area_dir_hi
        sta VID_ADDR_HI
        lda VC_ROW
        sta VID_ADDR_LO
        lda VC_TILE
        sta VID_DATA
        rts

; ----------------------------------------------------------------------------
; void vc_put_cell(uint8_t col, uint8_t row, uint8_t tile)
;   Convención: popa = col ; popa = row ; A = tile
; ----------------------------------------------------------------------------
_vc_put_cell:
        sta VC_TILE
        jsr popa
        sta VC_ROW
        jsr popa
        sta VC_COL
        jsr calc_cell          ; ptr = row*64 + col
        lda ptr
        sta VID_ADDR_LO
        lda ptr+1
        sta VID_ADDR_HI        ; área 00 = tilemap
        lda VC_TILE
        sta VID_DATA
        rts

; ----------------------------------------------------------------------------
; void vc_put_attr(uint8_t col, uint8_t row, uint8_t attr)
;   Convención: popa = col ; popa = row ; A = attr
; ----------------------------------------------------------------------------
_vc_put_attr:
        sta VC_TILE            ; attr
        jsr popa
        sta VC_ROW
        jsr popa
        sta VC_COL
        jsr calc_cell
        lda ptr
        sta VID_ADDR_LO
        lda ptr+1
        ora #AREA_ATTR
        sta VID_ADDR_HI
        lda VC_TILE
        sta VID_DATA
        rts

; ----------------------------------------------------------------------------
; ptr = VC_ROW*64 + VC_COL   (stride 64 = shift). Destruye A.
; ----------------------------------------------------------------------------
calc_cell:
        lda VC_ROW
        lsr a
        lsr a
        sta ptr+1              ; ptr_hi = row >> 2
        lda VC_ROW
        and #$03
        asl a
        asl a
        asl a
        asl a
        asl a
        asl a
        sta ptr                ; (row & 3) << 6
        lda ptr
        clc
        adc VC_COL
        sta ptr
        bcc @done
        inc ptr+1
@done:
        rts

; ----------------------------------------------------------------------------
; void vc_load_bg_pattern(uint8_t tile, const uint8_t *plan0, const uint8_t *plan1)
;   Convención: popa = tile ; popax = plan0 ; A/X = plan1 (lo/hi)
; ----------------------------------------------------------------------------
_vc_load_bg_pattern:
        sta sptr1              ; A = plan1 lo
        stx sptr1+1            ; X = plan1 hi
        jsr popax
        sta sptr0
        stx sptr0+1            ; plan0
        jsr popa               ; tile
        jsr calc_pat_base      ; VC_PATHI:VC_PATDIR = tile*8
        ldy #0
@loop:
        jsr calc_pat_dir
        ora #AREA_PAT_BG0
        sta VID_ADDR_HI
        lda (sptr0),y
        sta VID_DATA
        jsr calc_pat_dir
        ora #AREA_PAT_BG1
        sta VID_ADDR_HI
        lda (sptr1),y
        sta VID_DATA
        iny
        cpy #8
        bne @loop
        rts

; ----------------------------------------------------------------------------
; void vc_load_spr_pattern(uint8_t spr, const uint8_t *plan0, const uint8_t *plan1)
;   Convención: popa = spr ; popax = plan0 ; A/X = plan1 (lo/hi)
; ----------------------------------------------------------------------------
_vc_load_spr_pattern:
        sta sptr1
        stx sptr1+1
        jsr popax
        sta sptr0
        stx sptr0+1
        jsr popa               ; spr
        jsr calc_pat_base      ; VC_PATHI:VC_PATDIR = spr*8
        ldy #0
@loop:
        jsr calc_pat_dir
        ora #AREA_PAT_SPR0
        sta VID_ADDR_HI
        lda (sptr0),y
        sta VID_DATA
        jsr calc_pat_dir
        ora #AREA_PAT_SPR1
        sta VID_ADDR_HI
        lda (sptr1),y
        sta VID_DATA
        iny
        cpy #8
        bne @loop
        rts

; ----------------------------------------------------------------------------
; calc_pat_base: A = índice de patrón -> VC_PATHI:VC_PATDIR = A*8 (16 bits).
;   Destruye A y X.
; ----------------------------------------------------------------------------
calc_pat_base:
        ldx #0
        stx VC_PATHI           ; parte alta = 0
        ldx #3
@shl:
        asl a
        rol VC_PATHI
        dex
        bne @shl
        sta VC_PATDIR
        rts

; ----------------------------------------------------------------------------
; calc_pat_dir: dentro del bucle de carga. Entrada: Y = fila (0..7).
;   Devuelve: VID_ADDR_LO = (base+fila) & $FF ; A = bits 2:0 de (base+fila)>>8.
;   No destruye Y.
; ----------------------------------------------------------------------------
calc_pat_dir:
        tya
        clc
        adc VC_PATDIR
        sta VID_ADDR_LO
        lda VC_PATHI
        adc #0
        rts

; ----------------------------------------------------------------------------
; void vc_oam_put(uint8_t spr, uint8_t field, uint8_t data)
;   Convención: popa = spr ; popa = field ; A = data
; ----------------------------------------------------------------------------
_vc_oam_put:
        sta VC_TILE            ; data
        jsr popa
        sta VC_FIELD
        jsr popa
        sta VC_SPRIDX
        lda VC_TILE            ; restaurar A = data (popa lo habia pisado con spr)
        jmp oam_write          ; escribe data en (spr,field); no retorna aqui

; ----------------------------------------------------------------------------
; oam_write: escribe A (dato) en el byte OAM del sprite VC_SPRIDX, campo
;   VC_FIELD. Fija la dirección y dispara la escritura. Destruye A.
;   byte = VC_SPRIDX*5 + VC_FIELD ; área OAM ($C0), dir alta = 0.
;
;   Bajo TEST_HOOKS la proporciona el test de host (registra addr/dato).
; ----------------------------------------------------------------------------
.ifdef TEST_HOOKS
        .import oam_write
.else
oam_write:
        sta VC_TILE            ; dato a escribir
        lda VC_SPRIDX
        sta VC_TMP
        asl a                  ; *2
        asl a                  ; *4
        clc
        adc VC_TMP             ; *5
        clc
        adc VC_FIELD
        sta VID_ADDR_LO
        lda #AREA_OAM
        sta VID_ADDR_HI
        lda VC_TILE
        sta VID_DATA
        rts
.endif

; ----------------------------------------------------------------------------
; void vc_sprite_move(uint8_t spr, uint16_t x, uint8_t y, uint8_t base_flags)
;   Escribe X (9 bits), Y y FLAGS (con el bit 8 de X) de un sprite.
;   Convención cc65: popa = y ; popax = x ; popa = spr ; A = base_flags
;
;   FLAGS final = base_flags | (bit8 de x)   (el resto de bits se preservan).
; ----------------------------------------------------------------------------
_vc_sprite_move:
        sta VC_ROW             ; base_flags
        jsr popa
        sta VC_COL             ; y
        jsr popax
        sta VC_PATDIR          ; x_lo
        stx VC_PATHI           ; x_hi (0 o 1)
        jsr popa
        sta VC_SPRIDX          ; spr

        ; --- campo X (+0): x_lo ---
        lda #VC_OAM_X
        sta VC_FIELD
        lda VC_PATDIR
        jsr oam_write

        ; --- campo Y (+1): y ---
        lda #VC_OAM_Y
        sta VC_FIELD
        lda VC_COL
        jsr oam_write

        ; --- campo FLAGS (+3): base_flags con el bit 8 de X ajustado ---
        lda #VC_OAM_FLAGS
        sta VC_FIELD
        lda VC_ROW             ; base_flags
        ldx VC_PATHI           ; 0 o 1
        beq @no_bit8
        ora #VC_SPR_XBIT8
        jmp @wr
@no_bit8:
        and #<(~VC_SPR_XBIT8)
@wr:
        jsr oam_write
        rts

; ----------------------------------------------------------------------------
; void vc_sprite_set(uint8_t spr, const vc_sprite_t *s)
;   Escribe los 5 campos del OAM desde la estructura (x_lo,y,tile,flags,coll).
;   Convención cc65: popax = spr ; A/X = puntero a la estructura (lo/hi)
;   (spr es uint8_t -> va por stack como 2 bytes? No: se promociona.
;    Verificado: pusha(spr) y el puntero en A/X)
; ----------------------------------------------------------------------------
_vc_sprite_set:
        sta sptr0              ; ptr bajo de la estructura
        stx sptr0+1            ; ptr alto
        jsr popa               ; spr (byte bajo del char promovido)
        sta VC_SPRIDX

        ; campo X (+0) = s[0]
        ldy #0
        lda #VC_OAM_X
        sta VC_FIELD
        lda (sptr0),y
        jsr oam_write
        ; campo Y (+1) = s[1]
        ldy #1
        lda #VC_OAM_Y
        sta VC_FIELD
        lda (sptr0),y
        jsr oam_write
        ; campo TILE (+2) = s[2]
        ldy #2
        lda #VC_OAM_TILE
        sta VC_FIELD
        lda (sptr0),y
        jsr oam_write
        ; campo FLAGS (+3) = s[3]
        ldy #3
        lda #VC_OAM_FLAGS
        sta VC_FIELD
        lda (sptr0),y
        jsr oam_write
        ; campo COLL (+4) = s[4]
        ldy #4
        lda #VC_OAM_COLL
        sta VC_FIELD
        lda (sptr0),y
        jmp oam_write          ; último: encadena el rts de oam_write

; ----------------------------------------------------------------------------
; void vc_sprite16_set(uint8_t first, uint16_t x, uint8_t y,
;                      uint8_t tile0, uint8_t flags)
;   Objeto 16x16 = 4 sprites consecutivos (first..first+3):
;     0 (x,   y)     tile0+0
;     1 (x+8, y)     tile0+1
;     2 (x,   y+8)   tile0+2
;     3 (x+8, y+8)   tile0+3
;   FLAGS de cada cuadrante = (bit8 de su X) | (flags & 0xF0) | (flags & 0x03).
;   COLL_POINT = VC_COLL_CENTER para todos.
;
;   Convención cc65: popa = tile0 ; popa = y ; popax = x ; popa = first ; A = flags
;
;   Nota: (flags & 0xF0) | (flags & 0x03) == flags & 0xF7  (descarta bits 3:2,
;   que incluyen el bit de X8; ése se calcula aparte).
; ----------------------------------------------------------------------------
_vc_sprite16_set:
        and #$F7               ; base_flags sin el bit de X8 (bit2)
        sta VC_SFLAGS
        jsr popa
        sta VC_STILE           ; tile0
        jsr popa
        sta VC_SY              ; y
        jsr popax
        sta VC_XLO             ; x_lo
        stx VC_XHI             ; x_hi (0 o 1)
        jsr popa
        sta VC_SPRIDX          ; first

        ; --- cuadrante 0: (x, y), tile0 ---
        jsr spr16_quad         ; usa VC_XLO/XHI/SY/STILE/SFLAGS
        ; --- cuadrante 1: (x+8, y), tile0+1 ---
        jsr spr16_next_x       ; x += 8
        inc VC_STILE
        inc VC_SPRIDX
        jsr spr16_quad
        ; --- cuadrante 2: (x, y+8), tile0+2 ---
        jsr spr16_prev_x       ; x -= 8  (vuelve a x)
        inc VC_STILE
        inc VC_SPRIDX
        lda VC_SY
        clc
        adc #8
        sta VC_SY
        jsr spr16_quad
        ; --- cuadrante 3: (x+8, y+8), tile0+3 ---
        jsr spr16_next_x
        inc VC_STILE
        inc VC_SPRIDX
        jsr spr16_quad
        rts

; ----------------------------------------------------------------------------
; spr16_quad: escribe los 5 campos del sprite VC_SPRIDX con la geometría actual
;   (VC_XLO/XHI, VC_SY, VC_STILE, VC_SFLAGS). Añade el bit8 de X a FLAGS.
; ----------------------------------------------------------------------------
spr16_quad:
        ; campo X (+0)
        lda #VC_OAM_X
        sta VC_FIELD
        lda VC_XLO
        jsr oam_write
        ; campo Y (+1)
        lda #VC_OAM_Y
        sta VC_FIELD
        lda VC_SY
        jsr oam_write
        ; campo TILE (+2)
        lda #VC_OAM_TILE
        sta VC_FIELD
        lda VC_STILE
        jsr oam_write
        ; campo FLAGS (+3) = base | bit8 de X
        lda #VC_OAM_FLAGS
        sta VC_FIELD
        lda VC_SFLAGS
        ldx VC_XHI
        beq @nob
        ora #VC_SPR_XBIT8
@nob:
        jsr oam_write
        ; campo COLL (+4) = centro (VC_COLL_CENTER = $24)
        lda #VC_OAM_COLL
        sta VC_FIELD
        lda #$24
        jmp oam_write          ; encadena el rts

; ----------------------------------------------------------------------------
; spr16_next_x / spr16_prev_x: VC_XHI:VC_XLO += 8 / -= 8 (16 bits, sin signo).
; ----------------------------------------------------------------------------
spr16_next_x:
        lda VC_XLO
        clc
        adc #8
        sta VC_XLO
        bcc @done
        inc VC_XHI
@done:
        rts

spr16_prev_x:
        lda VC_XLO
        sec
        sbc #8
        sta VC_XLO
        bcs @done
        dec VC_XHI
@done:
        rts

; ----------------------------------------------------------------------------
; void vc_fill_tilemap(uint8_t tile)
;   Convención: A = tile
; ----------------------------------------------------------------------------
_vc_fill_tilemap:
        sta VC_TILE
        lda #0
        sta VC_PAGE
@outer:
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda #AREA_TILEMAP
        ora VC_PAGE
        sta VID_ADDR_HI
        lda VC_TILE
        sta VID_DATA
        iny
        bne @inner
        inc VC_PAGE
        lda VC_PAGE
        cmp #MAP_BYTES_HI
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_clear_attr(void)   (sin argumentos)
; ----------------------------------------------------------------------------
_vc_clear_attr:
        lda #0
        sta VC_PAGE
@outer:
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda #AREA_ATTR
        ora VC_PAGE
        sta VID_ADDR_HI
        lda #0
        sta VID_DATA
        iny
        bne @inner
        inc VC_PAGE
        lda VC_PAGE
        cmp #MAP_BYTES_HI
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_clear_bg_patterns(void)   (sin argumentos)
;   Pone a 0 los 2048 bytes de cada plano de patrones de fondo.
;   ADVERTENCIA: borra la fuente de texto ($20-$7F). Ver el manual.
; ----------------------------------------------------------------------------
_vc_clear_bg_patterns:
        lda #0
        sta VC_PAGE
@outer:
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda #AREA_PAT_BG0
        ora VC_PAGE
        sta VID_ADDR_HI
        lda #0
        sta VID_DATA
        sty VID_ADDR_LO
        lda #AREA_PAT_BG1
        ora VC_PAGE
        sta VID_ADDR_HI
        lda #0
        sta VID_DATA
        iny
        bne @inner
        inc VC_PAGE
        lda VC_PAGE
        cmp #MAP_BYTES_HI
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_clear_spr_patterns(void)   (sin argumentos)
;   Pone a 0 el area de patrones de sprite: 512 bytes de cada plano.
;   El banco tiene 512 palabras por plano (patron*8+fila = 0..511), asi que
;   hacen falta 2 paginas (256+256) por plano, una por valor de dir_alta.
; ----------------------------------------------------------------------------
_vc_clear_spr_patterns:
        lda #0
        sta VC_PAGE
@outer:
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda #AREA_PAT_SPR0
        ora VC_PAGE
        sta VID_ADDR_HI
        lda #0
        sta VID_DATA
        sty VID_ADDR_LO
        lda #AREA_PAT_SPR1
        ora VC_PAGE
        sta VID_ADDR_HI
        lda #0
        sta VID_DATA
        iny
        bne @inner
        inc VC_PAGE
        lda VC_PAGE
        cmp #2                 ; 2 páginas = 512 bytes
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_copy_tilemap(const uint8_t *src)
;   Un único argumento puntero -> llega en A/X (lo en A, hi en X).
; ----------------------------------------------------------------------------
_vc_copy_tilemap:
        sta sptr0
        stx sptr0+1
        lda #0
        sta VC_PAGE
@outer:
        lda #AREA_TILEMAP
        ora VC_PAGE
        sta VID_ADDR_HI
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda (sptr0),y
        sta VID_DATA
        iny
        bne @inner
        inc sptr0+1
        inc VC_PAGE
        lda VC_PAGE
        cmp #MAP_BYTES_HI
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_copy_attr(const uint8_t *src)
;   Un único argumento puntero -> llega en A/X (lo en A, hi en X).
; ----------------------------------------------------------------------------
_vc_copy_attr:
        sta sptr0
        stx sptr0+1
        lda #0
        sta VC_PAGE
@outer:
        lda #AREA_ATTR
        ora VC_PAGE
        sta VID_ADDR_HI
        ldy #0
@inner:
        sty VID_ADDR_LO
        lda (sptr0),y
        sta VID_DATA
        iny
        bne @inner
        inc sptr0+1
        inc VC_PAGE
        lda VC_PAGE
        cmp #MAP_BYTES_HI
        bne @outer
        rts

; ----------------------------------------------------------------------------
; void vc_clear_oam(void)   (sin argumentos)
; ----------------------------------------------------------------------------
_vc_clear_oam:
        lda #0
        sta VC_SPRIDX
@loop:
        lda VC_SPRIDX
        sta VC_TMP
        asl a
        asl a
        clc
        adc VC_TMP             ; *5
        clc
        adc #1                 ; campo Y
        sta VID_ADDR_LO
        lda #AREA_OAM
        sta VID_ADDR_HI
        lda #248
        sta VID_DATA
        inc VC_SPRIDX
        lda VC_SPRIDX
        cmp #32
        bne @loop
        rts

; ----------------------------------------------------------------------------
; void vc_set_scroll_x(uint16_t x)   Convención: A = lo, X = hi
; ----------------------------------------------------------------------------
_vc_set_scroll_x:
        sta VID_SCROLL_X_LO
        txa
        and #$07
        sta VID_SCROLL_X_HI
        rts

; ----------------------------------------------------------------------------
; void vc_set_scroll_y(uint16_t y)   Convención: A = lo, X = hi
; ----------------------------------------------------------------------------
_vc_set_scroll_y:
        sta VID_SCROLL_Y_LO
        txa
        and #$07
        sta VID_SCROLL_Y_HI
        rts

; ----------------------------------------------------------------------------
; void vc_set_raster(uint8_t line0, uint8_t line1)
;   Convención: popa = line0 ; A = line1
; ----------------------------------------------------------------------------
_vc_set_raster:
        sta VC_TILE            ; line1
        jsr popa
        sta VID_RASTER_L0
        lda VC_TILE
        sta VID_RASTER_L1
        rts

; ----------------------------------------------------------------------------
; void vc_set_band2_scroll(uint16_t x, uint16_t y)
;   Convención: popax = x ; A = lo(y), X = hi(y)
; ----------------------------------------------------------------------------
_vc_set_band2_scroll:
        sta VC_COL             ; y lo
        stx VC_ROW             ; y hi
        jsr popax              ; x
        sta VID_BAND2_X_LO
        txa
        and #$07
        sta VID_BAND2_X_HI
        lda VC_COL
        sta VID_BAND2_Y_LO
        lda VC_ROW
        and #$07
        sta VID_BAND2_Y_HI
        rts

; ----------------------------------------------------------------------------
; void vc_set_band3_scroll(uint16_t x, uint16_t y)
; ----------------------------------------------------------------------------
_vc_set_band3_scroll:
        sta VC_COL
        stx VC_ROW
        jsr popax
        sta VID_BAND3_X_LO
        txa
        and #$07
        sta VID_BAND3_X_HI
        lda VC_COL
        sta VID_BAND3_Y_LO
        lda VC_ROW
        and #$07
        sta VID_BAND3_Y_HI
        rts

; ============================================================================
; Paletas programables ($D813-$D815, RGB444 con auto-incremento).
; ============================================================================

; ----------------------------------------------------------------------------
; void vc_pal_ptr(uint8_t entrada)   Convención: A = entrada (0..31)
;   0-15 = fondo, 16-31 = sprite.
; ----------------------------------------------------------------------------
_vc_pal_ptr:
        sta VID_PAL_PTR
        rts

; ----------------------------------------------------------------------------
; void vc_pal_set(uint8_t entrada, uint16_t rgb444)
;   Convención: popa = entrada ; A = lo(rgb444), X = hi(rgb444).
;   Deja PAL_PTR en entrada+1 (auto-incremento).
; ----------------------------------------------------------------------------
_vc_pal_set:
        sta VC_TILE            ; lo(rgb444)
        stx VC_TMP             ; hi(rgb444)
        jsr popa               ; entrada
        sta VID_PAL_PTR
        lda VC_TILE
        sta VID_PAL_LO
        lda VC_TMP
        sta VID_PAL_HI         ; escribe la entrada y PAL_PTR++
        rts

; ----------------------------------------------------------------------------
; void vc_pal_load(uint8_t entrada, const uint16_t *colores, uint8_t count)
;   Carga count colores (RGB444) consecutivos desde RAM/ROM, a partir de
;   'entrada' (auto-incremento).
;   Convención: popa = entrada ; popax = colores ; A = count.
; ----------------------------------------------------------------------------
_vc_pal_load:
        sta VC_PAGE            ; count
        jsr popax              ; puntero a colores
        sta sptr0
        stx sptr0+1
        jsr popa               ; entrada
        sta VID_PAL_PTR
        ldy #0                 ; indice * 2 (lo/hi por color)
        ldx #0                 ; contador (0..count-1)
@loop:
        lda (sptr0),y
        sta VID_PAL_LO
        iny
        lda (sptr0),y
        sta VID_PAL_HI         ; escribe y ptr++
        iny
        inx
        cpx VC_PAGE
        bne @loop
        rts
