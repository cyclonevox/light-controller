// 宿主 WiFiS3：beginAP/begin 是假的；WiFiServer 在本机回环口听 HTTP。
#pragma once

#include "Arduino.h"

enum {
  WL_NO_MODULE = 255,
  WL_IDLE_STATUS = 0,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_AP_LISTENING = 7
};

class WiFiClient : public HostPrint {
 public:
  WiFiClient() = default;
  explicit WiFiClient(int fd);
  WiFiClient(const WiFiClient& other);
  WiFiClient& operator=(const WiFiClient& other);
  ~WiFiClient();

  operator bool() const;
  bool connected();
  int available();
  int read();
  void stop();
  void writeBytes(const char* data, size_t len) override;

 private:
  int fd = -1;
};

class WiFiServer {
 public:
  explicit WiFiServer(uint16_t port);
  void begin();
  WiFiClient available();

 private:
  uint16_t port;
  int listenFd = -1;
};

class WiFiClass {
 public:
  int status();
  int beginAP(const char* ssid);
  int begin(const char* ssid, const char* pass);
  void disconnect();
  void setTimeout(unsigned long);
  void setHostname(const char*);
  IPAddress localIP();
};

extern WiFiClass WiFi;
