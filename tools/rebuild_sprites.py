#!/usr/bin/env python3
"""Reconstroi os sprites em src/sprites/*.h e src/Sprites.h:
- Converte tools/sprites/*.png para arrays C (bit=1=preto, MSB first)
- Gera 1 arquivo por estagio: src/sprites/<base>.h (arrays + dimensoes)
- Gera src/Sprites.h que inclui todos + aliases HAPPY/SAD + defines ICON

Também gera as variantes 4-gray (2 bits/pixel) em src/sprites_gray/*.h
e src/SpritesGray.h (dithering Floyd-Steinberg; 0b11=branco, 0b10=cinza
claro, 0b01=cinza escuro, 0b00=preto).

Uso:
  python3 rebuild_sprites.py
"""
import sys
import os
import re
from PIL import Image, ImageFilter

# Estagios: (base do arquivo em tools/sprites, prefixo C, tamanho)
STAGES = [
    ("egg",         "EGG",       (48, 48)),
    ("scyther",     "SCYTHER",   (160, 160)),
    ("scizor",      "SCIZOR",    (160, 160)),
    ("kleavor",     "KLEAVOR",   (160, 160)),
    ("megascizor",  "MEGASCIZOR", (176, 176)),
    ("pichu",       "PICHU",     (96, 96)),
    ("pikachu",     "PIKACHU",   (144, 144)),
    ("raichu",      "RAICHU",    (160, 160)),
    ("megaraichux", "MEGARAICHUX", (176, 176)),
    ("megaraichuy", "MEGARAICHUY", (176, 176)),
]

# Recorte automatico do conteudo (foto com muita margem vazia):
# preenche a moldura - 1.0 = sem recorte, 0.92 = conteudo ocupa 92%.
CROP = {"scizor": 0.92}

# Sprites com arte em paisagem: centralizar na moldura deixa a imagem
# "flutuando" acima da base (pe da sprite nao encosta no chao). Ancorar
# na BASE mantem o pe alinhado com as demais sprites da linha.
BOTTOM_ALIGN = {"pikachu", "raichu", "megaraichux", "megaraichuy"}

# Limiar adaptativo (Sauvola): sombra/gradiente suave vira branco,
# contornos e detalhes nítidos ficam preto. r = janela, k = sensibilidade.
SAUVOLA_R = 15
SAUVOLA_K = 0.2

def integral_sum(img):
    """Soma e soma de quadrados via imagem integral."""
    w, h = img.size
    px = img.load()
    S = [[0] * (w + 1) for _ in range(h + 1)]
    S2 = [[0] * (w + 1) for _ in range(h + 1)]
    for y in range(h):
        rs = rs2 = 0
        for x in range(w):
            v = px[x, y]
            rs += v
            rs2 += v * v
            S[y + 1][x + 1] = S[y][x + 1] + rs
            S2[y + 1][x + 1] = S2[y][x + 1] + rs2
    return S, S2

def sauvola(img, r=SAUVOLA_R, k=SAUVOLA_K):
    """Binarizacao adaptativa: sombras (gradientes suaves) viram branco,
    contornos nítidos ficam preto."""
    w, h = img.size
    S, S2 = integral_sum(img)
    px = img.load()
    out = Image.new("1", img.size, 1)
    opx = out.load()
    half = r // 2
    for y in range(h):
        for x in range(w):
            x0, x1 = max(0, x - half), min(w, x + half + 1)
            y0, y1 = max(0, y - half), min(h, y + half + 1)
            n = (x1 - x0) * (y1 - y0)
            s = S[y1][x1] - S[y0][x1] - S[y1][x0] + S[y0][x0]
            s2 = S2[y1][x1] - S2[y0][x1] - S2[y1][x0] + S2[y0][x0]
            mean = s / n
            std = (s2 / n - mean * mean) ** 0.5
            th = mean * (1 + k * (std / 128 - 1))
            opx[x, y] = 0 if px[x, y] <= th else 1
    return out

def color_binarize(img, dark_lum=100, green_min=110, green_delta=25):
    """Binarizacao por cor para o ovo: preto se escuro (contorno) ou
    verde-dominante (pintas). O resto fica branco."""
    if img.mode not in ("RGB", "RGBA"):
        img = img.convert("RGB")
    rgb = img.convert("RGB")
    lum = img.convert("L")
    w, h = img.size
    px = rgb.load()
    pl = lum.load()
    out = Image.new("1", img.size, 1)
    opx = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            is_dark = pl[x, y] < dark_lum
            is_green = g > r + green_delta and g > b + green_delta and g >= green_min
            if is_dark or is_green:
                opx[x, y] = 0
    return out

