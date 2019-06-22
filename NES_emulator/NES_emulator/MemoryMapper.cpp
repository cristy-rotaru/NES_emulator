#include "MemoryMapper.h"

#include "CartridgeReader.h"
#include "MemoryBus.h"
#include "PictureProcessingUnit.h"
#include "CentralProcessingUnit.h"

#define MM__NO_MAPPER_SELECTED (0xFFFFu)

#define MM__NAME_TABLE_HORIZONTAL (0x00u)
#define MM__NAME_TABLE_VERTICAL (0x01u)
#define MM__NAME_TABLE_ONE_SCREEN_HIGHER (0x08u)
#define MM__NAME_TABLE_ONE_SCREEN_LOWER (0x09u)

#define MM__CHARACTER_RAM_SIZE (0x2000u)

static struct
{
	void(*writePRG)(uint16_t address, uint8_t data);
	uint8_t(*readPRG)(uint16_t address);
	void(*writeCHR)(uint16_t address, uint8_t data);
	uint8_t(*readCHR)(uint16_t address);
} MM_busFunctions;

static union
{
	struct
	{
		uint8_t bankCount;
		uint8_t *characterRAM;
	} MapperNone;

	struct
	{
		uint8_t bankCount;
		uint8_t *characterRAM;
		uint8_t modePRG;
		uint8_t modeCHR;
		uint8_t temporaryRegister;
		int8_t writeCounter;
		uint8_t registerPRG;
		uint8_t registerCHR0;
		uint8_t registerCHR1;
		uint8_t *firstBankPRG;
		uint8_t *secondBankPRG;
		uint8_t *firstBankCHR;
		uint8_t *secondBankCHR;
	} MapperMMC1;

	struct
	{
		uint8_t bankCount;
		uint8_t *characterRAM;
		uint8_t *lastBank;
		uint8_t selectedBank;
	}MapperUNROM;

	struct
	{
		uint8_t bankCount;
		uint8_t selectedBank;
	}MapperCNROM;

	struct
	{
		uint8_t bankSelect;
		uint8_t *PRGbankFixed0;
		uint8_t *PRGbankFixed1;
		uint8_t *PRGbank0;
		uint8_t *PRGbank1;
		uint8_t *CHR2k0;
		uint8_t *CHR2k1;
		uint8_t *CHR1k0;
		uint8_t *CHR1k1;
		uint8_t *CHR1k2;
		uint8_t *CHR1k3;
		bool invertPRG;
		bool invertCHR;
		bool enableIRQ;
		bool causeIRQ;
		uint8_t counterValue;
		uint8_t counterReloadValue;
		bool counterReload;
	}MapperMMC3;
} MM_parameters;

static uint16_t MM_currentMapper = MM__NO_MAPPER_SELECTED;
static uint8_t MM_nameTableMirroring;

/* MAPPER_NONE functions */
void Mapper_None__writePRG(uint16_t address, uint8_t data);
uint8_t Mapper_None__readPRG(uint16_t address);
void Mapper_None__writeCHR(uint16_t address, uint8_t data);
uint8_t Mapper_None__readCHR(uint16_t address);

/* MAPPER_MMC1 functions */
void Mapper_MMC1__writePRG(uint16_t address, uint8_t data);
uint8_t Mapper_MMC1__readPRG(uint16_t address);
void Mapper_MMC1__writeCHR(uint16_t address, uint8_t data);
uint8_t Mapper_MMC1__readCHR(uint16_t address);

/* MAPPER_UNROM functions */
void Mapper_UNROM__writePRG(uint16_t address, uint8_t data);
uint8_t Mapper_UNROM__readPRG(uint16_t address);
void Mapper_UNROM__writeCHR(uint16_t address, uint8_t data);
uint8_t Mapper_UNROM__readCHR(uint16_t address);

/* MAPPER_CNROM functions */
void Mapper_CNROM__writePRG(uint16_t address, uint8_t data);
uint8_t Mapper_CNROM__readPRG(uint16_t address);
void Mapper_CNROM__writeCHR(uint16_t address, uint8_t data);
uint8_t Mapper_CNROM__readCHR(uint16_t address);

/* MAPPER_MMC3 functions */
void Mapper_MMC3__writePRG(uint16_t address, uint8_t data);
uint8_t Mapper_MMC3__readPRG(uint16_t address);
void Mapper_MMC3__writeCHR(uint16_t address, uint8_t data);
uint8_t Mapper_MMC3__readCHR(uint16_t address);

namespace MM
{
	void init()
	{
		MM_busFunctions.writePRG = nullptr;
		MM_busFunctions.readPRG = nullptr;
		MM_busFunctions.writeCHR = nullptr;
		MM_busFunctions.readCHR = nullptr;
	}

