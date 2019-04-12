#include "MemoryBus.h"

#include "MemoryMapper.h"
#include "CartridgeReader.h"
#include "GameController.h"
#include "AudioProcessingUnit.h"
#include "CentralProcessingUnit.h"
#include "PictureProcessingUnit.h"

#include <stdlib.h>
#include <time.h>

#define MB__NAME_TABLE_HORIZONTAL (0x00u)
#define MB__NAME_TABLE_VERTICAL (0x01u)
#define MB__NAME_TABLE_ONE_SCREEN_HIGHER (0x08u)
#define MB__NAME_TABLE_ONE_SCREEN_LOWER (0x09u)

#define MB__REGISTER_PPU_CONTROL (0x2000u) 
#define MB__REGISTER_PPU_MASK (0x2001u)
#define MB__REGISTER_PPU_STATUS (0x2002u)
#define MB__REGISTER_PPU_SPRITE_ADDRESS (0x2003u)
#define MB__REGISTER_PPU_SPRITE_DATA (0x2004u)
#define MB__REGISTER_PPU_SCROLL (0x2005u)
#define MB__REGISTER_PPU_ADDRESS (0x2006u)
#define MB__REGISTER_PPU_DATA (0x2007u)
#define MB__REGISTER_APU_SQ1_VOLUME (0x4000u)
#define MB__REGISTER_APU_SQ1_SWEEP (0x4001u)
#define MB__REGISTER_APU_SQ1_PERIOD_LO (0x4002u)
#define MB__REGISTER_APU_SQ1_PERIOD_HI (0x4003u)
#define MB__REGISTER_APU_SQ2_VOLUME (0x4004u)
#define MB__REGISTER_APU_SQ2_SWEEP (0x4005u)
#define MB__REGISTER_APU_SQ2_PERIOD_LO (0x4006u)
#define MB__REGISTER_APU_SQ2_PERIOD_HI (0x4007u)
#define MB__REGISTER_APU_TRIANGLE_LINEAR_COUNTER (0x4008u)
#define MB__REGISTER_APU_TRIANGLE_PERIOD_LO (0x400Au)
#define MB__REGISTER_APU_TRIANGLE_PERIOD_HI (0x400Bu)
#define MB__REGISTER_APU_NOISE_VOLUME (0x400Cu)
#define MB__REGISTER_APU_NOISE_LOOP (0x400Eu)
#define MB__REGISTER_APU_NOISE_LENGTH (0x400Fu)
#define MB__REGISTER_APU_DMC_FREQUENCY (0x4010u)
#define MB__REGISTER_APU_DMC_RAW (0x4011u)
#define MB__REGISTER_APU_DMC_START (0x4012u)
#define MB__REGISTER_APU_DMC_LENGTH (0x4013u)
#define MB__REGISTER_PPU_SPRITE_DMA (0x4014u)
#define MB__REGISTER_APU_CHANNELS (0x4015u)
#define MB__REGISTER_JOY1 (0x4016u) // R
#define MB__REGISTER_JOY2 (0x4017u) // R
#define MB__REGISTER_JOY_STROBE (0x4016u) // W
#define MB__REGISTER_APU_FRAME_COUNTER_CONTROL (0x4017u) // W

static uint8_t MB_RAM[0x0800]; // mirrored 4 times
static uint8_t MB_vRAM[0x0800];
static uint8_t *MB_externalRAM = nullptr;
static uint8_t MB_palette[0x20];

static bool MB_enableRAM = true;
static bool MB_protectRAM = false;

static std::size_t MB_nameTable[4];

namespace MB
{
	void init()
	{
		srand(clock());
	}

