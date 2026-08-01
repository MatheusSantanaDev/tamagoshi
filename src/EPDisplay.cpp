#include "EPDisplay.h"
#include "Sprites.h"
#include "SpritesGray.h"
#include <time.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "FreeMonoBold9pt8b.h"

// ============================================================
// Layout constants (adaptativo ao tamanho do display)
// ============================================================
#define BAR_LABEL_X  10
#define BAR_VALUE_X  70
#define BAR_W        (DISPLAY_WIDTH - 10 - BAR_VALUE_X)
#define BAR_H        10
#define BAR_GAP      18
#define BAR_BOTTOM_MARGIN 6

// Base (y do rodapé) do sprite na tela principal: o sprite cresce para cima
#define SPRITE_BOTTOM 272

EPDisplay::EPDisplay()
    : _epd(nullptr), _graySprite(nullptr), _grayX(0), _grayY(0),
      _grayW(0), _grayH(0), _graySpriteSet(false),
      _clockX(72), _clockY(36), _clockW(96), _clockH(18),
      _lastClockSec(-1) {}

void EPDisplay::begin() {
    initDisplay();
}

void EPDisplay::initDisplay() {
    // Cria instância do display com os pinos configurados
    // GxEPD2_BW<DISPLAY_CLASS, DISPLAY_HEIGHT>
    _epd = new DisplayType(DISPLAY_CLASS(EP_CS, EP_DC, EP_RST, EP_BUSY));

#if GRAY_MODE
    // Em gray mode o GxEPD2 só serve de canvas (buffer 1bpp);
    // o hardware é controlado pelo driver 4-gray.
    _gray.begin();
#else
    _epd->init();
#endif
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);
    _epd->setRotation(0);
    _epd->setTextColor(GxEPD_BLACK);
    _epd->setFont(&FreeMonoBold9pt8b);
}

int EPDisplay::centerX(int width) const {
    return (DISPLAY_WIDTH - width) / 2;
}

int EPDisplay::centerY(int height) const {
    return (DISPLAY_HEIGHT - height) / 2;
}

void EPDisplay::getSpriteSize(const Pokemon& pet, int16_t& w, int16_t& h) const {
    switch (pet.getStage()) {
        case STAGE_EGG:
            w = EGG_SPRITE_W;       h = EGG_SPRITE_H;       break;
        case STAGE_SCYTHER:
            w = SCYTHER_SPRITE_W;   h = SCYTHER_SPRITE_H;   break;
        case STAGE_SCIZOR:
            w = SCIZOR_SPRITE_W;    h = SCIZOR_SPRITE_H;    break;
        case STAGE_KLEAVOR:
            w = KLEAVOR_SPRITE_W;   h = KLEAVOR_SPRITE_H;   break;
        case STAGE_MEGASCIZOR:
            w = MEGASCIZOR_SPRITE_W; h = MEGASCIZOR_SPRITE_H; break;
        case STAGE_PICHU:
            w = PICHU_SPRITE_W;       h = PICHU_SPRITE_H;       break;
        case STAGE_PIKACHU:
            w = PIKACHU_SPRITE_W;     h = PIKACHU_SPRITE_H;     break;
        case STAGE_RAICHU:
            w = RAICHU_SPRITE_W;      h = RAICHU_SPRITE_H;      break;
        case STAGE_MEGARAICHUX:
            w = MEGARAICHUX_SPRITE_W; h = MEGARAICHUX_SPRITE_H; break;
        case STAGE_MEGARAICHUY:
            w = MEGARAICHUY_SPRITE_W; h = MEGARAICHUY_SPRITE_H; break;
        default:
            w = EGG_SPRITE_W;       h = EGG_SPRITE_H;       break;
    }
}

void EPDisplay::drawSprite(int16_t x, int16_t y, const unsigned char* bitmap,
                           int16_t w, int16_t h, uint16_t color) {
    // bit=1 = color (preto), MSB first — padrão XBM
    _epd->drawBitmap(x, y, bitmap, w, h, color);
}

