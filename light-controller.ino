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
*/

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "TextAnimation.h"

#include <EEPROM.h>
#include <WiFiS3.h>
#include "web_pages.h"

const int RELAY_PIN = 8;
const bool RELAY_ACTIVE_HIGH = true;
const char AP_SSID[] = "Light-Setup";

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

const int BOOT_FLAG_ADDR = sizeof(WifiCreds);

TEXT_ANIMATION_DEFINE(anim, 160)

ArduinoLEDMatrix matrix;
WiFiServer server(80);

bool lightOn = false;
bool configMode = false;
char scrollText[48] = "";
volatile bool scrollAgain = false;

void applyRelay() {
  const bool level = lightOn ? RELAY_ACTIVE_HIGH : !RELAY_ACTIVE_HIGH;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);
}

bool hasIp(IPAddress ip) {
  return !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

void matrixCallback() {
  scrollAgain = true;
}

void startScroll(const char* text) {
  strncpy(scrollText, text, sizeof(scrollText) - 1);
  scrollText[sizeof(scrollText) - 1] = '\0';

  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_5x7);
  matrix.textScrollSpeed(70);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print("   ");
  matrix.println(scrollText);
  matrix.endTextAnimation(SCROLL_LEFT, anim);
  matrix.loadTextAnimationSequence(anim);
  matrix.play();
}

