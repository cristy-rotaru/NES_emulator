#include "CartridgeReader.h"

#include <fstream>

static uint8_t CR_mirroring = 0;
static uint8_t CR_mapper = 0;
static bool CR_hasBatteryBackedRAM = false;
static uint8_t *CR_PRGROM = nullptr;
static uint8_t *CR_CHRROM = nullptr;
static uint8_t CR_PRGBankCount = 0;
static uint8_t CR_CHRBankCount = 0;
static uint8_t CR_systemType = 0;

namespace CR
{
	uint8_t loadFile(std::string file_name)
	{
		std::ifstream nesRom(file_name, std::ios_base::in | std::ios_base::binary);
		if (!nesRom)
		{
			return (STATUS_CR_LOAD_FILE_PROTECTED_OR_NONEXISTENT);
		}

		uint8_t romHeader[16]; // the NES file has a 16byte header
		if (!nesRom.read((char*)romHeader, 16))
		{
			return (STATUS_CR_LOAD_UNABLE_TO_READ_FROM_FILE);
		}

		if (romHeader[0] != 'N' || romHeader[1] != 'E' || romHeader[2] != 'S' || romHeader[3] != '\x1A')
		{
			return (STATUS_CR_LOAD_WRONG_FILE_FORMAT);
		}

		CR_PRGBankCount = romHeader[4]; // the number of 16KB brogram banks
		if (!CR_PRGBankCount)
		{
			return (STATUS_CR_LOAD_NO_PROGRAM_ROM_BANKS);
		}

		CR_CHRBankCount = romHeader[5]; // the number of 8KB video-rom banks

		if (romHeader[6] & 0x04) // has trainer at $7000-$71FF
		{
			return (STATUS_CR_LOAD_ROM_HAS_TRAINER);
		}

		if (romHeader[9] & 0x01)
		{
			CR_systemType = SYSTEM_PAL;
		}
		else
		{
			CR_systemType = SYSTEM_NTSC;
		}

		CR_hasBatteryBackedRAM = (romHeader[6] & 0x02) ? true : false;
		CR_mirroring = romHeader[6] & 0x09;
		CR_mapper = ((romHeader[6] >> 4) & 0x0F) | (romHeader[7] & 0xF0);

		CR_PRGROM = new uint8_t[CR_PRGBankCount * 0x4000];
		CR_CHRROM = CR_CHRBankCount ? new uint8_t[CR_CHRBankCount * 0x2000] : nullptr;

		if (!CR_PRGROM || (CR_CHRBankCount && !CR_CHRROM))
		{
			return (STATUS_CR_LOAD_MEMORY_ALLOCATION_FAILURE);
		}

		if (!nesRom.read((char*)CR_PRGROM, CR_PRGBankCount * 0x4000))
		{
			return (STATUS_CR_LOAD_UNABLE_TO_READ_FROM_FILE);
		}

		if (CR_CHRBankCount)
		{
			if (!nesRom.read((char*)CR_CHRROM, CR_CHRBankCount * 0x2000))
			{
				return (STATUS_CR_LOAD_UNABLE_TO_READ_FROM_FILE);
			}
		}

		return (STATUS_CR_LOAD_SUCCESS);
	}

	uint8_t* getROM()
	{
		return (CR_PRGROM);
	}

	uint8_t* getVideoROM()
	{
		return (CR_CHRROM);
	}

	uint8_t getROMBankCount()
	{
		return (CR_PRGBankCount);
	}

	uint8_t getVROMBankCount()
	{
		return (CR_CHRBankCount);
	}

	uint8_t getNameTableMirroring()
	{
		return (CR_mirroring);
	}

	uint8_t getMapperType()
	{
		return (CR_mapper);
	}

	uint8_t getSystemType()
	{
		return (CR_systemType);
	}

	bool getBatteryBackedRAMAvailability()
	{
		return (CR_hasBatteryBackedRAM);
	}

	void clean()
	{
		if (CR_PRGROM)
		{
			delete[] CR_PRGROM;
		}

		if (CR_CHRROM)
		{
			delete[] CR_CHRROM;
		}
	}
}