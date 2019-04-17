#include "PictureProcessingUnit.h"

#include "CartridgeReader.h"
#include "MemoryBus.h"
#include "CentralProcessingUnit.h"
#include "RenderingWindow.h"

#include <string>

#include "debug.h"

#define PPU__SCANLINE_CYCLE_LENGTH (341u)
#define PPU__SCANLINE_CYCLE_END (340u)
#define PPU__SCANLINE_COUNT (240u)
#define PPU__SCANLINE_DOTS (256u)
#define PPU__SCANLINE_FRAME_END (261u)

#define PPU__PIPELINE_PRERENDER (0u)
#define PPU__PIPELINE_RENDER (1u)
#define PPU__PIPELINE_POSTRENDER (2u)
#define PPU__PIPELINE_VERTICAL_BLANK (3u)

#define PPU__CHARACTER_PAGE_LOW (0u)
#define PPU__CHARACTER_PAGE_HIGH (1u)

#define PPU__BITS_HORIZONTAL (0x041Fu)
#define PPU__BITS_VERTICAL (0x7BE0u)

inline uint32_t PPU_color__convertToGrayScale(uint32_t color);

static const uint32_t PPU_colorsNTSC[] = // to RGBA
{ // 64 colors
	0x717171FFu, 0x0018D3FFu, 0x0704B0FFu, 0x4110B0FFu,	0x7D0082FFu, 0x820030FFu, 0x820A00FFu, 0x6A1900FFu,
	0x443200FFu, 0x086700FFu, 0x006000FFu, 0x005104FFu,	0x004052FFu, 0x000000FFu, 0x000000FFu, 0x000000FFu,
	0xB3B3B3FFu, 0x0C6FF2FFu, 0x2051F2FFu, 0x6E33FDFFu,	0xBE0ECCFFu, 0xD10F62FFu, 0xD2340FFFu, 0xBE5308FFu,
	0x997200FFu, 0x15A000FFu, 0x07A100FFu, 0x00A03EFFu,	0x00848AFFu, 0x000000FFu, 0x000000FFu, 0x000000FFu,
	0xFEFDFEFFu, 0x4EB6FEFFu, 0x838DFEFFu, 0xB577FBFFu,	0xF570FDFFu, 0xFD61B2FFu, 0xFC7B63FFu, 0xF19F33FFu,
	0xCDBB00FFu, 0xA8E80EFFu, 0x5ADE42FFu, 0x53EF8EFFu,	0x23D9DAFFu, 0x626262FFu, 0x000000FFu, 0x000000FFu,
	0xFFFFFFFFu, 0xB1E2FEFFu, 0xC4C6FCFFu, 0xE0C0FDFFu,	0xF9DBFCFFu, 0xFDB1D2FFu, 0xF8CDBFFFu, 0xFADDA6FFu,
	0xF1DF85FFu, 0xD1F381FFu, 0xBBF6B4FFu, 0xF5F5D2FFu,	0x79F0F7FFu, 0xD4D4D4FFu, 0x000000FFu, 0x000000FFu
};

static const uint32_t PPU_colorsPAL[] = // to RGBA
{ // 64 colors
	0x696969FFu, 0x002985FFu, 0x0010A5FFu, 0x31089FFFu, 0x6B0476FFu, 0x8F0033FFu, 0x950000FFu, 0x760700FFu,
	0x382200FFu, 0x003700FFu, 0x004200FFu, 0x003F00FFu, 0x003A48FFu, 0x000000FFu, 0x000000FFu, 0x000000FFu,
	0xBDBDBDFFu, 0x006FE0FFu, 0x2653FFFFu, 0x7E37FFFFu, 0xCA23CEFFu, 0xF61E7BFFu, 0xFA2916FFu, 0xCF4300FFu,
	0x856200FFu, 0x2D7B00FFu, 0x008A00FFu, 0x008D45FFu, 0x038496FFu, 0x000000FFu, 0x000000FFu, 0x000000FFu,
	0xFEFFFFFFu, 0x35AAFFFFu, 0x6585FFFFu, 0xA574FFFFu, 0xFE78FFFFu, 0xFF84C6FFu, 0xFF8A7DFFu, 0xFFA23CFFu,
	0xE7B800FFu, 0x93D100FFu, 0x47E33BFFu, 0x17E894FFu, 0x12DFEFFFu, 0x4E4E4EFFu, 0x000000FFu, 0x000000FFu,
	0xFEFFFFFFu, 0xA9DAFFFFu, 0xB8C7FFFFu, 0xD6C0FFFFu, 0xFEC7FFFFu, 0xFFCBE8FFu, 0xFFCEC8FFu, 0xFFDCB3FFu,
	0xFFF0A8FFu, 0xE0FAAAFFu, 0xBFFCBCFFu, 0xADFEDDFFu, 0xACFAFFFFu, 0xC6C6C6FFu, 0x000000FFu, 0x000000FFu
};

