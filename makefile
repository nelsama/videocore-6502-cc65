# ============================================================================
# makefile - Biblioteca del Core de Video (vc)
# ============================================================================
# Construye la biblioteca estatica output/vc.lib a partir de:
#   src/video.s    (nucleo en ensamblador: puerto indirecto, sprites, VBLANK)
#   src/collide.s  (nucleo de colisiones AABB en ensamblador)
#   src/gfx.c      (capa de alto nivel en C: texto, helpers, utilidades)
#
# Los ejemplos viven en examples/ (cada uno con su propio makefile).
#
# Uso:
#   make          - compila la biblioteca
#   make test     - ejecuta los tests unitarios de los nucleos asm (sim65)
#   make clean    - limpia archivos generados
# ============================================================================

CC65_HOME ?= D:/cc65

CC   = $(CC65_HOME)/bin/cl65.exe
CA65 = $(CC65_HOME)/bin/ca65.exe
AR   = $(CC65_HOME)/bin/ar65.exe

SRC_DIR     = src
INCLUDE_DIR = include
BUILD_DIR   = build
OUTPUT_DIR  = output

LIB = $(OUTPUT_DIR)/vc.lib
LIB_ASM     = $(SRC_DIR)/video.s $(SRC_DIR)/collide.s
LIB_C       = $(SRC_DIR)/gfx.c $(SRC_DIR)/gfx_vram.c $(SRC_DIR)/gfx_tiles.c \
              $(SRC_DIR)/gfx_text.c $(SRC_DIR)/gfx_pal.c
LIB_OBJECTS = $(BUILD_DIR)/video.o $(BUILD_DIR)/collide.o \
              $(BUILD_DIR)/gfx.o $(BUILD_DIR)/gfx_vram.o \
              $(BUILD_DIR)/gfx_tiles.o $(BUILD_DIR)/gfx_text.o \
              $(BUILD_DIR)/gfx_pal.o

CFLAGS  = -t none -O --cpu 6502 -I $(SRC_DIR) -I $(INCLUDE_DIR)
ASFLAGS = -t none --cpu 6502

# ============================================================================
# REGLAS
# ============================================================================

all: dirs $(LIB)
	@echo "========================================"
	@echo "Biblioteca generada: $(LIB)"
	@ls -l $(LIB) | awk '{print "Tamano (con simbolos): " $$5 " bytes"}'
	@echo "========================================"

dirs:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)

$(BUILD_DIR)/video.o: $(LIB_ASM)
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/collide.o: $(SRC_DIR)/collide.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/gfx.o: $(LIB_C) $(SRC_DIR)/video.h
	$(CC) -c $(CFLAGS) -o $@ $(SRC_DIR)/gfx.c

$(BUILD_DIR)/gfx_vram.o: $(SRC_DIR)/gfx_vram.c $(SRC_DIR)/video.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/gfx_tiles.o: $(SRC_DIR)/gfx_tiles.c $(SRC_DIR)/video.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/gfx_text.o: $(SRC_DIR)/gfx_text.c $(SRC_DIR)/video.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/gfx_pal.o: $(SRC_DIR)/gfx_pal.c $(SRC_DIR)/video.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(LIB): $(LIB_OBJECTS)
	@rm -f $@
	$(AR) a $@ $(LIB_OBJECTS)

# ============================================================================
# UTILIDADES
# ============================================================================

test: dirs
	@sh tests/run.sh

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Limpieza completa"

help:
	@echo "Uso:"
	@echo "  make        - compila la biblioteca (output/vc.lib)"
	@echo "  make test   - ejecuta los tests unitarios (sim65)"
	@echo "  make clean  - limpia generados"
	@echo "  Para la demo: cd examples/demo && make"

.PHONY: all dirs test clean help
