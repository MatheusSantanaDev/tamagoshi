#include "EPGray.h"
#include <esp_task_wdt.h>

void EPGray::sendCmd(uint8_t c) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(GRAY_EP_DC, LOW);
    digitalWrite(GRAY_EP_CS, LOW);
    SPI.transfer(c);
    digitalWrite(GRAY_EP_CS, HIGH);
    digitalWrite(GRAY_EP_DC, HIGH);
    SPI.endTransaction();
}

void EPGray::sendData(const uint8_t* d, size_t n) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(GRAY_EP_DC, HIGH);
    digitalWrite(GRAY_EP_CS, LOW);
    if (n == 1) {
        SPI.transfer(*d);
    } else {
        SPI.transferBytes((uint8_t*)d, nullptr, n);
    }
    digitalWrite(GRAY_EP_CS, HIGH);
    SPI.endTransaction();
}

bool EPGray::waitBusy(const char* step) {
    uint32_t start = millis();
    while (digitalRead(GRAY_EP_BUSY) == LOW) {   // busy = LOW (igual GxEPD2)
        // Refreshes do painel 4-gray passam de 5s (limite do watchdog do
        // loop): alimenta o watchdog para o refresh lento nunca virar um
        // reset (que gerava boot-loop e travava os inputs).
        esp_task_wdt_reset();
        if (millis() - start > 10000) {
            Serial.printf("EPGray: busy timeout (%s)\n", step);
            return false;
        }
        delay(5);
    }
    return true;
}

void EPGray::begin() {
    pinMode(GRAY_EP_CS, OUTPUT);
    pinMode(GRAY_EP_DC, OUTPUT);
    pinMode(GRAY_EP_RST, OUTPUT);
    pinMode(GRAY_EP_BUSY, INPUT);
    digitalWrite(GRAY_EP_CS, HIGH);
    digitalWrite(GRAY_EP_RST, HIGH);
    SPI.begin(GRAY_EP_SCLK, -1, GRAY_EP_MOSI, GRAY_EP_CS);
    clear();
}

void EPGray::clear() {
    memset(_fb, 0xFF, sizeof(_fb));
}

void EPGray::blit2bpp(int16_t x, int16_t y, const uint8_t* data,
                      int16_t w, int16_t h) {
    if (x < 0 || y < 0 || x + w > GRAY_W || y + h > GRAY_H) return;

    if ((x % 4) == 0 && (w % 4) == 0) {
        // Caminho rápido: linhas alinhadas a 4 pixels
        const uint8_t* src = data;
        int rowBytes = w / 4;
        for (int16_t row = 0; row < h; row++) {
            uint8_t* dst = &_fb[((y + row) * GRAY_W + x) / 4];
            memcpy(dst, src, rowBytes);
            src += rowBytes;
        }
        return;
    }

    // Caminho genérico: pixel a pixel
    for (int16_t row = 0; row < h; row++) {
        for (int16_t col = 0; col < w; col++) {
            const uint8_t b = data[row * (w / 4) + col / 4];
            uint8_t v = (b >> (6 - 2 * (col % 4))) & 3;
            uint8_t& f = _fb[((y + row) * GRAY_W + (x + col)) / 4];
            int shift = 6 - 2 * ((x + col) % 4);
            f = (f & ~(3 << shift)) | (v << shift);
        }
    }
}

void EPGray::overlay1bpp(const uint8_t* data, int16_t w, int16_t h) {
    // Buffer do GxEPD2 v1.5.x: bit=1 = BRANCO, bit=0 = PRETO
    // (drawPixel(GxEPD_BLACK) limpa o bit). Escreve preto (0b00)
    // apenas onde o bit=0, deixando o fundo (0b11) intacto.
    int16_t pitch = (w + 7) / 8;
    if (w > GRAY_W || h > GRAY_H) {
        pitch = (GRAY_W + 7) / 8;
        w = GRAY_W;
        h = GRAY_H;
    }
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            if (!(data[y * pitch + x / 8] & (1 << (7 - (x % 8))))) {
                uint8_t& f = _fb[(y * GRAY_W + x) / 4];
                int shift = 6 - 2 * (x % 4);
                f = (f & ~(3 << shift)) | (0 << shift);
            }
        }
    }
}

