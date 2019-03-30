#pragma once

#include <cstdint>

namespace PPU
{
	void reset();
	void registerScanlineCallback(void(*callback)(bool vblank));
	void step();
	void writeRegisterControl(uint8_t value);
	void writeRegisterMask(uint8_t value);
	void writeRegisterSpriteAddress(uint8_t value);
	void writeRegisterSpriteData(uint8_t value);
	void writeRegisterScroll(uint8_t value);
	void writeRegisterAddress(uint8_t value);
	void writeRegisterData(uint8_t value);
	uint8_t readRegisterControl();
	uint8_t readRegisterMask();
	uint8_t readRegisterStatus();
	uint8_t readRegisterSpriteData();
	uint8_t readRegisterData();
	void executeDMA(uint8_t *page_pointer);
}