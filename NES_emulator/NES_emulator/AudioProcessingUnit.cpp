#include "AudioProcessingUnit.h"

#include "CartridgeReader.h"
#include "AudioDevice.h"
#include "CentralProcessingUnit.h"
#include "MemoryBus.h"

#define APU__SEQUENCE_STEP1 (3728u)
#define APU__SEQUENCE_STEP2 (7456u)
#define APU__SEQUENCE_STEP3 (11185u)
#define APU__SEQUENCE_STEP4 (14914u)
#define APU__SEQUENCE_STEP5 (18640u)

#define APU__CHANNEL_PULSE1 (0x01u)
#define APU__CHANNEL_PULSE2 (0x02u)
#define APU__CHANNEL_TRIANGLE (0x04u)
#define APU__CHANNEL_NOISE (0x08u)
#define APU__CHANNEL_DMC (0x10u)

#define APU__5FRAME_SEQUENCE (0x80u)
#define APU__DISABLE_INTERRUPT (0x40u)

#define APU__INTERRUPT_DMC (0x80u)
#define APU__INTERRUPT_FRAME (0x40u)

#define APU__PULSE_DUTY_CYCLE_MASK (0xC0u)
#define APU__PULSE_LENGTH_COUNTER_HALT_MASK (0x20u)
#define APU__PULSE_CONSTANT_VOLUME_ENVELOPE_FLAG_MASK (0x10u)
#define APU__PULSE_VOLUME_ENVELOPE_PERIOD_MASK (0x0Fu)
#define APU__PULSE_SWEEP_ENABLE_MASK (0x80u)
#define APU__PULSE_SWEEP_PERIOD_MASK (0x70u)
#define APU__PULSE_SWEEP_NEGATE_MASK (0x08u)
#define APU__PULSE_SWEEP_SHIFT_COUNT_MASK (0x07u)
#define APU__PULSE_TIMER_LOW_MASK (0x00FFu)
#define APU__PULSE_TIMER_HIGH_MASK (0x07u)
#define APU__PULSE_LENGTH_COUNTER_LOAD_MASK (0xF8u)

#define APU__PULSE_DUTY_CYCLE_SHIFT (6u)
#define APU__PULSE_VOLUME_ENVELOPE_PERIOD_SHIFT (0u)
#define APU__PULSE_SWEEP_PERIOD_SHIFT (5u)
#define APU__PULSE_SWEEP_SHIFT_COUNT_SHIFT (0u)
#define APU__PULSE_TIMER_HIGH_SHIFT (0u)
#define APU__PULSE_LENGTH_COUNTER_LOAD_SHIFT (3u)

#define APU__PULSE_DUTY_CYCLE_COUNT (4u)
#define APU__PULSE_DUTY_CYCLE_INITIAL_POSITION (0x80u)

#define APU__TRIANGLE_LINEAR_COUNTER_CONTROL_MASK (0x80u)
#define APU__TRIANGLE_LINEAR_COUNTER_RELOAD_MASK (0x7Fu)
#define APU__TRIANGLE_TIMER_LOW_MASK (0x00FFu)
#define APU__TRIANGLE_TIMER_HIGH_MASK (0x07u)
#define APU__TRIANGLE_LENGTH_COUNTER_LOAD_MASK (0xF8u)

#define APU__TRIANGLE_LINEAR_COUNTER_RELOAD_SHIFT (0u)
#define APU__TRIANGLE_TIMER_HIGH_SHIFT (0u)
#define APU__TRIANGLE_LENGTH_COUNTER_LOAD_SHIFT (3u)

#define APU__TRIANGLE_SEQUENCE_LENGTH (32u)

#define APU__NOISE_LENGTH_COUNTER_HALT_MASK (0x20u)
#define APU__NOISE_CONSTANT_VOLUME_ENVELOPE_FLAG_MASK (0x10u)
#define APU__NOISE_VOLUME_ENVELOPE_PERIOD_MASK (0x0Fu)
#define APU__NOISE_LOOP_MASK (0x80u)
#define APU__NOISE_PERIOD_MASK (0x0Fu)
#define APU__NOISE_LENGTH_COUNTER_LOAD_MASK (0xF8u)

#define APU__NOISE_VOLUME_ENVELOPE_PERIOD_SHIFT (0u)
#define APU__NOISE_PERIOD_SHIFT (0u)
#define APU__NOISE_LENGTH_COUNTER_LOAD_SHIFT (3u)

#define APU__NOISE_TIMER_PERIODS (16u)

#define APU__DMC_ENABLE_IRQ_MASK (0x80u)
#define APU__DMC_LOOP_MASK (0x40u)
#define APU__DMC_RATE_MASK (0x0Fu)
#define APU__DMC_RAW_SAMPLE_MASK (0x7Fu)

#define APU__DMC_RATE_SHIFT (0u)
#define APU__DMC_RAW_SAMPLE_SHIFT (0u)
#define APU__DMC_ADDRESS_SHIFT (6u)
#define APU__DMC_LENGTH_SHIFT (4u)

#define APU__DMC_RATE_VALUES_COUNT (16u)

#define APU__LENGTH_VALUES (32u)

#define APU__OUTPUT_VOLUME (0xFFFFu)

#define APU__WEIGHT_PULSE (0.00752f)
#define APU__WEIGHT_TRIANGLE (0.00851f)
#define APU__WEIGHT_NOISE (0.00494f)
#define APU__WEIGHT_DMC (0.00335f)

#define APU__GAUSS_VALUES_COUNT_NTSC (19u)
#define APU__GAUSS_VALUES_COUNT_PAL (17u)

