// 继电器和 LED 点阵。net 只通过这里开关灯、滚文字，不直接碰 GPIO。
#pragma once

void boardBegin();
void setLight(bool on);
void toggleLight();
bool isLightOn();
void displayScroll(const char* text);
void displayPoll();