	bool loadMapperInformation()
	{
		if (CR::getBatteryBackedRAMAvailability())
		{
			MB_externalRAM = new uint8_t[0x2000];
			if (!MB_externalRAM)
			{
				return (false);
			}
		}

		switch (MM::getNameTableMirroring())
		{
			case MB__NAME_TABLE_HORIZONTAL:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0;
				MB_nameTable[2] = 0x400;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_VERTICAL:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0x400;
				MB_nameTable[2] = 0;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_ONE_SCREEN_HIGHER:
			{
				MB_nameTable[0] = 0x400;
				MB_nameTable[1] = 0x400;
				MB_nameTable[2] = 0x400;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_ONE_SCREEN_LOWER:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0;
				MB_nameTable[2] = 0;
				MB_nameTable[3] = 0;

				break;
			}

			default:
			{
				return (false);
			}
		}

		return (true);
	}

	void changeMirroring(uint8_t mirroring)
	{
		switch (mirroring)
		{
			case MB__NAME_TABLE_HORIZONTAL:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0;
				MB_nameTable[2] = 0x400;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_VERTICAL:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0x400;
				MB_nameTable[2] = 0;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_ONE_SCREEN_HIGHER:
			{
				MB_nameTable[0] = 0x400;
				MB_nameTable[1] = 0x400;
				MB_nameTable[2] = 0x400;
				MB_nameTable[3] = 0x400;

				break;
			}

			case MB__NAME_TABLE_ONE_SCREEN_LOWER:
			{
				MB_nameTable[0] = 0;
				MB_nameTable[1] = 0;
				MB_nameTable[2] = 0;
				MB_nameTable[3] = 0;

				break;
			}
		}
	}

	void configureMemory(bool enable_external_RAM, bool protect_external_RAM)
	{
		MB_enableRAM = enable_external_RAM;
		MB_protectRAM = protect_external_RAM;
	}

