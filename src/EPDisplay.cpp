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
static void formatAge(char* buf, size_t n, int minutes);  // definida abaixo

// Barras da tela do pet: linha i (0=baixo, 5=topo).
// PET_BAR_X e PET_BAR_W multiplos de 8: a janela parcial (byte-alinhada a
// 8px) cobre exatamente a barra, sem margem sobrando de lado (evita linha
// de ghosting no canto inferior).
#define PET_BAR_LABEL_X 10
#define PET_BAR_X       72
#define PET_BAR_W       (DISPLAY_WIDTH - PET_BAR_X - 8)
#define PET_BAR_H       9
#define PET_BAR_GAP     16
#define PET_BARS_BOTTOM (DISPLAY_HEIGHT - 8)

static int petBarTop(int row) {
    return PET_BARS_BOTTOM - PET_BAR_H - row * PET_BAR_GAP;
}

// Base (y do rodapé) do sprite na tela principal: o sprite cresce para cima
#define SPRITE_BOTTOM 272

// Icone de coco (1bpp, bit=1 = preto)
#define POOP_ICON_W 12
#define POOP_ICON_H 9
static const unsigned char POOP_ICON[] = {
    0x3C, 0x00,
    0x7E, 0x00,
    0xFF, 0x00,
    0xFF, 0x00,
    0x7E, 0x00,
    0x3C, 0x00,
    0x3C, 0x00,
    0x3C, 0x00,
    0x3C, 0x00
};

EPDisplay::EPDisplay()
    : _epd(nullptr), _graySprite(nullptr), _grayX(0), _grayY(0),
      _grayW(0), _grayH(0), _graySpriteSet(false),
      _clockX(72), _clockY(36), _clockW(96), _clockH(18),
      _lastClockSec(-1), _lastPetLvl(-1), _lastPetDirt(-1),
      _lastPetSprite(nullptr) {
    memset(_lastPetBars, -1, sizeof(_lastPetBars));
    _lastPetMsg[0] = '\0';
}

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

// Mensagem de status do pet (usada no desenho e no snapshot)
static const char* petStatusMessage(const Pokemon& pet) {
    if (pet.isEgg()) {
        if (pet.getWarmth() < WARMTH_SLOW_MIN) {
            return "Ovo esta frio!";
        } else if (pet.getWarmth() >= WARMTH_FAST_MIN) {
            return "Aquecido! Quase la!";
        }
        return "Aque\xE7""a o ovo!";
    }
    if (pet.isDead()) {
        return "SEM VIDA...";
    }
    if (pet.isCritical()) {
        return "PRECISA DE CUIDADOS!";
    }
    return "Feliz e saudavel!";
}

void EPDisplay::drawBarRow(int row, const char* label, int value, int maxVal) {
    int top = petBarTop(row);
    _epd->setCursor(PET_BAR_LABEL_X, top + 9);
    _epd->print(label);
    drawProgressBar(PET_BAR_X, top, PET_BAR_W, PET_BAR_H,
                    value, maxVal, GxEPD_BLACK);
}

void EPDisplay::drawPoops(const Pokemon& pet) {
    int level = pet.getDirt() / DIRT_LEVEL_STEP;   // 0..4
    if (level <= 0) return;

    int16_t sw, sh;
    getSpriteSize(pet, sw, sh);
    int spriteX = (DISPLAY_WIDTH - sw) / 2;

    for (int k = 0; k < level; k++) {
        bool left = (k % 2 == 0);
        int row = k / 2;
        int x = left ? spriteX - POOP_ICON_W - 6 : spriteX + sw + 6;
        int y = SPRITE_BOTTOM - POOP_ICON_H - row * (POOP_ICON_H + 2);
        if (x >= 0 && x + POOP_ICON_W <= DISPLAY_WIDTH) {
            drawSprite(x, y, POOP_ICON, POOP_ICON_W, POOP_ICON_H, GxEPD_BLACK);
        }
    }
}

