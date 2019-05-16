#pragma once

#include <cstdint>

namespace APU
{
	void reset();
	void step();
	void writeRegisterSQ1Volume(uint8_t value);
	void writeRegisterSQ1Sweep(uint8_t value);
	void writeRegisterSQ1PeriodLow(uint8_t value);
	void writeRegisterSQ1PeriodHigh(uint8_t value);
	void writeRegisterSQ2Volume(uint8_t value);
	void writeRegisterSQ2Sweep(uint8_t value);
	void writeRegisterSQ2PeriodLow(uint8_t value);
	void writeRegisterSQ2PeriodHigh(uint8_t value);
	void writeRegisterTriangleLinearCounter(uint8_t value);
	void writeRegisterTriangleTimerLow(uint8_t value);
	void writeRegisterTriangleTimerHigh(uint8_t value);
	void writeRegisterNoiseVolume(uint8_t value);
	void writeRegisterNoiseMode(uint8_t value);
	void writeRegisterNoiseLength(uint8_t value);
	void writeRegisterDMCFrequency(uint8_t value);
	void writeRegisterDMCRaw(uint8_t value);
	void writeRegisterDMCAddress(uint8_t value);
	void writeRegisterDMCLength(uint8_t value);
	void writeRegisterChannels(uint8_t value);
	void writeRegisterFrameCounter(uint8_t value);
	uint8_t readRegisterStatus();
}