def to_c_array(img, w, h, color_mode=False):
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img)
    if color_mode:
        # Binarizacao por cor: precisa da imagem RGB, antes de virar cinza
        rgb = img.convert("RGB")
        rgb.thumbnail((w, h), Image.LANCZOS)
        canvas = Image.new("RGB", (w, h), (255, 255, 255))
        canvas.paste(rgb, ((w - rgb.width) // 2, (h - rgb.height) // 2))
        img = color_binarize(canvas)
    else:
        img = img.convert("L")
        img.thumbnail((w, h), Image.LANCZOS)
        canvas = Image.new("L", (w, h), 255)
        canvas.paste(img, ((w - img.width) // 2, (h - img.height) // 2))
        img = sauvola(canvas)
    px = img.load()
    out = []
    for y in range(h):
        byte = 0
        for x in range(w):
            bit = 1 if px[x, y] == 0 else 0
            byte = (byte << 1) | bit
            if x % 8 == 7:
                out.append(byte)
                byte = 0
        if w % 8 != 0:
            out.append(byte << (8 - (w % 8)))
    return out

def fmt_array(name, bytes_, w, h):
    lines = [f"static const unsigned char {name}[] PROGMEM = {{"]
    for i in range(0, len(bytes_), 8):
        chunk = bytes_[i:i + 8]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ("," if i + 8 < len(bytes_) else ""))
    lines.append("};")
    return "\n".join(lines)

# ============================================================
# Variantes 4-gray (2 bits/pixel)
# ============================================================

# Modo de conversao (versao final): silhueta Sauvola + cinza nas sombras.
# ("lum", wthr, gthr) = pipeline validada na linha Pichu: luminancia no
#   alvo, Sauvola, limiares (verdes/amarelos ficam bons).
# ("color", lo, hi, wthr, gthr) = sprites escuros (vermelho/marrom):
#   mascara Sauvola+guard na resolucao original (contorno/detalhe nitido,
#   OR-downsample) e tons = luminancia do alvo mapeada de [lo,hi] para
#   [0,255] com janela FIXA - sprites da mesma cor usam a mesma janela
#   (mega scizor replica o scizor aprovado).
# ("semantic", black_max, white_min, wthr, gthr, detail_thr, band_edge) =
#   MAPEAMENTO SEMANTICO: escuro na cor (marrom escuro, preto, vermelho
#   escuro) -> PRETO (L < black_max, na resolucao original, detalhes finos
#   preservados); claro na cor (bege, claro) -> BRANCO (L >= white_min);
#   meio -> CINZA. Detalhe por contraste local (detail_thr: menor = mais
#   detalhe). band_edge=1: linha preta de 1px onde a faixa de tom muda
#   (contornos entre sombreamento bem definidos, estilo cel-shading).
#   band_head (8o param, opcional): linhas de banda nao saem acima dessa
#   linha (ex.: rosto do megascizor vira borrao com band_edge completo).
#   band_zone (9o param, opcional: (x0,x1,y0,y1)): zona retangular
#   adicional sem linhas de banda (ex.: pescoco do megascizor).
GRAY_MODES = {
    "scyther": ("lum", 180, 110),
    "scizor": ("color", 11, 138, 180, 110),
    "kleavor": ("semantic", 60, 160, 180, 100, 12, 0),
    "megascizor": ("semantic", 70, 120, 180, 110, 22, 1, 36, (60, 120, 34, 46)),
    "pichu": ("lum", 180, 110),
    "pikachu": ("lum", 180, 110),
    "raichu": ("lum", 230, 195),
    "megaraichux": ("lum", 180, 110),
    "megaraichuy": ("lum", 180, 110),
}
DEFAULT_GRAY_MODE = ("lum", 200, 120)

def to_c_array_2bpp(img, w, h, mode=None, bottom_align=False):
    """Imagem -> array 2bpp (4 pixels/byte, MSB first).
    Valores: 0b11=branco, 0b10=cinza claro, 0b01=cinza escuro, 0b00=preto.
    Versao final: base = silhueta Sauvola (1bpp, ja validada). Onde a
    silhueta e preta vira preto puro (contorno/detalhe nitido); onde e
    branca, o fundo (>= wthr) fica branco limpo e sombras viram cinza.
    mode = ("lum"|"color", wthr, gthr); padrao GRAY_MODES/DEFAULT."""
    if mode is None:
        mode = DEFAULT_GRAY_MODE
    mode_name = mode[0]
    if mode_name == "color":
        _, lo, hi, wthr, gthr = mode
    elif mode_name == "semantic":
        _, black_max, white_min, wthr, gthr, detail_thr, band_edge = mode[:7]
        band_head = mode[7] if len(mode) > 7 else 0
        band_zone = mode[8] if len(mode) > 8 else None
    else:
        _, wthr, gthr = mode
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img)
    rgb = img.convert("RGB")

    if mode_name == "semantic":
        # MAPEAMENTO SEMANTICO na resolucao ORIGINAL: o que e escuro na
        # cor fica escuro (preto), o que e claro fica claro (branco) e o
        # meio vira cinza. Preto = L < black_max (marrom escuro, preto,
        # vermelho escuro) - absoluto, preserva detalhes finos; branco =
        # L >= white_min (bege, claro); meio mapeado para cinza.
        sw, sh = rgb.size
        lum = rgb.convert("L")
        lum_px = lum.load()
        mean = lum.filter(ImageFilter.BoxBlur(1))
        mean_px = mean.load()
        mask = Image.new("1", (sw, sh), 1)
        mpx = mask.load()
        for y in range(sh):
            for x in range(sw):
                if lum_px[x, y] >= 245:
                    continue
                if ((x == 0 or x == sw - 1 or y == 0 or y == sh - 1) or
                    (x > 0 and lum_px[x - 1, y] >= 245) or
                    (x < sw - 1 and lum_px[x + 1, y] >= 245) or
                    (y > 0 and lum_px[x, y - 1] >= 245) or
                    (y < sh - 1 and lum_px[x, y + 1] >= 245)):
                    # Contorno da silhueta: borda do sprite (vizinho do
                    # fundo branco) vira preto puro - o contorno nao some
                    # onde o traco e fino/claro.
                    mpx[x, y] = 0
                elif lum_px[x, y] < black_max:
                    mpx[x, y] = 0
                elif lum_px[x, y] < mean_px[x, y] - detail_thr:
                    # Detalhe por contraste local: tons claros do corpo
                    # (ex.: marrom claro no bege) que o painel nao
                    # diferencia de branco viram linha preta de detalhe.
                    mpx[x, y] = 0
        mask_t = Image.new("1", (w, h), 1)
        mt = mask_t.load()
        for ty in range(h):
            sy0 = ty * sh // h
            sy1 = max(sy0 + 1, ((ty + 1) * sh + h - 1) // h)
            for tx in range(w):
                sx0 = tx * sw // w
                sx1 = max(sx0 + 1, ((tx + 1) * sw + w - 1) // w)
                dark = False
                for sy in range(sy0, sy1):
                    for sx in range(sx0, sx1):
                        if mpx[sx, sy] == 0:
                            dark = True
                            break
                    if dark:
                        break
                if dark:
                    mt[tx, ty] = 0
        lum_t = lum.resize((w, h), Image.LANCZOS)
        pt = lum_t.load()
        span = max(1, white_min - black_max)
        for y in range(h):
            for x in range(w):
                v = pt[x, y]
                nv = int((v - black_max) * 255 / span)
                pt[x, y] = max(0, min(255, nv))
        if band_edge:
            # Contorno entre sombreamento: linha preta de 1px onde a faixa
            # de tom muda. Linhas acima de band_head (zona da cabeca/rosto)
            # nao saem - la o sombreamento e sutil demais e vira borrao.
            bands = [[0] * w for _ in range(h)]
            for y in range(h):
                for x in range(w):
                    if mt[x, y] == 0:
                        bands[y][x] = 0
                    else:
                        v = pt[x, y]
                        bands[y][x] = 3 if v >= wthr else 2 if v >= gthr else 1
            for y in range(h):
                for x in range(w):
                    b = bands[y][x]
                    if b == 0:
                        continue
                    if y < band_head:
                        continue
                    if band_zone is not None:
                        zx0, zx1, zy0, zy1 = band_zone
                        if zx0 <= x <= zx1 and zy0 <= y <= zy1:
                            continue
                    if (x > 0 and bands[y][x - 1] != b and bands[y][x - 1] != 0) or \
                       (x < w - 1 and bands[y][x + 1] != b and bands[y][x + 1] != 0) or \
                       (y > 0 and bands[y - 1][x] != b and bands[y - 1][x] != 0) or \
                       (y < h - 1 and bands[y + 1][x] != b and bands[y + 1][x] != 0):
                        mt[x, y] = 0
    elif mode_name == "color":
        # Mascara na RESOLUCAO ORIGINAL (ate 2x o alvo): o contorno e os
        # detalhes tem separacao nitida em 600px, mas no downscale o
        # antialiasing mistura contorno/sombra/corpo e nenhum limiar
        # separa mais. Sauvola + guard (preto so se L < 65).
        cap = max(w, h) * 2
        s = min(w / rgb.width, h / rgb.height, 1.0)
        cc_w = max(1, round(rgb.width * s * cap / max(w, h)))
        cc_h = max(1, round(rgb.height * s * cap / max(w, h)))
        rgb2 = rgb.resize((cc_w, cc_h), Image.LANCZOS)
        canvas = Image.new("RGB", (cap, cap), (255, 255, 255))
        canvas.paste(rgb2, ((cap - cc_w) // 2, (cap - cc_h) // 2))
        lum = canvas.convert("L")
        lum_px = lum.load()
        mask = sauvola(lum)
        m = mask.load()
        for y in range(cap):
            for x in range(cap):
                if m[x, y] == 0 and lum_px[x, y] >= 65:
                    m[x, y] = 1
        # OR-downsample: pixel alvo preto se QUALQUER pixel no bloco era
        # preto (preserva linhas de 1px do contorno/detalhe).
        mask_t = Image.new("1", (w, h), 1)
        mt = mask_t.load()
        for ty in range(h):
            sy0 = ty * cap // h
            sy1 = max(sy0 + 1, ((ty + 1) * cap + h - 1) // h)
            for tx in range(w):
                sx0 = tx * cap // w
                sx1 = max(sx0 + 1, ((tx + 1) * cap + w - 1) // w)
                dark = False
                for sy in range(sy0, sy1):
                    for sx in range(sx0, sx1):
                        if m[sx, sy] == 0:
                            dark = True
                            break
                    if dark:
                        break
                if dark:
                    mt[tx, ty] = 0
        # Tons: luminancia do alvo mapeada de [lo,hi] para [0,255] com
        # janela fixa (mesma cor = mesma janela = mesmo look no corpo).
        lum_t = lum.resize((w, h), Image.LANCZOS)
        pt = lum_t.load()
        for y in range(h):
            for x in range(w):
                v = pt[x, y]
                if v < 250:
                    nv = int((v - lo) * 255 / (hi - lo))
                    pt[x, y] = max(0, min(255, nv))
    else:
        rgb.thumbnail((w, h), Image.LANCZOS)
        canvas = Image.new("RGB", (w, h), (255, 255, 255))
        if bottom_align:
            # Pe da arte encostado na base da moldura (consistencia entre
            # sprites com arte em paisagem e em retrato).
            canvas.paste(rgb, ((w - rgb.width) // 2, h - rgb.height))
        else:
            canvas.paste(rgb, ((w - rgb.width) // 2, (h - rgb.height) // 2))
        lum = canvas.convert("L")
        mask = sauvola(lum)
        m = mask.load()
        mt = m
        pt = lum.load()

    out = []
    for y in range(h):
        byte = 0
        for x in range(w):
            if mt[x, y] == 0:
                v = 0
            else:
                old = pt[x, y]
                v = 3 if old >= wthr else 2 if old >= gthr else 1
            byte = (byte << 2) | v
            if x % 4 == 3:
                out.append(byte)
                byte = 0
        if w % 4 != 0:
            out.append(byte << (8 - 2 * (w % 4)))
    return out

def crop_content(img, fill):
    """Recorta as margens vazias da imagem e deixa o conteudo ocupando
    `fill` da moldura (0 < fill <= 1). fill=1.0 nao altera nada."""
    if fill >= 1.0:
        return img
    img = img.convert("RGBA")
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    c = Image.alpha_composite(bg, img).convert("L")
    px = c.load()
    w, h = c.size
    xs = [x for x in range(w) for y in range(h) if px[x, y] < 245]
    ys = [y for x in range(w) for y in range(h) if px[x, y] < 245]
    if not xs:
        return img
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    pad_w = bw * (1 - fill) / (2 * fill)
    pad_h = bh * (1 - fill) / (2 * fill)
    box = (max(0, int(x0 - pad_w)), max(0, int(y0 - pad_h)),
           min(w, int(x1 + 1 + pad_w)), min(h, int(y1 + 1 + pad_h)))
    return img.crop(box)

def parse_array(text, name):
    m = re.search(r"static const unsigned char %s\[\] PROGMEM = \{(.*?)\};" % name, text, re.S)
    if not m:
        raise SystemExit(f"Array {name} nao encontrado em Sprites.h")
    hexes = re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))
    return [int(h, 16) for h in hexes]

def array_to_img(bytes_, w, h):
    img = Image.new("1", (w, h), 1)
    px = img.load()
    byte_width = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            byte = bytes_[y * byte_width + x // 8]
            bit = (byte >> (7 - (x % 8))) & 1
            px[x, y] = 1 - bit
    return img

def stage_header(prefix, size):
    return (f"// {prefix} ({size[0]}x{size[1]})\n"
            f"#define {prefix}_SPRITE_W {size[0]}\n"
            f"#define {prefix}_SPRITE_H {size[1]}\n")

def main():
    src = "tools/sprites"
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sprites_dir = os.path.join(script_dir, "..", "src", "sprites")
    gray_dir = os.path.join(script_dir, "..", "src", "sprites_gray")
    os.makedirs(sprites_dir, exist_ok=True)
    os.makedirs(gray_dir, exist_ok=True)
    sprites_h = os.path.join(script_dir, "..", "src", "Sprites.h")
    sprites_gray_h = os.path.join(script_dir, "..", "src", "SpritesGray.h")

    # Imagens disponiveis (nome base -> caminho)
    pngs = {}
    for f in os.listdir(src):
        if f.lower().endswith((".png", ".jpg", ".jpeg")):
            pngs[os.path.splitext(f)[0].lower()] = os.path.join(src, f)

    # Le Sprites.h antigo como fallback (mantem sprites se imagem faltar)
    old = ""
    if os.path.exists(sprites_h):
        with open(sprites_h, "r") as f:
            old = f.read()

    # 1) Ovo: 4 variacoes a partir da mesma imagem
    egg_variants = ["EGG_IDLE", "EGG_WARM", "EGG_COLD", "EGG_HATCHING"]
    egg_arrays = []
    egg_path = pngs.get("egg")
    if egg_path:
        egg_img = Image.open(egg_path)
        for name in egg_variants:
            egg_arrays.append((name, to_c_array(egg_img, *STAGES[0][2], color_mode=True)))
    else:
        for name in egg_variants:
            arr = parse_array(old, name)
            img = array_to_img(arr, 32, 32)
            egg_arrays.append((name, to_c_array(img, *STAGES[0][2], color_mode=False)))
        print("[aviso] sem imagem do ovo - mantidos sprites antigos (32x32)")

    # 2) Demais estagios: 1 array por estagio (sauvola)
    arrays = {STAGES[0][0]: egg_arrays}
    aliases = []
    for base, prefix, size in STAGES[1:]:
        if base in pngs:
            img = Image.open(pngs[base])
            img = crop_content(img, CROP.get(base, 1.0))
            arr = to_c_array(img, *size)
            arrays[base] = [(f"{prefix}_IDLE", arr)]
            aliases.append(f"#define {prefix}_HAPPY {prefix}_IDLE")
            aliases.append(f"#define {prefix}_SAD   {prefix}_IDLE")
        elif old:
            fname = f"{prefix}_IDLE"
            arr = parse_array(old, fname)
            img = array_to_img(arr, 32, 32)
            arrays[base] = [(f"{prefix}_IDLE", to_c_array(img, *size))]
            aliases.append(f"#define {prefix}_HAPPY {prefix}_IDLE")
            aliases.append(f"#define {prefix}_SAD   {prefix}_IDLE")
            print(f"[aviso] sem imagem para {base} - mantido sprite antigo (32x32)")
        else:
            print(f"[aviso] sem imagem para {base} - estagio ignorado")

    # 3) Grava um arquivo por estagio
    includes = []
    gray_includes = []
    for base, prefix, size in STAGES:
        if base not in arrays:
            continue
        block = "// Gerado por tools/rebuild_sprites.py\n"
        block += stage_header(prefix, size)
        for name, arr in arrays[base]:
            block += "\n" + fmt_array(name, arr, *size) + "\n"
        fname = os.path.join(sprites_dir, f"{base}.h")
        with open(fname, "w") as f:
            f.write(block)
        includes.append(f'#include "sprites/{base}.h"')

        # Variante 4-gray (2bpp): mesmo resize/recorte, silhueta + sombras
        gray_block = "// Gerado por tools/rebuild_sprites.py (variante 4-gray, 2 bits/pixel)\n"
        gray_block += (f"#define {prefix}_GRAY_W {size[0]}\n"
                       f"#define {prefix}_GRAY_H {size[1]}\n")
        if base in pngs:
            img = Image.open(pngs[base])
            img = crop_content(img, CROP.get(base, 1.0))
            mode = GRAY_MODES.get(base, DEFAULT_GRAY_MODE)
            for name, arr in arrays[base]:
                gname = name.replace(f"{prefix}_", f"{prefix}_GRAY_")
                gray_block += "\n" + fmt_array(gname, to_c_array_2bpp(img, *size, mode,
                                              base in BOTTOM_ALIGN), *size) + "\n"
        elif old:
            fname_old = f"{prefix}_IDLE"
            arr = parse_array(old, fname_old)
            img = array_to_img(arr, 32, 32)
            gray_block += "\n" + fmt_array(f"{prefix}_GRAY_IDLE", to_c_array_2bpp(img, *size), *size) + "\n"
            for name, _ in arrays[base]:
                if name != f"{prefix}_IDLE":
                    gray_block += f"\n#define {name.replace(f'{prefix}_', f'{prefix}_GRAY_')} {prefix}_GRAY_IDLE\n"
        gfname = os.path.join(gray_dir, f"{base}.h")
        with open(gfname, "w") as f:
            f.write(gray_block)
        gray_includes.append(f'#include "sprites_gray/{base}.h"')

    # 4) Sprites.h: guard + includes + aliases + icones
    header = f"""#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

// ============================================================
// Sprites por estagio (gerado por tools/rebuild_sprites.py).
// Cada arquivo define as dimensoes (PREFIX_SPRITE_W/H) e o(s)
// array(s) do estagio. Convencao: 1 byte = 8 pixels, MSB first,
// bit=1 = preto, bit=0 = branco.
// ============================================================
{chr(10).join(includes)}

// ============================================================
// Aliases (variacoes de humor usam o mesmo sprite)
// ============================================================
{chr(10).join(aliases)}

// ============================================================
// Icon dimensions
// ============================================================
#define ICON_W  16
#define ICON_H  16
#define ICON_BYTES_PER_ROW  2

#endif
"""
    with open(sprites_h, "w") as f:
        f.write(header)

    # 5) SpritesGray.h: variantes 4-gray
    gray_aliases = []
    for base, prefix, size in STAGES[1:]:
        gray_aliases.append(f"#define {prefix}_GRAY_HAPPY {prefix}_GRAY_IDLE")
        gray_aliases.append(f"#define {prefix}_GRAY_SAD   {prefix}_GRAY_IDLE")
    gray_header = f"""#ifndef SPRITES_GRAY_H
#define SPRITES_GRAY_H

#include <Arduino.h>

// ============================================================
// Sprites 4-gray por estagio (gerado por tools/rebuild_sprites.py).
// Formato: 2 bits/pixel, 4 pixels/byte, MSB first.
//   0b11 = branco | 0b10 = cinza claro | 0b01 = cinza escuro | 0b00 = preto
// ============================================================
{chr(10).join(gray_includes)}

// ============================================================
// Aliases (variacoes de humor usam o mesmo sprite)
// ============================================================
{chr(10).join(gray_aliases)}

#endif
"""
    with open(sprites_gray_h, "w") as f:
        f.write(gray_header)

    sizes = {f"{base}({p})": size for base, p, size in STAGES}
    print(f"Sprites.h atualizado. Arquivos: {[os.path.basename(i) for i in includes]}")
    print(f"SpritesGray.h atualizado. Arquivos: {[os.path.basename(i) for i in gray_includes]}")
    print(f"Sizes: {sizes}")

if __name__ == "__main__":
    main()