// Regioes (esq/dir) dos cocos ao lado do sprite
void EPDisplay::snapshotPet(const Pokemon& pet) {
    _lastPetLvl = pet.getLvl();
    _lastPetSprite = pet.getCurrentSprite();
    _lastPetDirt = pet.getDirt() / DIRT_LEVEL_STEP;
    strncpy(_lastPetMsg, petStatusMessage(pet), sizeof(_lastPetMsg) - 1);
    _lastPetMsg[sizeof(_lastPetMsg) - 1] = '\0';

    if (pet.isEgg()) {
        _lastPetBars[0] = pet.getWarmth();
        _lastPetBars[1] = -1;
        _lastPetBars[2] = -1;
        _lastPetBars[3] = -1;
        _lastPetBars[4] = -1;
        _lastPetBars[5] = -1;
    } else {
        _lastPetBars[0] = pet.getHappiness();
        _lastPetBars[1] = pet.getHunger();
        _lastPetBars[2] = pet.getEnergy();
        _lastPetBars[3] = pet.getHealth();
        _lastPetBars[4] = pet.getSleep();
        _lastPetBars[5] = pet.getHygiene();
    }

    // Caixa exata dos digitos do "lvl N" (o prefixo "lvl " nunca muda; so
    // o numero e atualizado no refresh parcial)
    int16_t tx0, ty0;
    uint16_t tw, th;
    lvlNumberBounds(pet.getLvl(), tx0, ty0, tw, th);
    _lastLvlX = tx0;
    _lastLvlW = tw;
}

void EPDisplay::drawPet(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    // Nome e estágio no topo
    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(10, 18);
    _epd->print(pet.getStageName());

    // Nivel (lvl = tempo de vida do estagio, atualiza conforme avanca)
    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->setCursor(10, 34);
    _epd->printf("lvl %d", pet.getLvl());

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

    // Cocos no chao ao lado do sprite
    if (!pet.isEgg() && !pet.isDead()) {
        drawPoops(pet);
    }

    // Mensagem em posicao fixa (logo abaixo da base do sprite grande)
    _epd->setFont(&FreeMonoBold9pt8b);
    const char* msg = petStatusMessage(pet);
    int16_t tx0, ty0;
    uint16_t tw, th;
    _epd->getTextBounds(msg, 0, 0, &tx0, &ty0, &tw, &th);
    _epd->setCursor((DISPLAY_WIDTH - tw) / 2, SPRITE_BOTTOM + 18 - ty0);
    _epd->print(msg);

    // Barras ancoradas no extremo inferior.
    // Linha i: label em PET_BAR_LABEL_X, barra em PET_BAR_X.
    if (pet.isEgg()) {
        // === MODO OVO: apenas Calor (incubacao e interna) ===
        drawBarRow(0, "Calor", pet.getWarmth(), STATS_MAX);
    } else {
        // === MODO POKEMON NORMAL ===
        drawBarRow(5, "Fel.", pet.getHappiness(), STATS_MAX);
        drawBarRow(4, "Fome", pet.getHunger(), STATS_MAX);
        drawBarRow(3, "Ener.", pet.getEnergy(), STATS_MAX);
        drawBarRow(2, "Sau.", pet.getHealth(), STATS_MAX);
        drawBarRow(1, "Sono", pet.getSleep(), STATS_MAX);
        drawBarRow(0, "Hig.", pet.getHygiene(), STATS_MAX);
    }

    snapshotPet(pet);

    refresh(); // full refresh
}

// ============================================================
// Atualizacao parcial da tela do pet (barras, cocos e idade)
// ============================================================
void EPDisplay::pushPartialRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
#if GRAY_MODE
    _gray.clearRegion(x, y, w, h);
    _gray.overlay1bppRect(_epd->uiBuffer(), x, y, w, h);
    _gray.pushWindow(x, y, w, h);
#else
    _epd->displayWindow(x, y, w, h);
#endif
}

void EPDisplay::updateBarPartial(int row, const char* label, int value, int maxVal) {
    (void)label; // o label nao muda, nao precisa redesenhar
    int top = petBarTop(row);

    // Reescreve o interior inteiro (preenchimento todo) de uma vez: atualizar
    // so o pedacinho que mudou deixa residuos (ghosting) no e-paper.
    int newFillW = map(constrain(value, 0, maxVal), 0, maxVal, 0, PET_BAR_W - 2);

    _epd->fillRect(PET_BAR_X + 1, top + 1, PET_BAR_W - 2, PET_BAR_H - 2, GxEPD_WHITE);
    if (newFillW > 0) {
        _epd->fillRect(PET_BAR_X + 1, top + 1, newFillW, PET_BAR_H - 2, GxEPD_BLACK);
    }

    // Push da barra INTEIRA (borda + interior): a area recarregada coincide
    // exatamente com a barra.
    pushPartialRegion(PET_BAR_X, top, PET_BAR_W, PET_BAR_H);
    _lastPetBars[row] = value;
}

