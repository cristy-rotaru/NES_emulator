#pragma once

#include <cstdint>

#define INTERRUPT_IRQ (0u)
#define INTERRUPT_NMI (1u)
#define INTERRUPT_BRK (2u)

namespace CPU
{
	void reset(); // MemoryBus and MemoryMapper have to be already initialized
	void step();
	void causeInterrupt(uint8_t interrupt);
	void skipCyclesForDMA();
}