static const uint32_t *PPU_paletteInUse;

static uint8_t PPU_spriteMemory[256];
static uint8_t PPU_scanlineSprites[8];
static int8_t PPU_scanlineSpriteCount = 0;

static uint8_t PPU_pipelineStage;

static uint16_t PPU_cycle;
static uint16_t PPU_scanline;

static bool PPU_evenFrame;
static bool PPU_verticalBlank;
static bool PPU_spriteZeroHit;
static bool PPU_firstWrite;

static uint16_t PPU_dataAddress; // internal registers
static uint16_t PPU_temporaryAddress;
static uint8_t PPU_fineVerticalScroll;
static uint8_t PPU_dataBuffer;
static uint8_t PPU_spriteDataAddress;

static bool PPU_longSprites;
static bool PPU_interruptEnabled;
static bool PPU_grayscaleMode;
static bool PPU_showSprites;
static bool PPU_showBackground;
static bool PPU_hideEdgeSprites;
static bool PPU_hideEdgeBackground;

static uint8_t PPU_spritePage;
static uint8_t PPU_backgroundPage;

static uint16_t PPU_dataAddressIncrement;

static void(*PPU_scanlineEndCallback)(bool vblank) = nullptr;

namespace PPU
{
	void reset()
	{
		PPU_longSprites = false;
		PPU_interruptEnabled = false;
		PPU_grayscaleMode = false;
		PPU_verticalBlank = false;
		PPU_showSprites = true;
		PPU_showBackground = true;
		PPU_evenFrame = true;
		PPU_firstWrite = true;

		PPU_spritePage = PPU__CHARACTER_PAGE_LOW;
		PPU_backgroundPage = PPU__CHARACTER_PAGE_LOW;

		PPU_dataAddressIncrement = 0x0001;

		PPU_pipelineStage = PPU__PIPELINE_PRERENDER;
		PPU_scanlineSpriteCount = 0;

		PPU_cycle = 0;
		PPU_scanline = 0;

		PPU_dataAddress = 0;
		PPU_temporaryAddress = 0;
		PPU_fineVerticalScroll = 0;
		PPU_dataBuffer = 0;
		PPU_spriteDataAddress = 0;

		if (CR::getSystemType() == SYSTEM_NTSC)
		{
			PPU_paletteInUse = PPU_colorsNTSC;
		}
		else
		{
			PPU_paletteInUse = PPU_colorsPAL;
		}
	}

	void registerScanlineCallback(void(*callback)(bool vblank))
	{
		PPU_scanlineEndCallback = callback;
	}

