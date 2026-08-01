#include "Pokemon.h"
#include "Sprites.h"
#include "SpritesGray.h"
#include "Evolution.h"
#include "TimeKeeper.h"
#include <nvs_flash.h>
#include <nvs.h>

Pokemon::Pokemon()
    : _stage(STAGE_EGG), _hunger(80), _happiness(70),
      _health(100), _warmth(50), _incubationMinutes(0),
      _ageMinutes(0), _isAlive(true), _minutesAtCurrentStage(0),
      _lastTickEpoch(0), _lostCount(0), _winCount(0),
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
    _hunger = 80;
    _happiness = 70;
    _health = 100;
    _ageMinutes = 0;
    _isAlive = true;
    _minutesAtCurrentStage = 0;
    _lastTickEpoch = 0;
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
}

void Pokemon::play() {
    if (!_isAlive) return;

    if (_stage == STAGE_EGG) {
        // Brincar também ajuda a aquecer um pouco
        _warmth = min(_warmth + 5, STATS_MAX);
        return;
    }

    _happiness = min(_happiness + PLAY_AMOUNT, STATS_MAX);
    if (_health < 50) {
        _health = min(_health + 2, STATS_MAX);
    }
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
    _incubationMinutes = 0;
    _ageMinutes = 0;
    _minutesAtCurrentStage = 0;
    _lastTickEpoch = 0;
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

// Um minuto de jogo: idade, decaimento e incubacao
void Pokemon::tickMinute() {
    _ageMinutes++;

    if (_stage == STAGE_EGG) {
        // Decaimento do calor
        _warmth = max(_warmth - WARMTH_DECAY, STATS_MIN);

        // Se ovo estiver quente o suficiente, acumula progresso
        if (_warmth >= HATCH_WARMTH_MIN) {
            _incubationMinutes++;
        } else {
            // Se esfriar, o progresso regride
            if (_incubationMinutes > 0) {
                _incubationMinutes--;
            }
        }
    } else {
        // Comportamento normal (fora do ovo)
        _minutesAtCurrentStage++;
        _hunger = max(_hunger - HUNGER_DECAY, STATS_MIN);
        _happiness = max(_happiness - HAPPY_DECAY, STATS_MIN);

        if (_hunger <= 0 || _happiness <= 0) {
            _health = max(_health - HEALTH_DECAY, STATS_MIN);
        }

        if (_hunger > 70 && _happiness > 70 && _health < STATS_MAX) {
            _health = min(_health + 2, STATS_MAX);
        }

        // Velhice: nos estagios finais o Pokemon morre naturalmente
        bool isFinalStage = (_stage == STAGE_KLEAVOR ||
                             _stage == STAGE_MEGARAICHUX ||
                             _stage == STAGE_MEGARAICHUY);
        if (isFinalStage && _ageMinutes >= MAX_LIFE_MINUTES) {
            _isAlive = false;
            recordLife(true);
        } else if (_health <= 0) {
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

bool Pokemon::checkEvolution() {
    if (!_isAlive) return false;

    switch (_stage) {
        case STAGE_EGG:
            if (_incubationMinutes >= HATCH_TIME_MINUTES && _warmth >= HATCH_WARMTH_MIN) {
                evolve();
                return true;
            }
            break;
        case STAGE_SCYTHER:
            if (_ageMinutes >= EVOLVE_SCYTHER_TIME && _happiness >= EVOLVE_MIN_HAPPY) {
                evolve();
                return true;
            }
            break;
        case STAGE_SCIZOR:
            if (_ageMinutes >= EVOLVE_SCIZOR_TIME &&
                _happiness >= EVOLVE_MIN_HAPPY &&
                _hunger >= EVOLVE_MIN_FEED) {
                evolve();
                return true;
            }
            break;
        case STAGE_KLEAVOR:
            // Estagio final: nao evolui
            break;
        case STAGE_MEGASCIZOR:
            break;
        case STAGE_PICHU:
            if (_ageMinutes >= EVOLVE_PICHU_TIME && _happiness >= EVOLVE_MIN_HAPPY) {
                evolve();
                return true;
            }
            break;
        case STAGE_PIKACHU:
            if (_ageMinutes >= EVOLVE_PIKACHU_TIME &&
                _happiness >= EVOLVE_MIN_HAPPY &&
                _hunger >= EVOLVE_MIN_FEED) {
                evolve();
                return true;
            }
            break;
        case STAGE_RAICHU:
            if (_ageMinutes >= EVOLVE_RAICHU_TIME &&
                _happiness >= EVOLVE_MIN_HAPPY &&
                _hunger >= EVOLVE_MIN_FEED) {
                evolve();
                return true;
            }
            break;
        case STAGE_MEGARAICHUX:
        case STAGE_MEGARAICHUY:
            break;
    }
    return false;
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
        _hunger = 80;
        _happiness = 70;
        _health = 100;
        Serial.printf("[EVOLVE] Ovo chocou: %s\n", getStageName());
        _minutesAtCurrentStage = 0;
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
        if (_warmth >= HATCH_WARMTH_MIN && _incubationMinutes >= HATCH_TIME_MINUTES) {
            return EGG_HATCHING;
        }
        if (_warmth >= HATCH_WARMTH_MIN) {
            return EGG_WARM;
        }
        if (_warmth > 20) {
            return EGG_IDLE;
        }
        return EGG_COLD;
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
        if (_warmth >= HATCH_WARMTH_MIN && _incubationMinutes >= HATCH_TIME_MINUTES) {
            return EGG_GRAY_HATCHING;
        }
        if (_warmth >= HATCH_WARMTH_MIN) {
            return EGG_GRAY_WARM;
        }
        if (_warmth > 20) {
            return EGG_GRAY_IDLE;
        }
        return EGG_GRAY_COLD;
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

// Humor: comportamento momentaneo. Felicidade ajustada por saude/fome
// (um pokemon doente ou faminto fica pior do que a felicidade indica).
const char* Pokemon::getMood() const {
    if (!_isAlive) return "Sem vida";
    if (_stage == STAGE_EGG) return "Intrigado";

    int score = _happiness;
    if (_health < 30) score -= 30;
    if (_hunger < 20) score -= 20;
    if (_health > 60 && _hunger > 60) score += 10;

    if (score >= 80) return "Euforico";
    if (score >= 60) return "Animado";
    if (score >= 40) return "Calmo";
    if (score >= 20) return "Aborrecido";
    return "Deprimido";
}

// Estado: condicao geral, prioridade da pior situacao
const char* Pokemon::getState() const {
    if (!_isAlive) return "Morto";
    if (_stage == STAGE_EGG) return "Incubando";
    if (_hunger <= 20) return "Faminto";
    if (_happiness <= 20) return "Irritado";
    if (_health <= 30) return "Doente";
    if (_happiness <= 40) return "Triste";
    if (_hunger <= 40) return "Com fome";
    return "Feliz";
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
    nvs_set_i32(handle, "age", _ageMinutes);
    nvs_set_i32(handle, "stageMins", _minutesAtCurrentStage);
    nvs_set_u8(handle, "alive", _isAlive ? 1 : 0);

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
    if (nvs_get_i32(handle, "age", &i32val) == ESP_OK) {
        _ageMinutes = i32val;
    }
    if (nvs_get_i32(handle, "stageMins", &i32val) == ESP_OK) {
        _minutesAtCurrentStage = i32val;
    }
    if (nvs_get_u8(handle, "alive", &u8val) == ESP_OK) {
        _isAlive = (u8val == 1);
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
    Serial.printf(" Age:%dmin\n", _ageMinutes);
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