void formatIpText(IPAddress ip, char* out, size_t outLen) {
  snprintf(out, outLen, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void loadCreds(WifiCreds& creds) {
  EEPROM.get(0, creds);
}

void saveCreds(const WifiCreds& creds) {
  EEPROM.put(0, creds);
}

bool consumeDoubleReset() {
  BootFlag flag = BOOT_IDLE;
  EEPROM.get(BOOT_FLAG_ADDR, flag);
  if (flag == BOOT_ARMED) {
    flag = BOOT_IDLE;
    EEPROM.put(BOOT_FLAG_ADDR, flag);
    return true;
  }

  flag = BOOT_ARMED;
  EEPROM.put(BOOT_FLAG_ADDR, flag);
  startScroll("RST2 SETUP");
  delay(3000);
  flag = BOOT_IDLE;
  EEPROM.put(BOOT_FLAG_ADDR, flag);
  return false;
}

void forgetWifi() {
  WifiCreds creds = {};
  creds.state = WIFI_NONE;
  saveCreds(creds);
}

bool credsUsable(const WifiCreds& creds) {
  return creds.state == WIFI_SAVED && creds.ssid[0] != '\0';
}

void urlDecode(const char* src, char* dst, size_t dstLen) {
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && j + 1 < dstLen; i++) {
    if (src[i] == '+') {
      dst[j++] = ' ';
    } else if (src[i] == '%' && src[i + 1] && src[i + 2]) {
      char hex[3] = { src[i + 1], src[i + 2], 0 };
      dst[j++] = (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      dst[j++] = src[i];
    }
  }
  dst[j] = '\0';
}

bool extractParam(const String& req, const char* key, char* dest, size_t destLen) {
  const String token = String(key) + "=";
  int start = req.indexOf(token);
  if (start < 0) {
    dest[0] = '\0';
    return false;
  }
  start += token.length();
  int amp = req.indexOf('&', start);
  int space = req.indexOf(' ', start);
  int end = req.length();
  if (amp >= 0) {
    end = amp;
  }
  if (space >= 0 && space < end) {
    end = space;
  }
  const String enc = req.substring(start, end);
  urlDecode(enc.c_str(), dest, destLen);
  return true;
}

void sendHtml(WiFiClient& client, const char* body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(strlen(body));
  client.println();
  client.print(body);
}

void sendResponse(WiFiClient& client, const char* status, const char* type, const char* body) {
  client.print("HTTP/1.1 ");
  client.println(status);
  client.print("Content-Type: ");
  client.println(type);
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(strlen(body));
  client.println();
  client.print(body);
}

void sendState(WiFiClient& client) {
  char body[24];
  snprintf(body, sizeof(body), "{\"on\":%s}", lightOn ? "true" : "false");
  sendResponse(client, "200 OK", "application/json", body);
}

void rebootSoon() {
  delay(400);
  NVIC_SystemReset();
}

bool requestReady(const String& req) {
  return req.length() > 2500 || req.endsWith("\r\n\r\n") || req.indexOf('\n') >= 0;
}

void dispatchRequest(WiFiClient& client, const String& req) {
  if (req.startsWith("GET /save")) {
    WifiCreds creds = {};
    extractParam(req, "ssid", creds.ssid, sizeof(creds.ssid));
    extractParam(req, "pass", creds.pass, sizeof(creds.pass));
    if (creds.ssid[0] != '\0') {
      creds.state = WIFI_SAVED;
      saveCreds(creds);
      sendHtml(client, SAVED_PAGE);
      delay(1);
      client.stop();
      rebootSoon();
      return;
    }
    sendHtml(client, SETUP_PAGE);
  } else if (req.startsWith("GET /forget?confirm=1")) {
    forgetWifi();
    sendHtml(client, SAVED_PAGE);
    delay(1);
    client.stop();
    rebootSoon();
    return;
  } else if (configMode) {
    sendHtml(client, SETUP_PAGE);
  } else if (req.startsWith("GET /api/toggle")) {
    lightOn = !lightOn;
    applyRelay();
    sendState(client);
  } else if (req.startsWith("GET /api/on")) {
    lightOn = true;
    applyRelay();
    sendState(client);
  } else if (req.startsWith("GET /api/off")) {
    lightOn = false;
    applyRelay();
    sendState(client);
  } else if (req.startsWith("GET /api/status")) {
    sendState(client);
  } else {
    sendHtml(client, LIGHT_PAGE);
  }

  delay(1);
  client.stop();
}

void handleClient(WiFiClient client) {
  String req;
  const unsigned long start = millis();
  while (client.connected() && millis() - start < 2000) {
    while (client.available()) {
      req += (char)client.read();
      if (requestReady(req)) {
        dispatchRequest(client, req);
        return;
      }
    }
  }
  dispatchRequest(client, req);
}

void startConfigPortal() {
  configMode = true;
  WiFi.disconnect();
  delay(200);

  if (WiFi.beginAP(AP_SSID) != WL_AP_LISTENING) {
    Serial.println("AP failed");
    delay(2000);
    NVIC_SystemReset();
  }

  server.begin();
  IPAddress ip = WiFi.localIP();
  if (!hasIp(ip)) {
    ip = IPAddress(192, 168, 4, 1);
  }

  char text[40];
  snprintf(text, sizeof(text), "SETUP %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  startScroll(text);

  Serial.print("Config AP ");
  Serial.print(AP_SSID);
  Serial.print("  http://");
  Serial.println(ip);
}

bool connectStation(const WifiCreds& creds) {
  configMode = false;
  WiFi.setTimeout(15000);
  WiFi.setHostname("light");

  Serial.print("Connecting to ");
  Serial.println(creds.ssid);

  for (int attempt = 0; attempt < 2; attempt++) {
    if (WiFi.begin(creds.ssid, creds.pass[0] ? creds.pass : nullptr) != WL_CONNECTED) {
      Serial.println("WiFi failed");
      delay(500);
      continue;
    }

    IPAddress ip = WiFi.localIP();
    const unsigned long start = millis();
    while (!hasIp(ip) && millis() - start < 15000) {
      delay(250);
      ip = WiFi.localIP();
    }

    if (hasIp(ip)) {
      server.begin();
      char text[40];
      formatIpText(ip, text, sizeof(text));
      startScroll(text);
      Serial.print("Open http://");
      Serial.println(ip);
      return true;
    }

    WiFi.disconnect();
    delay(500);
  }

  return false;
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  lightOn = false;
  applyRelay();

  matrix.begin();
  matrix.setCallback(matrixCallback);

  Serial.begin(115200);
  const unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 2000) {
    delay(10);
  }
  Serial.println();
  Serial.println("light-controller boot");

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("No WiFi module");
    startScroll("NO WIFI");
    while (true) delay(1000);
  }

  if (consumeDoubleReset()) {
    startConfigPortal();
    return;
  }

  WifiCreds creds = {};
  loadCreds(creds);
  if (!credsUsable(creds) || !connectStation(creds)) {
    startConfigPortal();
  }
}

void loop() {
  if (scrollAgain && scrollText[0] != '\0') {
    scrollAgain = false;
    startScroll(scrollText);
  }

  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
}