	void step()
	{
		switch (PPU_pipelineStage)
		{
			case PPU__PIPELINE_PRERENDER:
			{
				if (PPU_cycle == 1)
				{
					PPU_verticalBlank = false;
					PPU_spriteZeroHit = false;
				}
				else if (PPU_cycle == PPU__SCANLINE_DOTS + 2 && PPU_showSprites && PPU_showBackground)
				{ // switch to horizontal
					PPU_dataAddress &= ~PPU__BITS_HORIZONTAL;
					PPU_dataAddress |= PPU_temporaryAddress & PPU__BITS_HORIZONTAL;
				}
				else if (PPU_cycle > 280 && PPU_cycle < 305 && PPU_showSprites && PPU_showBackground)
				{ // switch to vertical
					PPU_dataAddress &= ~PPU__BITS_VERTICAL;
					PPU_dataAddress |= PPU_temporaryAddress & PPU__BITS_VERTICAL;
				}

				if (PPU_cycle >= (PPU__SCANLINE_CYCLE_END - ((!PPU_evenFrame && PPU_showSprites && PPU_showBackground) ? 1u : 0u)))
				{ // every other frame is one cycle shorter if rendering is active
					PPU_pipelineStage = PPU__PIPELINE_RENDER;
					PPU_cycle = 0;
					PPU_scanline = 0;

					if (PPU_scanlineEndCallback)
					{
						PPU_scanlineEndCallback(false);
					}
				}

				break;
			}

			case PPU__PIPELINE_RENDER:
			{
				if (PPU_cycle > 0 && PPU_cycle <= PPU__SCANLINE_DOTS)
				{
					uint8_t colorSprite = 0;
					uint8_t colorBackground = 0;
					bool opaqueSprite = true;
					bool opaqueBackground = false;
					bool spriteForeground = false;

					uint16_t x = PPU_cycle - 1;
					uint16_t y = PPU_scanline;

					if (PPU_showBackground)
					{
						uint16_t xFine = (PPU_fineVerticalScroll + x) % 8;
						if (!PPU_hideEdgeBackground || x >= 8)
						{
							uint8_t tile = MB::readPictureBus((PPU_dataAddress & 0x0FFF) | 0x2000);
							uint16_t address = ((tile << 4) + ((PPU_dataAddress >> 12) & 0x0007)) | (PPU_backgroundPage << 12);

							colorBackground = (MB::readPictureBus(address) >> (xFine ^ 0x7)) & 0x01; // bit 0
							colorBackground |= ((MB::readPictureBus(address + 8) >> (xFine ^ 0x7)) & 0x01) << 1; // bit 1

							opaqueBackground = colorBackground ? true : false;

							address = (PPU_dataAddress & 0x0C00) | ((PPU_dataAddress >> 2) & 0x0007) | ((PPU_dataAddress >> 4) & 0x0038) | 0x23C0;
							uint8_t attribute = MB::readPictureBus(address);
							uint8_t shamt = (PPU_dataAddress & 0x02) | ((PPU_dataAddress >> 4) & 0x04);

							colorBackground |= ((attribute >> shamt) & 0x03) << 2; // bits 2 and 3
						}

						if (xFine == 7)
						{
							if ((PPU_dataAddress & 0x001F) == 31) // reached end of nametable
							{
								PPU_dataAddress &= ~0x001F; // reset pointer to the beginning of the nametable
								PPU_dataAddress ^= 0x0400; // change nametable
							}
							else
							{
								++PPU_dataAddress;
							}
						}
					}

					if (PPU_showSprites && (!PPU_hideEdgeSprites || x >= 8))
					{
						for (int8_t i = 0; i < PPU_scanlineSpriteCount; ++i)
						{
							uint8_t sprite = PPU_scanlineSprites[i];

							uint8_t xSpr = PPU_spriteMemory[4 * sprite + 3];
							uint8_t ySpr = PPU_spriteMemory[4 * sprite + 0] + 1;

							if ((int)x - (int)xSpr < 0 || (int)x - (int)xSpr >= 8)
							{
								continue;
							}

							uint8_t tile = PPU_spriteMemory[4 * sprite + 1];
							uint8_t attribute = PPU_spriteMemory[4 * sprite + 2];

							uint8_t length = PPU_longSprites ? 16 : 8;

							uint8_t xShift = (x - xSpr) % 8;
							uint8_t yOffset = (y - ySpr) % length;

							if ((attribute & 0x40) == 0) // if not flipping horizontally
							{
								xShift ^= 0x07;
							}
							if (attribute & 0x80) // if flipping vertically
							{
								yOffset ^= (length - 1);
							}

							uint16_t address = 0;
							if (PPU_longSprites)
							{
								yOffset = (yOffset & 0x07) | ((yOffset & 0x08) << 1);
								address = (((tile & 0xFE) << 4) + yOffset) | ((uint16_t)(tile & 0x01) << 12);
							}
							else
							{
								address = ((tile << 4) + yOffset) | ((PPU_spritePage == PPU__CHARACTER_PAGE_HIGH) ? 0x1000 : 0x0000);
							}

							colorSprite = (MB::readPictureBus(address) >> xShift) & 0x01; // bit 0
							colorSprite |= ((MB::readPictureBus(address + 8) >> xShift) & 0x01) << 1; // bit 1

							opaqueSprite = colorSprite ? true : false;
							if (!opaqueSprite)
							{
								colorSprite = 0;
								continue;
							}

							colorSprite |= ((attribute & 0x03) << 2) | 0x10; // bits 2, 3 and 4
							spriteForeground = (attribute & 0x20) ? false : true;

							if (!PPU_spriteZeroHit && PPU_showBackground && sprite == 0 && opaqueSprite && opaqueBackground)
							{
								PPU_spriteZeroHit = true;
							}

							break; // highest priority sprite has been found
						}
					}

					uint8_t paletteAddress = colorBackground;
					if ((!opaqueBackground && opaqueSprite) || (opaqueBackground && opaqueSprite && spriteForeground))
					{
						paletteAddress = colorSprite;
					}
					else if (!opaqueBackground && !opaqueSprite)
					{
						paletteAddress = 0;
					}

					uint32_t colorToDisplay = PPU_paletteInUse[MB::readPictureBus((uint16_t)paletteAddress | 0x3F20)];
					if (PPU_grayscaleMode)
					{
						colorToDisplay = PPU_color__convertToGrayScale(colorToDisplay);
					}

					RW::setPixel(x, y, colorToDisplay);
				}
				else if (PPU_cycle == PPU__SCANLINE_DOTS + 1 && PPU_showBackground)
				{
					if ((PPU_dataAddress & 0x7000) != 0x7000)
					{ // next fine y
						PPU_dataAddress += 0x1000;
					}
					else
					{
						PPU_dataAddress &= ~0x7000;

						uint16_t y = (PPU_dataAddress & 0x03E0) >> 5;
						if (y == 29)
						{
							y = 0;
							PPU_dataAddress ^= 0x0800; // switch vertical nametable
						}
						else if (y == 31)
						{
							y = 0;
						}
						else
						{
							++y;
						}

						PPU_dataAddress = (PPU_dataAddress & ~0x03E0) | (y << 5);
					}
				}
				else if (PPU_cycle == PPU__SCANLINE_DOTS + 2 && PPU_showSprites && PPU_showBackground)
				{
					PPU_dataAddress &= ~PPU__BITS_HORIZONTAL;
					PPU_dataAddress |= PPU_temporaryAddress & PPU__BITS_HORIZONTAL;
				}

				if (PPU_cycle >= PPU__SCANLINE_CYCLE_END)
				{
					PPU_scanlineSpriteCount = 0;
					uint8_t range = PPU_longSprites ? 16 : 8;

					for (uint8_t i = PPU_spriteDataAddress >> 2; i < 64; ++i)
					{
						int16_t difference = (PPU_scanline - PPU_spriteMemory[4 * i]);
						if (difference >= 0 && difference < range)
						{
							PPU_scanlineSprites[PPU_scanlineSpriteCount] = i;
							++PPU_scanlineSpriteCount;
							if (PPU_scanlineSpriteCount >= 8)
							{
								break;
							}
						}
					}

					if (PPU_scanlineEndCallback)
					{
						PPU_scanlineEndCallback(false);
					}

					++PPU_scanline;
					PPU_cycle = 0;
				}

				if (PPU_scanline >= PPU__SCANLINE_COUNT)
				{
					PPU_pipelineStage = PPU__PIPELINE_POSTRENDER;
				}

				break;
			}

			case PPU__PIPELINE_POSTRENDER:
			{
				if (PPU_cycle >= PPU__SCANLINE_CYCLE_END)
				{
					if (PPU_scanlineEndCallback)
					{
						PPU_scanlineEndCallback(true);
					}

					++PPU_scanline;
					PPU_cycle = 0;

					PPU_pipelineStage = PPU__PIPELINE_VERTICAL_BLANK;

					RW::redraw(); // it takes 89079 or 89080 cycles between each frame
				}

				break;
			}

			case PPU__PIPELINE_VERTICAL_BLANK:
			{
				if (PPU_cycle == 1 && PPU_scanline == PPU__SCANLINE_COUNT + 1)
				{
					PPU_verticalBlank = true;
					if (PPU_interruptEnabled)
					{
						CPU::causeInterrupt(INTERRUPT_NMI);
					}
				}

				if (PPU_cycle >= PPU__SCANLINE_CYCLE_END)
				{
					if (PPU_scanlineEndCallback)
					{
						PPU_scanlineEndCallback(true);
					}

					++PPU_scanline;
					PPU_cycle = 0;
				}

				if (PPU_scanline >= PPU__SCANLINE_FRAME_END)
				{
					PPU_pipelineStage = PPU__PIPELINE_PRERENDER;
					PPU_scanline = 0;
					PPU_evenFrame = !PPU_evenFrame;
				}

				break;
			}
		}

		++PPU_cycle;
	}

