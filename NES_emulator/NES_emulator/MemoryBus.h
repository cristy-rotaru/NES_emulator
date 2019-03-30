#pragma once

#include <cstdint>

namespace MB
{
	void init();
	bool loadMapperInformation(); // mapper and cartridge must be already initialized
	void changeMirroring(uint8_t mirroring);
	void configureMemory(bool enable_external_RAM, bool protect_external_RAM);
	void writeMainBus(uint16_t address, uint8_t data);
	uint8_t readMainBus(uint16_t address);
	void writePictureBus(uint16_t address, uint8_t data);
	uint8_t readPictureBus(uint16_t address);
	void clean();
}