void EPGray::clearRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t row = y; row < y + h; row++) {
        for (int16_t col = x; col < x + w; col++) {
            if (col < 0 || col >= GRAY_W || row < 0 || row >= GRAY_H) continue;
            uint8_t& f = _fb[(row * GRAY_W + col) / 4];
            int shift = 6 - 2 * (col % 4);
            f = (f & ~(3 << shift)) | (0b11 << shift);
        }
    }
}

void EPGray::overlay1bppRect(const uint8_t* data, int16_t x, int16_t y,
                             int16_t w, int16_t h) {
    int16_t pitch = (GRAY_W + 7) / 8;   // pitch do buffer 1bpp (linha cheia)
    for (int16_t row = 0; row < h; row++) {
        for (int16_t col = 0; col < w; col++) {
            int16_t px = x + col;
            int16_t py = y + row;
            if (px < 0 || px >= GRAY_W || py < 0 || py >= GRAY_H) continue;
            if (!(data[py * pitch + px / 8] & (1 << (7 - (px % 8))))) {
                uint8_t& f = _fb[(py * GRAY_W + px) / 4];
                int shift = 6 - 2 * (px % 4);
                f = (f & ~(3 << shift)) | (0 << shift);
            }
        }
    }
}

void EPGray::init4G() {
    digitalWrite(GRAY_EP_RST, LOW);
    delay(10);
    digitalWrite(GRAY_EP_RST, HIGH);
    delay(10);

    sendCmd(0x00);
    sendData(0x1F);             // PANEL SETTING (BWOTP)
    sendCmd(0x04);              // power on
    waitBusy("pon");
    sendCmd(0xE0);
    sendData(0x02);             // CCSET
    sendCmd(0xE5);
    sendData(0x5A);             // TSSET: 4 gray
}

void EPGray::push() {
    init4G();

    // Plano "old" (0x10): bit ALTO de cada pixel; Plano "new" (0x13): bit BAIXO.
    // O framebuffer 2bpp (_fb) e mantido intacto para os refreshes parciais.
    uint8_t* oldP = _plane;
    uint8_t* newP = _plane2;
    for (unsigned i = 0; i < GRAY_PLANE_BYTES; i++) {
        uint8_t b0 = _fb[i * 2];
        uint8_t b1 = _fb[i * 2 + 1];
        oldP[i] =
            ((b0 >> 7) & 1) << 7 | ((b0 >> 5) & 1) << 6 |
            ((b0 >> 3) & 1) << 5 | ((b0 >> 1) & 1) << 4 |
            ((b1 >> 7) & 1) << 3 | ((b1 >> 5) & 1) << 2 |
            ((b1 >> 3) & 1) << 1 | ((b1 >> 1) & 1);
        newP[i] =
            ((b0 >> 6) & 1) << 7 | ((b0 >> 4) & 1) << 6 |
            ((b0 >> 2) & 1) << 5 | ((b0 >> 0) & 1) << 4 |
            ((b1 >> 6) & 1) << 3 | ((b1 >> 4) & 1) << 2 |
            ((b1 >> 2) & 1) << 1 | ((b1 >> 0) & 1);
    }

    sendCmd(0x10);
    sendData(oldP, GRAY_PLANE_BYTES);
    sendCmd(0x13);
    sendData(newP, GRAY_PLANE_BYTES);

    sendCmd(0x50);
    sendData(0x97);             // VCOM/data interval - borda preta (full)

    sendCmd(0x12);              // refresh
    delay(1);
    waitBusy("refresh");

    sendCmd(0x02);              // power off
    waitBusy("poff");
    sendCmd(0x07);
    sendData(0xA5);             // deep sleep
}