static const float APU_gaussFilterValuesNTSC[APU__GAUSS_VALUES_COUNT_NTSC] = { 0.000004f, 0.000036f, 0.000272f, 0.001585f, 0.007035f, 0.023798f, 0.061393f, 0.120795f, 0.181297f, 0.207571f, 0.181297f, 0.120795f, 0.061393f, 0.023798f, 0.007035f, 0.001585f, 0.000272f, 0.000036f, 0.000004 };
static const float APU_gaussFilterValuesPAL[APU__GAUSS_VALUES_COUNT_PAL] = { 0.000005f, 0.000061f, 0.000542f, 0.003452f, 0.015696f, 0.050946f, 0.118092f, 0.195541f, 0.231330f, 0.195541f, 0.118092f, 0.050946f, 0.015696f, 0.003452f, 0.000542f, 0.000061f, 0.000005f };
static const float *APU_gaussFilterInUse;
static uint8_t APU_gaussFilterLength;

static constexpr uint8_t APU_lengthCounterLUT[APU__LENGTH_VALUES] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30 };

static constexpr uint8_t APU_pulseDutyCycles[APU__PULSE_DUTY_CYCLE_COUNT] = { 0b01000000, 0b01100000, 0b01111000, 0b10011111 };

static constexpr uint8_t APU_triangleSequence[APU__TRIANGLE_SEQUENCE_LENGTH] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static const uint16_t APU_noisePeriodsNTSC[APU__NOISE_TIMER_PERIODS] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
static const uint16_t APU_noisePeriodsPAL[APU__NOISE_TIMER_PERIODS] = { 4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778 };
static const uint16_t *APU_noisePeriods;

static const uint16_t APU_dmcRateNTSC[APU__DMC_RATE_VALUES_COUNT] = { 214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27 };
static const uint16_t APU_dmcRatePAL[APU__DMC_RATE_VALUES_COUNT] = { 199, 177, 158, 149, 138, 118, 105, 99, 88, 74, 66, 59, 49, 38, 33, 25 };
static const uint16_t *APU_dmcRates;

static uint16_t APU_cycleCount;
static uint8_t APU_frameSteps;
static bool APU_enableInterrupt;
static bool APU_setInterruptRequest;

static float APU_mixedSample;
static uint8_t APU_mixingPosition;

static bool APU_requestFrameIRQ;
static bool APU_requetsDMCIRQ;

static bool APU_channelPulse1Enable;
static bool APU_channelPulse2Enable;
static bool APU_channelTriangleEnable;
static bool APU_channelNoiseEnable;
static bool APU_channelDeltaSignaEnable;

static uint8_t APU_Pulse1_dutyCyclePosition;
static uint8_t APU_Pulse1_lengthCounter;
static uint8_t APU_Pulse1_envelope;
static uint8_t APU_Pulse1_envelopeDivider;
static uint8_t APU_Pulse1_sweepDivider;
static bool APU_Pulse1_resetEnvelope;
static bool APU_Pulse1_resetSweep;
static uint16_t APU_Pulse1_timerValue;
static bool APU_Pulse1_outputHigh;

static uint8_t APU_Pulse1_dutyCycle;
static bool APU_Pulse1_lengthCounterHalt;
static bool APU_Pulse1_constantVolume_envelopeFlag;
static uint8_t APU_Pulse1_volume_envelopePeriod;
static bool APU_Pulse1_sweepEnable;
static uint8_t APU_Pulse1_sweepPeriod;
static bool APU_Pulse1_sweepNegate;
static uint8_t APU_Pulse1_sweepShiftCount;
static uint16_t APU_Pulse1_timer;
static uint8_t APU_Pulse1_lengthCounterLoad;

static uint8_t APU_Pulse2_dutyCyclePosition;
static uint8_t APU_Pulse2_lengthCounter;
static uint8_t APU_Pulse2_envelope;
static uint8_t APU_Pulse2_envelopeDivider;
static uint8_t APU_Pulse2_sweepDivider;
static bool APU_Pulse2_resetEnvelope;
static bool APU_Pulse2_resetSweep;
static uint16_t APU_Pulse2_timerValue;
static bool APU_Pulse2_outputHigh;

static uint8_t APU_Pulse2_dutyCycle;
static bool APU_Pulse2_lengthCounterHalt;
static bool APU_Pulse2_constantVolume_envelopeFlag;
static uint8_t APU_Pulse2_volume_envelopePeriod;
static bool APU_Pulse2_sweepEnable;
static uint8_t APU_Pulse2_sweepPeriod;
static bool APU_Pulse2_sweepNegate;
static uint8_t APU_Pulse2_sweepShiftCount;
static uint16_t APU_Pulse2_timer;
static uint8_t APU_Pulse2_lengthCounterLoad;

static uint8_t APU_Triangle_lengthCounter;
static bool APU_Triangle_linearCounterHalt;
static uint8_t APU_Triangle_linearCounter;
static uint8_t APU_Triangle_currentSample;
static uint16_t APU_Triangle_timerValue;
static uint8_t APU_Triangle_sequencePosition;

static bool APU_Triangle_linearCounterControl;
static uint8_t APU_Triangle_linearCounterReload;
static uint16_t APU_Triangle_timer;
static uint8_t APU_Triangle_lengthCounterLoad;

static uint8_t APU_Noise_lengthCounter;
static uint16_t APU_Noise_shiftRegister;
static uint8_t APU_Noise_envelope;
static uint8_t APU_Noise_envelopeDivider;
static bool APU_Noise_resetEnvelope;
static uint8_t APU_Noise_currentSample;
static uint16_t APU_Noise_timerValue;

