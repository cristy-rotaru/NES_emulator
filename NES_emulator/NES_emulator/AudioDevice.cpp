#include "debug.h"

#include "AudioDevice.h"

#define SDL_MAIN_HANDLED //required by the audio library
#include <SDL.h>
#include <Windows.h>

#include <thread>
#include <mutex>

#define AD__SAMPLE_RATE (48000u)

#define AD__SAMPLE_COUNT (1024u)
#define AD__SAMPLE_REDUNDANCY (288u)

static volatile bool AD_shutdown;

static std::thread *AD_streamLoader;
static std::mutex AD_streamMutex;

static uint16_t AD_sampleBuffer[AD__SAMPLE_COUNT + AD__SAMPLE_REDUNDANCY];
static int16_t AD_samplesInBuffer;

void AD_streamLoader__loop();
void AD_SDL__callback(void *userData, uint8_t *stream, int len);

namespace AD
{
	void init()
	{
		AD_shutdown = false;
		AD_samplesInBuffer = 0;

		AD_streamLoader = new std::thread(AD_streamLoader__loop);
		AD_streamLoader->detach();

		Sleep(200); // makes sure audio in on when exiting
	}

	void queueSample(uint16_t sample)
	{
		std::lock_guard<std::mutex> mutexGuard(AD_streamMutex);

		if (AD_samplesInBuffer < (AD__SAMPLE_COUNT + AD__SAMPLE_REDUNDANCY))
		{
			AD_sampleBuffer[AD_samplesInBuffer] = sample;
			++AD_samplesInBuffer;
		}
	}

	bool bufferFull()
	{
		if (AD_samplesInBuffer < (AD__SAMPLE_COUNT + AD__SAMPLE_REDUNDANCY))
		{
			return (false);
		}
		else
		{
			return (true);
		}
	}

	void dispose()
	{
		AD_shutdown = true;
	}
}

void AD_streamLoader__loop()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		debug("Could not initialize audio library\n", DEBUG_LEVEL_WARNING);
		return;
	}

	SDL_AudioSpec request, created;

	request.freq = AD__SAMPLE_RATE;
	request.format = AUDIO_U16;
	request.channels = 1;
	request.samples = AD__SAMPLE_COUNT;
	request.callback = AD_SDL__callback;
	request.userdata = nullptr;

	SDL_OpenAudio(&request, &created);
	SDL_PauseAudio(0); // starts audio

	while (AD_shutdown == false);

	SDL_PauseAudio(1); // stops audio
	SDL_CloseAudio();
}

void AD_SDL__callback(void *userData, uint8_t *stream, int len)
{
	std::lock_guard<std::mutex> mutexGuard(AD_streamMutex);

	if (AD_samplesInBuffer < len / 2)
	{
		debug(AD_samplesInBuffer << " " << len, DEBUG_LEVEL_INFO);
	}

	memcpy(stream, AD_sampleBuffer, len);
	AD_samplesInBuffer -= len / sizeof(uint16_t);

	if (AD_samplesInBuffer > 0)
	{
		memcpy(AD_sampleBuffer, &AD_sampleBuffer[len / sizeof(uint16_t)], AD_samplesInBuffer * sizeof(uint16_t));
	}

	if (AD_samplesInBuffer < 0)
	{
		AD_samplesInBuffer = 0;
	}
}