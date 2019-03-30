#pragma once

#include <cstdint>

#define INTERRUPT_IRQ (0u)
#define INTERRUPT_NMI (1u)
#define INTERRUPT_BRK (2u)

#define INTERRUPT_SOURCE_NONE (0x00u)
#define INTERRUPT_SOURCE_APU (0x01u)
#define INTERRUPT_SOURCE_MM (0x02u)

namespace CPU
{
	void reset(); // MemoryBus and MemoryMapper have to be already initialized
	void step();
	void pullInterruptPin(uint8_t source);
	void releaseInterruptPin(uint8_t source);
	void causeInterrupt(uint8_t interrupt);
	void skipCyclesForDMA();
}