static bool APU_Noise_lengthCounterHalt;
static bool APU_Noise_constantVolume_envelopeFlag;
static uint8_t APU_Noise_volume_envelopePeriod;
static bool APU_Noise_loop;
static uint8_t APU_Noise_period;
static uint8_t APU_Noise_lengthCounterLoad;

static uint8_t APU_DMC_selectedRate;
static bool APU_DMC_enableIRQ;
static bool APU_DMC_loop;
static uint16_t APU_DMC_addressStart;
static uint16_t APU_DMC_sampleLength;

static uint16_t APU_DMC_rateCounter;
static uint16_t APU_DMC_currentAddress;
static uint8_t APU_DMC_counter;
static uint8_t APU_DMC_output;
static uint16_t APU_DMC_samplesRemaining;
static bool APU_DMC_stopped;
static uint8_t APU_DMC_shift;
static uint8_t APU_DMC_currentSample;

void APU_envelope__step();
void APU_triangle__step();
void APU_length__step();
void APU_sweep__step();

namespace APU
{
	void reset()
	{
		if (CR::getSystemType() == SYSTEM_NTSC)
		{
			APU_gaussFilterLength = APU__GAUSS_VALUES_COUNT_NTSC;
			APU_gaussFilterInUse = APU_gaussFilterValuesNTSC;

			APU_noisePeriods = APU_noisePeriodsNTSC;
			APU_dmcRates = APU_dmcRateNTSC;
		}
		else
		{
			APU_gaussFilterLength = APU__GAUSS_VALUES_COUNT_PAL;
			APU_gaussFilterInUse = APU_gaussFilterValuesPAL;

			APU_noisePeriods = APU_noisePeriodsPAL;
			APU_dmcRates = APU_dmcRatePAL;
		}

		APU_cycleCount = 0;
		APU_frameSteps = 4;
		APU_enableInterrupt = true;
		APU_setInterruptRequest = false;

		APU_mixedSample = 0.0f;
		APU_mixingPosition = 0;

		APU_requestFrameIRQ = false;
		APU_requetsDMCIRQ = false;

		APU_channelPulse1Enable = false;
		APU_channelPulse2Enable = false;
		APU_channelTriangleEnable = false;
		APU_channelNoiseEnable = false;
		APU_channelDeltaSignaEnable = false;

		/* Pulse1 */
		APU_Pulse1_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
		APU_Pulse1_lengthCounter = 0;
		APU_Pulse1_envelope = 0;
		APU_Pulse1_envelopeDivider = 0;
		APU_Pulse1_sweepDivider = 0;
		APU_Pulse1_resetEnvelope = true;
		APU_Pulse1_resetSweep = true;
		APU_Pulse1_timerValue = 0;
		APU_Pulse1_outputHigh = false;

		APU_Pulse1_dutyCycle = 0;
		APU_Pulse1_lengthCounterHalt = false;
		APU_Pulse1_constantVolume_envelopeFlag = false;
		APU_Pulse1_volume_envelopePeriod = 0;
		APU_Pulse1_sweepEnable = false;
		APU_Pulse1_sweepPeriod = 0;
		APU_Pulse1_sweepNegate = false;
		APU_Pulse1_sweepShiftCount = 0;
		APU_Pulse1_timer = 0;
		APU_Pulse1_lengthCounterLoad = 0;

		/* Pulse2 */
		APU_Pulse2_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
		APU_Pulse2_lengthCounter = 0;
		APU_Pulse2_envelope = 0;
		APU_Pulse2_envelopeDivider = 0;
		APU_Pulse2_sweepDivider = 0;
		APU_Pulse2_resetEnvelope = true;
		APU_Pulse2_resetSweep = true;
		APU_Pulse2_timerValue = 0;
		APU_Pulse2_outputHigh = false;

		APU_Pulse2_dutyCycle = 0;
		APU_Pulse2_lengthCounterHalt = false;
		APU_Pulse2_constantVolume_envelopeFlag = false;
		APU_Pulse2_volume_envelopePeriod = 0;
		APU_Pulse2_sweepEnable = false;
		APU_Pulse2_sweepPeriod = 0;
		APU_Pulse2_sweepNegate = false;
		APU_Pulse2_sweepShiftCount = 0;
		APU_Pulse2_timer = 0;
		APU_Pulse2_lengthCounterLoad = 0;

		/* Triangle */
		APU_Triangle_lengthCounter = 0;
		APU_Triangle_linearCounterHalt = false;
		APU_Triangle_linearCounter = 0;
		APU_Triangle_currentSample = 0;
		APU_Triangle_timerValue = 0;
		APU_Triangle_sequencePosition = 0;

		APU_Triangle_linearCounterControl = false;
		APU_Triangle_linearCounterReload = 0;
		APU_Triangle_timer = 0;
		APU_Triangle_lengthCounterLoad = 0;

		/* Noise */
		APU_Noise_lengthCounter = 0;
		APU_Noise_shiftRegister = 1;
		APU_Noise_envelope = 0;
		APU_Noise_envelopeDivider = 0;
		APU_Noise_resetEnvelope = true;
		APU_Noise_currentSample = 0;
		APU_Noise_timerValue = 0;

		APU_Noise_lengthCounterHalt = false;
		APU_Noise_constantVolume_envelopeFlag = false;
		APU_Noise_volume_envelopePeriod = 0;
		APU_Noise_loop = false;
		APU_Noise_period = 0;
		APU_Noise_lengthCounterLoad = 0;

		/* DMC */
		APU_DMC_addressStart = 0xC000;
		APU_DMC_currentAddress = 0xC000;
		APU_DMC_counter = 8;
		APU_DMC_selectedRate = 0;
		APU_DMC_rateCounter = 1;
		APU_DMC_sampleLength = 1;
		APU_DMC_output = 0;
		APU_DMC_samplesRemaining = 1;
		APU_DMC_stopped = true;
		APU_DMC_shift = 0x00;
	}

