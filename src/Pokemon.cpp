#include "Pokemon.h"
#include "Sprites.h"
#include "SpritesGray.h"
#include "Evolution.h"
#include "TimeKeeper.h"
#include <nvs_flash.h>
#include <nvs.h>

Pokemon::Pokemon()
    : _stage(STAGE_EGG), _hunger(80), _happiness(70),
      _health(100), _warmth(50), _energy(100), _sleep(100),
      _hygiene(100), _dirt(0), _incubationMinutes(0),
      _incubationFraction(0.0f), _ageMinutes(0), _isAlive(true), _minutesAtCurrentStage(0),
      _lastTickEpoch(0), _personality(P_GULOSO), _criticalMinutes(0),
      _megaLifeMinutes(0), _megaContinuousMinutes(0),
      _lostCount(0), _winCount(0),
      _shortestLife(0), _longestLife(0) {}

void Pokemon::begin(time_t now) {
    load();
    catchUpFrom(now);
}

void Pokemon::reset() {
    resetEgg();
}

void Pokemon::resetEgg() {
    _stage = STAGE_EGG;
    _warmth = 50;
    _incubationMinutes = 0;
    _incubationFraction = 0.0f;
    _hunger = 80;
    _happiness = 70;
    _health = 100;
    _energy = 100;
    _sleep = 100;
    _hygiene = 100;
    _dirt = 0;
    _ageMinutes = 0;
    _isAlive = true;
    _minutesAtCurrentStage = 0;
    _lastTickEpoch = 0;
    _personality = P_GULOSO;   // Sorteada de novo na chocagem
    _criticalMinutes = 0;
    _megaLifeMinutes = 0;
    _megaContinuousMinutes = 0;
    save();
}

void Pokemon::feed() {
    if (!_isAlive) return;

    if (_stage == STAGE_EGG) {
        // Durante fase ovo, botão alimentar aquece o ovo
        warm();
        return;
    }

    _hunger = min(_hunger + FEED_AMOUNT, STATS_MAX);
    if (_health < 50) {
        _health = min(_health + 3, STATS_MAX);
    }

    // Comer gera coco
    _dirt = min(_dirt + DIRT_PER_FEED, STATS_MAX);

    // Comer tambem alegra (Guloso fica muito feliz)
    _happiness = min(_happiness +
                     (FEED_HAPPY_BASE * PERSONALITIES[_personality].feedHappyMult) / 100,
                     STATS_MAX);
}

void Pokemon::play() {
    if (!_isAlive) return;

    if (_stage == STAGE_EGG) {
        // Brincar também ajuda a aquecer um pouco
        _warmth = min(_warmth + 5, STATS_MAX);
        return;
    }

    _happiness = min(_happiness +
                     (PLAY_AMOUNT * PERSONALITIES[_personality].playHappyMult) / 100,
                     STATS_MAX);
    if (_health < 50) {
        _health = min(_health + 2, STATS_MAX);
    }

    // Brincar gasta energia (personalidade influencia o esforco)
    _energy = max(_energy - (PLAY_ENERGY_COST * PERSONALITIES[_personality].playEnergyMult) / 100,
                  STATS_MIN);
}

void Pokemon::clean() {
    if (!_isAlive || _stage == STAGE_EGG) return;
    _dirt = 0;
    _hygiene = STATS_MAX;
}

void Pokemon::warm() {
    if (!_isAlive || _stage != STAGE_EGG) return;
    _warmth = min(_warmth + WARM_AMOUNT, STATS_MAX);
}

void Pokemon::forceStage(PokemonStage stage) {
    _stage = stage;
    _isAlive = true;
    _hunger = 80;
    _happiness = 70;
    _health = 100;
    _warmth = 50;
    _energy = 100;
    _sleep = 100;
    _hygiene = 100;
    _dirt = 0;
    _incubationMinutes = 0;
    _incubationFraction = 0.0f;
    _ageMinutes = 0;
    _minutesAtCurrentStage = 0;
    _lastTickEpoch = 0;
    _criticalMinutes = 0;
    _megaLifeMinutes = 0;
    _megaContinuousMinutes = 0;
}

