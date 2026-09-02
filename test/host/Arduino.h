// 电脑上跑固件时用的 Arduino 替身：Serial、delay、GPIO 记到文件里。
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#define PROGMEM
#define HIGH 0x1
#define LOW 0x0
#define OUTPUT 0x1
#define INPUT 0x0
#define LED_BUILTIN 13

class String {
 public:
  String() = default;
  String(const char* value) : data(value ? value : "") {}

  String& operator+=(char c) {
    data.push_back(c);
    return *this;
  }

  unsigned int length() const { return static_cast<unsigned int>(data.size()); }
  const char* c_str() const { return data.c_str(); }

  bool startsWith(const char* prefix) const {
    if (prefix == nullptr) {
      return false;
    }
    const size_t n = strlen(prefix);
    return data.size() >= n && data.compare(0, n, prefix) == 0;
  }

  bool endsWith(const char* suffix) const {
    if (suffix == nullptr) {
      return false;
    }
    const size_t n = strlen(suffix);
    return data.size() >= n && data.compare(data.size() - n, n, suffix) == 0;
  }

  int indexOf(char ch) const {
    const auto pos = data.find(ch);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  int indexOf(char ch, unsigned int from) const {
    const auto pos = data.find(ch, from);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

 private:
  std::string data;
};

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    octets[0] = a;
    octets[1] = b;
    octets[2] = c;
    octets[3] = d;
  }

  uint8_t operator[](int index) const { return octets[index]; }

  void printTo(class HostPrint& out) const;

 private:
  uint8_t octets[4] = {0, 0, 0, 0};
};

class HostPrint {
 public:
  virtual ~HostPrint() = default;
  virtual void writeBytes(const char* data, size_t len) = 0;

  void print(const char* value);
  void print(int value);
  void print(unsigned long value);
  void print(const IPAddress& value);
  void println();
  void println(const char* value);
  void println(int value);
  void println(unsigned long value);
  void println(const IPAddress& value);
};

class HardwareSerial : public HostPrint {
 public:
  void begin(unsigned long) {}
  operator bool() const { return true; }
  void writeBytes(const char* data, size_t len) override;
};

extern HardwareSerial Serial;

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
unsigned long millis();
void delay(unsigned long ms);
void NVIC_SystemReset();