void EPDisplay::setGraySprite(const uint8_t* data, int16_t x, int16_t y,
                              int16_t w, int16_t h) {
#if GRAY_MODE
    _graySprite = data;
    _grayX = x;
    _grayY = y;
    _grayW = w;
    _grayH = h;
    _graySpriteSet = true;
#else
    (void)data; (void)x; (void)y; (void)w; (void)h;
#endif
}

void EPDisplay::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                int value, int maxVal, uint16_t barColor) {
    // Borda
    _epd->drawRect(x, y, w, h, GxEPD_BLACK);

    // Preenchimento
    int fillW = map(constrain(value, 0, maxVal), 0, maxVal, 0, w - 2);
    if (fillW > 0) {
        _epd->fillRect(x + 1, y + 1, fillW, h - 2, barColor);
    }
}

// ============================================================
// Tela principal - mostra o Pokémon com mini stats
// ============================================================
void EPDisplay::drawPet(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    // Nome e estágio no topo
    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(10, 18);
    _epd->print(pet.getStageName());

    // Idade
    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->setCursor(DISPLAY_WIDTH - 70, 18);
    _epd->printf("%d min", pet.getAge());

    // Sprite ancorado pela BASE (cresce para cima, usando o espaco do topo).
    // Base fixa em SPRITE_BOTTOM; o topo sobe conforme o tamanho do estagio.
    // Apenas o ovo fica centralizado (ficava muito embaixo ancorado).
    int16_t sw, sh;
    getSpriteSize(pet, sw, sh);
    int spriteX = (DISPLAY_WIDTH - sw) / 2;
    int spriteY;
    if (pet.getStage() == STAGE_EGG) {
        spriteY = (DISPLAY_HEIGHT - sh) / 2;
    } else {
        spriteY = SPRITE_BOTTOM - sh;
    }
#if GRAY_MODE
    setGraySprite(pet.getCurrentGraySprite(), spriteX, spriteY, sw, sh);
#else
    drawSprite(spriteX, spriteY, pet.getCurrentSprite(), sw, sh, GxEPD_BLACK);
#endif

    _epd->setFont(&FreeMonoBold9pt8b);

    // Mensagem em posicao fixa (logo abaixo da base do sprite grande)
    const char* msg;
    if (pet.isEgg()) {
        if (pet.getWarmth() < 20) {
            msg = "Ovo esta frio!";
        } else if (pet.getWarmth() >= HATCH_WARMTH_MIN) {
            msg = "Aquecido! Quase la!";
        } else {
            msg = "Aque\xE7""a o ovo!";
        }
    } else if (pet.isDead()) {
        msg = "SEM VIDA...";
    } else if (pet.isCritical()) {
        msg = "PRECISA DE CUIDADOS!";
    } else {
        msg = "Feliz e saudavel!";
    }
    int16_t tx0, ty0;
    uint16_t tw, th;
    _epd->getTextBounds(msg, 0, 0, &tx0, &ty0, &tw, &th);
    _epd->setCursor((DISPLAY_WIDTH - tw) / 2, SPRITE_BOTTOM + 18 - ty0);
    _epd->print(msg);

    // Barras ancoradas no extremo inferior.
    // Cada linha: label em BAR_LABEL_X, barra em BAR_VALUE_X.
    // Label termina em x=65 (5 chars * 11px), barra comeca em x=70.
    int barY = DISPLAY_HEIGHT - BAR_BOTTOM_MARGIN - BAR_H;

    if (pet.isEgg()) {
        // === MODO OVO: Calor + progresso de incubação ===
        _epd->setCursor(BAR_LABEL_X, barY - BAR_GAP - 2);
        _epd->print("Calor");
        drawProgressBar(BAR_VALUE_X, barY - BAR_GAP - 12, BAR_W, BAR_H,
                        pet.getWarmth(), STATS_MAX, GxEPD_BLACK);

        _epd->setCursor(BAR_LABEL_X, barY - 2);
        _epd->print("Choco");
        drawProgressBar(BAR_VALUE_X, barY - 12, BAR_W, BAR_H,
                        pet.getIncubationProgress(), HATCH_TIME_MINUTES, GxEPD_BLACK);
    } else {
        // === MODO POKEMON NORMAL ===
        _epd->setCursor(BAR_LABEL_X, barY - 2 * BAR_GAP - 2);
        _epd->print("Fome");
        drawProgressBar(BAR_VALUE_X, barY - 2 * BAR_GAP - 12, BAR_W, BAR_H,
                        pet.getHunger(), STATS_MAX, GxEPD_BLACK);

        _epd->setCursor(BAR_LABEL_X, barY - BAR_GAP - 2);
        _epd->print("Fel");
        drawProgressBar(BAR_VALUE_X, barY - BAR_GAP - 12, BAR_W, BAR_H,
                        pet.getHappiness(), STATS_MAX, GxEPD_BLACK);

        _epd->setCursor(BAR_LABEL_X, barY - 2);
        _epd->print("Sau");
        drawProgressBar(BAR_VALUE_X, barY - 12, BAR_W, BAR_H,
                        pet.getHealth(), STATS_MAX, GxEPD_BLACK);
    }

    refresh(); // full refresh
}