	void step()
	{
		++APU_cycleCount;

		if (APU_cycleCount == APU__SEQUENCE_STEP1)
		{
			APU_envelope__step();
			APU_triangle__step();
		}

		if (APU_cycleCount == APU__SEQUENCE_STEP2)
		{
			APU_envelope__step();
			APU_triangle__step();
			APU_length__step();
			APU_sweep__step();
		}

		if (APU_cycleCount == APU__SEQUENCE_STEP3)
		{
			APU_envelope__step();
			APU_triangle__step();
		}

		if (APU_cycleCount == APU__SEQUENCE_STEP4)
		{
			if (APU_frameSteps == 4)
			{
				APU_envelope__step();
				APU_triangle__step();
				APU_length__step();
				APU_sweep__step();

				if (APU_enableInterrupt)
				{
					APU_requestFrameIRQ = true;
					CPU::pullInterruptPin(INTERRUPT_SOURCE_APU);
				}

				APU_cycleCount = 0;
			}
		}

		if (APU_cycleCount == APU__SEQUENCE_STEP5)
		{
			APU_envelope__step();
			APU_triangle__step();
			APU_length__step();
			APU_sweep__step();

			APU_cycleCount = 0;
		}

		/* Pulse 1 */
		if (APU_Pulse1_timerValue == 0)
		{
			APU_Pulse1_outputHigh = (APU_pulseDutyCycles[APU_Pulse1_dutyCycle] & APU_Pulse1_dutyCyclePosition) ? true : false;
			APU_Pulse1_dutyCyclePosition >>= 1;
			if (APU_Pulse1_dutyCyclePosition == 0)
			{
				APU_Pulse1_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
			}
		}

		APU_Pulse1_timerValue += 1;
		APU_Pulse1_timerValue %= (APU_Pulse1_timer + 1);

		/* Pulse 2 */
		if (APU_Pulse2_timerValue == 0)
		{
			APU_Pulse2_outputHigh = (APU_pulseDutyCycles[APU_Pulse2_dutyCycle] & APU_Pulse2_dutyCyclePosition) ? true : false;
			APU_Pulse2_dutyCyclePosition >>= 1;
			if (APU_Pulse2_dutyCyclePosition == 0)
			{
				APU_Pulse2_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
			}
		}

		APU_Pulse2_timerValue += 1;
		APU_Pulse2_timerValue %= (APU_Pulse2_timer + 1);

		/* Triangle */
		for (int i = 0; i < 2; ++i) // because the timer for Triangle is 2 * APUclock
		{
			if ((APU_Triangle_timerValue == 0) && APU_Triangle_lengthCounter && APU_Triangle_linearCounter)
			{
				APU_Triangle_currentSample = APU_triangleSequence[APU_Triangle_sequencePosition];
				APU_Triangle_sequencePosition = (APU_Triangle_sequencePosition + 1) % APU__TRIANGLE_SEQUENCE_LENGTH;
			}

			APU_Triangle_timerValue += 1;
			APU_Triangle_timerValue %= (APU_Triangle_timer + 1);
		}

		/* Noise */
		if (APU_Noise_timerValue == 0)
		{
			uint16_t newBit;
			if (APU_Noise_loop)
			{
				newBit = (APU_Noise_shiftRegister ^ (APU_Noise_shiftRegister >> 6)) & 1;
			}
			else
			{
				newBit = (APU_Noise_shiftRegister ^ (APU_Noise_shiftRegister >> 1)) & 1;
			}
			newBit <<= 14;

			APU_Noise_shiftRegister >>= 1;
			APU_Noise_shiftRegister |= newBit;

			APU_Noise_currentSample = APU_Noise_shiftRegister & 1;
		}

		APU_Noise_timerValue += 1;
		APU_Noise_timerValue %= APU_noisePeriods[APU_Noise_period];

		/* DMC */
		if (APU_channelDeltaSignaEnable && !APU_DMC_stopped)
		{
			--APU_DMC_rateCounter;

			if (APU_DMC_rateCounter == 0)
			{
				APU_DMC_rateCounter = APU_dmcRates[APU_DMC_selectedRate];

				if (APU_DMC_shift == 0)
				{
					APU_DMC_currentSample = MB::readMainBus(APU_DMC_currentAddress);
					CPU::skipCyclesForDMCFetch();

					++APU_DMC_currentAddress;
					--APU_DMC_samplesRemaining;

					if (APU_DMC_samplesRemaining == 0)
					{
						if (APU_DMC_loop)
						{
							APU_DMC_samplesRemaining = APU_DMC_sampleLength;
							APU_DMC_currentAddress = APU_DMC_addressStart;
						}
						else
						{
							APU_DMC_stopped = true;

							if (APU_DMC_enableIRQ)
							{
								APU_requetsDMCIRQ = true;
								CPU::pullInterruptPin(INTERRUPT_SOURCE_DMC);
							}
						}
					}

					APU_DMC_shift = 0x01;
				}

				if (APU_DMC_currentSample & APU_DMC_shift)
				{
					if (APU_DMC_counter < 126)
					{
						APU_DMC_counter += 2;
					}
				}
				else
				{
					if (APU_DMC_counter > 1)
					{
						APU_DMC_counter -= 2;
					}
				}

				APU_DMC_shift <<= 1;

				APU_DMC_output = APU_DMC_counter;
			}
		}

		/* Render */
		float sample = 0.0f;

		/* Pulse1 render */
		uint8_t samplePulse1;

		uint16_t period1 = (APU_Pulse1_timer + 1) >> APU_Pulse1_sweepShiftCount;
		if (APU_Pulse1_sweepNegate)
		{
			period1 = -period1; // for pulse2 will be    period = -period + 1;
		}
		period1 += APU_Pulse1_timer + 1;

		if ((APU_Pulse1_timer + 1 < 8) || (period1 > 0x07FF) || (APU_Pulse1_lengthCounter == 0) || (APU_Pulse1_outputHigh == false))
		{
			samplePulse1 = 0;
		}
		else
		{
			if (APU_Pulse1_constantVolume_envelopeFlag)
			{
				samplePulse1 = APU_Pulse1_volume_envelopePeriod;
			}
			else
			{
				samplePulse1 = APU_Pulse1_envelope;
			}
		}

		if (APU_channelPulse1Enable)
		{
			sample += samplePulse1 * APU__WEIGHT_PULSE;
		}

		/* Pulse2 render */
		uint8_t samplePulse2;

		uint16_t period2 = (APU_Pulse2_timer + 1) >> APU_Pulse2_sweepShiftCount;
		if (APU_Pulse2_sweepNegate)
		{
			period2 = -period2 + 1;
		}
		period2 += APU_Pulse2_timer + 1;

		if ((APU_Pulse2_timer + 1 < 8) || (period2 > 0x07FF) || (APU_Pulse2_lengthCounter == 0) || (APU_Pulse2_outputHigh == false))
		{
			samplePulse2 = 0;
		}
		else
		{
			if (APU_Pulse2_constantVolume_envelopeFlag)
			{
				samplePulse2 = APU_Pulse2_volume_envelopePeriod;
			}
			else
			{
				samplePulse2 = APU_Pulse2_envelope;
			}
		}

		if (APU_channelPulse2Enable)
		{
			sample += samplePulse2 * APU__WEIGHT_PULSE;
		}

		/* Triangle render */
		if (APU_channelTriangleEnable)
		{
			sample += APU_Triangle_currentSample * APU__WEIGHT_TRIANGLE;
		}

		/* Noise render */
		uint8_t sampleNoise;

		if ((APU_Noise_currentSample == 0) || (APU_Noise_lengthCounter == 0))
		{
			sampleNoise = 0;
		}
		else
		{
			if (APU_Noise_constantVolume_envelopeFlag)
			{
				sampleNoise = APU_Noise_volume_envelopePeriod;
			}
			else
			{
				sampleNoise = APU_Noise_envelope;
			}
		}

		if (APU_channelNoiseEnable)
		{
			sample += sampleNoise * APU__WEIGHT_NOISE;
		}

		/*DMC render */
		uint8_t sampleDMC = APU_DMC_output;

		if (APU_channelDeltaSignaEnable)
		{
			sample += sampleDMC * APU__WEIGHT_DMC;
		}

		/* Mixing */
		APU_mixedSample += sample * APU_gaussFilterInUse[APU_mixingPosition];

		++APU_mixingPosition;
		if (APU_mixingPosition == APU_gaussFilterLength)
		{
			AD::queueSample((uint16_t)(APU_mixedSample * APU__OUTPUT_VOLUME));

			APU_mixingPosition = 0;
			APU_mixedSample = 0.0f;
		}
	}