	bool setMapper(uint8_t mapper_type)
	{
		switch (mapper_type)
		{
			case MAPPER_NONE:
			{
				MM_busFunctions.writePRG = Mapper_None__writePRG;
				MM_busFunctions.readPRG = Mapper_None__readPRG;
				MM_busFunctions.writeCHR = Mapper_None__writeCHR;
				MM_busFunctions.readCHR = Mapper_None__readCHR;

				MM_nameTableMirroring = CR::getNameTableMirroring();

				MM_parameters.MapperNone.bankCount = CR::getROMBankCount();
				if (MM_parameters.MapperNone.bankCount > 2)
				{
					return (false);
				}

				if (CR::getVROMBankCount())
				{
					MM_parameters.MapperNone.characterRAM = nullptr;
				}
				else
				{ // will use character RAM
					MM_parameters.MapperNone.characterRAM = new uint8_t[MM__CHARACTER_RAM_SIZE];
					if (!MM_parameters.MapperNone.characterRAM)
					{
						return (false);
					}
				}

				break;
			}

			case MAPPER_MMC1:
			{
				MM_busFunctions.writePRG = Mapper_MMC1__writePRG;
				MM_busFunctions.readPRG = Mapper_MMC1__readPRG;
				MM_busFunctions.writeCHR = Mapper_MMC1__writeCHR;
				MM_busFunctions.readCHR = Mapper_MMC1__readCHR;

				MM_parameters.MapperMMC1.bankCount = CR::getROMBankCount();

				if (CR::getVROMBankCount())
				{
					MM_parameters.MapperMMC1.characterRAM = nullptr;
					MM_parameters.MapperMMC1.firstBankCHR = CR::getVideoROM();
					MM_parameters.MapperMMC1.secondBankCHR = MM_parameters.MapperMMC1.firstBankCHR;
				}
				else
				{
					MM_parameters.MapperMMC1.characterRAM = new uint8_t[MM__CHARACTER_RAM_SIZE];
					if (!MM_parameters.MapperMMC1.characterRAM)
					{
						return (false);
					}
				}

				MM_parameters.MapperMMC1.firstBankPRG = CR::getROM();
				MM_parameters.MapperMMC1.secondBankPRG = &MM_parameters.MapperMMC1.firstBankPRG[(MM_parameters.MapperMMC1.bankCount - 1) * 0x4000];

				MM_nameTableMirroring = MM__NAME_TABLE_HORIZONTAL;
				MB::changeMirroring(MM_nameTableMirroring);

				MM_parameters.MapperMMC1.modePRG = 0x03;
				MM_parameters.MapperMMC1.modeCHR = 0x00;

				MM_parameters.MapperMMC1.registerPRG = 0x00;
				MM_parameters.MapperMMC1.registerCHR0 = 0x00;
				MM_parameters.MapperMMC1.registerCHR1 = 0x00;

				MM_parameters.MapperMMC1.temporaryRegister = 0x00;
				MM_parameters.MapperMMC1.writeCounter = 0;

				break;
			}

			case MAPPER_UNROM:
			{
				MM_busFunctions.writePRG = Mapper_UNROM__writePRG;
				MM_busFunctions.readPRG = Mapper_UNROM__readPRG;
				MM_busFunctions.writeCHR = Mapper_UNROM__writeCHR;
				MM_busFunctions.readCHR = Mapper_UNROM__readCHR;

				MM_nameTableMirroring = CR::getNameTableMirroring();

				MM_parameters.MapperUNROM.bankCount = CR::getROMBankCount();

				if (CR::getVROMBankCount())
				{
					MM_parameters.MapperUNROM.characterRAM = nullptr;
				}
				else
				{
					MM_parameters.MapperUNROM.characterRAM = new uint8_t[MM__CHARACTER_RAM_SIZE];
					if (!MM_parameters.MapperUNROM.characterRAM)
					{
						return (false);
					}
				}

				MM_parameters.MapperUNROM.lastBank = &CR::getROM()[(MM_parameters.MapperUNROM.bankCount - 1) * 0x4000];
				MM_parameters.MapperUNROM.selectedBank = 0;

				break;
			}

			case MAPPER_CNROM:
			{
				MM_busFunctions.writePRG = Mapper_CNROM__writePRG;
				MM_busFunctions.readPRG = Mapper_CNROM__readPRG;
				MM_busFunctions.writeCHR = Mapper_CNROM__writeCHR;
				MM_busFunctions.readCHR = Mapper_CNROM__readCHR;

				MM_nameTableMirroring = CR::getNameTableMirroring();

				MM_parameters.MapperCNROM.bankCount = CR::getROMBankCount();
				MM_parameters.MapperCNROM.selectedBank = 0;

				break;
			}

			case MAPPER_MMC3:
			{
				return (false); // no longer supported

				MM_busFunctions.writePRG = Mapper_MMC3__writePRG;
				MM_busFunctions.readPRG = Mapper_MMC3__readPRG;
				MM_busFunctions.writeCHR = Mapper_MMC3__writeCHR;
				MM_busFunctions.readCHR = Mapper_MMC3__readCHR;

				MM_nameTableMirroring = CR::getNameTableMirroring();

				MM_parameters.MapperMMC3.PRGbankFixed0 = &CR::getROM()[(CR::getROMBankCount() - 1) * 0x4000];
				MM_parameters.MapperMMC3.PRGbankFixed1 = &MM_parameters.MapperMMC3.PRGbankFixed0[0x2000];
				MM_parameters.MapperMMC3.PRGbank0 = MM_parameters.MapperMMC3.PRGbankFixed0;
				MM_parameters.MapperMMC3.PRGbank1 = MM_parameters.MapperMMC3.PRGbankFixed1;

				MM_parameters.MapperMMC3.CHR2k0 = CR::getVideoROM();
				MM_parameters.MapperMMC3.CHR2k1 = &CR::getVideoROM()[0x0800];
				MM_parameters.MapperMMC3.CHR1k0 = &CR::getVideoROM()[0x1000];
				MM_parameters.MapperMMC3.CHR1k1 = &CR::getVideoROM()[0x1400];
				MM_parameters.MapperMMC3.CHR1k2 = &CR::getVideoROM()[0x1800];
				MM_parameters.MapperMMC3.CHR1k3 = &CR::getVideoROM()[0x1C00];

				MM_parameters.MapperMMC3.counterReloadValue = 0xFF;
				MM_parameters.MapperMMC3.enableIRQ = false;
				MM_parameters.MapperMMC3.causeIRQ = false;

				MM_parameters.MapperMMC3.invertPRG = false;
				MM_parameters.MapperMMC3.invertCHR = false;

				void(*callback)(bool vblank) = [](bool vblank) -> void
				{
					if (vblank)
					{
						return;
					}

					if (MM_parameters.MapperMMC3.counterReload)
					{
						MM_parameters.MapperMMC3.counterValue = MM_parameters.MapperMMC3.counterReloadValue;
						MM_parameters.MapperMMC3.counterReload = false;
					}
					else if (MM_parameters.MapperMMC3.counterValue > 0)
					{
						--MM_parameters.MapperMMC3.counterValue;

						if (!MM_parameters.MapperMMC3.counterValue)
						{
							MM_parameters.MapperMMC3.counterReload = true;

							if (MM_parameters.MapperMMC3.enableIRQ)
							{
								CPU::pullInterruptPin(INTERRUPT_SOURCE_MM);
							}
						}
					}
				};

				PPU::registerScanlineCallback(callback);

				break;
			}

			case MAPPER_MMC5:
			case MAPPER_FRONT_FAR_EAST:
			case MAPPER_AXROM:
			case MAPPER_FRONT_FAR_EAST2:
			case MAPPER_MMC2:
			case MAPPER_MMC4:
			case MAPPER_COLOR_DREAMS:
			case MAPPER_MMC3A:
			case MAPPER_CPROM:
			case MAPPER_MMC3_PIRATE14:
			case MAPPER_P15:
			case MAPPER_BANDAI_EPROM:
			case MAPPER_JALECO_SS8806:
			case MAPPER_NAMCO163:
			case MAPPER_VRC4A:
			case MAPPER_VRC2A:
			case MAPPER_VRC2B:
			case MAPPER_VRC6A:
			case MAPPER_VRC4B:
			case MAPPER_VRC6B:
			case MAPPER_VRC4:
			case MAPPER_ACTION53:
			case MAPPER_SEALIE_COMPUTING:
			case MAPPER_UNROM_512:
			case MAPPER_NSF:
			case MAPPER_G101:
			case MAPPER_TC0190:
			case MAPPER_BNROM_NINA001:
			case MAPPER_D35:
			case MAPPER_01_22000_400:
			case MAPPER_MMC3_EUROPE_MULTICART:
			case MAPPER_M38:
			case MAPPER_M39:
			case MAPPER_LF36:
			case MAPPER_CALTRON:
			case MAPPER_P42:
			case MAPPER_P43:
			case MAPPER_MMC3_MULTICART44:
			case MAPPER_MMC3_MULTICART45:
			case MAPPER_RUMBLE_STATION:
			case MAPPER_MMC3_MULTICART47:
			case MAPPER_TC0690:
			case MAPPER_MMC3_MULTICART49:
			case MAPPER_M50:
			case MAPPER_BMC51:
			case MAPPER_MMC3_MULTICART52:
			case MAPPER_SUPERVISION:
			case MAPPER_NOVEL_DIAMOND:
			case MAPPER_KEISER202:
			case MAPPER_M57:
			case MAPPER_M58:
			case MAPPER_M59:
			case MAPPER_M60:
			case MAPPER_M61:
			case MAPPER_M62:
			case MAPPER_BMC63:
			case MAPPER_RAMBO_1:
			case MAPPER_H3001:
			case MAPPER_GXROM:
			case MAPPER_SUNSOFT3:
			case MAPPER_SUNSOFT4:
			case MAPPER_SUNSOFT_FME7:
			case MAPPER_BANDAI_74161A:
			case MAPPER_BF909X:
			case MAPPER_JALECO_JF17A:
			case MAPPER_VRC3:
			case MAPPER_MMC3_CHR1:
			case MAPPER_VRC1:
			case MAPPER_NAMCO108:
			case MAPPER_LROG017:
			case MAPPER_JALECO_JF16:
			case MAPPER_NINA03A:
			case MAPPER_X1_005A:
			case MAPPER_M81:
			case MAPPER_X1_017:
			case MAPPER_M83:
			case MAPPER_VRC7:
			case MAPPER_JALECO_JF13:
			case MAPPER_M87:
			case MAPPER_D88:
			case MAPPER_SUNSOFT2A:
			case MAPPER_JY_COMPANY:
			case MAPPER_M91:
			case MAPPER_JALECO_JF17B:
			case MAPPER_SUNSOFT2B:
			case MAPPER_HVC_UN1ROM:
			case MAPPER_NAMCO3425:
			case MAPPER_OEKA_KIDS:
			case MAPPER_TAM_S1:
			case MAPPER_VS_SYSTEM:
			case MAPPER_M101:
			case MAPPER_M103:
			case MAPPER_GOLDEN_FIVE:
			case MAPPER_MMC1_NES_EVENT:
			case MAPPER_P106:
			case MAPPER_M107:
			case MAPPER_M108:
			case MAPPER_GTROM:
			case MAPPER_M112:
			case MAPPER_NINA03B:
			case MAPPER_MMC3_PIRATE114:
			case MAPPER_MMC3_PIRATE115:
			case MAPPER_SOMARI_P:
			case MAPPER_M117:
			case MAPPER_TXSROM:
			case MAPPER_MMC3_CHR2:
			case MAPPER_M120:
			case MAPPER_MMC3_PIRATE121:
			case MAPPER_MMC3_PIRATE123:
			case MAPPER_UNL_LH32:
			case MAPPER_MMC3_PIRATE126:
			case MAPPER_TXC_MGC:
			case MAPPER_SACHEN3009:
			case MAPPER_MMC3_PIRATE134:
			case MAPPER_SACHEN3011:
			case MAPPER_SACHEN8259D:
			case MAPPER_SACHEN8259B:
			case MAPPER_SACHEN8259C:
			case MAPPER_JALECO_JF11:
			case MAPPER_SACHEN8259A:
			case MAPPER_D142:
			case MAPPER_M143:
			case MAPPER_SACHEN72007:
			case MAPPER_NINA03C:
			case MAPPER_SACHEN3018:
			case MAPPER_800008:
			case MAPPER_SACHEN0036:
			case MAPPER_SACHEN_74LS374N:
			case MAPPER_BANDAI_74161B:
			case MAPPER_D153:
			case MAPPER_NAMCO3453:
			case MAPPER_MMC1A:
			case MAPPER_DIS23C01:
			case MAPPER_D157:
			case MAPPER_RAMBO_1M:
			case MAPPER_D159:
			case MAPPER_M162:
			case MAPPER_NANJING:
			case MAPPER_M164:
			case MAPPER_MMC2_MMC3_HYBRID:
			case MAPPER_SUBOR:
			case MAPPER_D167:
			case MAPPER_M168:
			case MAPPER_M170:
			case MAPPER_KEISER7058:
			case MAPPER_SUPER_MEGA_P4070:
			case MAPPER_M173:
			case MAPPER_M174:
			case MAPPER_KEISER7022:
			case MAPPER_M176:
			case MAPPER_HENGGE_DIANZI:
			case MAPPER_M178:
			case MAPPER_M179:
			case MAPPER_UNROM_U:
			case MAPPER_MMC3_PIRATE182:
			case MAPPER_P183:
			case MAPPER_SUNSOFT1:
			case MAPPER_CNROM_W:
			case MAPPER_M186:
			case MAPPER_MMC3_PIRATE187:
			case MAPPER_BANDAI_M60001:
			case MAPPER_MMC3_M1:
			case MAPPER_M190:
			case MAPPER_MMC3_CHR3:
			case MAPPER_MMC3_CHR4:
			case MAPPER_TC112:
			case MAPPER_MMC3_CHR5:
			case MAPPER_MMC3_CHR6:
			case MAPPER_MMC3_PIRATE196:
			case MAPPER_MMC3_PIRATE197:
			case MAPPER_MMC3_PIRATE198:
			case MAPPER_MMC3_PIRATE199:
			case MAPPER_M200:
			case MAPPER_M201:
			case MAPPER_P202:
			case MAPPER_M203:
			case MAPPER_M204:
			case MAPPER_M205:
			case MAPPER_MIMIC_1:
			case MAPPER_X1_005B:
			case MAPPER_M208:
			case MAPPER_D209:
			case MAPPER_D210:
			case MAPPER_D211:
			case MAPPER_P212:
			case MAPPER_M213:
			case MAPPER_M214:
			case MAPPER_UNL_8237:
			case MAPPER_M216:
			case MAPPER_M217:
			case MAPPER_M218:
			case MAPPER_MMC3_PIRATE219:
			case MAPPER_M220:
			case MAPPER_N625092:
			case MAPPER_CTC31:
			case MAPPER_M225:
			case MAPPER_M226:
			case MAPPER_M227:
			case MAPPER_ACTIVE_ENTERPRISES:
			case MAPPER_BMC31:
			case MAPPER_M230:
			case MAPPER_M231:
			case MAPPER_BF9096:
			case MAPPER_M233:
			case MAPPER_M234:
			case MAPPER_M235:
			case MAPPER_M236:
			case MAPPER_M238:
			case MAPPER_M240:
			case MAPPER_M242:
			case MAPPER_M243:
			case MAPPER_M244:
			case MAPPER_MMC3_PIRATE245:
			case MAPPER_M246:
			case MAPPER_M249:
			case MAPPER_NITRA:
			case MAPPER_M252:
			case MAPPER_M253:
			case MAPPER_MMC3_PIRATE254:
			case MAPPER_M255:
			{ // not supported yet
				return (false);
			}

			case MAPPER_FDS_20:
			case MAPPER_UNDOCUMENTED_55:
			case MAPPER_BROKEN_84:
			case MAPPER_UNDOCUMENTED_98:
			case MAPPER_BROKEN_100:
			case MAPPER_UNDOCUMENTED_102:
			case MAPPER_UNDOCUMENTED_109:
			case MAPPER_UNDOCUMENTED_110:
			case MAPPER_UNDOCUMENTED_122:
			case MAPPER_UNDOCUMENTED_124:
			case MAPPER_UNDOCUMENTED_127:
			case MAPPER_UNDOCUMENTED_128:
			case MAPPER_UNDOCUMENTED_129:
			case MAPPER_UNDOCUMENTED_130:
			case MAPPER_UNDOCUMENTED_131:
			case MAPPER_UNDOCUMENTED_135:
			case MAPPER_BROKEN_144:
			case MAPPER_BROKEN_151:
			case MAPPER_BROKEN_160:
			case MAPPER_UNDOCUMENTED_161:
			case MAPPER_UNDOCUMENTED_169:
			case MAPPER_BROKEN_181:
			case MAPPER_UNDOCUMENTED_223:
			case MAPPER_UNDOCUMENTED_224:
			case MAPPER_UNDOCUMENTED_237:
			case MAPPER_UNDOCUMENTED_239:
			case MAPPER_BROKEN_241:
			case MAPPER_UNDOCUMENTED_247:
			case MAPPER_UNDOCUMENTED_248:
			case MAPPER_UNDOCUMENTED_251:
			{ // not game cartridges
				return (false);
			}

			default:
			{ // not recognized
				return (false);
			}
		}

		MM_currentMapper = mapper_type;
		return (true);
	}

