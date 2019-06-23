#include "AudioDevice.h"
#include "AudioProcessingUnit.h"
#include "CartridgeReader.h"
#include "CentralProcessingUnit.h"
#include "GameController.h"
#include "MemoryBus.h"
#include "MemoryMapper.h"
#include "PictureProcessingUnit.h"
#include "RenderingWindow.h"

#include <iostream>

#define CLOCK_DIVIDER_CPU_NTSC (12u)
#define CLOCK_DIVIDER_APU_NTSC (24u)
#define CLOCK_DIVIDER_PPU_NTSC (4u)

#define CLOCK_DIVIDER_CPU_PAL (16u)
#define CLOCK_DIVIDER_APU_PAL (32u)
#define CLOCK_DIVIDER_PPU_PAL (5u)

#define min(x1, x2) ((x1) < (x2) ? (x1) : (x2))

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		std::string path = argv[1];
		uint8_t code = 0;
		if (code = CR::loadFile(path))
		{
			std::cout << "Error loading ROM file. Code: " << (int)code << std::endl;

			return (1);
		}
	}
	else
	{
		std::cout << "One argument expected: path to ROM file" << std::endl;

		getchar();
		return (1);
	}

	MM::init();
	if (!MM::setMapper(CR::getMapperType()))
	{
		std::cout << "Mapper " << (int)CR::getMapperType() << " not supported" << std::endl;

		getchar();
		return (1);
	}

	MB::init();
	if (!MB::loadMapperInformation())
	{
		std::cout << "An error accured while loading mapper info" << std::endl;

		getchar();
		return (1);
	}

	RW::init();
	GC::init();
	AD::init();

	APU::reset();
	CPU::reset();
	PPU::reset();

	uint8_t cpuDivider, apuDivider, ppuDivider;
	uint8_t cpuCountdown, apuCountdown, ppuCountdown;

	if (CR::getSystemType() == SYSTEM_NTSC)
	{
		cpuDivider = CLOCK_DIVIDER_CPU_NTSC;
		apuDivider = CLOCK_DIVIDER_APU_NTSC;
		ppuDivider = CLOCK_DIVIDER_PPU_NTSC;
	}
	else
	{ // PAL system
		cpuDivider = CLOCK_DIVIDER_CPU_PAL;
		apuDivider = CLOCK_DIVIDER_APU_PAL;
		ppuDivider = CLOCK_DIVIDER_PPU_PAL;
	}

	cpuCountdown = cpuDivider;
	apuCountdown = apuDivider;
	ppuCountdown = ppuDivider;

	for (;;) // program loop
	{
		uint8_t windowEvent = RW::pollWindowEvent();
		if (windowEvent == EVENT_BUTTON_CLOSE_PRESSED)
		{
			break;
		}

		/* The audio device is used to keep track of time */
		while (AD::bufferFull() == false)
		{
			uint8_t step = min(cpuCountdown, apuCountdown);
			step = min(step, ppuCountdown);

			cpuCountdown -= step;
			apuCountdown -= step;
			ppuCountdown -= step;

			if (cpuCountdown == 0)
			{
				CPU::step();
				cpuCountdown = cpuDivider;
			}

			if (apuCountdown == 0)
			{
				APU::step();
				apuCountdown = apuDivider;
			}

			if (ppuCountdown == 0)
			{
				PPU::step();
				ppuCountdown = ppuDivider;
			}
		}
	}

	RW::dispose();
	AD::dispose();

	CR::clean();
	MB::clean();

	return (0);
}