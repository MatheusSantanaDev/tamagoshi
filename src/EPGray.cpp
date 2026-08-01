#include "EPGray.h"

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
    // O framebuffer 2bpp não é mais necessário depois da extração:
    // _plane guarda o plano old e _fb (reusado) o plano new.
    uint8_t* oldP = _plane;
    uint8_t* newP = _fb;
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

    sendCmd(0x12);              // refresh
    delay(1);
    waitBusy("refresh");

    sendCmd(0x02);              // power off
    waitBusy("poff");
    sendCmd(0x07);
    sendData(0xA5);             // deep sleep
}

void EPGray::powerOff() {
    sendCmd(0x02);
    waitBusy("poff");
    sendCmd(0x07);
    sendData(0xA5);
}
