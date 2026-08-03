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
import math
from collections import Counter, deque
from PIL import Image, ImageFilter

# Estagios: (base do arquivo em tools/sprites, prefixo C, tamanho)
# Molduras nao quadradas preservam a proporcao da arte (ex.: magmar/
# magmortar/onix sao paisagem). Tamanhos crescem com a linha (baby < S1 <
# S2 < final).
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
    # Linha Elekid (com baby): 96 -> 144 -> 160
    ("elekid",      "ELEKID",     (96, 79)),
    ("electabuzz",  "ELECTABUZZ", (144, 146)),
    ("electivire",  "ELECTIVIRE", (160, 154)),
    # Linha Magby (com baby): 96 -> 144 -> 160
    ("magby",       "MAGBY",      (96, 126)),
    ("magmar",      "MAGMAR",     (144, 94)),
    ("magmortar",   "MAGMORTAR",  (160, 118)),
    # Linha Rhyhorn (sem baby): 160 -> 160 -> 176
    ("rhyhorn",     "RHYHORN",    (160, 130)),
    ("rhydon",      "RHYDON",     (160, 139)),
    ("rhyperior",   "RHYPERIOR",  (176, 152)),
    # Linha Onix (sem baby + Mega): 160 -> 160 -> 176
    ("onix",        "ONIX",       (160, 137)),
    ("steelix",     "STEELIX",    (160, 160)),
    ("megasteelix", "MEGASTEELIX", (176, 167)),
    # Linha Tangela (sem baby, 2 estagios): 160 -> 160
    ("tangela",     "TANGELA",    (160, 152)),
    ("tangrowth",   "TANGROWTH",  (160, 148)),
]

# Recorte automatico do conteudo (foto com muita margem vazia):
# preenche a moldura - 1.0 = sem recorte, 0.92 = conteudo ocupa 92%.
CROP = {"scizor": 0.92}

# Altura do CORPO em pixels na tela final (nao a moldura!) por estagio.
# Referencia aprovada: baby 96 | S1 144 | sem-baby/final 160 | mega 176.
BODY_H = {
    "elekid": 96, "electabuzz": 144, "electivire": 160,
    "magby": 96, "magmar": 144, "magmortar": 160,
    "rhyhorn": 160, "rhydon": 160, "rhyperior": 176,
    "onix": 160, "steelix": 160, "megasteelix": 176,
    "tangela": 160, "tangrowth": 160,
}

# Sprites com arte em paisagem: centralizar na moldura deixa a imagem
# "flutuando" acima da base (pe da sprite nao encosta no chao). Ancorar
# na BASE mantem o pe alinhado com as demais sprites da linha.
BOTTOM_ALIGN = {"pikachu", "raichu", "megaraichux", "megaraichuy"}

# Limiar adaptativo (Sauvola): sombra/gradiente suave vira branco,
# contornos e detalhes nítidos ficam preto. r = janela, k = sensibilidade.
SAUVOLA_R = 15
SAUVOLA_K = 0.2

# Icones avulsos (nao sao estagios): (base do arquivo em tools/sprites,
# prefixo C, tamanho, opcoes). Opcoes: thr = limiar global (L < thr vira
# preto; padrao 127), crop = recorta o conteudo denso (L < 150) antes de
# redimensionar, para o desenho preencher a moldura. Gera
# sprites/<base>.h (1bpp) e sprites_gray/<base>.h (2bpp) com os arrays
# <PREFIXO> e <PREFIXO>_GRAY.
ICONS = [
    ("coco", "COCO", (24, 24), {"thr": 170, "crop": True}),
]

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

