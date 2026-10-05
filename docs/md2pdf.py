#!/usr/bin/env python3
"""Convierte un Markdown a PDF (estilo manual), sin dependencias externas de red.

Uso:
    python md2pdf.py VIDEO-LIB.md [salida.pdf]

Requisitos: markdown, xhtml2pdf.
"""
import sys
import os
import shutil
import markdown
from xhtml2pdf import pisa

# Fuentes locales del proyecto (se copian de C:/Windows/Fonts la primera vez).
# Cubren los simbolos tecnicos (>=, ->, etc.). xhtml2pdf solo puede leer archivos
# dentro del directorio del documento, por eso viven junto a este script.
HERE = os.path.dirname(os.path.abspath(__file__))
BODY_TTF = os.path.join(HERE, "_body.ttf")
MONO_TTF = os.path.join(HERE, "_mono.ttf")
WIN_FONTS = "C:/Windows/Fonts"


def ensure_fonts():
    """Copia las fuentes del sistema si no estan ya (Windows)."""
    pairs = [
        (os.path.join(WIN_FONTS, "calibri.ttf"), BODY_TTF),
        (os.path.join(WIN_FONTS, "consola.ttf"), MONO_TTF),
    ]
    for src, dst in pairs:
        if not os.path.exists(dst) and os.path.exists(src):
            try:
                shutil.copyfile(src, dst)
            except OSError:
                pass

CSS = """
@page { size: A4; margin: 1.6cm 1.5cm; }
body   { font-family: BodyFont, Helvetica, Arial, sans-serif; font-size: 9.5pt;
         line-height: 1.45; color: #1a1a1a; }
h1     { font-size: 20pt; color: #0b3d91; border-bottom: 2px solid #0b3d91;
         padding-bottom: 4px; margin-top: 0; }
h2     { font-size: 14pt; color: #0b3d91; margin-top: 18px;
         border-bottom: 1px solid #cfd8e3; padding-bottom: 2px; }
h3     { font-size: 11.5pt; color: #234a7d; margin-top: 14px; }
h4     { font-size: 10pt; color: #234a7d; margin-top: 12px; }
p, li  { font-size: 9.5pt; }
code   { font-family: MonoFont, "Courier New", monospace; font-size: 8.6pt;
         background: #f2f4f7; padding: 1px 3px; }
pre    { font-family: MonoFont, "Courier New", monospace; font-size: 8.4pt;
         background: #f6f8fa; border: 1px solid #d8dee4; padding: 7px 9px;
         line-height: 1.35; }
pre code { background: none; padding: 0; }
table  { border-collapse: collapse; width: 100%; margin: 8px 0; }
th     { background: #e6ecf4; border: 1px solid #b9c6d8; padding: 4px 6px;
         font-size: 8.8pt; text-align: left; }
td     { border: 1px solid #cdd7e4; padding: 4px 6px; font-size: 8.8pt; }
blockquote { border-left: 3px solid #9bb0cc; margin: 8px 0; padding: 2px 10px;
             color: #33445c; background: #f7f9fc; }
hr     { border: none; border-top: 1px solid #cfd8e3; margin: 14px 0; }
a      { color: #0b3d91; text-decoration: none; }
"""


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + ".pdf"

    ensure_fonts()

    with open(src, "r", encoding="utf-8") as f:
        text = f.read()

    # Quita variacion de emoji y simbolos que ninguna fuente de texto cubre.
    for ch in ("\ufe0f", "\u26a0\ufe0f", "\u26a0"):
        text = text.replace(ch, "!")

    html_body = markdown.markdown(
        text,
        extensions=["extra", "tables", "fenced_code", "sane_lists", "toc"],
    )

    # Registra las fuentes TTF locales para los simbolos tecnicos.
    font_css = ""
    if os.path.exists(BODY_TTF):
        font_css += (
            "@font-face { font-family: BodyFont; src: url('_body.ttf'); }"
        )
    if os.path.exists(MONO_TTF):
        font_css += (
            "@font-face { font-family: MonoFont; src: url('_mono.ttf'); }"
        )

    html = (
        "<html><head><meta charset='utf-8'>"
        "<style>" + font_css + CSS + "</style></head><body>"
        + html_body + "</body></html>"
    )

    with open(out, "wb") as f:
        result = pisa.CreatePDF(html, dest=f, encoding="utf-8")

    if result.err:
        print("ERROR al generar el PDF")
        return 1
    print(f"PDF generado: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