// [TESTE] Decai um stat por vez, reciclando ao zerar. So mexe nos 4 stats
// das barras novas (energia/sono/higiene/coco) - felicidade e saude ficam
// intactos, entao o sprite e a mensagem nao mudam e qualquer flash de tela
// inteira fica visivel.
void Pokemon::testCycleBars() {
    static int idx = 0;
    static const int STEP = 3;

    switch (idx) {
        case 0:
            if (_energy <= 0) { _energy = STATS_MAX; idx = 1; }
            else _energy = max(_energy - STEP, STATS_MIN);
            break;
        case 1:
            if (_sleep <= 0) { _sleep = STATS_MAX; idx = 2; }
            else _sleep = max(_sleep - STEP, STATS_MIN);
            break;
        case 2:
            if (_hygiene <= 0) { _hygiene = STATS_MAX; idx = 3; }
            else _hygiene = max(_hygiene - STEP, STATS_MIN);
            break;
        default:
            if (_dirt >= STATS_MAX) { _dirt = 0; idx = 0; }
            else _dirt = min(_dirt + STEP, STATS_MAX);
            break;
    }
}

void Pokemon::update(unsigned long deltaMs) {
    if (!_isAlive) return;

    static unsigned long accumulated = 0;
    accumulated += deltaMs;

    const unsigned long ONE_MINUTE = 60000;
    while (accumulated >= ONE_MINUTE) {
        accumulated -= ONE_MINUTE;
        tickMinute();
    }
}

// Recupera energia/sono enquanto o ESP32 esta em modo dormir
void Pokemon::sleepRecovery(unsigned long deltaMs) {
    if (!_isAlive) return;

    static unsigned long accumulated = 0;
    accumulated += deltaMs;

    const unsigned long ONE_MINUTE = 60000;
    while (accumulated >= ONE_MINUTE) {
        accumulated -= ONE_MINUTE;
        const PersonalitySpec& p = PERSONALITIES[_personality];
        _energy = min(_energy + (SLEEP_RECOVERY_ENERGY * p.sleepRecMult) / 100, STATS_MAX);
        _sleep = min(_sleep + (SLEEP_RECOVERY_SLEEP * p.sleepRecMult) / 100, STATS_MAX);
    }
}

// Estagio final da linha (pode evoluir para Mega ou e terminal):
// tem relogio de vida proprio (FINAL_STAGE_LIFE_MIN)
static bool isFinalStage(PokemonStage s) {
    return s == STAGE_SCIZOR || s == STAGE_KLEAVOR || s == STAGE_RAICHU;
}

// Forma Mega (relogio acumulado de vida proprio)
static bool isMegaStage(PokemonStage s) {
    return s == STAGE_MEGASCIZOR || s == STAGE_MEGARAICHUX ||
           s == STAGE_MEGARAICHUY;
}

