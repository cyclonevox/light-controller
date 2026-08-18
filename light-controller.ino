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

const int RELAY_PIN = 8;
const bool RELAY_ACTIVE_HIGH = true;
const char AP_SSID[] = "Light-Setup";

enum WifiState {
  WIFI_NONE = 0,
  WIFI_SAVED = 1
};

struct WifiCreds {
  int state;
  char ssid[33];
  char pass[65];
};

const int BOOT_FLAG_ADDR = sizeof(WifiCreds);
const uint8_t BOOT_ARMED = 1;

TEXT_ANIMATION_DEFINE(anim, 160)

ArduinoLEDMatrix matrix;
WiFiServer server(80);

bool lightOn = false;
bool configMode = false;
char scrollText[48] = "";
volatile bool scrollAgain = false;

const char SETUP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#101216">
<title>配置灯光</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100dvh; font-family: system-ui, sans-serif;
    background: #101216; color: #e8eaed; padding: 28px 20px;
    display: flex; flex-direction: column; align-items: center;
  }
  h1 { font-size: 1.1rem; letter-spacing: .08em; color: #9aa0a6; }
  form, p { width: min(100%, 360px); }
  label { display: block; margin: 16px 0 8px; color: #9aa0a6; font-size: .9rem; }
  input {
    width: 100%; padding: 14px; border: 0; border-radius: 12px;
    background: #2a2e33; color: #fff; font-size: 1rem;
  }
  button {
    width: 100%; margin-top: 24px; padding: 14px; border: 0; border-radius: 12px;
    background: #e8b931; color: #1a1403; font-size: 1rem; font-weight: 700;
  }
  p { color: #9aa0a6; line-height: 1.5; font-size: .9rem; }
</style>
</head>
<body>
  <h1>配置 WiFi</h1>
  <p>只支持 2.4GHz。保存后板子会重启，点阵滚动 IP 后即可开关灯。</p>
  <form action="/save" method="get">
    <label>WiFi 名称</label>
    <input name="ssid" autocomplete="off" required>
    <label>密码（开放网络可留空）</label>
    <input name="pass" type="password">
    <button type="submit">保存并连接</button>
  </form>
</body>
</html>
)HTML";

const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#101216">
<title>灯光</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100dvh;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: system-ui, sans-serif; background: #101216; color: #e8eaed;
    gap: 28px; padding: 24px;
  }
  h1 { font-size: 1.1rem; font-weight: 600; letter-spacing: .12em; color: #9aa0a6; margin: 0; }
  #btn {
    width: min(64vw, 220px); aspect-ratio: 1; border-radius: 50%;
    border: 0; cursor: pointer; color: #fff; font-size: 1.6rem; font-weight: 700;
    background: #3c4043; box-shadow: 0 10px 30px rgba(0,0,0,.35);
    transition: transform .12s, background .2s, box-shadow .2s;
  }
  #btn.on { background: #e8b931; color: #1a1403; box-shadow: 0 0 0 10px rgba(232,185,49,.18), 0 10px 30px rgba(232,185,49,.25); }
  #btn:active { transform: scale(.96); }
  #state { margin: 0; color: #9aa0a6; font-size: .95rem; }
  form { margin: 0; }
  .link {
    background: none; border: 0; color: #9aa0a6; font-size: .85rem; text-decoration: underline;
  }
</style>
</head>
<body>
  <h1>灯光开关</h1>
  <button id="btn" type="button">关</button>
  <p id="state">正在连接…</p>
  <form action="/forget" method="get">
    <input type="hidden" name="confirm" value="1">
    <button class="link" type="submit">更换 WiFi</button>
  </form>
  <script>
    const btn = document.getElementById('btn');
    const state = document.getElementById('state');
    function render(on) {
      btn.className = on ? 'on' : '';
      btn.textContent = on ? '开' : '关';
      state.textContent = on ? '灯已打开' : '灯已关闭';
    }
    async function refresh() {
      const r = await fetch('/api/status');
      const j = await r.json();
      render(j.on);
    }
    btn.onclick = async () => {
      const r = await fetch('/api/toggle');
      const j = await r.json();
      render(j.on);
    };
    refresh();
    setInterval(refresh, 4000);
  </script>
</body>
</html>
)HTML";

const char SAVED_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>已保存</title>
<style>body{background:#101216;color:#e8eaed;font-family:system-ui;padding:40px;text-align:center}</style>
</head><body><p>已保存，板子正在重启并连接 WiFi。</p><p>请改连家里的 WiFi，看点阵上滚动的地址。</p></body></html>
)HTML";

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
  uint8_t flag = 0;
  EEPROM.get(BOOT_FLAG_ADDR, flag);
  if (flag == BOOT_ARMED) {
    flag = 0;
    EEPROM.put(BOOT_FLAG_ADDR, flag);
    return true;
  }

  flag = BOOT_ARMED;
  EEPROM.put(BOOT_FLAG_ADDR, flag);
  startScroll("RST2 SETUP");
  delay(3000);
  flag = 0;
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
    sendHtml(client, PAGE);
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