	void writePRG(uint16_t address, uint8_t data)
	{
		MM_busFunctions.writePRG(address, data);
	}

	uint8_t readPRG(uint16_t address)
	{
		return (MM_busFunctions.readPRG(address));
	}

	void writeCHR(uint16_t address, uint8_t data)
	{
		MM_busFunctions.writeCHR(address, data);
	}

	uint8_t readCHR(uint16_t address)
	{
		return (MM_busFunctions.readCHR(address));
	}

	uint8_t getNameTableMirroring()
	{
		return (MM_nameTableMirroring);
	}

	void clean()
	{
		switch (MM_currentMapper)
		{
			case MAPPER_NONE:
			{
				if (MM_parameters.MapperNone.characterRAM)
				{
					delete[] MM_parameters.MapperNone.characterRAM;
				}

				break;
			}

			case MAPPER_MMC1:
			{
				if (MM_parameters.MapperMMC1.characterRAM)
				{
					delete[] MM_parameters.MapperMMC1.characterRAM;
				}

				break;
			}

			case MAPPER_UNROM:
			{
				if (MM_parameters.MapperUNROM.characterRAM)
				{
					delete[] MM_parameters.MapperUNROM.characterRAM;
				}

				break;
			}

			case MAPPER_CNROM:
			{
				break;
			}

			case MAPPER_MMC3:
			{
				break;
			}
		}
	}
}