// Um minuto de jogo: idade, decaimento e incubacao
void Pokemon::tickMinute() {
    _ageMinutes++;

    if (_stage == STAGE_EGG) {
        // Decaimento do calor
        _warmth = max(_warmth - WARMTH_DECAY, STATS_MIN);

        // Calor so afeta a velocidade da incubacao:
        //   alto  (>= WARMTH_FAST_MIN) -> normal (+1 min/min)
        //   baixo (>= WARMTH_SLOW_MIN) -> devagar (+1 min a cada 2 min)
        //   critico (< WARMTH_SLOW_MIN) -> pausa
        // Nada regride: o ovo nunca perde progresso.
        if (_incubationMinutes < HATCH_TIME_MINUTES) {
            if (_warmth >= WARMTH_FAST_MIN) {
                _incubationFraction += 1.0f;
            } else if (_warmth >= WARMTH_SLOW_MIN) {
                _incubationFraction += 0.5f;
            }
            if (_incubationFraction >= 1.0f) {
                _incubationFraction -= 1.0f;
                _incubationMinutes++;
            }
        }
    } else {
        // Comportamento normal (fora do ovo)
        const PersonalitySpec& p = PERSONALITIES[_personality];
        _hunger = max(_hunger - (HUNGER_DECAY * p.hungerMult) / 100, STATS_MIN);
        _happiness = max(_happiness - (HAPPY_DECAY * p.happyMult) / 100, STATS_MIN);
        _energy = max(_energy - (ENERGY_DECAY * p.energyMult) / 100, STATS_MIN);
        _sleep = max(_sleep - (SLEEP_DECAY * p.sleepMult) / 100, STATS_MIN);
        _hygiene = max(_hygiene - (HYGIENE_DECAY * p.hygieneMult) / 100, STATS_MIN);
        _dirt = min(_dirt + DIRT_ACCUM_PER_MIN, STATS_MAX);

        if (_hunger <= 0 || _happiness <= 0) {
            _health = max(_health - (HEALTH_DECAY * p.healthMult) / 100, STATS_MIN);
        }

        // Cansado ou com sono: felicidade cai mais rapido
        if (_energy <= ENERGY_CRITICAL || _sleep <= SLEEP_CRITICAL) {
            _happiness = max(_happiness - 2, STATS_MIN);
        }

        // Sujo ou coco acumulado: saude cai
        if (_hygiene <= HYGIENE_CRITICAL || _dirt >= DIRT_CRITICAL) {
            _health = max(_health - (2 * p.healthMult) / 100, STATS_MIN);
        }

        if (_hunger > 70 && _happiness > 70 && _health < STATS_MAX &&
            _energy > 50 && _sleep > 50 && _hygiene > 40) {
            _health = min(_health + 2, STATS_MAX);
        }

        // Periodos criticos: conta minutos em estado critico no estagio
        // final normal (requisito da Mega). Durante a Mega nao conta -
        // a transformacao nao e "o estagio".
        if (!isMegaStage(_stage) &&
            (_health <= MOOD_HEALTH_LOW || _hunger <= MOOD_HUNGER_LOW ||
             _happiness <= MOOD_HAPPY_LOW ||
             _energy <= ENERGY_CRITICAL || _sleep <= SLEEP_CRITICAL ||
             _hygiene <= HYGIENE_CRITICAL || _dirt >= DIRT_CRITICAL)) {
            _criticalMinutes++;
        }

        // ============================================================
        // RELOGIOS DE VIDA (estagio final normal x Mega)
        // ============================================================
        if (isMegaStage(_stage)) {
            // Transformado: acumula o tempo da Mega (total e consecutivo).
            // O relogio do estagio final normal fica PAUSADO.
            _megaContinuousMinutes++;
            _megaLifeMinutes++;
        } else {
            // Normal: o relogio do estagio avanca (inclui o estagio final)
            _minutesAtCurrentStage++;
        }

        // Velhice: estagio final normal atinge o limite de vida,
        // OU a vida acumulada como Mega chega ao total. Ambos = VITORIA.
        if (isMegaStage(_stage)) {
            if (_megaLifeMinutes >= MEGA_LIFE_TOTAL_MIN) {
                _isAlive = false;
                recordLife(true);
            }
        } else if (isFinalStage(_stage)) {
            if (_minutesAtCurrentStage >= FINAL_STAGE_LIFE_MIN) {
                _isAlive = false;
                recordLife(true);
            }
        }

        if (_isAlive && _health <= 0) {
            _isAlive = false;
            _health = 0;
            recordLife(false);
        }
    }

    clampStats();
}

// Aplica o tempo que passou enquanto o ESP32 estava desligado.
// Ex.: ovo esfria, fome/felicidade caem, idade avanca.
// So roda com relogio real (NTP): com o relogio padrao o tempo decorrido
// real e desconhecido, e o catch-up ficaria errado.
void Pokemon::catchUpFrom(time_t now) {
    if (!timeIsSynced()) return;
    if (!_isAlive || _lastTickEpoch <= 0) return;
    if (now <= _lastTickEpoch) return;

    int minutes = (int)((now - _lastTickEpoch) / 60);
    if (minutes <= 0) return;

    if (minutes > MAX_CATCHUP_MINUTES) {
        Serial.printf("[CATCH-UP] %.1f dias desligado - limitando a %d min\n",
                      (float)(now - _lastTickEpoch) / 86400.0f,
                      MAX_CATCHUP_MINUTES);
        minutes = MAX_CATCHUP_MINUTES;
    }

    for (int i = 0; i < minutes && _isAlive; i++) {
        tickMinute();
    }

    Serial.printf("[CATCH-UP] %d min aplicados | %s\n", minutes, getStageName());
    save();
}