void EPDisplay::updateLvlPartial(const Pokemon& pet) {
    char lvlBuf[16];
    snprintf(lvlBuf, sizeof(lvlBuf), "lvl %d", pet.getLvl());

    int16_t tx0, ty0;
    uint16_t tw, th;
    lvlNumberBounds(pet.getLvl(), tx0, ty0, tw, th);

    // Limpa apenas a uniao das caixas dos digitos (antiga x nova): o
    // prefixo "lvl " nunca e tocado.
    int cx = _lastLvlX < 0 ? tx0 : (tx0 < _lastLvlX ? tx0 : _lastLvlX);
    int cw = tw > _lastLvlW ? (int)tw : _lastLvlW;
    _epd->fillRect(cx, ty0, cw, th, GxEPD_WHITE);
    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->setCursor(10, 34);
    _epd->print(lvlBuf);
    pushPartialRegion(cx, ty0, cw, th);

    _lastPetLvl = pet.getLvl();
    _lastLvlX = tx0;
    _lastLvlW = tw;
}

// Troca de frase (ex.: "Ovo esta frio!") com refresh parcial: so a regiao
// centrada em SPRITE_BOTTOM+18 e atualizada, sem rebuild da pagina.
void EPDisplay::updateMsgPartial(const Pokemon& pet) {
    const char* msg = petStatusMessage(pet);
    _epd->setFont(&FreeMonoBold9pt8b);

    // Mesma posicao do drawPet: caixa centrada na linha SPRITE_BOTTOM+18.
    // O topo dos glifos fica em cy + ty0 = SPRITE_BOTTOM + 18 (ty0 < 0).
    int16_t tx0, ty0;
    uint16_t tw, th;
    _epd->getTextBounds(msg, 0, 0, &tx0, &ty0, &tw, &th);
    int cx = (DISPLAY_WIDTH - tw) / 2;
    int cy = SPRITE_BOTTOM + 18 - ty0;

    // Uniao com a caixa da mensagem anterior (ambas centradas na mesma base)
    int16_t ox0, oy0;
    uint16_t ow, oh;
    _epd->getTextBounds(_lastPetMsg, 0, 0, &ox0, &oy0, &ow, &oh);
    int ocx = (DISPLAY_WIDTH - ow) / 2;

    int ux = cx < ocx ? cx : ocx;
    int uw = tw > ow ? (int)tw : (int)ow;
    int uy = cy + ty0;             // = SPRITE_BOTTOM + 18 (mesmo para as duas)
    int uh = th > oh ? (int)th : (int)oh;

    _epd->fillRect(ux, uy, uw, uh, GxEPD_WHITE);
    _epd->setCursor(cx, cy);
    _epd->print(msg);
    pushPartialRegion(ux, uy, uw, uh);

    strncpy(_lastPetMsg, msg, sizeof(_lastPetMsg) - 1);
    _lastPetMsg[sizeof(_lastPetMsg) - 1] = '\0';
}

// Caixa exata dos digitos de "lvl N", desenhado em (10, 34) com a fonte
// FreeMono (monospace, avancos iguais). A altura cobre qualquer digito
// (base sempre em 34+1), entao a uniao antigo/novo nao deixa residuo.
void EPDisplay::lvlNumberBounds(int lvl, int16_t& tx0, int16_t& ty0,
                                uint16_t& tw, uint16_t& th) {
    // Avanco monospace: largura de "00" menos a de "0"
    int16_t x0, y0;
    uint16_t w0, w1, h0, h1;
    _epd->setFont(&FreeMonoBold9pt8b);
    _epd->getTextBounds("0", 10, 34, &x0, &y0, &w0, &h0);
    _epd->getTextBounds("00", 10, 34, &x0, &y0, &w1, &h1);
    int advance = (int)w1 - (int)w0;

    // O numero comeca apos o prefixo fixo "lvl " (4 avancos)
    char num[8];
    snprintf(num, sizeof(num), "%d", lvl);
    int16_t nx0, ny0;
    uint16_t nw, nh;
    _epd->getTextBounds(num, 10 + 4 * advance, 34, &nx0, &ny0, &nw, &nh);

    tx0 = nx0;
    ty0 = 34 - 11;
    tw = nw;
    th = 12;
}

