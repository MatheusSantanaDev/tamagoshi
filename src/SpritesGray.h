#ifndef SPRITES_GRAY_H
#define SPRITES_GRAY_H

#include <Arduino.h>

// ============================================================
// Sprites 4-gray por estagio (gerado por tools/rebuild_sprites.py).
// Formato: 2 bits/pixel, 4 pixels/byte, MSB first.
//   0b11 = branco | 0b10 = cinza claro | 0b01 = cinza escuro | 0b00 = preto
// ============================================================
#include "sprites_gray/egg.h"
#include "sprites_gray/scyther.h"
#include "sprites_gray/scizor.h"
#include "sprites_gray/kleavor.h"
#include "sprites_gray/megascizor.h"
#include "sprites_gray/pichu.h"
#include "sprites_gray/pikachu.h"
#include "sprites_gray/raichu.h"
#include "sprites_gray/megaraichux.h"
#include "sprites_gray/megaraichuy.h"

// ============================================================
// Aliases (variacoes de humor usam o mesmo sprite)
// ============================================================
#define SCYTHER_GRAY_HAPPY SCYTHER_GRAY_IDLE
#define SCYTHER_GRAY_SAD   SCYTHER_GRAY_IDLE
#define SCIZOR_GRAY_HAPPY SCIZOR_GRAY_IDLE
#define SCIZOR_GRAY_SAD   SCIZOR_GRAY_IDLE
#define KLEAVOR_GRAY_HAPPY KLEAVOR_GRAY_IDLE
#define KLEAVOR_GRAY_SAD   KLEAVOR_GRAY_IDLE
#define MEGASCIZOR_GRAY_HAPPY MEGASCIZOR_GRAY_IDLE
#define MEGASCIZOR_GRAY_SAD   MEGASCIZOR_GRAY_IDLE
#define PICHU_GRAY_HAPPY PICHU_GRAY_IDLE
#define PICHU_GRAY_SAD   PICHU_GRAY_IDLE
#define PIKACHU_GRAY_HAPPY PIKACHU_GRAY_IDLE
#define PIKACHU_GRAY_SAD   PIKACHU_GRAY_IDLE
#define RAICHU_GRAY_HAPPY RAICHU_GRAY_IDLE
#define RAICHU_GRAY_SAD   RAICHU_GRAY_IDLE
#define MEGARAICHUX_GRAY_HAPPY MEGARAICHUX_GRAY_IDLE
#define MEGARAICHUX_GRAY_SAD   MEGARAICHUX_GRAY_IDLE
#define MEGARAICHUY_GRAY_HAPPY MEGARAICHUY_GRAY_IDLE
#define MEGARAICHUY_GRAY_SAD   MEGARAICHUY_GRAY_IDLE

#endif
