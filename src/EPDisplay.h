#ifndef EPDISPLAY_H
#define EPDISPLAY_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "config.h"
#include "EPGray.h"
#include "Pokemon.h"

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

    // Modo dormir: desenha "Zzz" perto da cabeca do sprite no drawPet.
    // O main controla: liga na fase "pokemon" do sono, desliga ao acordar.
    void setSleepZzz(bool on) { _sleepZzz = on; }

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

    // Refreshes parciais. O desenho (buffer) acontece no loop principal;
    // o push (lento, varios segundos) roda numa task separada, entao o
    // loop nunca fica bloqueado esperando o painel.
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
    int _lastPetBars[5];
    int _lastPetDirt;
    const unsigned char* _lastPetSprite;
    char _lastPetMsg[24];

    // Caixa do texto "lvl N" (refresh parcial justo, sem piscar area maior)
    int _lastLvlX = 0, _lastLvlW = 0;

    // Fila de jobs do display: o loop principal enfileira regioes (e
    // desenhos completos); a task de display drena e faz os pushes lentos.
    // O conteudo e lido do framebuffer AO PUSHAR, entao re-enfileirar a
    // mesma regiao e um no-op (a regiao pendente ja reflete o estado atual).
    struct DisplayJob {
        int16_t x, y, w, h;
    };
    static const int DISPLAY_JOB_MAX = 24;
    DisplayJob _jobs[DISPLAY_JOB_MAX];
    volatile int _jobHead = 0, _jobTail = 0;
    volatile bool _fullPending = false;
    SemaphoreHandle_t _jobSem = nullptr;
    TaskHandle_t _taskHandle = nullptr;
    void enqueueRegion(int16_t x, int16_t y, int16_t w, int16_t h);
    void enqueueFull();
    void displayTaskLoop();
    static void displayTaskEntry(void* param);

    // "Zzz" perto da cabeca do sprite (modo dormir, fase pokemon)
    bool _sleepZzz = false;

    // Caixa do texto "Zzz" no buffer 1bpp: o sprite cinza e opaco e cobre
    // o texto, entao o refresh() reaplica a regiao depois do blit2bpp
    int _zzzX = 0, _zzzY = 0, _zzzW = 0, _zzzH = 0;
};

#endif