EvolutionResult Pokemon::checkEvolution() {
    if (!_isAlive) return EVO_NONE;

    switch (_stage) {
        case STAGE_EGG:
            // Choca quando a incubacao completa; calor so define a velocidade
            if (_incubationMinutes >= HATCH_TIME_MINUTES) {
                evolve();
                return EVO_STAGE;
            }
            break;

        // Evolucao normal: so o tempo no estagio
        case STAGE_PICHU:
            if (_minutesAtCurrentStage >= CLASS_TIMES_MIN[CLASS_BABY]) {
                evolve();
                return EVO_STAGE;
            }
            break;
        case STAGE_SCYTHER:
        case STAGE_PIKACHU:
            if (_minutesAtCurrentStage >= CLASS_TIMES_MIN[CLASS_STAGE1]) {
                evolve();
                return EVO_STAGE;
            }
            break;
        // Mega: requisitos no estagio final (tempo + stats + criticos)
        case STAGE_SCIZOR:
        case STAGE_RAICHU:
            if (_minutesAtCurrentStage >= CLASS_TIMES_MIN[CLASS_STAGE2]) {
                if (megaRequirementsMet()) {
                    evolveToMega();
                    return EVO_MEGA;
                }
            }
            break;

        // Mega e temporaria: maximo de MEGA_MAX_CONTINUOUS_MIN
        // consecutivos; ao fim, reverte ao estagio final anterior.
        case STAGE_MEGASCIZOR:
        case STAGE_MEGARAICHUX:
        case STAGE_MEGARAICHUY:
            if (_megaContinuousMinutes >= MEGA_MAX_CONTINUOUS_MIN) {
                revertFromMega();
                return EVO_REVERT;
            }
            break;

        case STAGE_KLEAVOR:
            break; // Estagio terminal: nunca evolui, morre de velhice
    }
    return EVO_NONE;
}

// Requisitos da Mega: tempo atingido + stats altos + poucos periodos
// criticos no estagio
bool Pokemon::megaRequirementsMet() const {
    if (_happiness < MEGA_REQ_HAPPINESS) return false;
    if (_health < MEGA_REQ_HEALTH) return false;
    if (_hygiene < MEGA_REQ_HYGIENE) return false;
    if (_sleep < MEGA_REQ_SLEEP) return false;
    if (_criticalMinutes > MEGA_REQ_MAX_CRITICAL_MIN) return false;
    return true;
}

// Entrada em Mega: alvo sorteado (Scizor -> Mega Scizor;
// Raichu -> Mega Raichu X/Y). O relogio do estagio final normal fica
// PAUSADO (_minutesAtCurrentStage intacto). O relogio acumulado da
// Mega (_megaLifeMinutes) continua de onde parou; apenas a duracao
// consecutiva da nova transformacao zera.
void Pokemon::evolveToMega() {
    for (size_t r = 0; r < EVOLUTION_RULES_COUNT; r++) {
        if (EVOLUTION_RULES[r].from != _stage) {
            continue;
        }
        const EvolutionRule& rule = EVOLUTION_RULES[r];
        int total = 0;
        for (int i = 0; i < rule.optionCount; i++) {
            total += rule.options[i].weight;
        }
        int roll = (int)(esp_random() % total);
        int acc = 0;
        for (int i = 0; i < rule.optionCount; i++) {
            acc += rule.options[i].weight;
            if (roll < acc) {
                _stage = rule.options[i].target;
                break;
            }
        }
        break;
    }
    _megaContinuousMinutes = 0;
    _criticalMinutes = 0;
    save();
    Serial.printf("[EVOLVE] MEGA EVOLUCAO: %s | vida Mega %d min\n",
                  getStageName(), _megaLifeMinutes);
}

