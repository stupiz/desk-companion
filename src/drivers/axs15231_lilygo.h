#pragma once

#include <stdint.h>

namespace lilygo_display {

bool begin();
void setRotation(uint8_t rotation);
bool present(uint16_t* framebuffer, uint16_t width, uint16_t height);

}  // namespace lilygo_display