	void writeRegisterSQ1Volume(uint8_t value)
	{
		APU_Pulse1_dutyCycle = (value & APU__PULSE_DUTY_CYCLE_MASK) >> APU__PULSE_DUTY_CYCLE_SHIFT;
		APU_Pulse1_lengthCounterHalt = (value & APU__PULSE_LENGTH_COUNTER_HALT_MASK) ? true : false;
		APU_Pulse1_constantVolume_envelopeFlag = (value & APU__PULSE_CONSTANT_VOLUME_ENVELOPE_FLAG_MASK) ? true : false;
		APU_Pulse1_volume_envelopePeriod = (value & APU__PULSE_VOLUME_ENVELOPE_PERIOD_MASK) >> APU__PULSE_VOLUME_ENVELOPE_PERIOD_SHIFT;
	}

	void writeRegisterSQ1Sweep(uint8_t value)
	{
		APU_Pulse1_sweepEnable = (value & APU__PULSE_SWEEP_ENABLE_MASK) ? true : false;
		APU_Pulse1_sweepPeriod = (value & APU__PULSE_SWEEP_PERIOD_MASK) >> APU__PULSE_SWEEP_PERIOD_SHIFT;
		APU_Pulse1_sweepNegate = (value & APU__PULSE_SWEEP_NEGATE_MASK) ? true : false;
		APU_Pulse1_sweepShiftCount = (value & APU__PULSE_SWEEP_SHIFT_COUNT_MASK) >> APU__PULSE_SWEEP_SHIFT_COUNT_SHIFT;
	}

	void writeRegisterSQ1PeriodLow(uint8_t value)
	{
		APU_Pulse1_timer &= ~APU__PULSE_TIMER_LOW_MASK;
		APU_Pulse1_timer |= value;
	}