// Fim da Mega (12h consecutivas): volta ao estagio final anterior.
// O relogio normal volta a contar de onde estava pausado; o relogio
// acumulado da Mega fica pausado no ponto em que estava.
void Pokemon::revertFromMega() {
    if (_stage == STAGE_MEGASCIZOR) {
        _stage = STAGE_SCIZOR;
    } else if (_stage == STAGE_MEGARAICHUX || _stage == STAGE_MEGARAICHUY) {
        _stage = STAGE_RAICHU;
    } else {
        return;
    }
    save();
    Serial.printf("[EVOLVE] Mega acabou - voltou para %s | vida Mega %d min\n",
                  getStageName(), _megaLifeMinutes);
}

void Pokemon::evolve() {
    if (_stage == STAGE_EGG) {
        // Ovo choca um pokemon base com a chance configurada
        int total = 0;
        for (size_t i = 0; i < HATCH_CHANCE_COUNT; i++) {
            total += HATCH_CHANCES[i].weight;
        }
        int roll = (int)(esp_random() % total);
        int acc = 0;
        for (size_t i = 0; i < HATCH_CHANCE_COUNT; i++) {
            acc += HATCH_CHANCES[i].weight;
            if (roll < acc) {
                _stage = HATCH_CHANCES[i].stage;
                break;
            }
        }
        _warmth = 0;
        _incubationMinutes = 0;
        _incubationFraction = 0.0f;
        _hunger = 80;
        _happiness = 70;
        _health = 100;
        _energy = 100;
        _sleep = 100;
        _hygiene = 100;
        _dirt = 0;
        // Personalidade (oculta) sorteada na chocagem
        _personality = (Personality)(esp_random() % P_COUNT);
        Serial.printf("[EVOLVE] Ovo chocou: %s (personalidade: %s)\n",
                      getStageName(), getPersonalityName());
        _minutesAtCurrentStage = 0;
        _criticalMinutes = 0;
        save();
        return;
    }

    // Evolucao normal: alvo sorteado pelos pesos configurados
    for (size_t r = 0; r < EVOLUTION_RULES_COUNT; r++) {
        if (EVOLUTION_RULES[r].from != _stage) {
            continue;
        }
        const EvolutionRule& rule = EVOLUTION_RULES[r];
        int total = 0;
        for (int i = 0; i < rule.optionCount; i++) {
            total += rule.options[i].weight;
        }
        int roll = (int)(esp_random() % total);
        int acc = 0;
        for (int i = 0; i < rule.optionCount; i++) {
            acc += rule.options[i].weight;
            if (roll < acc) {
                _stage = rule.options[i].target;
                break;
            }
        }
        _minutesAtCurrentStage = 0;
        _criticalMinutes = 0;
        save();
        Serial.printf("[EVOLVE] %s\n", getStageName());
        return;
    }
}

void Pokemon::clampStats() {
    _warmth = constrain(_warmth, STATS_MIN, STATS_MAX);
    _incubationMinutes = max(_incubationMinutes, 0);
    _hunger = constrain(_hunger, STATS_MIN, STATS_MAX);
    _happiness = constrain(_happiness, STATS_MIN, STATS_MAX);
    _health = constrain(_health, STATS_MIN, STATS_MAX);
    _energy = constrain(_energy, STATS_MIN, STATS_MAX);
    _sleep = constrain(_sleep, STATS_MIN, STATS_MAX);
    _hygiene = constrain(_hygiene, STATS_MIN, STATS_MAX);
    _dirt = constrain(_dirt, STATS_MIN, STATS_MAX);
}

const char* Pokemon::getStageName() const {
    if (_stage >= STAGE_EGG && _stage <= STAGE_MEGARAICHUY) {
        return STAGE_NAMES[_stage];
    }
    return "???";
}

