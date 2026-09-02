// 宿主 EEPROM：读写 EEPROM_FILE 里的一块内存镜像。
#pragma once

#include "Arduino.h"

class EEPROMClass {
 public:
  template <typename T>
  T& get(int address, T& value) {
    readBytes(address, &value, sizeof(T));
    return value;
  }

  template <typename T>
  const T& put(int address, const T& value) {
    writeBytes(address, &value, sizeof(T));
    return value;
  }

 private:
  void readBytes(int address, void* dest, size_t len);
  void writeBytes(int address, const void* src, size_t len);
};

extern EEPROMClass EEPROM;
