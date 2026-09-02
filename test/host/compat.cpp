// 宿主运行时：假 EEPROM（文件）、假 WiFi、本机 127.0.0.1 TCP 充当 HTTP。
#include "Arduino.h"
#include "EEPROM.h"
#include "WiFiS3.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr size_t kEepromSize = 256;

std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
std::map<uint8_t, uint8_t> pins;
std::vector<uint8_t> eeprom(kEepromSize, 0xFF);
bool eepromLoaded = false;
int wifiStatus = WL_IDLE_STATUS;
IPAddress wifiIp;

const char* envOr(const char* name, const char* fallback) {
  const char* value = std::getenv(name);
  return (value != nullptr && value[0] != '\0') ? value : fallback;
}

void appendFile(const char* path, const char* data, size_t len) {
  if (path == nullptr || path[0] == '\0') {
    return;
  }
  std::ofstream out(path, std::ios::app);
  out.write(data, static_cast<std::streamsize>(len));
}

void writeFile(const char* path, const std::string& body) {
  if (path == nullptr || path[0] == '\0') {
    return;
  }
  std::ofstream out(path, std::ios::trunc);
  out << body;
}

void loadEeprom() {
  if (eepromLoaded) {
    return;
  }
  eepromLoaded = true;
  std::ifstream in(envOr("EEPROM_FILE", ""), std::ios::binary);
  if (!in) {
    return;
  }
  in.read(reinterpret_cast<char*>(eeprom.data()), static_cast<std::streamsize>(eeprom.size()));
}

void saveEeprom() {
  const char* path = envOr("EEPROM_FILE", "");
  if (path[0] == '\0') {
    return;
  }
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(eeprom.data()), static_cast<std::streamsize>(eeprom.size()));
}

void setNonBlock(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace

HardwareSerial Serial;
EEPROMClass EEPROM;
WiFiClass WiFi;

void HostPrint::print(const char* value) {
  if (value == nullptr) {
    value = "";
  }
  writeBytes(value, strlen(value));
}

void HostPrint::print(int value) {
  const std::string text = std::to_string(value);
  writeBytes(text.c_str(), text.size());
}

void HostPrint::print(unsigned long value) {
  const std::string text = std::to_string(value);
  writeBytes(text.c_str(), text.size());
}

void HostPrint::print(const IPAddress& value) {
  value.printTo(*this);
}

void HostPrint::println() {
  writeBytes("\n", 1);
}

void HostPrint::println(const char* value) {
  print(value);
  println();
}

void HostPrint::println(int value) {
  print(value);
  println();
}

void HostPrint::println(unsigned long value) {
  print(value);
  println();
}

void HostPrint::println(const IPAddress& value) {
  print(value);
  println();
}

void IPAddress::printTo(HostPrint& out) const {
  out.print(static_cast<unsigned long>(octets[0]));
  out.print(".");
  out.print(static_cast<unsigned long>(octets[1]));
  out.print(".");
  out.print(static_cast<unsigned long>(octets[2]));
  out.print(".");
  out.print(static_cast<unsigned long>(octets[3]));
}

void HardwareSerial::writeBytes(const char* data, size_t len) {
  fwrite(data, 1, len, stdout);
  fflush(stdout);
  appendFile(envOr("SERIAL_LOG", ""), data, len);
}

void pinMode(uint8_t, uint8_t) {}

void digitalWrite(uint8_t pin, uint8_t value) {
  pins[pin] = value;
  std::ostringstream body;
  for (const auto& entry : pins) {
    body << static_cast<int>(entry.first) << '=' << static_cast<int>(entry.second) << '\n';
  }
  writeFile(envOr("PIN_FILE", ""), body.str());
}

int digitalRead(uint8_t pin) {
  const auto it = pins.find(pin);
  return it == pins.end() ? LOW : it->second;
}

unsigned long millis() {
  const auto now = std::chrono::steady_clock::now();
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
}

void delay(unsigned long) {}

void NVIC_SystemReset() {
  _exit(82);
}

void EEPROMClass::readBytes(int address, void* dest, size_t len) {
  loadEeprom();
  if (address < 0 || static_cast<size_t>(address) + len > eeprom.size()) {
    std::memset(dest, 0xFF, len);
    return;
  }
  std::memcpy(dest, eeprom.data() + address, len);
}

void EEPROMClass::writeBytes(int address, const void* src, size_t len) {
  loadEeprom();
  if (address < 0 || static_cast<size_t>(address) + len > eeprom.size()) {
    return;
  }
  std::memcpy(eeprom.data() + address, src, len);
  saveEeprom();
}

WiFiClient::WiFiClient(int clientFd) : fd(clientFd) {
  if (fd >= 0) {
    setNonBlock(fd);
  }
}

WiFiClient::WiFiClient(const WiFiClient& other) : fd(other.fd) {
  const_cast<WiFiClient&>(other).fd = -1;
}

WiFiClient& WiFiClient::operator=(const WiFiClient& other) {
  if (this != &other) {
    stop();
    fd = other.fd;
    const_cast<WiFiClient&>(other).fd = -1;
  }
  return *this;
}

WiFiClient::~WiFiClient() {
  stop();
}

WiFiClient::operator bool() const {
  return fd >= 0;
}

bool WiFiClient::connected() {
  if (fd < 0) {
    return false;
  }
  char peek = 0;
  const ssize_t n = recv(fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT);
  if (n == 0) {
    return false;
  }
  return true;
}

int WiFiClient::available() {
  if (fd < 0) {
    return 0;
  }
  int count = 0;
  if (ioctl(fd, FIONREAD, &count) < 0) {
    return 0;
  }
  return count;
}

int WiFiClient::read() {
  if (fd < 0) {
    return -1;
  }
  unsigned char byte = 0;
  const ssize_t n = recv(fd, &byte, 1, 0);
  return n == 1 ? byte : -1;
}

void WiFiClient::stop() {
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
    fd = -1;
  }
}