const unsigned char* Pokemon::getCurrentSprite() const {
    if (!_isAlive) {
        return EGG_COLD;
    }

    if (_stage == STAGE_EGG) {
        // Sprite do ovo fica FIXO (IDLE) ao esquentar/esfriar: a mudanca de
        // calor so troca a frase (refresh parcial). So o choco muda o sprite.
        if (_warmth >= WARMTH_FAST_MIN && _incubationMinutes >= HATCH_TIME_MINUTES) {
            return EGG_HATCHING;
        }
        return EGG_IDLE;
    }

    bool isHappy = (_happiness >= 50);
    bool isSad = (_happiness < 30 || _health < 30);

    switch (_stage) {
        case STAGE_SCYTHER:
            if (isSad) return SCYTHER_SAD;
            if (isHappy) return SCYTHER_HAPPY;
            return SCYTHER_IDLE;
        case STAGE_SCIZOR:
            if (isSad) return SCIZOR_SAD;
            if (isHappy) return SCIZOR_HAPPY;
            return SCIZOR_IDLE;
        case STAGE_KLEAVOR:
            if (isSad) return KLEAVOR_SAD;
            if (isHappy) return KLEAVOR_HAPPY;
            return KLEAVOR_IDLE;
        case STAGE_MEGASCIZOR:
            if (isSad) return MEGASCIZOR_SAD;
            if (isHappy) return MEGASCIZOR_HAPPY;
            return MEGASCIZOR_IDLE;
        case STAGE_PICHU:
            if (isSad) return PICHU_SAD;
            if (isHappy) return PICHU_HAPPY;
            return PICHU_IDLE;
        case STAGE_PIKACHU:
            if (isSad) return PIKACHU_SAD;
            if (isHappy) return PIKACHU_HAPPY;
            return PIKACHU_IDLE;
        case STAGE_RAICHU:
            if (isSad) return RAICHU_SAD;
            if (isHappy) return RAICHU_HAPPY;
            return RAICHU_IDLE;
        case STAGE_MEGARAICHUX:
            if (isSad) return MEGARAICHUX_SAD;
            if (isHappy) return MEGARAICHUX_HAPPY;
            return MEGARAICHUX_IDLE;
        case STAGE_MEGARAICHUY:
            if (isSad) return MEGARAICHUY_SAD;
            if (isHappy) return MEGARAICHUY_HAPPY;
            return MEGARAICHUY_IDLE;
        default:
            return SCYTHER_IDLE;
    }
}

const unsigned char* Pokemon::getCurrentGraySprite() const {
    if (!_isAlive) {
        return EGG_GRAY_COLD;
    }

    if (_stage == STAGE_EGG) {
        // Sprite do ovo fica FIXO (IDLE) ao esquentar/esfriar: a mudanca de
        // calor so troca a frase (refresh parcial). So o choco muda o sprite.
        if (_warmth >= WARMTH_FAST_MIN && _incubationMinutes >= HATCH_TIME_MINUTES) {
            return EGG_GRAY_HATCHING;
        }
        return EGG_GRAY_IDLE;
    }

    bool isHappy = (_happiness >= 50);
    bool isSad = (_happiness < 30 || _health < 30);

    switch (_stage) {
        case STAGE_SCYTHER:
            if (isSad) return SCYTHER_GRAY_SAD;
            if (isHappy) return SCYTHER_GRAY_HAPPY;
            return SCYTHER_GRAY_IDLE;
        case STAGE_SCIZOR:
            if (isSad) return SCIZOR_GRAY_SAD;
            if (isHappy) return SCIZOR_GRAY_HAPPY;
            return SCIZOR_GRAY_IDLE;
        case STAGE_KLEAVOR:
            if (isSad) return KLEAVOR_GRAY_SAD;
            if (isHappy) return KLEAVOR_GRAY_HAPPY;
            return KLEAVOR_GRAY_IDLE;
        case STAGE_MEGASCIZOR:
            if (isSad) return MEGASCIZOR_GRAY_SAD;
            if (isHappy) return MEGASCIZOR_GRAY_HAPPY;
            return MEGASCIZOR_GRAY_IDLE;
        case STAGE_PICHU:
            if (isSad) return PICHU_GRAY_SAD;
            if (isHappy) return PICHU_GRAY_HAPPY;
            return PICHU_GRAY_IDLE;
        case STAGE_PIKACHU:
            if (isSad) return PIKACHU_GRAY_SAD;
            if (isHappy) return PIKACHU_GRAY_HAPPY;
            return PIKACHU_GRAY_IDLE;
        case STAGE_RAICHU:
            if (isSad) return RAICHU_GRAY_SAD;
            if (isHappy) return RAICHU_GRAY_HAPPY;
            return RAICHU_GRAY_IDLE;
        case STAGE_MEGARAICHUX:
            if (isSad) return MEGARAICHUX_GRAY_SAD;
            if (isHappy) return MEGARAICHUX_GRAY_HAPPY;
            return MEGARAICHUX_GRAY_IDLE;
        case STAGE_MEGARAICHUY:
            if (isSad) return MEGARAICHUY_GRAY_SAD;
            if (isHappy) return MEGARAICHUY_GRAY_HAPPY;
            return MEGARAICHUY_GRAY_IDLE;
        default:
            return SCYTHER_GRAY_IDLE;
    }
}