/* MAPPER_NONE function definitions */
void Mapper_None__writePRG(uint16_t address, uint8_t data)
{
	return; // this mapper does not allow writing to PRG-ROM
}

uint8_t Mapper_None__readPRG(uint16_t address)
{
	if (MM_parameters.MapperNone.bankCount == 1)
	{ // uses mirroring
		return (CR::getROM()[(address - 0x8000) & 0x3FFF]);
	}
	else
	{
		return (CR::getROM()[address - 0x8000]);
	}
}

void Mapper_None__writeCHR(uint16_t address, uint8_t data)
{
	if (MM_parameters.MapperNone.characterRAM)
	{
		MM_parameters.MapperNone.characterRAM[address] = data;
	}
	// else ... writing to CHR-ROM is not allowed in this mapper
}

uint8_t Mapper_None__readCHR(uint16_t address)
{
	if (MM_parameters.MapperNone.characterRAM)
	{
		return (MM_parameters.MapperNone.characterRAM[address]);
	}
	else
	{
		return (CR::getVideoROM()[address]);
	}
}

/* MAPPER_MMC1 function definitions */
void Mapper_MMC1__writePRG(uint16_t address, uint8_t data)
{
	if (!(data & 0x80))
	{
		MM_parameters.MapperMMC1.temporaryRegister = (MM_parameters.MapperMMC1.temporaryRegister >> 1) | ((data & 0x01) << 4);
		++MM_parameters.MapperMMC1.writeCounter;

		if (MM_parameters.MapperMMC1.writeCounter == 5)
		{
			if (address < 0xA000)
			{ // control register
				switch (MM_parameters.MapperMMC1.temporaryRegister & 0x03)
				{
					case 0:
					{
						MM_nameTableMirroring = MM__NAME_TABLE_ONE_SCREEN_LOWER;
						break;
					}

					case 1:
					{
						MM_nameTableMirroring = MM__NAME_TABLE_ONE_SCREEN_HIGHER;
						break;
					}

					case 2:
					{
						MM_nameTableMirroring = MM__NAME_TABLE_VERTICAL;
						break;
					}

					case 3:
					{
						MM_nameTableMirroring = MM__NAME_TABLE_HORIZONTAL;
						break;
					}
				}
				MB::changeMirroring(MM_nameTableMirroring);

				MM_parameters.MapperMMC1.modePRG = (MM_parameters.MapperMMC1.temporaryRegister & 0x0C) >> 2;
				MM_parameters.MapperMMC1.modeCHR = (MM_parameters.MapperMMC1.temporaryRegister & 0x10) >> 4;

				if (MM_parameters.MapperMMC1.modePRG < 2)
				{
					MM_parameters.MapperMMC1.firstBankPRG = &CR::getROM()[(MM_parameters.MapperMMC1.registerPRG & ~0x01) * 0x4000];
					MM_parameters.MapperMMC1.secondBankPRG = &MM_parameters.MapperMMC1.firstBankPRG[0x4000];
				}
				else if (MM_parameters.MapperMMC1.modePRG == 2)
				{
					MM_parameters.MapperMMC1.firstBankPRG = CR::getROM();
					MM_parameters.MapperMMC1.secondBankPRG = &MM_parameters.MapperMMC1.firstBankPRG[MM_parameters.MapperMMC1.registerPRG * 0x4000];
				}
				else
				{
					MM_parameters.MapperMMC1.firstBankPRG = &CR::getROM()[MM_parameters.MapperMMC1.registerPRG * 0x4000];
					MM_parameters.MapperMMC1.secondBankPRG = &CR::getROM()[(MM_parameters.MapperMMC1.bankCount - 1) * 0x4000];
				}

				if (MM_parameters.MapperMMC1.modeCHR)
				{
					MM_parameters.MapperMMC1.firstBankCHR = &CR::getVideoROM()[MM_parameters.MapperMMC1.registerCHR0 * 0x1000];
					MM_parameters.MapperMMC1.secondBankCHR = &CR::getVideoROM()[MM_parameters.MapperMMC1.registerCHR1 * 0x1000];
				}
				else
				{
					MM_parameters.MapperMMC1.firstBankCHR = &CR::getVideoROM()[(MM_parameters.MapperMMC1.registerCHR0 | 0x01) * 0x1000];
					MM_parameters.MapperMMC1.secondBankCHR = &MM_parameters.MapperMMC1.firstBankCHR[0x1000];
				}
			}
			else if (address < 0xC000)
			{ // CHR0 register
				MM_parameters.MapperMMC1.registerCHR0 = MM_parameters.MapperMMC1.temporaryRegister;
				MM_parameters.MapperMMC1.firstBankCHR = &CR::getVideoROM()[(MM_parameters.MapperMMC1.registerCHR0 | (MM_parameters.MapperMMC1.modeCHR ? 0x00 : 0x01)) * 0x1000];

				if (!MM_parameters.MapperMMC1.modeCHR)
				{
					MM_parameters.MapperMMC1.secondBankCHR = &MM_parameters.MapperMMC1.firstBankCHR[0x1000];
				}
			}
			else if (address < 0xE000)
			{ // CHR1 register
				MM_parameters.MapperMMC1.registerCHR1 = MM_parameters.MapperMMC1.temporaryRegister;

				if (MM_parameters.MapperMMC1.modeCHR)
				{
					MM_parameters.MapperMMC1.secondBankCHR = &CR::getVideoROM()[MM_parameters.MapperMMC1.registerCHR1 * 0x1000];
				}
			}
			else
			{ // PRG register
				MM_parameters.MapperMMC1.temporaryRegister &= 0x0F;
				MM_parameters.MapperMMC1.registerPRG = MM_parameters.MapperMMC1.temporaryRegister;

				if (MM_parameters.MapperMMC1.modePRG < 2)
				{
					MM_parameters.MapperMMC1.firstBankPRG = &CR::getROM()[(MM_parameters.MapperMMC1.registerPRG & ~0x01) * 0x4000];
					MM_parameters.MapperMMC1.secondBankPRG = &MM_parameters.MapperMMC1.firstBankPRG[0x4000];
				}
				else if (MM_parameters.MapperMMC1.modePRG == 2)
				{
					MM_parameters.MapperMMC1.firstBankPRG = CR::getROM();
					MM_parameters.MapperMMC1.secondBankPRG = &MM_parameters.MapperMMC1.firstBankPRG[MM_parameters.MapperMMC1.registerPRG * 0x4000];
				}
				else
				{
					MM_parameters.MapperMMC1.firstBankPRG = &CR::getROM()[MM_parameters.MapperMMC1.registerPRG * 0x4000];
					MM_parameters.MapperMMC1.secondBankPRG = &CR::getROM()[(MM_parameters.MapperMMC1.bankCount - 1) * 0x4000];
				}
			}

			MM_parameters.MapperMMC1.temporaryRegister = 0;
			MM_parameters.MapperMMC1.writeCounter = 0;
		}
	}
	else
	{
		MM_parameters.MapperMMC1.temporaryRegister = 0x00;
		MM_parameters.MapperMMC1.writeCounter = 0;
		MM_parameters.MapperMMC1.modePRG = 3;

		MM_parameters.MapperMMC1.firstBankPRG = &CR::getROM()[MM_parameters.MapperMMC1.registerPRG * 0x4000];
		MM_parameters.MapperMMC1.secondBankPRG = &CR::getROM()[(MM_parameters.MapperMMC1.bankCount - 1) * 0x4000];
	}
}

