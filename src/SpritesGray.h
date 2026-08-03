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
#include "sprites_gray/elekid.h"
#include "sprites_gray/electabuzz.h"
#include "sprites_gray/electivire.h"
#include "sprites_gray/magby.h"
#include "sprites_gray/magmar.h"
#include "sprites_gray/magmortar.h"
#include "sprites_gray/rhyhorn.h"
#include "sprites_gray/rhydon.h"
#include "sprites_gray/rhyperior.h"
#include "sprites_gray/onix.h"
#include "sprites_gray/steelix.h"
#include "sprites_gray/megasteelix.h"
#include "sprites_gray/tangela.h"
#include "sprites_gray/tangrowth.h"
#include "sprites_gray/coco.h"

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
#define ELEKID_GRAY_HAPPY ELEKID_GRAY_IDLE
#define ELEKID_GRAY_SAD   ELEKID_GRAY_IDLE
#define ELECTABUZZ_GRAY_HAPPY ELECTABUZZ_GRAY_IDLE
#define ELECTABUZZ_GRAY_SAD   ELECTABUZZ_GRAY_IDLE
#define ELECTIVIRE_GRAY_HAPPY ELECTIVIRE_GRAY_IDLE
#define ELECTIVIRE_GRAY_SAD   ELECTIVIRE_GRAY_IDLE
#define MAGBY_GRAY_HAPPY MAGBY_GRAY_IDLE
#define MAGBY_GRAY_SAD   MAGBY_GRAY_IDLE
#define MAGMAR_GRAY_HAPPY MAGMAR_GRAY_IDLE
#define MAGMAR_GRAY_SAD   MAGMAR_GRAY_IDLE
#define MAGMORTAR_GRAY_HAPPY MAGMORTAR_GRAY_IDLE
#define MAGMORTAR_GRAY_SAD   MAGMORTAR_GRAY_IDLE
#define RHYHORN_GRAY_HAPPY RHYHORN_GRAY_IDLE
#define RHYHORN_GRAY_SAD   RHYHORN_GRAY_IDLE
#define RHYDON_GRAY_HAPPY RHYDON_GRAY_IDLE
#define RHYDON_GRAY_SAD   RHYDON_GRAY_IDLE
#define RHYPERIOR_GRAY_HAPPY RHYPERIOR_GRAY_IDLE
#define RHYPERIOR_GRAY_SAD   RHYPERIOR_GRAY_IDLE
#define ONIX_GRAY_HAPPY ONIX_GRAY_IDLE
#define ONIX_GRAY_SAD   ONIX_GRAY_IDLE
#define STEELIX_GRAY_HAPPY STEELIX_GRAY_IDLE
#define STEELIX_GRAY_SAD   STEELIX_GRAY_IDLE
#define MEGASTEELIX_GRAY_HAPPY MEGASTEELIX_GRAY_IDLE
#define MEGASTEELIX_GRAY_SAD   MEGASTEELIX_GRAY_IDLE
#define TANGELA_GRAY_HAPPY TANGELA_GRAY_IDLE
#define TANGELA_GRAY_SAD   TANGELA_GRAY_IDLE
#define TANGROWTH_GRAY_HAPPY TANGROWTH_GRAY_IDLE
#define TANGROWTH_GRAY_SAD   TANGROWTH_GRAY_IDLE

#endif