void WiFiClient::writeBytes(const char* data, size_t len) {
  if (fd < 0 || data == nullptr || len == 0) {
    return;
  }
  size_t sent = 0;
  while (sent < len) {
    const ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
    if (n < 0) {
      break;
    }
    sent += static_cast<size_t>(n);
  }
}

WiFiServer::WiFiServer(uint16_t requested) : port(requested) {
  const char* overridePort = std::getenv("HTTP_PORT");
  if (overridePort != nullptr && overridePort[0] != '\0') {
    port = static_cast<uint16_t>(std::atoi(overridePort));
  }
}

void WiFiServer::begin() {
  if (listenFd >= 0) {
    close(listenFd);
    listenFd = -1;
  }
  listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd < 0) {
    return;
  }
  int yes = 1;
  setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(listenFd);
    listenFd = -1;
    return;
  }
  listen(listenFd, 8);
  setNonBlock(listenFd);
}

WiFiClient WiFiServer::available() {
  if (listenFd < 0) {
    return WiFiClient();
  }
  const int clientFd = accept(listenFd, nullptr, nullptr);
  if (clientFd < 0) {
    return WiFiClient();
  }
  return WiFiClient(clientFd);
}

int WiFiClass::status() {
  if (std::strcmp(envOr("WIFI_MODULE", "ok"), "none") == 0) {
    return WL_NO_MODULE;
  }
  return wifiStatus;
}

int WiFiClass::beginAP(const char*) {
  wifiStatus = WL_AP_LISTENING;
  wifiIp = IPAddress(192, 168, 4, 1);
  return WL_AP_LISTENING;
}

int WiFiClass::begin(const char*, const char*) {
  if (std::strcmp(envOr("WIFI_SIM", "ok"), "fail") == 0) {
    wifiStatus = WL_CONNECT_FAILED;
    wifiIp = IPAddress();
    return WL_CONNECT_FAILED;
  }
  wifiStatus = WL_CONNECTED;
  wifiIp = IPAddress(192, 168, 1, 50);
  return WL_CONNECTED;
}

void WiFiClass::disconnect() {
  wifiStatus = WL_IDLE_STATUS;
  wifiIp = IPAddress();
}

void WiFiClass::setTimeout(unsigned long) {}

void WiFiClass::setHostname(const char*) {}

IPAddress WiFiClass::localIP() {
  return wifiIp;
}