	void writeRegisterControl(uint8_t value)
	{
		PPU_interruptEnabled = value & 0x80 ? true : false;
		PPU_longSprites = value & 0x20 ? true : false;
		PPU_spritePage = value & 0x08 ? PPU__CHARACTER_PAGE_HIGH : PPU__CHARACTER_PAGE_LOW;
		PPU_backgroundPage = value & 0x10 ? PPU__CHARACTER_PAGE_HIGH : PPU__CHARACTER_PAGE_LOW;
		PPU_dataAddressIncrement = value & 0x04 ? 0x20 : 0x01;

		PPU_temporaryAddress &= ~0x0C00;
		PPU_temporaryAddress |= (value & 0x03) << 10;
	}

	void writeRegisterMask(uint8_t value)
	{
		PPU_grayscaleMode = value & 0x01 ? true : false;
		PPU_hideEdgeBackground = value & 0x02 ? false : true;
		PPU_hideEdgeSprites = value & 0x04 ? false : true;
		PPU_showBackground = value & 0x08 ? true : false;
		PPU_showSprites = value & 0x10 ? true : false;
	}

	void writeRegisterSpriteAddress(uint8_t value)
	{
		PPU_spriteDataAddress = value;
	}

	void writeRegisterSpriteData(uint8_t value)
	{
		PPU_spriteMemory[PPU_spriteDataAddress++] = value;
	}

