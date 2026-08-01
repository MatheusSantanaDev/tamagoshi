#ifndef EPDISPLAY_H
#define EPDISPLAY_H

#include <Arduino.h>
#include "config.h"
#include "Pokemon.h"
#include "EPGray.h"

// ============================================================
// Acesso ao framebuffer 1bpp do GxEPD2_BW (privado). Em GRAY_MODE,
// a UI é desenhada nesse buffer e depois composta sobre o framebuffer
// 4-gray (texto/barras viram preto; sprites ficam em 2bpp).
// Hack: private->protected apenas neste header, para expor _buffer.
// ============================================================
#define private protected
#include <GxEPD2_BW.h>
#undef private
#include <GxEPD2_3C.h>

// ============================================================
// CONFIG DO DISPLAY: WeAct 3.7" (240x416, UC8253)
// ============================================================
#define DISPLAY_CLASS GxEPD2_370_GDEY037T03
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 416

typedef GxEPD2_BW<DISPLAY_CLASS, DISPLAY_HEIGHT> DisplayBase;

// Expõe o buffer 1bpp (row-major, bit7 = pixel mais à esquerda)
class UiDisplay : public DisplayBase {
public:
    using DisplayBase::DisplayBase;
    const uint8_t* uiBuffer() const { return _buffer; }
};

typedef UiDisplay DisplayType;

class EPDisplay {
public:
    EPDisplay();
    void begin();

    // Telas principais
    void drawPet(const Pokemon& pet);
    void drawStats(const Pokemon& pet, time_t now);
    void drawEvolution(const Pokemon& pet);
    void drawWarning(const Pokemon& pet);
    void drawSleeping();
    void drawDead(const Pokemon& pet);

    // Tela do pet: atualiza apenas barras/cocos/idade que mudaram
    // (refresh parcial por regiao, sem reconstruir a tela inteira)
    void drawPetUpdates(const Pokemon& pet);

    // Atualiza apenas os digitos do relogio da tela de stats
    // (refresh parcial da regiao, sem reconstruir a tela inteira)
    void drawClockTick(time_t now);

    // Utilitários
    void drawSprite(int16_t x, int16_t y, const unsigned char* bitmap,
                    int16_t w, int16_t h, uint16_t color);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                         int value, int maxVal, uint16_t barColor);
    void showMessage(const char* line1, const char* line2, const char* line3);

    // Força refresh completo
    void refresh();

    // Power save
    void powerOff();
    void powerOn();

private:
    DisplayType* _epd;
    EPGray _gray;

    // Sprite 2bpp do pet, posicionado no framebuffer gray
    void setGraySprite(const uint8_t* data, int16_t x, int16_t y,
                       int16_t w, int16_t h);

    // Slot do sprite para compor no refresh (fb é limpo antes)
    const uint8_t* _graySprite;
    int16_t _grayX, _grayY, _grayW, _grayH;
    bool _graySpriteSet;

    // Regiao do relogio na tela de status (refresh parcial)
    int _clockX, _clockY, _clockW, _clockH;
    int _lastClockSec;

    void initDisplay();
    int centerX(int width) const;
    int centerY(int height) const;
    void getSpriteSize(const Pokemon& pet, int16_t& w, int16_t& h) const;

    // Tela do pet
    void drawBarRow(int row, const char* label, int value, int maxVal);
    void drawPoops(const Pokemon& pet);
    void snapshotPet(const Pokemon& pet);

    // Refreshes parciais
    void pushPartialRegion(int16_t x, int16_t y, int16_t w, int16_t h);
    void updateBarPartial(int row, const char* label, int value, int maxVal);
    void updatePoopsPartial(const Pokemon& pet);
    void updateLvlPartial(const Pokemon& pet);
    void updateMsgPartial(const Pokemon& pet);
    // Caixa exata dos digitos de "lvl N" (o prefixo "lvl " nao muda)
    void lvlNumberBounds(int lvl, int16_t& tx0, int16_t& ty0,
                         uint16_t& tw, uint16_t& th);

    // Snapshot da tela do pet (para saber o que mudou)
    int _lastPetLvl;
    int _lastPetBars[6];
    int _lastPetDirt;
    const unsigned char* _lastPetSprite;
    char _lastPetMsg[24];

    // Caixa do texto "lvl N" (refresh parcial justo, sem piscar area maior)
    int _lastLvlX = 0, _lastLvlW = 0;
};

#endif
