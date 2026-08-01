#ifndef POKEMON_H
#define POKEMON_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

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

    // Demo: força um estágio com stats cheios
    void forceStage(PokemonStage stage);

    // Updates
    void update(unsigned long deltaMs);
    bool checkEvolution();

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

private:
    PokemonStage _stage;
    int _hunger;
    int _happiness;
    int _health;
    int _warmth;
    int _incubationMinutes;
    int _ageMinutes;
    bool _isAlive;
    unsigned long _minutesAtCurrentStage;
    time_t _lastTickEpoch;   // Ultimo segundo de jogo salvo (para catch-up)

    void tickMinute();
    void clampStats();
    void evolve();
};

#endif