	void writeRegisterSQ1PeriodHigh(uint8_t value)
	{
		APU_Pulse1_timer &= APU__PULSE_TIMER_LOW_MASK;
		APU_Pulse1_timer |= (value & APU__PULSE_TIMER_HIGH_MASK) << (8 - APU__PULSE_TIMER_HIGH_SHIFT);
		APU_Pulse1_lengthCounterLoad = (value & APU__PULSE_LENGTH_COUNTER_LOAD_MASK) >> APU__PULSE_LENGTH_COUNTER_LOAD_SHIFT;

		APU_Pulse1_lengthCounter = APU_lengthCounterLUT[APU_Pulse1_lengthCounterLoad];
		APU_Pulse1_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
		APU_Pulse1_resetEnvelope = true;
	}

	void writeRegisterSQ2Volume(uint8_t value)
	{
		APU_Pulse2_dutyCycle = (value & APU__PULSE_DUTY_CYCLE_MASK) >> APU__PULSE_DUTY_CYCLE_SHIFT;
		APU_Pulse2_lengthCounterHalt = (value & APU__PULSE_LENGTH_COUNTER_HALT_MASK) ? true : false;
		APU_Pulse2_constantVolume_envelopeFlag = (value & APU__PULSE_CONSTANT_VOLUME_ENVELOPE_FLAG_MASK) ? true : false;
		APU_Pulse2_volume_envelopePeriod = (value & APU__PULSE_VOLUME_ENVELOPE_PERIOD_MASK) >> APU__PULSE_VOLUME_ENVELOPE_PERIOD_SHIFT;
	}

	void writeRegisterSQ2Sweep(uint8_t value)
	{
		APU_Pulse2_sweepEnable = (value & APU__PULSE_SWEEP_ENABLE_MASK) ? true : false;
		APU_Pulse2_sweepPeriod = (value & APU__PULSE_SWEEP_PERIOD_MASK) >> APU__PULSE_SWEEP_PERIOD_SHIFT;
		APU_Pulse2_sweepNegate = (value & APU__PULSE_SWEEP_NEGATE_MASK) ? true : false;
		APU_Pulse2_sweepShiftCount = (value & APU__PULSE_SWEEP_SHIFT_COUNT_MASK) >> APU__PULSE_SWEEP_SHIFT_COUNT_SHIFT;
	}

	void writeRegisterSQ2PeriodLow(uint8_t value)
	{
		APU_Pulse2_timer &= ~APU__PULSE_TIMER_LOW_MASK;
		APU_Pulse2_timer |= value;
	}

	void writeRegisterSQ2PeriodHigh(uint8_t value)
	{
		APU_Pulse2_timer &= APU__PULSE_TIMER_LOW_MASK;
		APU_Pulse2_timer |= (value & APU__PULSE_TIMER_HIGH_MASK) << (8 - APU__PULSE_TIMER_HIGH_SHIFT);
		APU_Pulse2_lengthCounterLoad = (value & APU__PULSE_LENGTH_COUNTER_LOAD_MASK) >> APU__PULSE_LENGTH_COUNTER_LOAD_SHIFT;

		APU_Pulse2_lengthCounter = APU_lengthCounterLUT[APU_Pulse2_lengthCounterLoad];
		APU_Pulse2_dutyCyclePosition = APU__PULSE_DUTY_CYCLE_INITIAL_POSITION;
		APU_Pulse2_resetEnvelope = true;
	}

	void writeRegisterTriangleLinearCounter(uint8_t value)
	{
		APU_Triangle_linearCounterControl = (value & APU__TRIANGLE_LINEAR_COUNTER_CONTROL_MASK) ? true : false;
		APU_Triangle_linearCounterReload = (value & APU__TRIANGLE_LINEAR_COUNTER_RELOAD_MASK) >> APU__TRIANGLE_LINEAR_COUNTER_RELOAD_SHIFT;
	}

	void writeRegisterTriangleTimerLow(uint8_t value)
	{
		APU_Triangle_timer &= ~APU__TRIANGLE_TIMER_LOW_MASK;
		APU_Triangle_timer |= value;
	}

	void writeRegisterTriangleTimerHigh(uint8_t value)
	{
		APU_Triangle_timer &= APU__TRIANGLE_TIMER_LOW_MASK;
		APU_Triangle_timer |= (value & APU__TRIANGLE_TIMER_HIGH_MASK) << (8 - APU__TRIANGLE_TIMER_HIGH_SHIFT);
		APU_Triangle_lengthCounterLoad = (value & APU__TRIANGLE_LENGTH_COUNTER_LOAD_MASK) >> APU__TRIANGLE_LENGTH_COUNTER_LOAD_SHIFT;

		APU_Triangle_lengthCounter = APU_lengthCounterLUT[APU_Triangle_lengthCounterLoad];
		APU_Triangle_linearCounterHalt = true;
	}

	void writeRegisterNoiseVolume(uint8_t value)
	{
		APU_Noise_lengthCounterHalt = (value & APU__NOISE_LENGTH_COUNTER_HALT_MASK) ? true : false;
		APU_Noise_constantVolume_envelopeFlag = (value & APU__NOISE_CONSTANT_VOLUME_ENVELOPE_FLAG_MASK) ? true : false;
		APU_Noise_volume_envelopePeriod = (value & APU__NOISE_VOLUME_ENVELOPE_PERIOD_MASK) >> APU__NOISE_VOLUME_ENVELOPE_PERIOD_SHIFT;
	}

	void writeRegisterNoiseLoop(uint8_t value)
	{
		APU_Noise_loop = (value & APU__NOISE_LOOP_MASK) ? true : false;
		APU_Noise_period = (value & APU__NOISE_PERIOD_MASK) >> APU__NOISE_PERIOD_SHIFT;
	}

