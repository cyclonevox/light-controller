// 配网和网页服务：读 EEPROM、开热点或连路由、处理 /save 和 /api/*。
#pragma once

bool netBegin();
void netBoot();
void netPoll();
