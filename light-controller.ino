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

  用法：USB 给 Uno 上电，串口监视器 115200 看 IP，
  手机连同一个 WiFi，浏览器打开 http://那个IP
*/

#include <WiFiS3.h>
#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

const int RELAY_PIN = 8;

// 多数 S/+/- 模块是高电平吸合。若网页显示开着、灯却灭，改成 false。
const bool RELAY_ACTIVE_HIGH = true;

WiFiServer server(80);
bool lightOn = false;

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
</style>
</head>
<body>
  <h1>灯光开关</h1>
  <button id="btn" type="button">关</button>
  <p id="state">正在连接…</p>
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

void applyRelay() {
  const bool level = lightOn ? RELAY_ACTIVE_HIGH : !RELAY_ACTIVE_HIGH;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  digitalWrite(LED_BUILTIN, lightOn ? HIGH : LOW);
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

void sendPage(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(strlen(PAGE));
  client.println();
  client.print(PAGE);
}

void sendState(WiFiClient& client) {
  char body[24];
  snprintf(body, sizeof(body), "{\"on\":%s}", lightOn ? "true" : "false");
  sendResponse(client, "200 OK", "application/json", body);
}

void handleClient(WiFiClient client) {
  String req;
  const unsigned long start = millis();
  while (client.connected() && millis() - start < 1500) {
    while (client.available()) {
      const char c = client.read();
      req += c;
      if (req.length() > 800 || req.endsWith("\r\n\r\n")) {
        goto handle;
      }
    }
  }

handle:
  if (req.startsWith("GET /api/toggle")) {
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
    sendPage(client);
  }

  delay(1);
  client.stop();
}

bool hasIp(IPAddress ip) {
  return !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

void connectWiFi() {
  WiFi.setTimeout(20000);

  for (;;) {
    Serial.print("Connecting to ");
    Serial.println(ssid);

    while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
      Serial.println("WiFi failed, retry...");
      delay(1000);
    }

    // R4 连上热点后 DHCP 还要一会儿，立刻读 IP 会得到 0.0.0.0
    IPAddress ip = WiFi.localIP();
    const unsigned long start = millis();
    while (!hasIp(ip) && millis() - start < 20000) {
      delay(250);
      Serial.print(".");
      ip = WiFi.localIP();
    }
    Serial.println();

    if (hasIp(ip)) {
      server.begin();
      Serial.print("SSID ");
      Serial.println(WiFi.SSID());
      Serial.print("Open http://");
      Serial.println(ip);
      return;
    }

    Serial.println("No DHCP IP, reconnecting");
    WiFi.disconnect();
    delay(1000);
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  lightOn = false;
  applyRelay();

  Serial.begin(115200);
  const unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 4000) {
    delay(10);
  }
  delay(300);
  Serial.println();
  Serial.println("light-controller boot");

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("No WiFi module");
    while (true) delay(1000);
  }

  connectWiFi();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
}