bool Pokemon::isCritical() const {
    if (_stage == STAGE_EGG) {
        return _warmth <= 0;
    }
    return _isAlive && (_health < 20 || _hunger <= 0 || _happiness <= 0);
}

bool Pokemon::isDead() const {
    return !_isAlive;
}

// Humor: sistema priorizado (pior situacao primeiro).
// Doente > Faminto > Cansado > Irritado > Triste > Feliz > Neutro
const char* Pokemon::getMood() const {
    if (!_isAlive) return "Sem vida";
    if (_stage == STAGE_EGG) return "Intrigado";
    if (_health <= MOOD_HEALTH_LOW) return "Doente";
    if (_hunger <= MOOD_HUNGER_LOW) return "Faminto";
    if (_energy <= MOOD_ENERGY_LOW && _sleep <= MOOD_SLEEP_LOW) return "Cansado";
    if (_hygiene <= MOOD_HYGIENE_LOW) return "Irritado";
    if (_happiness <= MOOD_HAPPY_LOW && _hunger <= MOOD_HUNGER_MID) return "Triste";
    if (_happiness >= MOOD_HAPPY_HIGH && _hunger >= MOOD_HUNGER_OK &&
        _energy >= MOOD_ENERGY_HIGH) return "Feliz";
    return "Neutro";
}

// Registra o fim de uma vida no historico permanente
void Pokemon::recordLife(bool victory) {
    if (victory) {
        _winCount++;
        Serial.printf("[LIFE] Velhice - VITORIA! (%d)\n", _winCount);
    } else {
        _lostCount++;
        Serial.printf("[LIFE] Descuido - PERDA! (%d)\n", _lostCount);
    }

    if (_shortestLife == 0 || _ageMinutes < _shortestLife) {
        _shortestLife = _ageMinutes;
    }
    if (_ageMinutes > _longestLife) {
        _longestLife = _ageMinutes;
    }

    Serial.printf("[LIFE] Vida: %dmin (curta:%d longa:%d)\n",
                  _ageMinutes, _shortestLife, _longestLife);
    save();
}

// ============================================================
// NVS Save/Load
// ============================================================
#define NVS_NAMESPACE "tamagoshi"

void Pokemon::save() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Serial.printf("NVS open for write failed: %d\n", err);
        return;
    }

    nvs_set_u8(handle, "stage", (uint8_t)_stage);
    nvs_set_i32(handle, "warmth", _warmth);
    nvs_set_i32(handle, "incubMin", _incubationMinutes);
    nvs_set_i32(handle, "hunger", _hunger);
    nvs_set_i32(handle, "happy", _happiness);
    nvs_set_i32(handle, "health", _health);
    nvs_set_i32(handle, "energy", _energy);
    nvs_set_i32(handle, "sleep", _sleep);
    nvs_set_i32(handle, "hygiene", _hygiene);
    nvs_set_i32(handle, "dirt", _dirt);
    nvs_set_i32(handle, "age", _ageMinutes);
    nvs_set_i32(handle, "stageMins", _minutesAtCurrentStage);
    nvs_set_u8(handle, "alive", _isAlive ? 1 : 0);
    nvs_set_u8(handle, "personality", (uint8_t)_personality);
    nvs_set_i32(handle, "critMin", _criticalMinutes);
    nvs_set_i32(handle, "megaLife", _megaLifeMinutes);
    nvs_set_i32(handle, "megaCont", _megaContinuousMinutes);

    // Historico permanente (nao reseta junto com o Pokemon)
    nvs_set_i32(handle, "lost", _lostCount);
    nvs_set_i32(handle, "wins", _winCount);
    nvs_set_i32(handle, "shortLife", _shortestLife);
    nvs_set_i32(handle, "longLife", _longestLife);

    // Marca o instante deste save para o catch-up no proximo boot.
    // So grava com relogio sincronizado (NTP): com o relogio padrao
    // (fallback) o timestamp nao representa o tempo real.
    if (timeIsSynced()) {
        _lastTickEpoch = epochNow();
        nvs_set_i64(handle, "lastTick", (int64_t)_lastTickEpoch);
    }

    nvs_commit(handle);
    nvs_close(handle);

    Serial.printf("[SAVE] %s | Warmth:%d Incub:%dmin\n",
                  getStageName(), _warmth, _incubationMinutes);
}

