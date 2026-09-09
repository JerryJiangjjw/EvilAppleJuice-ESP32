# EvilAppleJuice ESP32

Spam BLE advertisements on iPhones!

|iPhone 15s (latest)|Older iPhones|
|-------------------|-------------|
|<video controls width="250" src="https://user-images.githubusercontent.com/6680615/274864225-53ed6d7c-0569-4f22-b55b-bc9973c4bc93.mp4"></video>|<video controls width="250" src="https://user-images.githubusercontent.com/6680615/274864287-c6e871fd-9fdf-4507-ae21-a566beead5cc.mp4"></video>|

Based off of the work of [ronaldstoner](https://github.com/ronaldstoner) in the [AppleJuice repository](https://github.com/ECTO-1A/AppleJuice/blob/e6a61f6a199075f5bb5b1a00768e317571d25bb9/ESP32-Arduino/applejuice.ino).

Also thanks to [simondankelmann](https://github.com/simondankelmann) for their discoveries in new advertising messages to pop-up new notifications in iOS devices [source](https://github.com/simondankelmann/Bluetooth-LE-Spam/blob/main/app/src/main/java/de/simon/dankelmann/bluetoothlespam/AdvertisementSetGenerators/ContinuityActionModalAdvertisementSetGenerator.kt)

## How?

<a href="https://youtu.be/y2WbXvp2viI?t=17m6s"><img width="960" height="540" alt="Image" src="https://github.com/user-attachments/assets/20795ed7-55e9-4cfc-979a-fff85cf88eca" /></a>

I gave a brief (3min) talk of how it works at COSCUP 2025, it's hosted on their YouTube channel: [https://youtu.be/y2WbXvp2viI?t=17m6s](https://youtu.be/y2WbXvp2viI?t=17m6s)

## Some features & testing

With the randomization optimizations it can render an iPhone almost useless with a single ESP32 (a new notification as soon as you close the old one).

Confirmed on:
* iPhone 15 (running iOS 17.1.2)
* iPhone 14 Pro Max (running iOS 17.2 b3) (See #19)
* iPhone 14 Pro (running iOS 16.6.1)
* iPhone 13 Pro (running iOS 17.4 (21E5184k))
* iPhone 11 (running iOS 16.6.1)
* iPhone X (running iOS 14.8 (18H17)) - only "AppleTV Keyboard", "TV Color Balance", "AppleTV Setup", "AppleTV Homekit Setup", "AppleTV New User".
* iPad Pro 11 (running iPadOS 17.3 (21D50))

Not working on:
* iPhone 4S (running iOS 10.3 (14E277))

Other observations:
* Doesn't seem to spawn notifications if Keyboard is open / Camera is open

### Video Demo

Single ESP32 vs. iPhone 14 Pro @ iOS 16.6.1

https://github.com/ECTO-1A/AppleJuice/assets/6680615/47466ed6-03c9-43b2-a0d0-aac2e2aaa228

### Security Vulnerability?

Since all we're doing is sending BLE advertisments from a "dumb" device, I argue there is no epxloit intent, just annoying.

[I've asked over a year ago on the Apple forums](https://discussions.apple.com/thread/255127943), if it's possible to disable the feature where iDevices are eagerly awaiting advertisments and popping up notifications, but to no reply. Clearly Tim Apple^ thinks that he know how you should use your device better than you - in fact even if you disable Bluetooth from the quick settings or whatever its called, these will still keep coming - you need to go into settings and turn of Bluetooth completely. Which means you can't use your Airpods or whatever wireless audio device you purchased when they removed the 3.5mm jack. 

*^ obviously I don't actually think Tim Cook is directly behind this, but rather Apple's smug nature of thinking they know what's best, and you're wrong if you don't think that's good design.*

## Notable Differences

This implementation makes the following changes:

* Random source MAC address (including `BLE_ADDR_TYPE_RANDOM`)
* Randomly pick BLE Advertisement Type ([this may lead to more success](https://github.com/ECTO-1A/AppleJuice/pull/25))
* Randomly pick one of the possible devices
* Sets the ESP32 BLE Power to the maximum (9dBm) to increase range

And it makes these random choices every time it runs (default re-advertise every second).

Given the 29 devices and the 3 advertisement types, there are a total of 87 unique possible advertisements (ignoring the random source MAC) possible, of which one is broadcast every second.

## Fork 新增功能（New in this fork）

本分支在原作基础上，针对 **ESP32-S3-DevKitC-1** 做了适配与简化：

### 硬件适配

* `platformio.ini` 新增 `esp32s3` 编译环境（目标板 `esp32-s3-devkitc-1`，Arduino 框架）
* 使用板载 **RGB LED（WS2812，GPIO48）** 作为状态指示灯（依赖 Adafruit NeoPixel，已声明于 `lib_deps`）
* 控制按键复用板载 **BOOT 键（GPIO0）**，无需外接按键

### 操作方式

| 操作 | 功能 |
|------|------|
| 短按 BOOT | 切换广播模式：**模式 1 = 固定 AirPods**（轰炸效果最强）/ **模式 2 = 随机设备**（29 种设备随机轮换） |
| 长按 BOOT（≥1 秒） | 电源开关：关闭后立即停止广播 |
| 状态记忆 | 开关与模式存入 flash（Preferences），断电/重启后保持上次状态 |

### 指示灯

| 状态 | 灯光 |
|------|------|
| 电源关闭 | 灯灭 |
| 模式 1（AirPods） | 红色闪烁 |
| 模式 2（随机） | 随机变色闪烁 |

### 注意事项

* **上电/复位瞬间按住 BOOT 键会进入下载模式**（GPIO0 为启动引导引脚）；正常开机后再按键才是模式/电源控制
* 原作 9 种双 LED 组合模式（`led.hpp` 状态表）已移除，简化为「2 模式 + 电源开关」，普通 GPIO LED 不再使用

## Usage

Clone the repo, and easiest would be to use VS Code w/ PlatformIO to upload it to your ESP32.

This project has been tested on an [ESP32-C3 from AirM2M](https://wiki.luatos.com/chips/esp32c3/board.html).

### Serial Port on Linux

For me, plugging in a ESP32-C3 assigns it to `/dev/ttyACM0`.

To allow the serial port to be writable by a user, you can do:

```
sudo chmod 666 /dev/ttyACM0
```

### Via Arduino-CLI

#### Windows

If you've setup the Arduino CLI, e.g. via https://wellys.com/posts/esp32_cli/ , then from the project root, run the following:

```
arduino-cli compile --fqbn esp32:esp32:esp32c6 EvilAppleJuice-ESP32-INO -v
arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32c6 EvilAppleJuice-ESP32-INO -v
arduino-cli monitor -c baudrate=115200 -p COM4
```

Replace `COM4` with the port the ESP32 is on, and `esp32c6` with the appropriate board.

#### Linux

Example for AirM2M's ESP32C3:
```
arduino-cli compile --fqbn esp32:esp32:AirM2M_CORE_ESP32C3 EvilAppleJuice-ESP32-INO -v && \
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:AirM2M_CORE_ESP32C3 EvilAppleJuice-ESP32-INO -v && \
arduino-cli monitor -c baudrate=115200 -p /dev/ttyACM0
```

## Project Layout

This was originally developer for use with PlatformIO + VSCode, but some people requested support for Arduino. This s achieved by basically copying the `main.cpp` file as `EvilAppleJuice-ESP32-INO.ino` and put into its own folder. We copy the other `.hpp` & `.cpp` files as is.
