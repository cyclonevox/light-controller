// 给宿主模拟预写 EEPROM：state / ssid / pass / 双击 Reset 标志。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "config.h"

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr, "usage: write_eeprom FILE state ssid pass bootflag\n");
    return 2;
  }

  WifiCreds creds{};
  creds.state = std::atoi(argv[2]);
  std::strncpy(creds.ssid, argv[3], sizeof(creds.ssid) - 1);
  std::strncpy(creds.pass, argv[4], sizeof(creds.pass) - 1);

  BootFlag flag = static_cast<BootFlag>(std::atoi(argv[5]));
  std::vector<unsigned char> buf(sizeof(WifiCreds) + sizeof(BootFlag), 0xFF);
  std::memcpy(buf.data(), &creds, sizeof(creds));
  std::memcpy(buf.data() + sizeof(WifiCreds), &flag, sizeof(flag));

  FILE* file = std::fopen(argv[1], "wb");
  if (file == nullptr) {
    std::perror(argv[1]);
    return 1;
  }
  std::fwrite(buf.data(), 1, buf.size(), file);
  std::fclose(file);
  return 0;
}
