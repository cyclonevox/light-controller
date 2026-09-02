// WiFi 配网 + HTTP。凭据存在 EEPROM；失败或双击 Reset 就回到 Light-Setup 热点。
#include <EEPROM.h>
#include <WiFiS3.h>

#include "board.h"
#include "config.h"
#include "net.h"
#include "src/web_pages.h"

static WiFiServer server(80);
static bool configMode = false;

static bool credsUsable(const WifiCreds& creds) {
  return creds.state == WIFI_SAVED && creds.ssid[0] != '\0';
}

static void urlDecode(const char* src, char* dst, size_t dstLen) {
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

static bool extractParam(const char* req, const char* key, char* dest, size_t destLen) {
  dest[0] = '\0';
  if (req == nullptr || key == nullptr || destLen == 0) {
    return false;
  }

  char token[40];
  snprintf(token, sizeof(token), "%s=", key);
  const char* found = strstr(req, token);
  if (found == nullptr) {
    return false;
  }

  const char* start = found + strlen(token);
  const char* end = start;
  while (*end != '\0' && *end != '&' && *end != ' ') {
    end++;
  }

  char enc[256];
  size_t n = static_cast<size_t>(end - start);
  if (n >= sizeof(enc)) {
    n = sizeof(enc) - 1;
  }
  memcpy(enc, start, n);
  enc[n] = '\0';
  urlDecode(enc, dest, destLen);
  return true;
}

static bool requestReady(const char* req) {
  if (req == nullptr) {
    return false;
  }
  const size_t n = strlen(req);
  return n > 2500 || strstr(req, "\r\n\r\n") != nullptr || strchr(req, '\n') != nullptr;
}

static bool hasIp(IPAddress ip) {
  return !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

static void formatIpText(IPAddress ip, char* out, size_t outLen) {
  snprintf(out, outLen, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void loadCreds(WifiCreds& creds) {
  EEPROM.get(0, creds);
}

static void saveCreds(const WifiCreds& creds) {
  EEPROM.put(0, creds);
}

static bool consumeDoubleReset() {
  BootFlag flag = BOOT_IDLE;
  EEPROM.get(BOOT_FLAG_ADDR, flag);
  if (flag == BOOT_ARMED) {
    flag = BOOT_IDLE;
    EEPROM.put(BOOT_FLAG_ADDR, flag);
    return true;
  }

  flag = BOOT_ARMED;
  EEPROM.put(BOOT_FLAG_ADDR, flag);
  displayScroll("RST2 SETUP");
  delay(3000);
  flag = BOOT_IDLE;
  EEPROM.put(BOOT_FLAG_ADDR, flag);
  return false;
}

static void forgetWifi() {
  WifiCreds creds = {};
  creds.state = WIFI_NONE;
  saveCreds(creds);
}

static void sendHtml(WiFiClient& client, const char* body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(strlen(body));
  client.println();
  client.print(body);
}

static void sendResponse(WiFiClient& client, const char* status, const char* type, const char* body) {
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

static void sendState(WiFiClient& client) {
  char body[24];
  snprintf(body, sizeof(body), "{\"on\":%s}", isLightOn() ? "true" : "false");
  sendResponse(client, "200 OK", "application/json", body);
}

static void rebootSoon() {
  delay(400);
  NVIC_SystemReset();
}

static void dispatchRequest(WiFiClient& client, const String& req) {
  if (req.startsWith("GET /save")) {
    WifiCreds creds = {};
    extractParam(req.c_str(), "ssid", creds.ssid, sizeof(creds.ssid));
    extractParam(req.c_str(), "pass", creds.pass, sizeof(creds.pass));
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
    toggleLight();
    sendState(client);
  } else if (req.startsWith("GET /api/on")) {
    setLight(true);
    sendState(client);
  } else if (req.startsWith("GET /api/off")) {
    setLight(false);
    sendState(client);
  } else if (req.startsWith("GET /api/status")) {
    sendState(client);
  } else {
    sendHtml(client, LIGHT_PAGE);
  }

  delay(1);
  client.stop();
}

static void handleClient(WiFiClient client) {
  String req;
  const unsigned long start = millis();
  while (client.connected() && millis() - start < 2000) {
    while (client.available()) {
      req += (char)client.read();
      if (requestReady(req.c_str())) {
        dispatchRequest(client, req);
        return;
      }
    }
  }
  dispatchRequest(client, req);
}

static void startConfigPortal() {
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
  displayScroll(text);

  Serial.print("Config AP ");
  Serial.print(AP_SSID);
  Serial.print("  http://");
  Serial.println(ip);
}

static bool connectStation(const WifiCreds& creds) {
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
      displayScroll(text);
      Serial.print("Open http://");
      Serial.println(ip);
      return true;
    }

    WiFi.disconnect();
    delay(500);
  }

  return false;
}

bool netBegin() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("No WiFi module");
    return false;
  }
  return true;
}

void netBoot() {
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

void netPoll() {
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
}
