#!/bin/sh
# ============================================================================
# tests/run.sh - Ejecuta los tests unitarios de los nucleos en ensamblador
# ============================================================================
# Usa sim65 (simulador de 6502 de cc65). No necesita hardware.
# Cada test devuelve 0 si pasa; >0 indica el caso que falla.
#
# Uso:  sh tests/run.sh
# ============================================================================

set -e
CC65_HOME="${CC65_HOME:-D:/cc65}"
CA65="$CC65_HOME/bin/ca65.exe"
CL65="$CC65_HOME/bin/cl65.exe"
SIM65="$CC65_HOME/bin/sim65.exe"

mkdir -p build
fail=0

run() {
    name="$1"; shift
    printf '%-26s' "$name"
    if "$SIM65" "$1" >/dev/null 2>&1; then
        echo "OK"
    else
        echo "FAIL (exit=$?)"
        fail=1
    fi
}

echo "== Tests de la biblioteca vc (sim65) =="

# --- collide.s ---
"$CA65" -t sim6502 --cpu 6502 -o build/collide_sim.o src/collide.s
"$CL65" -t sim6502 -o build/test_collide.prg tests/collide_test.s build/collide_sim.o
run "collisions (AABB)" build/test_collide.prg

# --- video.s (con TEST_HOOKS) ---
"$CA65" -t sim6502 --cpu 6502 -D TEST_HOOKS -o build/video_sim.o src/video.s
"$CL65" -t sim6502 -o build/test_sprite.prg tests/sprite_test.s build/video_sim.o
run "sprite move/set" build/test_sprite.prg

"$CL65" -t sim6502 -o build/test_sprite16.prg tests/sprite16_test.s build/video_sim.o
run "sprite 16x16" build/test_sprite16.prg

"$CL65" -t sim6502 -o build/test_pat.prg tests/pat_base_test.s build/video_sim.o
run "dir de patron (tile>=32)" build/test_pat.prg

"$CL65" -t sim6502 -o build/test_humano.prg tests/humano_test.s build/video_sim.o
run "sprite_set escribe TILE" build/test_humano.prg

"$CL65" -t sim6502 -o build/test_sprload.prg tests/spr_load_test.s build/video_sim.o
run "carga de patron de sprite" build/test_sprload.prg

"$CL65" -t sim6502 -o build/test_pal.prg tests/pal_test.s build/video_sim.o
run "paletas programables" build/test_pal.prg

echo "======================================="
if [ "$fail" = "0" ]; then
    echo "Todos los tests pasan."
else
    echo "HAY TESTS QUE FALLAN."
fi
exit "$fail"
