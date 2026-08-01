#ifndef EVOLUTION_H
#define EVOLUTION_H

#include "config.h"

// ============================================================
// LINHAS EVOLUTIVAS (configuravel)
//
// - O ovo sempre choca um pokemon BASE (HATCH_CHANCES).
//   Peso maior = mais chance. Ex.: 90/10 -> Pichu comum,
//   Scyther raro.
//
// - Cada estagio pode evoluir para 1 ou mais alvos
//   (EVOLUTION_RULES). Os pesos definem a chance relativa.
//   Soma dos pesos = 100% da escolha.
// ============================================================

struct HatchChance {
    PokemonStage stage;
    int weight;
};

// Chance de nascer ao chocar o ovo
static const HatchChance HATCH_CHANCES[] = {
    { STAGE_PICHU,   90 },   // comum
    { STAGE_SCYTHER, 10 },   // raro
};
#define HATCH_CHANCE_COUNT (sizeof(HATCH_CHANCES) / sizeof(HATCH_CHANCES[0]))

struct EvolveOption {
    PokemonStage target;
    int weight;
};

struct EvolutionRule {
    PokemonStage from;
    const EvolveOption* options;
    int optionCount;
};

// Scyther: 10% Kleavor, resto Scizor
static const EvolveOption SCYTHER_OPTIONS[] = {
    { STAGE_SCIZOR,    90 },
    { STAGE_KLEAVOR,   10 },
};

// Scizor evolui sempre para Mega Scizor
static const EvolveOption SCIZOR_OPTIONS[] = {
    { STAGE_MEGASCIZOR, 100 },
};

static const EvolveOption PICHU_OPTIONS[] = {
    { STAGE_PIKACHU, 100 },
};

static const EvolveOption PIKACHU_OPTIONS[] = {
    { STAGE_RAICHU, 100 },
};

// Raichu: 50/50 Mega X ou Mega Y
static const EvolveOption RAICHU_OPTIONS[] = {
    { STAGE_MEGARAICHUX, 50 },
    { STAGE_MEGARAICHUY, 50 },
};

static const EvolutionRule EVOLUTION_RULES[] = {
    { STAGE_SCYTHER, SCYTHER_OPTIONS, 2 },
    { STAGE_SCIZOR,  SCIZOR_OPTIONS,  1 },
    // Kleavor e estagio final (nao evolui)
    { STAGE_PICHU,   PICHU_OPTIONS,   1 },
    { STAGE_PIKACHU, PIKACHU_OPTIONS, 1 },
    { STAGE_RAICHU,  RAICHU_OPTIONS,  2 },
};
#define EVOLUTION_RULES_COUNT (sizeof(EVOLUTION_RULES) / sizeof(EVOLUTION_RULES[0]))

#endif