void Pokemon::load() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        Serial.println("[LOAD] No saved data, starting as egg.");
        resetEgg();
        return;
    }

    uint8_t u8val;
    int32_t i32val;
    int64_t i64val;

    if (nvs_get_u8(handle, "stage", &u8val) == ESP_OK) {
        _stage = (PokemonStage)u8val;
    }
    if (nvs_get_i32(handle, "warmth", &i32val) == ESP_OK) {
        _warmth = i32val;
    }
    if (nvs_get_i32(handle, "incubMin", &i32val) == ESP_OK) {
        _incubationMinutes = i32val;
    }
    if (nvs_get_i32(handle, "hunger", &i32val) == ESP_OK) {
        _hunger = i32val;
    }
    if (nvs_get_i32(handle, "happy", &i32val) == ESP_OK) {
        _happiness = i32val;
    }
    if (nvs_get_i32(handle, "health", &i32val) == ESP_OK) {
        _health = i32val;
    }
    if (nvs_get_i32(handle, "energy", &i32val) == ESP_OK) {
        _energy = i32val;
    }
    if (nvs_get_i32(handle, "sleep", &i32val) == ESP_OK) {
        _sleep = i32val;
    }
    if (nvs_get_i32(handle, "hygiene", &i32val) == ESP_OK) {
        _hygiene = i32val;
    }
    if (nvs_get_i32(handle, "dirt", &i32val) == ESP_OK) {
        _dirt = i32val;
    }
    if (nvs_get_i32(handle, "age", &i32val) == ESP_OK) {
        _ageMinutes = i32val;
    }
    if (nvs_get_i32(handle, "stageMins", &i32val) == ESP_OK) {
        _minutesAtCurrentStage = i32val;
    }
    if (nvs_get_u8(handle, "alive", &u8val) == ESP_OK) {
        _isAlive = (u8val == 1);
    }
    if (nvs_get_u8(handle, "personality", &u8val) == ESP_OK && u8val < P_COUNT) {
        _personality = (Personality)u8val;
    }
    if (nvs_get_i32(handle, "critMin", &i32val) == ESP_OK) {
        _criticalMinutes = i32val;
    }
    if (nvs_get_i32(handle, "megaLife", &i32val) == ESP_OK) {
        _megaLifeMinutes = i32val;
    }
    if (nvs_get_i32(handle, "megaCont", &i32val) == ESP_OK) {
        _megaContinuousMinutes = i32val;
    }
    if (nvs_get_i64(handle, "lastTick", &i64val) == ESP_OK) {
        _lastTickEpoch = (time_t)i64val;
    }
    if (nvs_get_i32(handle, "lost", &i32val) == ESP_OK) {
        _lostCount = i32val;
    }
    if (nvs_get_i32(handle, "wins", &i32val) == ESP_OK) {
        _winCount = i32val;
    }
    if (nvs_get_i32(handle, "shortLife", &i32val) == ESP_OK) {
        _shortestLife = i32val;
    }
    if (nvs_get_i32(handle, "longLife", &i32val) == ESP_OK) {
        _longestLife = i32val;
    }

    nvs_close(handle);
    clampStats();

    Serial.printf("[LOAD] %s | Warmth:%d Incub:%dmin",
                  getStageName(), _warmth, _incubationMinutes);
    if (_stage != STAGE_EGG) {
        Serial.printf(" Hunger:%d Happy:%d Health:%d", _hunger, _happiness, _health);
    }
    Serial.printf(" Age:%dmin | Personalidade: %s\n",
                  _ageMinutes, getPersonalityName());
}

void Pokemon::clearSave() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        Serial.println("[SAVE] Data erased.");
    }
}
