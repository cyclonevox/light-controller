// 板级常量和 EEPROM 布局：继电器脚、配网热点名、WiFi 凭据和双击 Reset 标志。
#pragma once

constexpr int RELAY_PIN = 8;
constexpr bool RELAY_ACTIVE_HIGH = true;
constexpr const char AP_SSID[] = "Light-Setup";

enum WifiState {
  WIFI_NONE = 0,
  WIFI_SAVED = 1
};

enum BootFlag {
  BOOT_IDLE = 0,
  BOOT_ARMED = 1
};

struct WifiCreds {
  int state;
  char ssid[33];
  char pass[65];
};

constexpr int BOOT_FLAG_ADDR = sizeof(WifiCreds);
