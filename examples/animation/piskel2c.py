#!/usr/bin/env python3
"""Decodifica un .piskel (PNG base64) y emite arrays C de patrones de sprite
en formato planar 2bpp (plano0/plano1) para la biblioteca vc.

Uso:
    python piskel2c.py <archivo.piskel> [--map R=2,P=3,Y=1]

Cada frame es 8x8. Cada color opaco se mapea a un indice 0-3 (0=transparente).
Sin --map, los colores se asignan por LUMINOSIDAD (oscuro->indice 1).

Con --map se fija a que indice va cada color, por su letra inicial en el
listado (R=rojo, P=rosa, Y=amarillo...). Ejemplo:
    --map R=2 P=3 Y=1

Se emiten dos arrays uint8_t[8] por frame listos para vc_load_spr_pattern().
El color REAL que se ve depende de la PALETA del sprite (FLAGS), no del indice.
"""
import base64, json, sys, struct, zlib

def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    width = height = color_type = None
    idat = b''
    while pos < len(data):
        length = struct.unpack('>I', data[pos:pos+4])[0]
        ctype = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+length]
        pos += 12 + length
        if ctype == b'IHDR':
            width, height, bd, color_type = struct.unpack('>IIBB', chunk[:10])
        elif ctype == b'IDAT':
            idat += chunk
        elif ctype == b'IEND':
            break
    raw = zlib.decompress(idat)
    nch = 4 if color_type == 6 else 3
    stride = width * nch
    out = bytearray(); prev = bytearray(stride); i = 0
    for y in range(height):
        ft = raw[i]; i += 1
        line = bytearray(raw[i:i+stride]); i += stride
        for x in range(stride):
            a = line[x-nch] if x >= nch else 0
            b = prev[x]
            c = prev[x-nch] if x >= nch else 0
            if ft == 1: line[x] = (line[x] + a) & 0xFF
            elif ft == 2: line[x] = (line[x] + b) & 0xFF
            elif ft == 3: line[x] = (line[x] + (a+b)//2) & 0xFF
            elif ft == 4:
                p = a+b-c; pa,pb,pc = abs(p-a),abs(p-b),abs(p-c)
                pr = a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out += line; prev = line
    return width, height, bytes(out), nch

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    path = args[0]
    # --map R=2 P=3 Y=1  ->  {'R':2,'P':3,'Y':1}  (varios tokens)
    forced = {}
    for k in range(1, len(args)):
        if args[k] == '--map':
            for tok in args[k+1:]:
                if '=' in tok:
                    letter, num = tok.split('=')
                    forced[letter.upper()] = int(num)
            break

    j = json.load(open(path))
    layers = j['piskel']['layers']
    if isinstance(layers, str):
        layers = json.loads(layers)
    l0 = json.loads(layers[0]) if isinstance(layers[0], str) else layers[0]
    png = base64.b64decode(l0['chunks'][0]['base64PNG'].split(',', 1)[1])
    open('anim_tmp.png', 'wb').write(png)
    w, h, px, nch = read_png('anim_tmp.png')
    nframes = w // 8

    # Recolecta colores opacos y ordenalos por LUMINOSIDAD (oscuro->claro).
    cols = []
    for f in range(nframes):
        for y in range(h):
            for x in range(8):
                idx = (y*w + f*8 + x) * nch
                a = px[idx+3] if nch == 4 else 255
                if a >= 128:
                    rgb = (px[idx], px[idx+1], px[idx+2])
                    if rgb not in cols:
                        cols.append(rgb)
    cols.sort(key=lambda c: 0.299*c[0] + 0.587*c[1] + 0.114*c[2])  # luma

    # Letra por color (por orden de luminosidad): R,P,Y,...
    letters = ['R', 'P', 'Y', 'G', 'C', 'M']
    collet = {rgb: letters[i] for i, rgb in enumerate(cols)}
    colmap = {rgb: i + 1 for i, rgb in enumerate(cols)}
    # Aplica el mapeo forzado (por letra)
    for rgb, let in collet.items():
        if let in forced:
            colmap[rgb] = forced[let]

    print(f"// {nframes} frames de 8x8; mapeo de colores (por luminosidad):")
    for rgb in cols:
        let = collet[rgb]
        print(f"//   '{let}' rgb{rgb} -> indice {colmap[rgb]}")
    print("// El color visible depende de la PALETA del sprite (FLAGS).")

    for f in range(nframes):
        p0 = [0]*8; p1 = [0]*8
        for y in range(h):
            for x in range(8):
                idx = (y*w + f*8 + x) * nch
                a = px[idx+3] if nch == 4 else 255
                ci = 0
                if a >= 128:
                    ci = colmap[(px[idx], px[idx+1], px[idx+2])]
                bit = 0x80 >> x
                if ci & 1: p0[y] |= bit
                if ci & 2: p1[y] |= bit
        def fmt(arr):
            return "{" + ",".join(f"0x{b:02X}" for b in arr) + "}"
        print(f"// frame {f}")
        print(f"static const uint8_t f{f}_p0[8] = {fmt(p0)};")
        print(f"static const uint8_t f{f}_p1[8] = {fmt(p1)};")

if __name__ == '__main__':
    main()
