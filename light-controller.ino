/*
  Uno R4 WiFi + 1路继电器模块（S / + / -）网页开关

  模块信号脚：
    +  -> Uno 5V
    -  -> Uno GND
    S  -> Uno D8

  模块螺丝（看印刷字，不要按颜色猜）：
    COM -> 12V 电源正极
    NO  -> 灯正极
    灯负极 -> 12V 电源负极
    NC  不接（常闭，这里用不到）

  日常用法（不需要电脑）：
    1. 给板子上电，手机连热点 Light-Setup
    2. 浏览器打开 http://192.168.4.1 ，填家里的 2.4GHz WiFi
    3. 板子重启并连上后，点阵会滚动 IP，手机改连家里 WiFi，打开该地址开关灯
    连不上家里 WiFi 时会自动回到配网热点。
    重新配网：开机 3 秒内再按一次 Reset（点阵会显示 RST2 SETUP）。
    网页里点「更换 WiFi」也可以。

  文件分工：
    light-controller.ino  上电后的 setup / loop，不放具体业务
    config.h              引脚、热点名、EEPROM 里的凭据布局
    board.cpp / board.h   继电器开关和点阵滚动
    net.cpp / net.h       配网、存 WiFi、HTTP 开关灯
    web/                  页面源文件；编译前打进 src/web_pages.h
*/

#include <Arduino.h>

#include "board.h"
#include "net.h"

void setup() {
  boardBegin();

  Serial.begin(115200);
  const unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 2000) {
    delay(10);
  }
  Serial.println();
  Serial.println("light-controller boot");

  if (!netBegin()) {
    displayScroll("NO WIFI");
    while (true) delay(1000);
  }

  netBoot();
}

void loop() {
  displayPoll();
  netPoll();
}