	void writeRegisterNoiseLength(uint8_t value)
	{
		APU_Noise_lengthCounterLoad = (value & APU__NOISE_LENGTH_COUNTER_LOAD_MASK) >> APU__NOISE_LENGTH_COUNTER_LOAD_SHIFT;

		APU_Noise_lengthCounter = APU_lengthCounterLUT[APU_Noise_lengthCounterLoad];
		APU_Noise_resetEnvelope = true;
	}

	void writeRegisterDMCFrequency(uint8_t value)
	{
		APU_DMC_enableIRQ = (value & APU__DMC_ENABLE_IRQ_MASK) ? true : false;

		if (APU_DMC_enableIRQ == false)
		{
			APU_requetsDMCIRQ = false;

			CPU::releaseInterruptPin(INTERRUPT_SOURCE_DMC);
		}

		APU_DMC_loop = (value & APU__DMC_LOOP_MASK) ? true : false;
		APU_DMC_selectedRate = (value & APU__DMC_RATE_MASK) >> APU__DMC_RATE_SHIFT;
	}

	void writeRegisterDMCRaw(uint8_t value)
	{
		if (APU_channelDeltaSignaEnable)
		{
			APU_DMC_output = (value & APU__DMC_RAW_SAMPLE_MASK) >> APU__DMC_RAW_SAMPLE_SHIFT;
		}
	}

	void writeRegisterDMCAddress(uint8_t value)
	{
		APU_DMC_addressStart = 0xC000 | (value << APU__DMC_ADDRESS_SHIFT);
	}

	void writeRegisterDMCLength(uint8_t value)
	{
		APU_DMC_sampleLength = (value << APU__DMC_LENGTH_SHIFT) + 1;
	}

	void writeRegisterChannels(uint8_t value)
	{
		APU_channelPulse1Enable = (value & APU__CHANNEL_PULSE1) ? true : false;
		APU_channelPulse2Enable = (value & APU__CHANNEL_PULSE2) ? true : false;
		APU_channelTriangleEnable = (value & APU__CHANNEL_TRIANGLE) ? true : false;
		APU_channelNoiseEnable = (value & APU__CHANNEL_NOISE) ? true : false;
		APU_channelDeltaSignaEnable = (value & APU__CHANNEL_DMC) ? true : false;

		APU_requetsDMCIRQ = false;
		CPU::releaseInterruptPin(INTERRUPT_SOURCE_DMC);

		APU_Pulse1_lengthCounter = 0;
		APU_Pulse2_lengthCounter = 0;
		APU_Triangle_lengthCounter = 0;
		APU_Noise_lengthCounter = 0;

		if (APU_channelDeltaSignaEnable == false)
		{
			APU_DMC_stopped = true;
		}
		else if (APU_DMC_stopped)
		{
			APU_DMC_stopped = false;

			APU_DMC_samplesRemaining = APU_DMC_sampleLength;
			APU_DMC_currentAddress = APU_DMC_addressStart;
			APU_DMC_shift = 0x00;
		}
	}

	void writeRegisterFrameCounter(uint8_t value)
	{
		APU_frameSteps = (value & APU__5FRAME_SEQUENCE) ? 5 : 4;
		APU_enableInterrupt = (value & APU__DISABLE_INTERRUPT) ? false : true;

		if (APU_enableInterrupt == false)
		{
			APU_requestFrameIRQ = false;

			CPU::releaseInterruptPin(INTERRUPT_SOURCE_APU);
		}

		if (APU__SEQUENCE_STEP4 < APU_cycleCount)
		{
			APU_cycleCount = APU__SEQUENCE_STEP4;
		}
		else if (APU__SEQUENCE_STEP3 < APU_cycleCount)
		{
			APU_cycleCount = APU__SEQUENCE_STEP3;
		}
		else if (APU__SEQUENCE_STEP2 < APU_cycleCount)
		{
			APU_cycleCount = APU__SEQUENCE_STEP2;
		}
		else if (APU__SEQUENCE_STEP1 < APU_cycleCount)
		{
			APU_cycleCount = APU__SEQUENCE_STEP1;
		}
		else
		{
			APU_cycleCount = 0;
		}

		if ((APU_frameSteps == 4) && (APU_cycleCount > APU__SEQUENCE_STEP3))
		{
			APU_cycleCount = APU__SEQUENCE_STEP3;
		}
	}

	uint8_t readRegisterStatus()
	{
		uint8_t status = 0x00;

		if (APU_requetsDMCIRQ)
		{
			status |= APU__INTERRUPT_DMC;
		}

		if (APU_requestFrameIRQ)
		{
			status |= APU__INTERRUPT_FRAME;
		}

		APU_requetsDMCIRQ = false;
		APU_requestFrameIRQ = false;

		CPU::releaseInterruptPin(INTERRUPT_SOURCE_APU);
		CPU::releaseInterruptPin(INTERRUPT_SOURCE_DMC);

		if (APU_Pulse1_lengthCounter)
		{
			status |= APU__CHANNEL_PULSE1;
		}

		if (APU_Pulse2_lengthCounter)
		{
			status |= APU__CHANNEL_PULSE2;
		}

		if (APU_Triangle_lengthCounter)
		{
			status |= APU__CHANNEL_TRIANGLE;
		}

		if (APU_Noise_lengthCounter)
		{
			status |= APU__CHANNEL_NOISE;
		}

		if (APU_channelDeltaSignaEnable)
		{
			status |= APU__CHANNEL_DMC;
		}

		return status;
	}
}