	void writeRegisterScroll(uint8_t value)
	{
		if (PPU_firstWrite)
		{
			PPU_temporaryAddress &= ~0x001F;
			PPU_temporaryAddress |= (value >> 3) & 0x001F;
			PPU_fineVerticalScroll = value & 0x07;
			PPU_firstWrite = false;
		}
		else
		{
			PPU_temporaryAddress &= ~0x73E0;
			PPU_temporaryAddress |= ((value & 0x07) << 12) | ((value & 0xF8) << 2);
			PPU_firstWrite = true;
		}
	}

	void writeRegisterAddress(uint8_t value)
	{
		if (PPU_firstWrite)
		{
			PPU_temporaryAddress &= ~0xFF00;
			PPU_temporaryAddress |= (value & 0x3F) << 8;
			PPU_firstWrite = false;
		}
		else
		{
			PPU_temporaryAddress &= ~0x00FF;
			PPU_temporaryAddress |= value;
			PPU_dataAddress = PPU_temporaryAddress;
			PPU_firstWrite = true;
		}
	}

	void writeRegisterData(uint8_t value)
	{
		MB::writePictureBus(PPU_dataAddress, value);
		PPU_dataAddress += PPU_dataAddressIncrement;
	}

	uint8_t readRegisterControl()
	{
		uint8_t control = (PPU_interruptEnabled ? 0x80 : 0x00) | (PPU_longSprites ? 0x20 : 0x00) | (PPU_dataAddressIncrement == 1 ? 0x00 : 0x04) | (PPU_spritePage == PPU__CHARACTER_PAGE_HIGH ? 0x08 : 0x00) | (PPU_backgroundPage == PPU__CHARACTER_PAGE_HIGH ? 0x10 : 0x00) | ((PPU_temporaryAddress >> 10) & 0x03);
		return (control);
	}

	uint8_t readRegisterMask()
	{
		uint8_t mask = (PPU_grayscaleMode ? 0x01 : 0x00) | (PPU_hideEdgeBackground ? 0x00 : 0x02) | (PPU_hideEdgeSprites ? 0x00 : 0x04) | (PPU_showBackground ? 0x08 : 0x00) | (PPU_showSprites ? 0x10 : 0x00);
		return (mask);
	}

	uint8_t readRegisterStatus()
	{
		uint8_t status = (PPU_verticalBlank ? 0x80 : 0x00) | (PPU_spriteZeroHit ? 0x40 : 0x00);

		PPU_firstWrite = true;
		PPU_verticalBlank = false;

		return (status);
	}

	uint8_t readRegisterSpriteData()
	{
		return (PPU_spriteMemory[PPU_spriteDataAddress]);
	}

	uint8_t readRegisterData()
	{
		uint8_t data = MB::readPictureBus(PPU_dataAddress);
		PPU_dataAddress += PPU_dataAddressIncrement;

		if (PPU_dataAddress < 0x3F00)
		{ // reads are delayed in this address space
			PPU_dataBuffer ^= data;
			data ^= PPU_dataBuffer;
			PPU_dataBuffer ^= data;
		}

		return (data);
	}

	void executeDMA(uint8_t *page_pointer)
	{
		if (page_pointer)
		{
			memcpy(PPU_spriteMemory + PPU_spriteDataAddress, page_pointer, 256u - PPU_spriteDataAddress);
			if (PPU_spriteDataAddress)
			{
				memcpy(PPU_spriteMemory, page_pointer + (256u - PPU_spriteDataAddress), PPU_spriteDataAddress);
			}
		}
	}
}

inline uint32_t PPU_color__convertToGrayScale(uint32_t color)
{
	uint16_t average = 0;
	uint8_t mod;

	average += (color & 0xFF000000) >> 24;
	average += (color & 0x00FF0000) >> 16;
	average += (color & 0x0000FF00) >> 8;

	mod = average % 3;
	average /= 3;

	if (mod == 2)
	{
		++average;
	}

	uint8_t gray = (uint8_t)average;

	return (gray << 24) | (gray << 16) | (gray << 8) | 0xFF;
}