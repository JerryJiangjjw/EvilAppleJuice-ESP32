// ESP32-S3-DevKitC-1 板载 RGB (WS2812, GPIO48) 底层驱动
// 提供: initRgb / setRgb / setRgbHsv / rgbOff
#pragma once

#include <Adafruit_NeoPixel.h>

#define EAJ_RGB_PIN 48          // ESP32-S3-DevKitC-1 板载 WS2812 引脚
#define EAJ_NUM_LEDS 1
#define EAJ_RGB_BRIGHTNESS 60   // 板载灯很亮，限流防刺眼

Adafruit_NeoPixel rgbLed(EAJ_NUM_LEDS, EAJ_RGB_PIN, NEO_GRB + NEO_KHZ800);

void initRgb() {
    rgbLed.begin();
    rgbLed.setBrightness(EAJ_RGB_BRIGHTNESS);
    rgbLed.show();
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
    rgbLed.setPixelColor(0, r, g, b);
    rgbLed.show();
}

void rgbOff() {
    rgbLed.setPixelColor(0, 0, 0, 0);
    rgbLed.show();
}

// HSV -> RGB 设色（hue 0-359），保证颜色鲜艳
void setRgbHsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255) {
    uint8_t region = (hue % 360) / 60;
    uint8_t remainder = (hue % 360) % 60;
    uint8_t p = val * (255 - sat) / 255;
    uint8_t q = val * (255 - sat * remainder / 255) / 255;
    uint8_t t = val * (255 - sat * (60 - remainder) / 255) / 255;
    switch (region) {
        case 0:  setRgb(val, t, p); break;
        case 1:  setRgb(q, val, p); break;
        case 2:  setRgb(p, val, t); break;
        case 3:  setRgb(p, q, val); break;
        case 4:  setRgb(t, p, val); break;
        default: setRgb(val, p, q); break;
    }
}