// ============================================================
// Tela de status detalhado
// ============================================================

// Formata idade em minutos: "12min" ou "2h30min"
static void formatAge(char* buf, size_t n, int minutes) {
    if (minutes < 60) {
        snprintf(buf, n, "%dmin", minutes);
    } else {
        snprintf(buf, n, "%dh%02dmin", minutes / 60, minutes % 60);
    }
}

// Nome curto do estagio (os nomes "Mega ..." nao cabem na coluna de valores)
static const char* shortStageName(PokemonStage stage) {
    switch (stage) {
        case STAGE_MEGASCIZOR:  return "MegaScizor";
        case STAGE_MEGARAICHUX: return "MegaRaichuX";
        case STAGE_MEGARAICHUY: return "MegaRaichuY";
        default:                return STAGE_NAMES[stage];
    }
}

void EPDisplay::drawStats(const Pokemon& pet, time_t now) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    // Titulo (FreeMono 12pt: 14px por caractere monoespacado)
    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(centerX(10 * 14), 25);
    _epd->print("== STATUS ==");

    // Relogio logo abaixo do titulo
    if (now > 0) {
        struct tm tm;
        localtime_r(&now, &tm);
        char clock[9];
        snprintf(clock, sizeof(clock), "%02d:%02d:%02d",
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
        _epd->setFont(&FreeMonoBold9pt8b);
        _epd->setCursor(centerX(8 * 11), 48);
        _epd->print(clock);
        _lastClockSec = tm.tm_sec;
    }

    // Lista de infos (rotulo em 10, valor em 112; rotulo max 9 chars)
    const int labelX = 10;
    const int valueX = 112;
    const int lineH = 19;
    int y = 78;
    char buf[16];

    _epd->setFont(&FreeMonoBold9pt8b);

    // Pokemon
    _epd->setCursor(labelX, y);
    _epd->print("Pokemon:");
    _epd->setCursor(valueX, y);
    _epd->print(shortStageName(pet.getStage()));
    y += lineH;

    // Idade
    _epd->setCursor(labelX, y);
    _epd->print("Idade:");
    formatAge(buf, sizeof(buf), pet.getAge());
    _epd->setCursor(valueX, y);
    _epd->print(buf);
    y += lineH;

    if (pet.isEgg()) {
        // Ovo: progresso em texto (sem barras)
        _epd->setCursor(labelX, y);
        _epd->print("Calor:");
        _epd->setCursor(valueX, y);
        _epd->printf("%d/%d", pet.getWarmth(), STATS_MAX);
        y += lineH;

        _epd->setCursor(labelX, y);
        _epd->print("Choco:");
        _epd->setCursor(valueX, y);
        _epd->printf("%d/%d min", pet.getIncubationProgress(), HATCH_TIME_MINUTES);
        y += lineH;
    } else {
        // Humor e estado
        _epd->setCursor(labelX, y);
        _epd->print("Humor:");
        _epd->setCursor(valueX, y);
        _epd->print(pet.getMood());
        y += lineH;

        _epd->setCursor(labelX, y);
        _epd->print("Estado:");
        _epd->setCursor(valueX, y);
        _epd->print(pet.getState());
        y += lineH;
    }

    // Historico permanente (nao reseta)
    _epd->setCursor(labelX, y);
    _epd->print("Perdidos:");
    _epd->setCursor(valueX, y);
    _epd->print(pet.getLostCount());
    y += lineH;

    _epd->setCursor(labelX, y);
    _epd->print("Vitorias:");
    _epd->setCursor(valueX, y);
    _epd->print(pet.getWinCount());
    y += lineH;

    _epd->setCursor(labelX, y);
    _epd->print("V.curta:");
    _epd->setCursor(valueX, y);
    if (pet.getShortestLife() > 0) {
        formatAge(buf, sizeof(buf), pet.getShortestLife());
        _epd->print(buf);
    } else {
        _epd->print("--");
    }
    y += lineH;

    _epd->setCursor(labelX, y);
    _epd->print("V.longa:");
    _epd->setCursor(valueX, y);
    if (pet.getLongestLife() > 0) {
        formatAge(buf, sizeof(buf), pet.getLongestLife());
        _epd->print(buf);
    } else {
        _epd->print("--");
    }

    refresh();
}

