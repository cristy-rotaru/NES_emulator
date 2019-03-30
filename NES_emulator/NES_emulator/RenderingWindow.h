#pragma once

#include <cstdint>

#define EVENT_NONE (0u)
#define EVENT_BUTTON_CLOSE_PRESSED (1u)

namespace RW
{
	void init(); // must always be called first
	void setPixel(size_t x_coordinate, size_t y_coordinate, uint32_t color);
	uint8_t pollWindowEvent();
	void redraw(bool forced = false);
	void dispose();
}