void EPGray::pushWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
    // Alinha a janela aos limites de byte (8px) exigidos pelo controlador:
    // canto esquerdo para BAIXO e canto direito para CIMA. Se so o esquerdo
    // for alinhado, o lado direito da regiao pode ficar de fora da janela
    // (nunca e atualizado, deixando ghosting).
    int16_t xr = x + w;               // borda direita (exclusiva)
    x -= x % 8;                       // esquerda -> multiplo de 8
    if (xr % 8) xr += 8 - xr % 8;     // direita  -> proximo multiplo de 8
    if (x < 0) { xr += x; x = 0; }
    if (xr > GRAY_W) xr = GRAY_W;
    w = xr - x;
    if (w <= 0 || h <= 0) return;
    if (y < 0) { h += y; y = 0; }
    if (h <= 0) return;
    if (y + h > GRAY_H) h = GRAY_H - y;

    init4G();

    uint16_t xe = xr - 1;
    uint16_t ye = y + h - 1;

    auto setWindow = [&]() {
        sendCmd(0x90);   // partial window
        uint8_t wd[7] = {
            (uint8_t)(x & 0xFF),
            (uint8_t)(xe & 0xFF),
            (uint8_t)((y >> 8) & 0xFF),
            (uint8_t)(y & 0xFF),
            (uint8_t)((ye >> 8) & 0xFF),
            (uint8_t)(ye & 0xFF),
            0x01           // PTL_EN
        };
        sendData(wd, sizeof(wd));
    };

    // Extrai 1 byte 1bpp (8 px) de 2 bytes 2bpp (4 px cada)
    auto planeByte = [&](int16_t row, int16_t byteCol, bool msb) {
        uint32_t px = x + byteCol * 8;
        uint32_t f0 = ((uint32_t)(y + row) * GRAY_W + px) / 4;
        uint8_t a = _fb[f0];
        uint8_t c = _fb[f0 + 1];
        if (msb) {
            return ((a >> 7) & 1) << 7 | ((a >> 5) & 1) << 6 |
                   ((a >> 3) & 1) << 5 | ((a >> 1) & 1) << 4 |
                   ((c >> 7) & 1) << 3 | ((c >> 5) & 1) << 2 |
                   ((c >> 3) & 1) << 1 | ((c >> 1) & 1);
        }
        return ((a >> 6) & 1) << 7 | ((a >> 4) & 1) << 6 |
               ((a >> 2) & 1) << 5 | ((a >> 0) & 1) << 4 |
               ((c >> 6) & 1) << 3 | ((c >> 4) & 1) << 2 |
               ((c >> 2) & 1) << 1 | ((c >> 0) & 1);
    };

    uint8_t line[GRAY_W / 8];
    int16_t wBytes = w / 8;

    // Plano old (0x10)
    sendCmd(0x91);   // partial in
    setWindow();
    sendCmd(0x10);
    for (int16_t row = 0; row < h; row++) {
        for (int16_t b = 0; b < wBytes; b++) {
            line[b] = planeByte(row, b, true);
        }
        sendData(line, wBytes);
    }
    sendCmd(0x92);   // partial out

    // Plano new (0x13)
    sendCmd(0x91);
    setWindow();
    sendCmd(0x13);
    for (int16_t row = 0; row < h; row++) {
        for (int16_t b = 0; b < wBytes; b++) {
            line[b] = planeByte(row, b, false);
        }
        sendData(line, wBytes);
    }
    sendCmd(0x92);

    // IMPORTANTE: o refresh 0x12 so atualiza a janela se a PTL estiver
    // ativa. Reaplica a janela e so depois desliga o modo parcial.
    sendCmd(0x91);
    setWindow();
    sendCmd(0x50);
    // 0x50: VBD[1:0] DDX[1:0] - CDI[2:0]
    // 0xD7 (VBD=11) empurra a borda (tudo fora da janela) para VCOM a cada
    // refresh parcial - foi o que gerou a linha no extremo inferior da tela.
    // 0x17 (VBD=00, DDX=01) deixa a borda FLUTUANTE (hi-Z): nada fora da
    // janela e dirigido, entao a linha some e o ghosting de 1px na borda da
    // janela nao acontece (os pixels de fora nem sao mexidos).
    sendData(0x17);             // VCOM/data interval - borda flutuante
    sendCmd(0x12);   // refresh - apenas a regiao da janela
    delay(1);
    waitBusy("pwin refresh");
    sendCmd(0x92);   // partial out

    sendCmd(0x02);
    waitBusy("poff");
    sendCmd(0x07);
    sendData(0xA5);
}

void EPGray::powerOff() {
    sendCmd(0x02);
    waitBusy("poff");
    sendCmd(0x07);
    sendData(0xA5);
}