void APU_envelope__step()
{
	/* Pulse1 envelope */
	if (APU_Pulse1_resetEnvelope)
	{
		APU_Pulse1_envelope = 15;
		APU_Pulse1_envelopeDivider = APU_Pulse1_volume_envelopePeriod + 1;
		APU_Pulse1_resetEnvelope = false;
	}
	else
	{
		if (APU_Pulse1_envelopeDivider)
		{
			--APU_Pulse1_envelopeDivider;
		}
		else
		{
			if (APU_Pulse1_envelope)
			{
				--APU_Pulse1_envelope;
			}
			else if (APU_Pulse1_lengthCounterHalt == true)
			{
				APU_Pulse1_envelope = 15;
			}

			APU_Pulse1_envelopeDivider = APU_Pulse1_volume_envelopePeriod + 1;
		}
	}

	/* Pulse2 envelope */
	if (APU_Pulse2_resetEnvelope)
	{
		APU_Pulse2_envelope = 15;
		APU_Pulse2_envelopeDivider = APU_Pulse2_volume_envelopePeriod + 1;
		APU_Pulse2_resetEnvelope = false;
	}
	else
	{
		if (APU_Pulse2_envelopeDivider)
		{
			--APU_Pulse2_envelopeDivider;
		}
		else
		{
			if (APU_Pulse2_envelope)
			{
				--APU_Pulse2_envelope;
			}
			else if (APU_Pulse2_lengthCounterHalt == true)
			{
				APU_Pulse2_envelope = 15;
			}

			APU_Pulse2_envelopeDivider = APU_Pulse2_volume_envelopePeriod + 1;
		}
	}

	/* Noise envelope */
	if (APU_Noise_resetEnvelope)
	{
		APU_Noise_envelope = 15;
		APU_Noise_envelopeDivider = APU_Noise_volume_envelopePeriod + 1;
		APU_Noise_resetEnvelope = false;
	}
	else
	{
		if (APU_Noise_envelopeDivider)
		{
			--APU_Noise_envelopeDivider;
		}
		else
		{
			if (APU_Noise_envelope)
			{
				--APU_Noise_envelope;
			}
			else if (APU_Noise_lengthCounterHalt)
			{
				APU_Noise_envelope = 15;
			}

			APU_Noise_envelopeDivider = APU_Noise_volume_envelopePeriod + 1;
		}
	}
}

void APU_triangle__step()
{
	if (APU_Triangle_linearCounterHalt)
	{
		APU_Triangle_linearCounter = APU_Triangle_linearCounterReload;
	}
	else if (APU_Triangle_linearCounter)
	{
		--APU_Triangle_linearCounter;
	}

	if (APU_Triangle_linearCounterControl == false)
	{
		APU_Triangle_linearCounterHalt = false;
	}
}

void APU_length__step()
{
	/* Pulse1 length */
	if ((APU_Pulse1_lengthCounterHalt == false) && APU_Pulse1_lengthCounter)
	{
		--APU_Pulse1_lengthCounter;
	}

	/* Pulse2 length */
	if ((APU_Pulse2_lengthCounterHalt == false) && APU_Pulse2_lengthCounter)
	{
		--APU_Pulse2_lengthCounter;
	}

	/* Triangle */
	if ((APU_Triangle_linearCounterControl == false) && APU_Triangle_lengthCounter)
	{
		--APU_Triangle_lengthCounter;
	}

	/* Noise */
	if ((APU_Noise_lengthCounterHalt == false) && APU_Noise_lengthCounter)
	{
		--APU_Noise_lengthCounter;
	}
}

void APU_sweep__step()
{
	/* Pulse1 sweep */
	if (APU_Pulse1_resetSweep)
	{
		APU_Pulse1_sweepDivider = APU_Pulse1_sweepPeriod + 1;
		APU_Pulse1_resetSweep = false;
	}
	else
	{
		if (APU_Pulse1_sweepDivider)
		{
			--APU_Pulse1_sweepDivider;
		}
		else
		{
			if (APU_Pulse1_sweepEnable)
			{
				APU_Pulse1_sweepDivider = APU_Pulse1_sweepPeriod + 1;

				uint16_t period = (APU_Pulse1_timer + 1) >> APU_Pulse1_sweepShiftCount;
				if (APU_Pulse1_sweepNegate)
				{
					period = -1 - period;
				}
				period += APU_Pulse1_timer + 1;

				if ((period <= 0x07FF) && (APU_Pulse1_timer + 1 >= 8))
				{
					APU_Pulse1_timer = period;
				}
			}
		}
	}

	/* Pulse2 sweep */
	if (APU_Pulse2_resetSweep)
	{
		APU_Pulse2_sweepDivider = APU_Pulse2_sweepPeriod + 1;
		APU_Pulse2_resetSweep = false;
	}
	else
	{
		if (APU_Pulse2_sweepDivider)
		{
			--APU_Pulse2_sweepDivider;
		}
		else
		{
			if (APU_Pulse2_sweepEnable)
			{
				APU_Pulse2_sweepDivider = APU_Pulse2_sweepPeriod + 1;

				uint16_t period = (APU_Pulse2_timer + 1) >> APU_Pulse2_sweepShiftCount;
				if (APU_Pulse2_sweepNegate)
				{
					period = -period;
				}
				period += APU_Pulse2_timer + 1;

				if ((period <= 0x07FF) && (APU_Pulse2_timer + 1 >= 8))
				{
					APU_Pulse2_timer = period;
				}
			}
		}
	}
}