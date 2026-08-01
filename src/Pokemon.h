#ifndef POKEMON_H
#define POKEMON_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

// Resultado da checagem de evolucao: nada, estagio normal, Mega ou
// fim da Mega (reverte ao estagio anterior).
enum EvolutionResult {
    EVO_NONE = 0,
    EVO_STAGE,
    EVO_MEGA,
    EVO_REVERT
};

class Pokemon {
public:
    Pokemon();
    void begin(time_t now = 0);
    void reset();
    void resetEgg();

    // Actions
    void feed();
    void play();
    void warm();
    void clean();

    // Demo: força um estágio com stats cheios
    void forceStage(PokemonStage stage);

    // [TESTE] Decai um stat por vez (energia->sono->higiene->coco) para
    // verificar o refresh parcial das barras. Sem risco de morte/evolucao.
    void testCycleBars();

    // Updates
    void update(unsigned long deltaMs);
    EvolutionResult checkEvolution();

    // Recupera energia/sono enquanto o ESP32 esta em modo dormir
    void sleepRecovery(unsigned long deltaMs);

    // Aplica os minutos decorridos enquanto o ESP32 estava desligado
    void catchUpFrom(time_t now);

    // Save/Load (NVS - Non-Volatile Storage)
    void save();
    void load();
    void clearSave();

    // Getters
    int getHunger() const { return _hunger; }
    int getHappiness() const { return _happiness; }
    int getHealth() const { return _health; }
    int getWarmth() const { return _warmth; }
    int getEnergy() const { return _energy; }
    int getSleep() const { return _sleep; }
    int getHygiene() const { return _hygiene; }
    int getDirt() const { return _dirt; }
    int getIncubationProgress() const { return _incubationMinutes; }
    int getAge() const { return _ageMinutes; }
    PokemonStage getStage() const { return _stage; }
    const char* getStageName() const;

    bool isEgg() const { return _stage == STAGE_EGG; }

    // Sprite selection
    const unsigned char* getCurrentSprite() const;

    // Sprite 4-gray (2bpp) - mesmo mapeamento de humor que getCurrentSprite
    const unsigned char* getCurrentGraySprite() const;

    // Check if critical
    bool isCritical() const;
    bool isDead() const;

    // Humor (unico, dinâmico - o que o jogador ve)
    const char* getMood() const;

    // Personalidade (oculta do jogador - afeta discretamente os stats)
    Personality getPersonality() const { return _personality; }
    const char* getPersonalityName() const {
        return PERSONALITIES[_personality].name;
    }
    // Minutos em estado critico no estagio atual (requisito da Mega)
    int getCriticalMinutes() const { return _criticalMinutes; }

    // Historico permanente (sobrevive ao reset do Pokemon)
    int getLostCount() const { return _lostCount; }
    int getWinCount() const { return _winCount; }
    int getShortestLife() const { return _shortestLife; }
    int getLongestLife() const { return _longestLife; }

private:
    PokemonStage _stage;
    int _hunger;
    int _happiness;
    int _health;
    int _warmth;
    int _energy;
    int _sleep;
    int _hygiene;
    int _dirt;
    int _incubationMinutes;
    float _incubationFraction;   // Fracao do minuto em incubacao (0.5 se devagar)

    int _ageMinutes;
    bool _isAlive;
    unsigned long _minutesAtCurrentStage;
    time_t _lastTickEpoch;   // Ultimo segundo de jogo salvo (para catch-up)

    Personality _personality;    // Sorteada na chocagem (oculta)
    int _criticalMinutes;        // Minutos criticos no estagio atual
    int _megaLifeMinutes;        // Vida total acumulada como Mega (nao reinicia)
    int _megaContinuousMinutes;  // Duracao da transformacao atual (max 12h)

    int32_t _lostCount;      // Mortes por descuido (fome, tristeza, doenca)
    int32_t _winCount;       // Mortes de velhice (chegou ao estagio final)
    int32_t _shortestLife;   // Vida mais curta (minutos)
    int32_t _longestLife;    // Vida mais longa (minutos)

    void tickMinute();
    void recordLife(bool victory);
    void clampStats();
    void evolve();
    bool megaRequirementsMet() const;
    void evolveToMega();
    void revertFromMega();
};

#endif
