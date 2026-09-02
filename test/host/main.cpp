// 宿主入口：调用草图的 setup()，然后一直 loop()。
#include <unistd.h>

void setup();
void loop();

int main() {
  setup();
  for (;;) {
    loop();
    usleep(2000);
  }
}
