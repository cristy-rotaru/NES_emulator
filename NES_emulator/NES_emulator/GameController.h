#pragma once

#include <cstdint>

namespace GC
{
	void init();
	void strobe(uint8_t s);
	uint8_t readController1();
	uint8_t readController2();
}