#pragma once

#include <cstdint>

namespace AD
{
	void init();
	void queueSample(uint16_t sample);
	bool bufferFull();
	void dispose();
}