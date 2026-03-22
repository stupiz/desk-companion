#pragma once

#include <Arduino.h>

#if defined(DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG)
#include <Arduino_GFX_Library.h>
#else
class Arduino_GFX;
#endif

namespace display {

bool begin();
bool isReady();
Arduino_GFX* gfx();
void present();
uint16_t width();
uint16_t height();
uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
void clear(uint16_t color = 0x0000);
void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 1, uint16_t bg = 0x0000);
void drawTextCentered(const char* text, int16_t centerX, int16_t y, uint16_t color, uint8_t size = 1,
                      uint16_t bg = 0x0000);
void drawTextRight(const char* text, int16_t rightX, int16_t y, uint16_t color, uint8_t size = 1,
                   uint16_t bg = 0x0000);
int16_t textWidth(const char* text, uint8_t size = 1);
int16_t textHeight(uint8_t size = 1);

}  // namespace display
