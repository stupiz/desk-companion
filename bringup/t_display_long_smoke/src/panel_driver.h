#pragma once

#include <stdint.h>

namespace smoke_panel {

static constexpr uint16_t kPanelWidth = 180;
static constexpr uint16_t kPanelHeight = 640;
static constexpr uint16_t kLogicalWidth = 640;
static constexpr uint16_t kLogicalHeight = 180;

bool begin();
bool pushNative(const uint16_t* pixels, uint16_t width, uint16_t height);

}  // namespace smoke_panel