def semantic_mask(canvas, lum, w, h, dark_lum=65, blue_d=20, feet_r=140,
                  mask_lum=None):
    """Silhueta semantica: corpo azul (b > r+blue_d), pes vermelhos
    (r > feet_r e VERMELHO SATURADO - senao o fundo branco cai dentro),
    ou pixel escuro (lum < mask_lum, padrao dark_lum).
    Imagem "1": 0 = pokemon, 1 = fundo. Base do 1bpp e do 4-gray de
    tangela/tangrowth. No 4-gray o mask_lum maior (95) inclui as sombras
    NEUTRAS (ex.: tangrowth RGB 74,74,82, L~75) que nao sao azuis nem
    vermelhas nem escuras o bastante - senao viram branco no display."""
    if mask_lum is None:
        mask_lum = dark_lum
    px = canvas.load()
    lx = lum.load()
    out = Image.new("1", (w, h), 1)
    op = out.load()
    for y in range(h):
        for x in range(w):
            rr, gg, bb = px[x, y][:3]
            red_sat = rr > feet_r and rr > gg + 40 and rr > bb + 40
            if (bb > rr + blue_d) or red_sat or lx[x, y] < mask_lum:
                op[x, y] = 0
    return out

def pokemon_1bpp(img, w, h, dark_lum=65, edge_thr=120, blue_d=20, feet_r=140):
    """1bpp por COR + bordas para artes escuras de corpo colorido
    (tangela/tangrowth): o pokemon e separado do fundo pela SATURACAO/cor
    (corpo azul: b > r+blue_d; pes vermelhos: r > feet_r), depois o bitmap
    final = so contornos (FIND_EDGES) + pixels realmente escuros
    (lum < dark_lum) em preto; corpo/fundo em branco. Estilo GameBoy.
    Nao usa Sauvola."""
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        img = Image.alpha_composite(bg, img)
    rgb = img.convert("RGB")
    lum = img.convert("L")
    lx = lum.load()
    W, H = w, h

    # Silhueta do pokemon: pela cor (corpo azul, pes vermelhos) OU pixel
    # escuro (contorno azul/preto). Sem isso, o contorno escuro vira um
    # "buraco" dentro da silhueta e o anel sai duplo.
    sm = semantic_mask(rgb, lum, W, H, dark_lum, blue_d, feet_r).load()
    sil = [[sm[x, y] == 0 for x in range(W)] for y in range(H)]

    # Contorno externo: silhueta vizinha(4) ao que NAO e pokemon.
    ring = [[False] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if not sil[y][x]:
                continue
            if (x > 0 and not sil[y][x - 1]) or \
               (x < W - 1 and not sil[y][x + 1]) or \
               (y > 0 and not sil[y - 1][x]) or \
               (y < H - 1 and not sil[y + 1][x]):
                ring[y][x] = True

    edges = lum.filter(ImageFilter.FIND_EDGES)
    ex = edges.load()

    out = Image.new("1", (W, H), 1)
    opx = out.load()
    for y in range(H):
        for x in range(W):
            v = lx[x, y]
            if v < dark_lum:
                opx[x, y] = 0
            elif sil[y][x] and (ring[y][x] or ex[x, y] > edge_thr):
                opx[x, y] = 0
            else:
                opx[x, y] = 1
    return out

def gray_array_to_img(bytes_, w, h):
    """2bpp -> imagem RGB (0=preto, 1=cinza escuro, 2=cinza claro, 3=br.)"""
    lut = [(0, 0, 0), (85, 85, 85), (170, 170, 170), (255, 255, 255)]
    img = Image.new("RGB", (w, h))
    px = img.load()
    byte_width = (w + 3) // 4
    for y in range(h):
        for x in range(w):
            byte = bytes_[y * byte_width + x // 4]
            v = (byte >> (6 - 2 * (x % 4))) & 3
            px[x, y] = lut[v]
    return img

def overlay_2bpp(arr, img1, w, h):
    """Forca v0 (preto) no array 2bpp onde a imagem 1bpp esta em preto:
    o traco (contorno/bordas/sombreamento) do 1bpp por cima do tom
    4-gray - estilo line-art. Fundo (1bpp branco) intacto."""
    px = img1.load()
    bw = w // 4
    for y in range(h):
        for x in range(w):
            if px[x, y] == 0:
                i = y * bw + x // 4
                arr[i] &= ~(3 << (6 - 2 * (x % 4)))
    return arr

def write_preview(path, img, scale=4):
    img = img.convert("RGB").resize((img.width * scale, img.height * scale),
                                    Image.NEAREST)
    img.save(path)

def to_c_array(img, w, h, color_mode=False):
    if img.mode == "1":
        # Mascara ja binarizada (ex.: pokemon_1bpp) - sem Sauvola.
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
    # Novos: limiares "lum" derivados da distribuicao de luminancia do
    # corpo (p90 ~ wthr, p50 ~ gthr). Ajuste visual se algum estagio ficar
    # chapado ou borrado - mesmo processo dos originais.
    "elekid": ("lum", 210, 150, True),
    "electabuzz": ("lum", 225, 160, True),
    "electivire": ("lum", 245, 210, True),
    "magby": ("lum", 210, 130, True),
    "magmar": ("lum", 245, 200, True),
    "magmortar": ("lum", 245, 205, True),
    "rhyhorn": ("lum", 200, 140, True),
    "rhydon": ("lum", 215, 140, True),
    "rhyperior": ("lum", 180, 110),
    "onix": ("lum", 240, 140, True),
    "steelix": ("lum", 230, 140, True),
    "megasteelix": ("lum", 230, 140, True),
    "tangela": ("tone", 200, 150, 55, True, True, True, 1, 2, True),
    "tangrowth": ("tone", 200, 145, 55, True, True, True, 1, 2, True),
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
    clean_bg = False  # forca fundo (fora da silhueta) branco puro
    if mode_name == "color":
        _, lo, hi, wthr, gthr = mode
    elif mode_name == "semantic":
        _, black_max, white_min, wthr, gthr, detail_thr, band_edge = mode[:7]
        band_head = mode[7] if len(mode) > 7 else 0
        band_zone = mode[8] if len(mode) > 8 else None
    elif mode_name == "tone":
        # ("tone", wthr, gthr, blk, [clean_bg], [outline], [semantic], [boost])
        # - silhueta so delimita corpo/fundo; o corpo mapeia por LUMINANCIA
        # (azul/verde escuro -> cinza, so o < blk vira preto).
        # outline=True pinta de preto o contorno (1px) da silhueta;
        # semantic=True usa a silhueta por COR (corpo azul/vermelho/escuro)
        # em vez da Sauvola - sem ela o corpo azul cai fora e vira branco.
        # boost (0-2) clareia os tons da silhueta em N niveis: o painel
        # 4-gray (SSD1681) rende v1/v2 mais escuros que o preview linear,
        # entao boost=1 (v1->v2, v2->v3) compensa. Anel/fundo intactos.
        # boost_cap limita o nivel final: (boost=1, cap=2) so levanta o
        # extremo escuro (v0->v1, v1->v2) e nao deixa o corpo virar branco.
        wthr, gthr, blk = mode[1], mode[2], mode[3]
        clean_bg = mode[4] if len(mode) > 4 else True
        outline = mode[5] if len(mode) > 5 else False
        semantic = mode[6] if len(mode) > 6 else False
        boost = mode[7] if len(mode) > 7 else 0
        boost_cap = mode[8] if len(mode) > 8 else 3
        overlay = mode[9] if len(mode) > 9 else False
    else:
        wthr, gthr = mode[1], mode[2]
        clean_bg = mode[3] if len(mode) > 3 else False
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
        if mode_name == "tone" and semantic:
            # mask_lum 95: sombras neutras (L 65-95, nao azuis/vermelhas)
            # entram na silhueta e viram cinza escuro em vez de branco.
            mt = semantic_mask(canvas, lum, w, h, mask_lum=95).load()
        else:
            mask = sauvola(lum)
            mt = mask.load()
        if mode_name == "tone" and outline:
            # contorno 1px preto: silhueta vizinha(4) ao que NAO e pokemon.
            ring = Image.new("L", (w, h), 255)
            rx = ring.load()
            if semantic:
                # anel INTERNO (em cima da silhueta): pega a borda do corpo
                # e cobre o anti-aliasing do resize (sem "sombra" opaca).
                for yy in range(h):
                    for xx in range(w):
                        if mt[xx, yy] != 0:
                            continue
                        if ((xx > 0 and mt[xx - 1, yy] != 0) or
                            (xx < w - 1 and mt[xx + 1, yy] != 0) or
                            (yy > 0 and mt[xx, yy - 1] != 0) or
                            (yy < h - 1 and mt[xx, yy + 1] != 0)):
                            rx[xx, yy] = 0
            else:
                # contorno externo (1px): flood a partir da borda da imagem
                # pelos pixels fora da silhueta; silhueta vizinha(4) = anel.
                outside = [[False] * w for _ in range(h)]
                stk = []
                for yy in range(h):
                    for xx in (0, w - 1):
                        if mt[xx, yy] != 0 and not outside[yy][xx]:
                            outside[yy][xx] = True
                            stk.append((xx, yy))
                for xx in range(w):
                    for yy in (0, h - 1):
                        if mt[xx, yy] != 0 and not outside[yy][xx]:
                            outside[yy][xx] = True
                            stk.append((xx, yy))
                while stk:
                    cx, cy = stk.pop()
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < w and 0 <= ny < h and not outside[ny][nx] \
                           and mt[nx, ny] != 0:
                            outside[ny][nx] = True
                            stk.append((nx, ny))
                for yy in range(h):
                    for xx in range(w):
                        if mt[xx, yy] == 0 and (
                            (xx > 0 and outside[yy][xx - 1]) or
                            (xx < w - 1 and outside[yy][xx + 1]) or
                            (yy > 0 and outside[yy - 1][xx]) or
                            (yy < h - 1 and outside[yy + 1][xx])):
                            rx[xx, yy] = 0
        pt = lum.load()
        if mode_name == "tone" and semantic:
            # Limiares ADAPTATIVOS por percentil da arte (so a silhueta):
            # distribui a arte nos 4 tons preservando a hierarquia original
            # (sombra < corpo < destaque). p15 -> v0/v1, p45 -> v1/v2,
            # p80 -> v2/v3; clamps garantem niveis nao-vazios.
            hist = [0] * 256
            for yy in range(h):
                for xx in range(w):
                    if mt[xx, yy] == 0:
                        hist[pt[xx, yy]] += 1
            total = sum(hist) or 1

            def pct(p):
                acc = 0
                for L in range(256):
                    acc += hist[L]
                    if acc >= total * p / 100:
                        return L
                return 255

            t0 = min(pct(15), 70)
            t1 = max(pct(45), t0 + 15)
            t2 = max(pct(80), t1 + 30)

    out = []
    for y in range(h):
        byte = 0
        for x in range(w):
            old = pt[x, y]
            if mode_name == "tone":
                if outline and rx[x, y] == 0:
                    v = 0
                elif mt[x, y] == 0 or not clean_bg:
                    if semantic:
                        v = 3 if old >= t2 else 2 if old >= t1 else (1 if old >= t0 else 0)
                        if boost:
                            v = min(boost_cap, v + boost)
                    else:
                        v = 3 if old >= wthr else 2 if old >= gthr else (1 if old >= blk else 0)
                else:
                    v = 3
            elif mt[x, y] == 0:
                v = 0
            elif clean_bg:
                v = 3
            else:
                v = 3 if old >= wthr else 2 if old >= gthr else 1
            byte = (byte << 2) | v
            if x % 4 == 3:
                out.append(byte)
                byte = 0
        if w % 4 != 0:
            out.append(byte << (8 - 2 * (w % 4)))
    return out

def icon_prepare(img, w, h, crop):
    """Compoe sobre branco, converte para L e (opcionalmente) recorta o
    conteudo denso (L < 150) para o desenho preencher a moldura."""
    img = img.convert("RGBA")
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, img).convert("L")
    if crop:
        px = img.load()
        cw, ch = img.size
        xs = [x for x in range(cw) for y in range(ch) if px[x, y] < 150]
        ys = [y for x in range(cw) for y in range(ch) if px[x, y] < 150]
        if xs:
            img = img.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))
    return img.resize((w, h), Image.LANCZOS)