// ============================================================
// Relogio em tempo real: atualiza somente a regiao dos digitos
// (refresh parcial), sem reconstruir a tela inteira.
// ============================================================
void EPDisplay::drawClockTick(time_t now) {
#if CLOCK_PARTIAL_UPDATE
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_sec == _lastClockSec) return;
    _lastClockSec = tm.tm_sec;

    char clock[9];
    snprintf(clock, sizeof(clock), "%02d:%02d:%02d",
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Apaga os digitos antigos e desenha os novos (mesmo cursor do drawStats)
    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->fillRect(_clockX, _clockY, _clockW, _clockH, GxEPD_WHITE);
    _epd->setCursor(centerX(8 * 11), 48);
    _epd->print(clock);

#if GRAY_MODE
    _gray.clearRegion(_clockX, _clockY, _clockW, _clockH);
    _gray.overlay1bppRect(_epd->uiBuffer(), _clockX, _clockY, _clockW, _clockH);
    _gray.pushWindow(_clockX, _clockY, _clockW, _clockH);
#else
    _epd->displayWindow(_clockX, _clockY, _clockW, _clockH);
#endif
#endif
}

// ============================================================
// Animação de evolução
// ============================================================
void EPDisplay::drawEvolution(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    const char* title = pet.getStage() == STAGE_SCYTHER ? "OVO CHOCANDO!" : "EVOLUINDO!";
    _epd->setCursor(centerX(strlen(title) * 14), 30);
    _epd->print(title);

    // Mostra sprite do estágio atual
    int16_t sw, sh;
    getSpriteSize(pet, sw, sh);
#if GRAY_MODE
    setGraySprite(pet.getCurrentGraySprite(), centerX(sw), centerY(sh), sw, sh);
#else
    drawSprite(centerX(sw), centerY(sh),
               pet.getCurrentSprite(), sw, sh, GxEPD_BLACK);
#endif

    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->setCursor(centerX(100), DISPLAY_HEIGHT - 20);
    _epd->print(pet.getStageName());

    refresh();
}