	void writeMainBus(uint16_t address, uint8_t data)
	{
		if (address < 0x2000)
		{
			MB_RAM[address & 0x07FF] = data;
		}
		else if (address < 0x4000)
		{ // PPU registers mirrored
			switch (address & 0x2007)
			{
				case MB__REGISTER_PPU_CONTROL:
				{
					PPU::writeRegisterControl(data);

					break;
				}

				case MB__REGISTER_PPU_MASK:
				{
					PPU::writeRegisterMask(data);

					break;
				}

				case MB__REGISTER_PPU_STATUS:
				{
					break; // read-only register ... writing to it has no effect
				}

				case MB__REGISTER_PPU_SPRITE_ADDRESS:
				{
					PPU::writeRegisterSpriteAddress(data);

					break;
				}

				case MB__REGISTER_PPU_SPRITE_DATA:
				{
					PPU::writeRegisterSpriteData(data);

					break;
				}

				case MB__REGISTER_PPU_SCROLL:
				{
					PPU::writeRegisterScroll(data);

					break;
				}

				case MB__REGISTER_PPU_ADDRESS:
				{
					PPU::writeRegisterAddress(data);

					break;
				}

				case MB__REGISTER_PPU_DATA:
				{
					PPU::writeRegisterData(data);

					break;
				}
			}
		}
		else if (address < 0x5000)
		{
			switch (address)
			{
				case MB__REGISTER_APU_SQ1_VOLUME:
				{
					APU::writeRegisterSQ1Volume(data);

					break;
				}

				case MB__REGISTER_APU_SQ1_SWEEP:
				{
					APU::writeRegisterSQ1Sweep(data);

					break;
				}

				case MB__REGISTER_APU_SQ1_PERIOD_LO:
				{
					APU::writeRegisterSQ1PeriodLow(data);

					break;
				}

				case MB__REGISTER_APU_SQ1_PERIOD_HI:
				{
					APU::writeRegisterSQ1PeriodHigh(data);

					break;
				}

				case MB__REGISTER_APU_SQ2_VOLUME:
				{
					APU::writeRegisterSQ2Volume(data);

					break;
				}

				case MB__REGISTER_APU_SQ2_SWEEP:
				{
					APU::writeRegisterSQ2Sweep(data);

					break;
				}

				case MB__REGISTER_APU_SQ2_PERIOD_LO:
				{
					APU::writeRegisterSQ2PeriodLow(data);

					break;
				}

				case MB__REGISTER_APU_SQ2_PERIOD_HI:
				{
					APU::writeRegisterSQ2PeriodHigh(data);

					break;
				}

				case MB__REGISTER_APU_TRIANGLE_LINEAR_COUNTER:
				{
					APU::writeRegisterTriangleLinearCounter(data);

					break;
				}

				case MB__REGISTER_APU_TRIANGLE_PERIOD_LO:
				{
					APU::writeRegisterTriangleTimerLow(data);

					break;
				}

				case MB__REGISTER_APU_TRIANGLE_PERIOD_HI:
				{
					APU::writeRegisterTriangleTimerHigh(data);

					break;
				}

				case MB__REGISTER_APU_NOISE_VOLUME:
				{
					APU::writeRegisterNoiseVolume(data);

					break;
				}

				case MB__REGISTER_APU_NOISE_LOOP:
				{
					APU::writeRegisterNoiseLoop(data);

					break;
				}

				case MB__REGISTER_APU_NOISE_LENGTH:
				{
					APU::writeRegisterNoiseLength(data);

					break;
				}

				case MB__REGISTER_APU_DMC_FREQUENCY:
				case MB__REGISTER_APU_DMC_RAW:
				case MB__REGISTER_APU_DMC_START:
				case MB__REGISTER_APU_DMC_LENGTH:
				{ // sound registers ... TBD
					break;
				}

				case MB__REGISTER_PPU_SPRITE_DMA:
				{
					CPU::skipCyclesForDMA();

					uint16_t address = (uint16_t)data << 8;
					uint8_t *pagePointer = nullptr;

					if (address < 0x2000)
					{
						pagePointer = &MB_RAM[address & 0x07FF];
					}
					else if (address < 0x5000)
					{
						pagePointer = nullptr; // register access is not permitted
					}
					else if (address < 0x6000)
					{
						pagePointer = nullptr; // extended ROM access is not allowed
					}
					else if (address < 0x8000)
					{
						if (MB_externalRAM)
						{
							pagePointer = &MB_externalRAM[address - 0x6000];
						}
						else
						{
							pagePointer = nullptr;
						}
					}
					else
					{
						pagePointer = nullptr;
					}

					PPU::executeDMA(pagePointer);

					break;
				}

				case MB__REGISTER_APU_CHANNELS:
				{
					APU::writeRegisterChannels(data);

					break;
				}

				case MB__REGISTER_JOY_STROBE:
				{
					GC::strobe(data);

					break;
				}

				case MB__REGISTER_APU_FRAME_COUNTER_CONTROL:
				{
					APU::writeRegisterFrameCounter(data);

					break;
				}

				default:
				{
					break; // other registers have no function
				}
			}
		}
		else if (address < 0x6000)
		{
			return; // access to expanded ROM not allowed
		}
		else if (address < 0x8000)
		{
			if (MB_externalRAM && MB_enableRAM && !MB_protectRAM)
			{
				MB_externalRAM[address - 0x6000] = data;
			}
		}
		else
		{
			MM::writePRG(address, data);
		}
	}

