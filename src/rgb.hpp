// ESP32-S3-DevKitC-1 板载 RGB (WS2812, GPIO48) 指示灯控制
// 用法：setup() 调 initRgb()，主循环调 updateRgb(currentMode)
#pragma once

#include <Adafruit_NeoPixel.h>
#include "led.hpp"

#define RGB_PIN 48   // ESP32-S3-DevKitC-1 板载 WS2812 所在引脚
#define NUM_LEDS 1

Adafruit_NeoPixel rgbLed(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// 各模式指示色（mode0=红 固定AirPods；mode2..8=固定设备；mode1 单独做彩虹）
struct RGBColor { uint8_t r, g, b; };
const RGBColor modeColors[9] = {
    {255, 0, 0},      // 0  红    固定 AirPods（轰炸中）
    {0, 0, 0},        // 1  彩虹循环（随机设备）→ 特殊处理
    {255, 128, 0},    // 2  橙
    {255, 255, 0},    // 3  黄
    {0, 255, 0},      // 4  绿
    {0, 255, 255},    // 5  青
    {0, 128, 255},    // 6  蓝
    {180, 0, 255},    // 7  紫
    {255, 255, 255}   // 8  白
};

void initRgb() {
    rgbLed.begin();
    rgbLed.setBrightness(40);  // 板载灯很亮，限流避免刺眼
    rgbLed.show();
}

void setRgbColor(const RGBColor& c) {
    rgbLed.setPixelColor(0, c.r, c.g, c.b);
    rgbLed.show();
}

// HSV -> RGB（用于彩虹）
RGBColor hsvToRgb(uint16_t hue, uint8_t sat = 255, uint8_t val = 255) {
    uint8_t region = (hue % 360) / 60;
    uint8_t remainder = (hue % 360) % 60;
    uint8_t p = val * (255 - sat) / 255;
    uint8_t q = val * (255 - sat * remainder / 255) / 255;
    uint8_t t = val * (255 - sat * (60 - remainder) / 255) / 255;
    RGBColor c;
    switch (region) {
        case 0:  c = {val, t, p}; break;
        case 1:  c = {q, val, p}; break;
        case 2:  c = {p, val, t}; break;
        case 3:  c = {p, q, val}; break;
        case 4:  c = {t, p, val}; break;
        default: c = {val, p, q}; break;
    }
    return c;
}

// 该模式是否带 FLASH 语义（原两灯状态表里任一灯为 FLASH）
bool modeHasFlash(int mode) {
    return stateTable[mode][0] == FLASH || stateTable[mode][1] == FLASH;
}

// 主循环调用：按当前模式刷新 RGB
void updateRgb(int mode) {
    if (mode == 1) {
        // 随机模式：彩虹自动循环
        uint16_t hue = (millis() / 40) % 360;
        setRgbColor(hsvToRgb(hue));
        return;
    }
    RGBColor c = modeColors[mode];
    if (modeHasFlash(mode)) {
        // 闪烁：700ms 周期亮/灭（灭半档更柔和）
        bool lit = (millis() % 700) < 350;
        if (!lit) { c.r = c.r / 6; c.g = c.g / 6; c.b = c.b / 6; }
    }
    setRgbColor(c);
}
