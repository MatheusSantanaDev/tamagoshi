#ifndef EPGRAY_H
#define EPGRAY_H

#include <Arduino.h>
#include <SPI.h>

// ============================================================
// Driver 4-gray do UC8253 (GDEY037T03) + framebuffer 2bpp.
// Sequencia do sample oficial Good Display A32-GDEY037T03-FP4G:
//   0x00=0x1F (BWOTP), PON(0x04), 0xE0=0x02, 0xE5=0x5A (4 gray)
//   Dados: plano 0x10 (old) + 0x13 (new), refresh 0x12
// Formato do framebuffer: 2 bits/pixel, 4 pixels/byte, MSB first.
//   0b11 = branco | 0b10 = cinza claro | 0b01 = cinza escuro | 0b00 = preto
// ============================================================
#define GRAY_EP_CS    5
#define GRAY_EP_DC    17
#define GRAY_EP_RST   16
#define GRAY_EP_BUSY  4
#define GRAY_EP_SCLK  18
#define GRAY_EP_MOSI  23

#define GRAY_W 240
#define GRAY_H 416
#define GRAY_FB_BYTES   (GRAY_W * GRAY_H / 4)   // 24960
#define GRAY_PLANE_BYTES (GRAY_W * GRAY_H / 8)  // 12480

class EPGray {
public:
    void begin();
    void clear();

    // Sprite 2bpp (w deve ser multiplo de 4)
    void blit2bpp(int16_t x, int16_t y, const uint8_t* data,
                  int16_t w, int16_t h);

    // Overlay 1bpp row-major (bit=1 -> preto), full screen
    void overlay1bpp(const uint8_t* data, int16_t w, int16_t h);

    // Regiao da tela como branco (0b11) no framebuffer
    void clearRegion(int16_t x, int16_t y, int16_t w, int16_t h);

    // Overlay 1bpp apenas em uma regiao (para refresh parcial)
    void overlay1bppRect(const uint8_t* data, int16_t x, int16_t y,
                         int16_t w, int16_t h);

    // Envia fb -> display (init + refresh completo + power off)
    void push();

    // Refresh parcial de uma janela (relogio, sem reconstruir a tela)
    void pushWindow(int16_t x, int16_t y, int16_t w, int16_t h);

    void powerOff();

private:
    uint8_t _fb[GRAY_FB_BYTES];     // Framebuffer 2bpp persistente
    uint8_t _plane[GRAY_PLANE_BYTES];  // Plano "old" (MSBs) p/ envio
    uint8_t _plane2[GRAY_PLANE_BYTES]; // Plano "new" (LSBs) p/ envio

    void sendCmd(uint8_t c);
    void sendData(const uint8_t* d, size_t n);
    void sendData(uint8_t d) { sendData(&d, 1); }
    bool waitBusy(const char* step);
    void init4G();
};

#endif
