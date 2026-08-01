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
      _grayW(0), _grayH(0), _graySpriteSet(false) {}

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
void EPDisplay::drawStats(const Pokemon& pet, time_t now) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    _epd->setCursor(centerX(120), 25);
    _epd->print("== STATUS ==");

    // Relogio real no canto superior direito (se NTP sincronizado)
    if (now > 0) {
        struct tm tm;
        localtime_r(&now, &tm);
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        _epd->setFont(&FreeMonoBold9pt8b);
        _epd->setCursor(DISPLAY_WIDTH - 72, 25);
        _epd->print(buf);
    }

    _epd->setFont(&FreeMonoBold9pt8b);
    int y = 55;

    // Nome
    _epd->setCursor(10, y);
    _epd->print("Pokemon: ");
    _epd->print(pet.getStageName());
    y += 20;

    // Idade
    _epd->setCursor(10, y);
    _epd->printf("Idade: %d min", pet.getAge());
    y += 25;

    if (pet.isEgg()) {
        // === OVO ===
        _epd->setCursor(10, y);
        _epd->print("Calor:");
        drawProgressBar(90, y - 10, DISPLAY_WIDTH - 100, BAR_H,
                        pet.getWarmth(), STATS_MAX, GxEPD_BLACK);
        _epd->setCursor(DISPLAY_WIDTH - 30, y);
        _epd->printf("%d", pet.getWarmth());
        y += 22;

        _epd->setCursor(10, y);
        _epd->print("Choco:");
        drawProgressBar(90, y - 10, DISPLAY_WIDTH - 100, BAR_H,
                        pet.getIncubationProgress(), HATCH_TIME_MINUTES, GxEPD_BLACK);
        _epd->setCursor(DISPLAY_WIDTH - 30, y);
        _epd->printf("%d", pet.getIncubationProgress());
        y += 30;

        _epd->setCursor(10, y);
        _epd->print("Btn1=Aquecer");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn2=Ajudar");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn3=Status");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn3(L)=Reset");
    } else {
        // === POKEMON NORMAL ===
        _epd->setCursor(10, y);
        _epd->print("Fome:");
        drawProgressBar(90, y - 10, DISPLAY_WIDTH - 100, BAR_H,
                        pet.getHunger(), STATS_MAX, GxEPD_BLACK);
        _epd->setCursor(DISPLAY_WIDTH - 30, y);
        _epd->printf("%d", pet.getHunger());
        y += 22;

        _epd->setCursor(10, y);
        _epd->print("Fel.:");
        drawProgressBar(90, y - 10, DISPLAY_WIDTH - 100, BAR_H,
                        pet.getHappiness(), STATS_MAX, GxEPD_BLACK);
        _epd->setCursor(DISPLAY_WIDTH - 30, y);
        _epd->printf("%d", pet.getHappiness());
        y += 22;

        _epd->setCursor(10, y);
        _epd->print("Saud.:");
        drawProgressBar(90, y - 10, DISPLAY_WIDTH - 100, BAR_H,
                        pet.getHealth(), STATS_MAX, GxEPD_BLACK);
        _epd->setCursor(DISPLAY_WIDTH - 30, y);
        _epd->printf("%d", pet.getHealth());
        y += 30;

        _epd->setCursor(10, y);
        _epd->print("Btn1=Alimentar");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn2=Brincar");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn3=Status");
        y += 15;
        _epd->setCursor(10, y);
        _epd->print("Btn3(L)=Reset");
    }

    refresh();
}

// ============================================================
// Animação de evolução
// ============================================================
void EPDisplay::drawEvolution(const Pokemon& pet) {
    _epd->setFullWindow();
    _epd->fillScreen(GxEPD_WHITE);

    _epd->setFont(&FreeMonoBold12pt7b);
    const char* title = pet.getStage() == STAGE_SCYTHER ? "OVO CHOCANDO!" : "EVOLUINDO!";
    _epd->setCursor(centerX(strlen(title) * 8), 30);
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
    _epd->setCursor(centerX(140), 30);
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
    _epd->setCursor(centerX(160), centerY(10));
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
    _epd->setCursor(centerX(140), 30);
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
        _epd->setCursor(centerX(strlen(line1) * 8), 40);
        _epd->print(line1);
    }
    if (line2) {
        _epd->setCursor(centerX(strlen(line2) * 8), 70);
        _epd->print(line2);
    }
    if (line3) {
        _epd->setCursor(centerX(strlen(line3) * 8), 100);
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