// ============================================================
// Tela de alerta
// ============================================================
void EPDisplay::drawWarning(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(centerX(strlen("!! CUIDADO !!") * 14), 30);
    _epd->print("!! CUIDADO !!");

    int y = 60;
    _epd->setFont(&FreeMonoBold9pt8b);

    if (pet.isEgg()) {
        _epd->setCursor(10, y);
        _epd->print("Ovo esta frio!");
        y += 20;
        _epd->setCursor(10, y);
        _epd->print("Aque\xE7""a o ovo!");
        y += 20;
    } else {
        if (pet.getHunger() <= 0) {
            _epd->setCursor(10, y);
            _epd->print("Fome critica!");
            y += 20;
        }
        if (pet.getHappiness() <= 0) {
            _epd->setCursor(10, y);
            _epd->print("Felicidade critica!");
            y += 20;
        }
        if (pet.getHealth() < 20) {
            _epd->setCursor(10, y);
            _epd->print("Saude baixa!");
            y += 20;
        }
    }

    // Sprite
    int16_t sw, sh;
    getSpriteSize(pet, sw, sh);
#if GRAY_MODE
    setGraySprite(pet.getCurrentGraySprite(), centerX(sw), DISPLAY_HEIGHT - sh - 20,
                  sw, sh);
#else
    drawSprite(centerX(sw), DISPLAY_HEIGHT - sh - 20,
               pet.getCurrentSprite(), sw, sh, GxEPD_BLACK);
#endif

    refresh();
}

// ============================================================
// Tela de dormindo
// ============================================================
void EPDisplay::drawSleeping() {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(centerX(strlen("Zzzzzz...") * 14), centerY(10));
    _epd->print("Zzzzzz...");

    refresh();
}

// ============================================================
// Tela de morto
// ============================================================
void EPDisplay::drawDead(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(centerX(strlen("--- SEM VIDAS ---") * 14), 30);
    _epd->print("--- SEM VIDAS ---");

    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->setCursor(centerX(120), 60);
    _epd->print("Seu Pokemon morreu.");

    _epd->setCursor(centerX(140), 85);
    _epd->print("Segure Status");
    _epd->setCursor(centerX(120), 105);
    _epd->print("para recomecar.");

    // Sprite triste
#if GRAY_MODE
    setGraySprite(SCYTHER_GRAY_IDLE, centerX(SCYTHER_SPRITE_W), 120,
                  SCYTHER_SPRITE_W, SCYTHER_SPRITE_H);
#else
    drawSprite(centerX(SCYTHER_SPRITE_W), 120,
               SCYTHER_SAD, SCYTHER_SPRITE_W, SCYTHER_SPRITE_H, GxEPD_BLACK);
#endif

    refresh();
}

// ============================================================
// Mensagem genérica
// ============================================================
void EPDisplay::showMessage(const char* line1, const char* line2, const char* line3) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    if (line1) {
        _epd->setCursor(centerX(strlen(line1) * 14), 40);
        _epd->print(line1);
    }
    if (line2) {
        _epd->setCursor(centerX(strlen(line2) * 14), 70);
        _epd->print(line2);
    }
    if (line3) {
        _epd->setCursor(centerX(strlen(line3) * 14), 100);
        _epd->print(line3);
    }

    refresh();
}

void EPDisplay::refresh() {
    _epd->setFullWindow();
#if GRAY_MODE
    _gray.clear();
    _gray.overlay1bpp(_epd->uiBuffer(), DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (_graySpriteSet) {
        _gray.blit2bpp(_grayX, _grayY, _graySprite, _grayW, _grayH);
        _graySpriteSet = false;
    }
    _gray.push();
#else
    _epd->display();
#endif
}

void EPDisplay::powerOff() {
#if GRAY_MODE
    _gray.powerOff();
#else
    _epd->powerOff();
#endif
}

void EPDisplay::powerOn() {
#if GRAY_MODE
    _gray.begin();
#else
    _epd->init();
#endif
}