uint8_t Mapper_MMC1__readPRG(uint16_t address)
{
	if (address < 0xC000)
	{
		return (MM_parameters.MapperMMC1.firstBankPRG[address & 0x3FFF]);
	}
	else
	{
		return (MM_parameters.MapperMMC1.secondBankPRG[address & 0x3FFF]);
	}
}

void Mapper_MMC1__writeCHR(uint16_t address, uint8_t data)
{
	if (MM_parameters.MapperMMC1.characterRAM)
	{
		MM_parameters.MapperMMC1.characterRAM[address] = data;
	}
}

uint8_t Mapper_MMC1__readCHR(uint16_t address)
{
	if (MM_parameters.MapperMMC1.characterRAM)
	{
		return (MM_parameters.MapperMMC1.characterRAM[address]);
	}
	else if (address < 0x1000)
	{
		return (MM_parameters.MapperMMC1.firstBankCHR[address & 0x0FFF]);
	}
	else
	{
		return (MM_parameters.MapperMMC1.secondBankCHR[address & 0x0FFF]);
	}
}

/* MAPPER_UNROM function definitions */
void Mapper_UNROM__writePRG(uint16_t address, uint8_t data)
{
	MM_parameters.MapperUNROM.selectedBank = data;
}

uint8_t Mapper_UNROM__readPRG(uint16_t address)
{
	if (address < 0xC000)
	{
		return (CR::getROM()[(address & 0x3FFF) | (MM_parameters.MapperUNROM.selectedBank << 14)]);
	}
	else
	{
		return (MM_parameters.MapperUNROM.lastBank[address & 0x3FFF]);
	}
}