	uint8_t readMainBus(uint16_t address)
	{
		if (address < 0x2000)
		{
			return (MB_RAM[address & 0x07FF]);
		}
		else if (address < 0x4000)
		{
			switch (address & 0x2007)
			{
				case MB__REGISTER_PPU_CONTROL:
				{
					return (PPU::readRegisterControl());
				}

				case MB__REGISTER_PPU_MASK:
				{
					return (PPU::readRegisterMask());
				}

				case MB__REGISTER_PPU_STATUS:
				{
					return (PPU::readRegisterStatus());
				}

				case MB__REGISTER_PPU_SPRITE_ADDRESS:
				{
					return (0x00); // write-only register
				}

				case MB__REGISTER_PPU_SPRITE_DATA:
				{
					return (PPU::readRegisterSpriteData());
				}

				case MB__REGISTER_PPU_SCROLL:
				case MB__REGISTER_PPU_ADDRESS:
				{
					return (0x00); // write-only register
				}

				case MB__REGISTER_PPU_DATA:
				{
					return (PPU::readRegisterData());
				}
			}
		}
		else if (address < 0x5000)
		{
			switch (address)
			{
				case MB__REGISTER_APU_SQ1_VOLUME:
				case MB__REGISTER_APU_SQ1_SWEEP:
				case MB__REGISTER_APU_SQ1_PERIOD_LO:
				case MB__REGISTER_APU_SQ1_PERIOD_HI:
				case MB__REGISTER_APU_SQ2_VOLUME:
				case MB__REGISTER_APU_SQ2_SWEEP:
				case MB__REGISTER_APU_SQ2_PERIOD_LO:
				case MB__REGISTER_APU_SQ2_PERIOD_HI:
				case MB__REGISTER_APU_TRIANGLE_LINEAR_COUNTER:
				case MB__REGISTER_APU_TRIANGLE_PERIOD_LO:
				case MB__REGISTER_APU_TRIANGLE_PERIOD_HI:
				case MB__REGISTER_APU_NOISE_VOLUME:
				case MB__REGISTER_APU_NOISE_LOOP:
				case MB__REGISTER_APU_NOISE_LENGTH:
				case MB__REGISTER_APU_DMC_FREQUENCY:
				case MB__REGISTER_APU_DMC_RAW:
				case MB__REGISTER_APU_DMC_START:
				case MB__REGISTER_APU_DMC_LENGTH:
				{ // sound registers ... TBD
					return (0x00);
				}

				case MB__REGISTER_PPU_SPRITE_DMA:
				{
					return (0x00); // write-only register
				}

				case MB__REGISTER_APU_CHANNELS:
				{ // sound registers ... TBD
					return (APU::readRegisterStatus());
				}

				case MB__REGISTER_JOY1:
				{
					return (GC::readController1());
				}

				case MB__REGISTER_JOY2:
				{
					return (GC::readController2());
				}

				default:
				{
					return (0x00); // unused memory
				}
			}
		}
		else if (address < 0x6000)
		{
			return (0x00); // expantion not available
		}
		else if (address < 0x8000)
		{
			if (MB_externalRAM)
			{
				if (MB_enableRAM)
				{
					return (MB_externalRAM[address - 0x6000]);
				}
				else
				{
					return ((uint8_t)rand());
					// open bus behaviour
					// used by some games to generate seeds for pseudorandom
				}
			}
			else
			{
				return (0x00); // in case external RAM is not available
			}
		}
		else
		{
			return (MM::readPRG(address));
		}

		return (0x00);
	}

	void writePictureBus(uint16_t address, uint8_t data)
	{
		if (address < 0x2000)
		{
			MM::writeCHR(address, data);
		}
		else if (address < 0x3F00) // here
		{
			uint8_t index = (address - 0x2000) / 0x0400;
			index %= 4;

			MB_vRAM[MB_nameTable[index] + (address & 0x03FF)] = data;
		}
		else if (address < 0x4000) // here
		{
			if (address == 0x3F10)
			{
				MB_palette[0] = data;
			}
			else
			{
				MB_palette[address & 0x1F] = data;
			}
		}
	}

	uint8_t readPictureBus(uint16_t address)
	{
		if (address < 0x2000)
		{
			return (MM::readCHR(address));
		}
		else if (address < 0x3F00) // here
		{
			uint8_t index = (address - 0x2000) / 0x0400;
			index %= 4;

			return (MB_vRAM[MB_nameTable[index] + (address & 0x03FF)]);
		}
		else if (address < 0x4000) // here
		{
			return (MB_palette[address & 0x1F]);
		}
		else
		{
			return (0x00);
		}
	}

	void clean()
	{
		if (MB_externalRAM)
		{
			delete[] MB_externalRAM;
		}
	}
}