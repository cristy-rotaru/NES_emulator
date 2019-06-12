#pragma once

#include <cstdint>
#include <string>

#define STATUS_CR_LOAD_SUCCESS (0x00u)
#define STATUS_CR_LOAD_FILE_PROTECTED_OR_NONEXISTENT (0x01u)
#define STATUS_CR_LOAD_UNABLE_TO_READ_FROM_FILE (0x02u)
#define STATUS_CR_LOAD_WRONG_FILE_FORMAT (0x04u)
#define STATUS_CR_LOAD_NO_PROGRAM_ROM_BANKS (0x08u)
#define STATUS_CR_LOAD_ROM_HAS_TRAINER (0x10u)
#define STATUS_CR_LOAD_MEMORY_ALLOCATION_FAILURE (0x20u)

#define SYSTEM_NTSC (0u)
#define SYSTEM_PAL (1u)

namespace CR
{
	uint8_t loadFile(std::string file_name);
	uint8_t* getROM();
	uint8_t* getVideoROM();
	uint8_t getROMBankCount();
	uint8_t getVROMBankCount();
	uint8_t getNameTableMirroring();
	uint8_t getMapperType();
	uint8_t getSystemType();
	bool getBatteryBackedRAMAvailability();
	void clean();
}