void Mapper_UNROM__writeCHR(uint16_t address, uint8_t data)
{
	if (MM_parameters.MapperUNROM.characterRAM)
	{
		MM_parameters.MapperUNROM.characterRAM[address] = data;
	}
}

uint8_t Mapper_UNROM__readCHR(uint16_t address)
{
	if (MM_parameters.MapperUNROM.characterRAM)
	{
		return (MM_parameters.MapperUNROM.characterRAM[address]);
	}
	else
	{
		return (CR::getVideoROM()[address]);
	}
}

/* MAPPER_CNROM function definitions */
void Mapper_CNROM__writePRG(uint16_t address, uint8_t data)
{
	MM_parameters.MapperCNROM.selectedBank = data & 0x3;
}

uint8_t Mapper_CNROM__readPRG(uint16_t address)
{
	if (MM_parameters.MapperCNROM.bankCount == 1)
	{
		return (CR::getROM()[(address - 0x8000) & 0x3FFF]);
	}
	else
	{
		return (CR::getROM()[address - 0x8000]);
	}
}

void Mapper_CNROM__writeCHR(uint16_t address, uint8_t data)
{
	return; // this mapper does not allow writing to CHR-ROM
}

uint8_t Mapper_CNROM__readCHR(uint16_t address)
{
	return (CR::getVideoROM()[address | (MM_parameters.MapperCNROM.selectedBank << 13)]);
}

