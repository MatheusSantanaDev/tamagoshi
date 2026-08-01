#!/usr/bin/env python3
"""Converte PNG/JPEG em array C para Sprites.h (bit=1=preto, MSB first).

Uso:
  python3 png_to_sprite.py imagem.png [largura] [altura]
      Converte uma imagem e imprime o array C (tamanho padrao: 48x48).

  python3 png_to_sprite.py pasta/ --size 48x48
      Converte todos os PNG/JPEG da pasta (nome_arquivo.png -> NOME_ARQUIVO)
      e imprime todos os arrays de uma vez, prontos para colar no Sprites.h.

Dicas:
  - Use imagens com fundo branco; fundo transparente vira branco.
  - O dithering (Floyd-Steinberg) melhora muito o resultado em tela B/W.
  - Confira se SPRITE_W/SPRITE_H no Sprites.h bate com o tamanho usado.
"""
import sys
import os
from PIL import Image

def to_c_array(img, w, h, dither=True):
    # Composita transparentes sobre branco
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img)
    img = img.convert("L")

    # Letterbox mantendo proporcao, centralizado
    img.thumbnail((w, h), Image.LANCZOS)
    canvas = Image.new("L", (w, h), 255)
    canvas.paste(img, ((w - img.width) // 2, (h - img.height) // 2))
    img = canvas

    if dither:
        img = img.convert("1", dither=Image.FLOYDSTEINBERG)
    else:
        img = img.point(lambda p: 255 if p > 127 else 0).convert("1")

    px = img.load()
    byte_width = (w + 7) // 8
    bytes_ = []
    for y in range(h):
        byte = 0
        for x in range(w):
            bit = 1 if px[x, y] == 0 else 0  # preto = 1, branco = 0
            byte = (byte << 1) | bit
            if x % 8 == 7:
                bytes_.append(byte)
                byte = 0
        if w % 8 != 0:
            bytes_.append(byte << (8 - (w % 8)))
    return bytes_, byte_width

def fmt_array(name, bytes_, w, h, byte_width):
    lines = []
    lines.append(f"// ---------- {name} ({w}x{h}) ----------")
    lines.append(f"static const unsigned char {name}[] PROGMEM = {{")
    for i in range(0, len(bytes_), 8):
        chunk = bytes_[i:i + 8]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ("," if i + 8 < len(bytes_) else ""))
    lines.append("};")
    return "\n".join(lines)

def name_from_file(fname):
    base = os.path.splitext(os.path.basename(fname))[0].upper()
    return "".join(c if c.isalnum() else "_" for c in base)

def main():
    args = [a for a in sys.argv[1:]]
    path = None
    size = (48, 48)
    if args and args[0] == "--size":
        w, h = args[1].lower().split("x")
        size = (int(w), int(h))
        path = args[2]
    else:
        path = args[0]
        if len(args) >= 3:
            size = (int(args[1]), int(args[2]))

    if os.path.isdir(path):
        files = sorted(f for f in os.listdir(path)
                       if f.lower().endswith((".png", ".jpg", ".jpeg")))
        if not files:
            print(f"Nenhuma imagem encontrada em {path}")
            sys.exit(1)
        blocks = []
        for f in files:
            fp = os.path.join(path, f)
            arr, bw = to_c_array(Image.open(fp), *size)
            blocks.append(fmt_array(name_from_file(f), arr, *size, bw))
        print("\n\n".join(blocks))
        print(f"\n// {len(files)} sprites gerados ({size[0]}x{size[1]}). "
              f"Atualize SPRITE_W={size[0]} e SPRITE_H={size[1]} no Sprites.h se mudou.")
    else:
        arr, bw = to_c_array(Image.open(path), *size)
        print(fmt_array(name_from_file(path), arr, *size, bw))

if __name__ == "__main__":
    main()