void EPDisplay::updatePoopsPartial(const Pokemon& pet) {
    int16_t sw, sh;
    getSpriteSize(pet, sw, sh);
    int spriteX = (DISPLAY_WIDTH - sw) / 2;

    // Regioes esq/dir (duas linhas de coco possiveis)
    int leftX = spriteX - POOP_ICON_W - 8;
    int rightX = spriteX + sw + 4;
    int regY = SPRITE_BOTTOM - POOP_ICON_H - (POOP_ICON_H + 2) - 2;
    int regH = (POOP_ICON_H + 2) * 2 + 4;
    int regW = POOP_ICON_W + 4;

    // Apaga as regioes no buffer 1bpp
    if (leftX >= 0) {
        _epd->fillRect(leftX, regY, regW, regH, GxEPD_WHITE);
    }
    if (rightX + regW <= DISPLAY_WIDTH) {
        _epd->fillRect(rightX, regY, regW, regH, GxEPD_WHITE);
    }

    // Redesenha os cocos atuais
    drawPoops(pet);

    // Push parcial de cada regiao
    if (leftX >= 0) {
        pushPartialRegion(leftX, regY, regW, regH);
    }
    if (rightX + regW <= DISPLAY_WIDTH) {
        pushPartialRegion(rightX, regY, regW, regH);
    }
    _lastPetDirt = pet.getDirt() / DIRT_LEVEL_STEP;
}

void EPDisplay::drawPetUpdates(const Pokemon& pet) {
    // Sprite mudou? -> redesenho completo. (No ovo o sprite so muda ao
    // chocar; esquentar/esfriar so troca a frase, abaixo.)
    if (pet.getCurrentSprite() != _lastPetSprite) {
        drawPet(pet);
        return;
    }

    // Frase mudou (sem mudar o sprite)? -> refresh parcial so da frase
    if (strcmp(petStatusMessage(pet), _lastPetMsg) != 0) {
        updateMsgPartial(pet);
    }

    // Nivel (lvl = tempo de vida do estagio; no ovo pausa quando esfria,
    // entao so redesenha quando o lvl realmente avanca)
    if (pet.getLvl() != _lastPetLvl) {
        updateLvlPartial(pet);
    }

    // Barras
    if (pet.isEgg()) {
        if (pet.getWarmth() != _lastPetBars[0]) {
            updateBarPartial(0, "Calor", pet.getWarmth(), STATS_MAX);
        }
    } else {
        if (pet.getHappiness() != _lastPetBars[0]) {
            updateBarPartial(5, "Fel.", pet.getHappiness(), STATS_MAX);
        }
        if (pet.getHunger() != _lastPetBars[1]) {
            updateBarPartial(4, "Fome", pet.getHunger(), STATS_MAX);
        }
        if (pet.getEnergy() != _lastPetBars[2]) {
            updateBarPartial(3, "Ener.", pet.getEnergy(), STATS_MAX);
        }
        if (pet.getHealth() != _lastPetBars[3]) {
            updateBarPartial(2, "Sau.", pet.getHealth(), STATS_MAX);
        }
        if (pet.getSleep() != _lastPetBars[4]) {
            updateBarPartial(1, "Sono", pet.getSleep(), STATS_MAX);
        }
        if (pet.getHygiene() != _lastPetBars[5]) {
            updateBarPartial(0, "Hig.", pet.getHygiene(), STATS_MAX);
        }
    }

    // Cocos
    if (!pet.isEgg() && !pet.isDead() &&
        pet.getDirt() / DIRT_LEVEL_STEP != _lastPetDirt) {
        updatePoopsPartial(pet);
    }
}

// ============================================================
// Tela de status detalhado
// ============================================================

// Formata idade em minutos: "5min", "2h30min" ou "1d2h"
static void formatAge(char* buf, size_t n, int minutes) {
    if (minutes < 60) {
        snprintf(buf, n, "%dmin", minutes);
    } else if (minutes < 24 * 60) {
        snprintf(buf, n, "%dh%02dmin", minutes / 60, minutes % 60);
    } else {
        snprintf(buf, n, "%dd%dh", minutes / (24 * 60), (minutes % (24 * 60)) / 60);
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
        // Ovo: calor em texto (sem barras)
        _epd->setCursor(labelX, y);
        _epd->print("Calor:");
        _epd->setCursor(valueX, y);
        _epd->printf("%d/%d", pet.getWarmth(), STATS_MAX);
        y += lineH;
    } else {
        // Humor (as barras da tela do pet ja mostram os stats)
        _epd->setCursor(labelX, y);
        _epd->print("Humor:");
        _epd->setCursor(valueX, y);
        _epd->print(pet.getMood());
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

    pushPartialRegion(_clockX, _clockY, _clockW, _clockH);
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
