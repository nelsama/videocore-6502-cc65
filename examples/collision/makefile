# ============================================================================
# examples/collision/makefile - Ejemplo de colisiones de la biblioteca vc
# ============================================================================
# Genera output/collision.bin (carga en $0800 con el monitor).
#
# Uso:
#   make          - compila el ejemplo
#   make clean    - limpia generados
#   make info     - tamano del binario
#   make map      - mapa de memoria
#
# Requiere que la biblioteca (vc.lib) y config/programa.cfg existan en la raiz
# del repositorio (dos niveles arriba).
# ============================================================================

CC65_HOME ?= D:/cc65

CC  = $(CC65_HOME)/bin/cl65.exe
CA65 = $(CC65_HOME)/bin/ca65.exe
LD  = $(CC65_HOME)/bin/ld65.exe

# Raiz del repositorio (dos niveles arriba de examples/demo)
ROOT = ../..

BUILD_DIR  = build
OUTPUT_DIR = output

LD_CONFIG = $(ROOT)/config/programa.cfg
LIB       = $(ROOT)/output/vc.lib

PROGRAM_NAME = collision
PROGRAM = $(OUTPUT_DIR)/$(PROGRAM_NAME).bin
MAP_FILE = $(OUTPUT_DIR)/$(PROGRAM_NAME).map

# Fuentes de la demo
APP_C   = main.c
APP_ASM = startup.s
APP_OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/startup.o

# Flags
CFLAGS  = -t none -O --cpu 6502 -I $(ROOT)/src -I include
ASFLAGS = -t none --cpu 6502
LDFLAGS = -C $(LD_CONFIG) -m $(MAP_FILE)

# ============================================================================
# REGLAS
# ============================================================================

all: dirs $(PROGRAM)
	@echo "========================================"
	@echo "Demo generada: $(PROGRAM)"
	@ls -l $(PROGRAM) | awk '{print "Tamano: " $$5 " bytes"}'
	@echo "========================================"

dirs:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)

$(BUILD_DIR)/main.o: $(APP_C) $(ROOT)/src/video.h include/romapi.h
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/startup.o: $(APP_ASM)
	$(CA65) $(ASFLAGS) -o $@ $<

$(PROGRAM): $(APP_OBJECTS) $(LIB)
	$(LD) $(LDFLAGS) -o $@ $(APP_OBJECTS) $(LIB) $(CC65_HOME)/lib/none.lib

# ============================================================================
# UTILIDADES
# ============================================================================

info:
	@if [ -f $(PROGRAM) ]; then \
		ls -l $(PROGRAM) | awk '{print "Tamano: " $$5 " bytes"}'; \
	else \
		echo "Error: demo no compilada"; \
	fi

map:
	@if [ -f $(MAP_FILE) ]; then cat $(MAP_FILE); else echo "Error: mapa no encontrado"; fi

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Limpieza completa"

.PHONY: all dirs clean info map
