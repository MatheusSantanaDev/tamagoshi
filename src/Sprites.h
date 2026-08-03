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
#include "sprites/elekid.h"
#include "sprites/electabuzz.h"
#include "sprites/electivire.h"
#include "sprites/magby.h"
#include "sprites/magmar.h"
#include "sprites/magmortar.h"
#include "sprites/rhyhorn.h"
#include "sprites/rhydon.h"
#include "sprites/rhyperior.h"
#include "sprites/onix.h"
#include "sprites/steelix.h"
#include "sprites/megasteelix.h"
#include "sprites/tangela.h"
#include "sprites/tangrowth.h"
#include "sprites/coco.h"

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
#define ELEKID_HAPPY ELEKID_IDLE
#define ELEKID_SAD   ELEKID_IDLE
#define ELECTABUZZ_HAPPY ELECTABUZZ_IDLE
#define ELECTABUZZ_SAD   ELECTABUZZ_IDLE
#define ELECTIVIRE_HAPPY ELECTIVIRE_IDLE
#define ELECTIVIRE_SAD   ELECTIVIRE_IDLE
#define MAGBY_HAPPY MAGBY_IDLE
#define MAGBY_SAD   MAGBY_IDLE
#define MAGMAR_HAPPY MAGMAR_IDLE
#define MAGMAR_SAD   MAGMAR_IDLE
#define MAGMORTAR_HAPPY MAGMORTAR_IDLE
#define MAGMORTAR_SAD   MAGMORTAR_IDLE
#define RHYHORN_HAPPY RHYHORN_IDLE
#define RHYHORN_SAD   RHYHORN_IDLE
#define RHYDON_HAPPY RHYDON_IDLE
#define RHYDON_SAD   RHYDON_IDLE
#define RHYPERIOR_HAPPY RHYPERIOR_IDLE
#define RHYPERIOR_SAD   RHYPERIOR_IDLE
#define ONIX_HAPPY ONIX_IDLE
#define ONIX_SAD   ONIX_IDLE
#define STEELIX_HAPPY STEELIX_IDLE
#define STEELIX_SAD   STEELIX_IDLE
#define MEGASTEELIX_HAPPY MEGASTEELIX_IDLE
#define MEGASTEELIX_SAD   MEGASTEELIX_IDLE
#define TANGELA_HAPPY TANGELA_IDLE
#define TANGELA_SAD   TANGELA_IDLE
#define TANGROWTH_HAPPY TANGROWTH_IDLE
#define TANGROWTH_SAD   TANGROWTH_IDLE

// ============================================================
// Icon dimensions
// ============================================================
#define ICON_W  16
#define ICON_H  16
#define ICON_BYTES_PER_ROW  2

#endif
