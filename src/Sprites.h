#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

// ============================================================
// Sprites por estagio (gerado por tools/rebuild_sprites.py).
// Cada arquivo define as dimensoes (PREFIX_SPRITE_W/H) e o(s)
// array(s) do estagio. Convencao: 1 byte = 8 pixels, MSB first,
// bit=1 = preto, bit=0 = branco.
// ============================================================
#include "sprites/egg.h"
#include "sprites/scyther.h"
#include "sprites/scizor.h"
#include "sprites/kleavor.h"
#include "sprites/megascizor.h"
#include "sprites/pichu.h"
#include "sprites/pikachu.h"
#include "sprites/raichu.h"
#include "sprites/megaraichux.h"
#include "sprites/megaraichuy.h"

// ============================================================
// Aliases (variacoes de humor usam o mesmo sprite)
// ============================================================
#define SCYTHER_HAPPY SCYTHER_IDLE
#define SCYTHER_SAD   SCYTHER_IDLE
#define SCIZOR_HAPPY SCIZOR_IDLE
#define SCIZOR_SAD   SCIZOR_IDLE
#define KLEAVOR_HAPPY KLEAVOR_IDLE
#define KLEAVOR_SAD   KLEAVOR_IDLE
#define MEGASCIZOR_HAPPY MEGASCIZOR_IDLE
#define MEGASCIZOR_SAD   MEGASCIZOR_IDLE
#define PICHU_HAPPY PICHU_IDLE
#define PICHU_SAD   PICHU_IDLE
#define PIKACHU_HAPPY PIKACHU_IDLE
#define PIKACHU_SAD   PIKACHU_IDLE
#define RAICHU_HAPPY RAICHU_IDLE
#define RAICHU_SAD   RAICHU_IDLE
#define MEGARAICHUX_HAPPY MEGARAICHUX_IDLE
#define MEGARAICHUX_SAD   MEGARAICHUX_IDLE
#define MEGARAICHUY_HAPPY MEGARAICHUY_IDLE
#define MEGARAICHUY_SAD   MEGARAICHUY_IDLE

// ============================================================
// Icon dimensions
// ============================================================
#define ICON_W  16
#define ICON_H  16
#define ICON_BYTES_PER_ROW  2

#endif