/* MAPPER_MMC3 function definitions */
void Mapper_MMC3__writePRG(uint16_t address, uint8_t data)
{
	if (address < 0xA000)
	{
		if (address & 1)
		{ // Bank data register
			switch (MM_parameters.MapperMMC3.bankSelect)
			{
				case 0: // CHR 2K bank 0
				{
					MM_parameters.MapperMMC3.CHR2k0 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 1: // CHR 2K bank 1
				{
					MM_parameters.MapperMMC3.CHR2k1 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 2: // CHR 1K bank 0
				{
					MM_parameters.MapperMMC3.CHR1k0 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 3: // CHR 1K bank 1
				{
					MM_parameters.MapperMMC3.CHR1k1 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 4: // CHR 1K bank 2
				{
					MM_parameters.MapperMMC3.CHR1k2 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 5: // CHR 1K bank 3
				{
					MM_parameters.MapperMMC3.CHR1k3 = &CR::getVideoROM()[data * 0x0400];

					break;
				}

				case 6: // PRG bank 0
				{
					MM_parameters.MapperMMC3.PRGbank0 = &CR::getROM()[data * 0x2000];
					//debug("6  " << std::hex << (int)address << "  " << (int)data, DEBUG_LEVEL_INFO);

					break;
				}

				case 7: // PRG bank 1
				{
					MM_parameters.MapperMMC3.PRGbank1 = &CR::getROM()[data * 0x2000];
					//debug("7  " << std::hex << (int)address << "  " << (int)data, DEBUG_LEVEL_INFO);

					break;
				}
			}
		}
		else
		{ // Bank select register
			MM_parameters.MapperMMC3.bankSelect = data & 0x07;
			MM_parameters.MapperMMC3.invertPRG = (data & 0x40) ? true : false;
			MM_parameters.MapperMMC3.invertCHR = (data & 0x80) ? true : false;
			//debug(std::hex << (int)address << "  " << (int)data, DEBUG_LEVEL_INFO);
		}
	}
	else if (address < 0xC000)
	{
		if (address & 1)
		{ // PRG RAM protect register
			MB::configureMemory(!!(data & 0x80), !(data & 0x40));
		}
		else
		{ // Mirroring register
			MM_nameTableMirroring = (data & 1) ^ 1;
			MB::changeMirroring(MM_nameTableMirroring);
		}
	}
	else if (address < 0xE000)
	{
		if (address & 1)
		{ // IRQ reload request register
			MM_parameters.MapperMMC3.counterReload = true;
			CPU::releaseInterruptPin(INTERRUPT_SOURCE_MM);
		}
		else
		{ // IRQ reset counter value register
			MM_parameters.MapperMMC3.counterReloadValue = data;
		}
	}
	else
	{
		if (address & 1)
		{ // IRQ enable register
			MM_parameters.MapperMMC3.enableIRQ = true;
		}
		else
		{ // IRQ disable register
			MM_parameters.MapperMMC3.enableIRQ = false;
			CPU::releaseInterruptPin(INTERRUPT_SOURCE_MM);
		}
	}
}

uint8_t Mapper_MMC3__readPRG(uint16_t address)
{
	if (MM_parameters.MapperMMC3.invertPRG)
	{
		if (address < 0xA000)
		{
			return (MM_parameters.MapperMMC3.PRGbankFixed0[address - 0x8000]);
		}
		else if (address < 0xC000)
		{
			return (MM_parameters.MapperMMC3.PRGbank1[address - 0xA000]);
		}
		else if (address < 0xE000)
		{
			return (MM_parameters.MapperMMC3.PRGbank0[address - 0xC000]);
		}
		else
		{
			return (MM_parameters.MapperMMC3.PRGbankFixed1[address - 0xE000]);
		}
	}
	else
	{
		if (address < 0xA000)
		{
			return (MM_parameters.MapperMMC3.PRGbank0[address - 0x8000]);
		}
		else if (address < 0xC000)
		{
			return (MM_parameters.MapperMMC3.PRGbank1[address - 0xA000]);
		}
		else if (address < 0xE000)
		{
			return (MM_parameters.MapperMMC3.PRGbankFixed0[address - 0xC000]);
		}
		else
		{
			return (MM_parameters.MapperMMC3.PRGbankFixed1[address - 0xE000]);
		}
	}
}

void Mapper_MMC3__writeCHR(uint16_t address, uint8_t data)
{
	return; // this mapper does not allow writing to CHR-ROM
}

uint8_t Mapper_MMC3__readCHR(uint16_t address)
{
	if (MM_parameters.MapperMMC3.invertCHR)
	{
		if (address < 0x0400)
		{
			return (MM_parameters.MapperMMC3.CHR1k0[address]);
		}
		else if (address < 0x0800)
		{
			return (MM_parameters.MapperMMC3.CHR1k1[address - 0x0400]);
		}
		else if (address < 0x0C00)
		{
			return (MM_parameters.MapperMMC3.CHR1k2[address - 0x0800]);
		}
		else if (address < 0x1000)
		{
			return (MM_parameters.MapperMMC3.CHR1k3[address - 0x0C00]);
		}
		else if (address < 0x1800)
		{
			return (MM_parameters.MapperMMC3.CHR2k0[address - 0x1000]);
		}
		else
		{
			return (MM_parameters.MapperMMC3.CHR2k1[address - 0x1800]);
		}
	}
	else
	{
		if (address < 0x0800)
		{
			return (MM_parameters.MapperMMC3.CHR2k0[address]);
		}
		else if (address < 0x1000)
		{
			return (MM_parameters.MapperMMC3.CHR2k1[address - 0x0800]);
		}
		else if (address < 0x1400)
		{
			return (MM_parameters.MapperMMC3.CHR1k0[address - 0x1000]);
		}
		else if (address < 0x1800)
		{
			return (MM_parameters.MapperMMC3.CHR1k1[address - 0x1400]);
		}
		else if (address < 0x1C00)
		{
			return (MM_parameters.MapperMMC3.CHR1k2[address - 0x1800]);
		}
		else
		{
			return (MM_parameters.MapperMMC3.CHR1k3[address - 0x1C00]);
		}
	}
}