def icon_to_c_array(img, w, h, thr=127, crop=False):
    """1bpp com limiar global (NAO sauvola). Para designs planos/escuros
    com fundo transparente (ex.: coco): escuro vira preto, o resto branco.
    bit=1 = preto, MSB first (padrao XBM). Remove componentes pretos com
    menos de 3 pixels (sobra de tracos finos apos o redimensionamento)."""
    img = icon_prepare(img, w, h, crop)
    bw, bh = img.size
    px = img.load()
    byte_width = (w + 7) // 8
    arr = bytearray(byte_width * h)
    for y in range(bh):
        for x in range(bw):
            if px[x, y] < thr:
                arr[y * byte_width + x // 8] |= 0x80 >> (x % 8)
    def is_black(x, y):
        return 0 <= x < bw and 0 <= y < bh and \
            (arr[y * byte_width + x // 8] & (0x80 >> (x % 8)))
    visited = [[False] * bw for _ in range(bh)]
    out = bytearray(arr)
    for y in range(bh):
        for x in range(bw):
            if not is_black(x, y) or visited[y][x]:
                continue
            stack = [(x, y)]
            visited[y][x] = True
            comp = []
            while stack:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for ny in (cy - 1, cy, cy + 1):
                    for nx in (cx - 1, cx, cx + 1):
                        if 0 <= nx < bw and 0 <= ny < bh and \
                           not visited[ny][nx] and is_black(nx, ny):
                            visited[ny][nx] = True
                            stack.append((nx, ny))
            if len(comp) < 3:
                for cx, cy in comp:
                    out[cy * byte_width + cx // 8] &= ~(0x80 >> (cx % 8))
    return bytes(out)

def icon_to_c_array_2bpp(img, w, h, thr=127, t1=90, crop=False):
    """2bpp com limiar global: 0b11=branco, 0b10=cinza claro, 0b01=cinza
    escuro, 0b00=preto. L >= thr -> branco; L < t1 -> preto; meio -> cinza
    escuro (para icones escuros o corpo vira cinza e o contorno preto)."""
    img = icon_prepare(img, w, h, crop)
    bw, bh = img.size
    px = img.load()
    byte_width = (w + 3) // 4
    arr = bytearray(byte_width * h)
    for y in range(bh):
        for x in range(bw):
            L = px[x, y]
            if L >= thr:
                v = 0b11
            elif L < t1:
                v = 0b00
            else:
                v = 0b01
            arr[y * byte_width + x // 4] |= v << (6 - 2 * (x % 4))
    return bytes(arr)

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

def fit_body(img, body_h, fill=0.97, max_w=232, resample=Image.LANCZOS):
    """Dimensiona pelo CORPO, nao pela moldura: recorta as margens e
    ajusta para o corpo do sprite ocupar ~`body_h` px de altura — comum a
    todos os estagios da mesma classe (o tamanho visual fica igual, ex.:
    todos os babies com corpo de ~96px). Preserva a proporcao da arte
    (molduras nao quadradas) e a largura respeita o limite da tela.
    resample: LANCZOS (padrao, fotografia) ou BOX (pixel art - reduz em
    passos de 2x para nao fundir contorno/corpo no downscale)."""
    img = crop_content(img, fill)          # margens removidas, corpo = fill da moldura
    w, h = img.size
    th = round(body_h / fill)              # altura da moldura final
    tw = round(th * w / h / 4) * 4         # largura pela proporcao (multipla de 4)
    if tw > max_w:                         # largura maxima na tela (240px)
        tw = max_w - (max_w % 4)
        th = round(tw * h / w)
    if resample == Image.BOX:
        # Reducao em passos de 2x (pixel art) ate 2x o alvo, depois BOX.
        cw, ch = w, h
        while cw > 2 * tw or ch > 2 * th:
            nw = max(tw, cw // 2)
            nh = max(th, ch // 2)
            img = img.resize((nw, nh), Image.BOX)
            cw, ch = nw, nh
    return img.resize((tw, th), resample)

def remove_bg(img, tol=25, sat=20, peel=True, peel_cap=0.10):
    """Remove o fundo conectado as bordas. Funciona com fundos de VARIOS
    tons (branco, cinza-claro ~237, preto e mesclas estilo 'rajado').
    1. Loop principal: recalcula a cor dominante da borda e remove a regiao
       conectada a ela ate sobrar so o corpo (borda saturada = corpo).
    2. peel=True: remove tambem CLUSTERS pequenos e NEUTROS conectados a
       borda (listras/sujeira), cada um com a propria cor de referencia,
       DESDE QUE o cluster nao estoure `peel_cap` da imagem — se estourar
       (e o corpo), descarta tudo e para. Nao come o corpo."""
    img = img.convert("RGB")
    w, h = img.size
    px = img.load()
    out = img.copy()
    o = out.load()
    removed = [[False] * w for _ in range(h)]
    area = w * h

    def border_dominant():
        counts = Counter()
        for x in range(w):
            for y in (0, h - 1):
                if not removed[y][x]:
                    p = px[x, y]
                    counts[(p[0] // 16 * 16 + 8, p[1] // 16 * 16 + 8,
                            p[2] // 16 * 16 + 8)] += 1
        for y in range(h):
            for x in (0, w - 1):
                if not removed[y][x]:
                    p = px[x, y]
                    counts[(p[0] // 16 * 16 + 8, p[1] // 16 * 16 + 8,
                            p[2] // 16 * 16 + 8)] += 1
        return counts

    def flood(ref, commit=True, cap=0):
        """Flood da borda removendo pixels <= tol de ref. Se cap>0 e o
        cluster passar de cap*area, desfaz (nao commita) e retorna False."""
        comp = []
        stack = []
        seeds = []
        for x in range(w):
            for y in (0, h - 1):
                if not removed[y][x] and math.dist(px[x, y], ref) <= tol:
                    comp.append((x, y))
                    stack.append((x, y))
        for y in range(h):
            for x in (0, w - 1):
                if not removed[y][x] and math.dist(px[x, y], ref) <= tol:
                    comp.append((x, y))
                    stack.append((x, y))
        used = set()
        while stack:
            x, y = stack.pop()
            if (x, y) in used:
                continue
            used.add((x, y))
            if cap and len(used) > area * cap:
                return False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and not removed[ny][nx] \
                   and (nx, ny) not in used \
                   and math.dist(px[nx, ny], ref) <= tol:
                    stack.append((nx, ny))
        for x, y in used:
            removed[y][x] = True
            o[x, y] = (255, 255, 255)
        return True

    while True:
        counts = border_dominant()
        if not counts:
            break
        ref = counts.most_common(1)[0][0]
        if max(ref) - min(ref) > sat:
            break
        if not flood(ref):
            break

    if peel:
        while True:
            seed = None
            for x in range(w):
                for y in (0, h - 1):
                    if not removed[y][x]:
                        p = px[x, y]
                        if max(p) - min(p) <= 90 and max(p) < 235:
                            seed = (x, y, p)
                            break
                if seed:
                    break
            if not seed:
                for y in range(h):
                    for x in (0, w - 1):
                        if not removed[y][x]:
                            p = px[x, y]
                            if max(p) - min(p) <= 90 and max(p) < 235:
                                seed = (x, y, p)
                                break
                    if seed:
                        break
            if not seed:
                break
            ref = (seed[2][0] // 16 * 16 + 8, seed[2][1] // 16 * 16 + 8,
                   seed[2][2] // 16 * 16 + 8)
            if not flood(ref, cap=peel_cap):
                break
    return out

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
    sizes = {STAGES[0][0]: STAGES[0][2]}
    aliases = []
    for base, prefix, size in STAGES[1:]:
        if base in pngs:
            img = Image.open(pngs[base])
            if base in BODY_H:
                # Novos: remove o fundo e dimensiona pelo CORPO
                img = fit_body(remove_bg(img), BODY_H[base])
                fw, fh = img.size
                if base in ("tangela", "tangrowth"):
                    # 1bpp por cor/bordas: contorno + escuros -> preto,
                    # corpo azul/verde -> branco. BOX em passos de 2x
                    # (pixel art) em vez de LANCZOS.
                    img2 = fit_body(remove_bg(img), BODY_H[base],
                                    resample=Image.BOX)
                    fw, fh = img2.size
                    arr = to_c_array(pokemon_1bpp(img2, fw, fh), fw, fh)
                else:
                    arr = to_c_array(img, fw, fh)
            else:
                # Existentes: pipeline original (recorte + fita no canvas) -
                # NAO mexe no visual ja aprovado.
                img = crop_content(img, CROP.get(base, 1.0))
                fw, fh = size
                arr = to_c_array(img, fw, fh)
            sizes[base] = (fw, fh)
            arrays[base] = [(f"{prefix}_IDLE", arr)]
            aliases.append(f"#define {prefix}_HAPPY {prefix}_IDLE")
            aliases.append(f"#define {prefix}_SAD   {prefix}_IDLE")
        elif old:
            sizes[base] = size
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
        fw, fh = sizes[base]
        block = "// Gerado por tools/rebuild_sprites.py\n"
        block += stage_header(prefix, (fw, fh))
        for name, arr in arrays[base]:
            block += "\n" + fmt_array(name, arr, fw, fh) + "\n"
        fname = os.path.join(sprites_dir, f"{base}.h")
        with open(fname, "w") as f:
            f.write(block)
        includes.append(f'#include "sprites/{base}.h"')

        # Variante 4-gray (2bpp): mesmo resize/recorte, silhueta + sombras
        gray_block = "// Gerado por tools/rebuild_sprites.py (variante 4-gray, 2 bits/pixel)\n"
        gray_block += (f"#define {prefix}_GRAY_W {fw}\n"
                       f"#define {prefix}_GRAY_H {fh}\n")
        if base in pngs:
            img = Image.open(pngs[base])
            if base in BODY_H:
                img = fit_body(remove_bg(img), BODY_H[base])
                gfw, gh = img.size
            else:
                img = crop_content(img, CROP.get(base, 1.0))
                gfw, gh = size
            mode = GRAY_MODES.get(base, DEFAULT_GRAY_MODE)
            overlay_img = None
            if len(mode) > 9 and mode[9]:
                # Composicao line-art: 1bpp (contorno/bordas/sombreamento)
                # por cima do tom 4-gray.
                overlay_img = fit_body(remove_bg(Image.open(pngs[base])),
                                       BODY_H[base], resample=Image.BOX)
            for name, arr in arrays[base]:
                gname = name.replace(f"{prefix}_", f"{prefix}_GRAY_")
                garr = to_c_array_2bpp(img, gfw, gh, mode,
                                       base in BOTTOM_ALIGN)
                if overlay_img is not None:
                    garr = overlay_2bpp(garr, pokemon_1bpp(overlay_img, gfw, gh),
                                        gfw, gh)
                gray_block += "\n" + fmt_array(gname, garr, gfw, gh) + "\n"
        elif old:
            fname_old = f"{prefix}_IDLE"
            arr = parse_array(old, fname_old)
            img = array_to_img(arr, 32, 32)
            gray_block += "\n" + fmt_array(f"{prefix}_GRAY_IDLE", to_c_array_2bpp(img, fw, fh), fw, fh) + "\n"
            for name, _ in arrays[base]:
                if name != f"{prefix}_IDLE":
                    gray_block += f"\n#define {name.replace(f'{prefix}_', f'{prefix}_GRAY_')} {prefix}_GRAY_IDLE\n"
        gfname = os.path.join(gray_dir, f"{base}.h")
        with open(gfname, "w") as f:
            f.write(gray_block)
        gray_includes.append(f'#include "sprites_gray/{base}.h"')

    # 3b) Icones avulsos (ex.: coco): 1 array 1bpp + 1 array 2bpp
    for entry in ICONS:
        base, prefix, size = entry[0], entry[1], entry[2]
        opts = entry[3] if len(entry) > 3 else {}
        thr = opts.get("thr", 127)
        crop = opts.get("crop", False)
        path = pngs.get(base)
        if not path:
            print(f"[aviso] sem imagem para o icone {base} - ignorado")
            continue
        img = Image.open(path)
        arr = icon_to_c_array(img, *size, thr=thr, crop=crop)
        block = "// Gerado por tools/rebuild_sprites.py\n"
        block += f"#define {prefix}_W {size[0]}\n#define {prefix}_H {size[1]}\n\n"
        block += fmt_array(prefix, arr, *size) + "\n"
        fname = os.path.join(sprites_dir, f"{base}.h")
        with open(fname, "w") as f:
            f.write(block)
        includes.append(f'#include "sprites/{base}.h"')

        gray_block = "// Gerado por tools/rebuild_sprites.py (variante 4-gray, 2 bits/pixel)\n"
        gray_block += f"#define {prefix}_GRAY_W {size[0]}\n#define {prefix}_GRAY_H {size[1]}\n\n"
        gray_block += fmt_array(f"{prefix}_GRAY", icon_to_c_array_2bpp(img, *size, thr=thr, crop=crop), *size) + "\n"
        gfname = os.path.join(gray_dir, f"{base}.h")
        with open(gfname, "w") as f:
            f.write(gray_block)
        gray_includes.append(f'#include "sprites_gray/{base}.h"')

    # 3c) Pre-visualizacoes (1bpp + 4-gray) p/ comparar com os PNGs
    # originais antes de gravar no ESP32. So para os sprites com
    # pipeline especifico.
    prev_dir = os.path.join(script_dir, "previews")
    os.makedirs(prev_dir, exist_ok=True)
    for base in ("tangela", "tangrowth"):
        if base not in pngs:
            continue
        img = fit_body(remove_bg(Image.open(pngs[base])), BODY_H[base])
        fw, fh = img.size
        if base in ("tangela", "tangrowth"):
            img_box = fit_body(remove_bg(Image.open(pngs[base])), BODY_H[base],
                               resample=Image.BOX)
            write_preview(os.path.join(prev_dir, f"{base}_1bpp.png"),
                          pokemon_1bpp(img_box, fw, fh))
        mode = GRAY_MODES.get(base, DEFAULT_GRAY_MODE)
        gray_arr = to_c_array_2bpp(img, fw, fh, mode, base in BOTTOM_ALIGN)
        if len(mode) > 9 and mode[9]:
            gray_arr = overlay_2bpp(gray_arr, pokemon_1bpp(img_box, fw, fh),
                                    fw, fh)
        write_preview(os.path.join(prev_dir, f"{base}_gray.png"),
                      gray_array_to_img(gray_arr, fw, fh))
        write_preview(os.path.join(prev_dir, f"{base}_orig.png"),
                      img, scale=2)
        print(f"[preview] tools/previews/{base}_*.png ({fw}x{fh})")

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
