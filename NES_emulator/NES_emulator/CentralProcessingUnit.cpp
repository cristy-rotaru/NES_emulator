#include "CentralProcessingUnit.h"

#include "MemoryBus.h"

#include "debug.h"

#define CPU__VECTOR_NMI (0xFFFAu)
#define CPU__VECTOR_RESET (0xFFFCu)
#define CPU__VECTOR_IRQ (0xFFFEu)

#define CPU__DMA_DURATION (514u)

#define CPU__STACK_POINTER_INITIAL_VALUE (0xFDu) 

#define CPU__PAGE_MASK (0xFF00u)
#define CPU__BRANCK_MASK (0x1Fu)
#define CPU__INSTRUCTION_TYPE_MASK (0x03u)
#define CPU__ADDRESSING_MODE_MASK (0x1Cu)
#define CPU__OPERATION_MASK (0xE0u)

#define CPU__FLAG_DEFAULT_HIGH (0x20u)
#define CPU__FLAG_NEGATIVE (0x80u)
#define CPU__FLAG_OVERFLOW (0x40u)
#define CPU__FLAG_BREAK (0x10u)
#define CPU__FLAG_DECIMAL (0x08u)
#define CPU__FLAG_INHIBIT (0x04u)
#define CPU__FLAG_ZERO (0x02u)
#define CPU__FLAG_CARRY (0x01u)

#define CPU__BRANCH_CONDITION_BIT (0x20u)
#define CPU__BRANCH_CONDITION_MASK (0xC0u)

#define CPU__BRANCH_FLAG_NEGATIVE (0x00u)
#define CPU__BRANCH_FLAG_OVERFLOW (0x40u)
#define CPU__BRANCH_FLAG_CARRY (0x80u)
#define CPU__BRANCH_FLAG_ZERO (0xC0u)

#define CPU__INSTRUCTION_TYPE_0 (0x00u)
#define CPU__INSTRUCTION_TYPE_1 (0x01u)
#define CPU__INSTRUCTION_TYPE_2 (0x02u)

#define CPU__ADDRESSING_TYPE_0_IMMEDIATE (0x00u)
#define CPU__ADDRESSING_TYPE_0_ZEROPAGE (0x04u)
#define CPU__ADDRESSING_TYPE_0_ABSOLUTE (0x0Cu)
#define CPU__ADDRESSING_TYPE_0_ZEROPAGE_X (0x14u)
#define CPU__ADDRESSING_TYPE_0_ABSOLUTE_X (0x1Cu)
#define CPU__ADDRESSING_TYPE_1_INDIRECT_X (0x00u)
#define CPU__ADDRESSING_TYPE_1_ZEROPAGE (0x04u)
#define CPU__ADDRESSING_TYPE_1_IMMEDIATE (0x08u)
#define CPU__ADDRESSING_TYPE_1_ABSOLUTE (0x0Cu)
#define CPU__ADDRESSING_TYPE_1_INDIRECT_Y (0x10u)
#define CPU__ADDRESSING_TYPE_1_ZEROPAGE_X (0x14u)
#define CPU__ADDRESSING_TYPE_1_ABSOLUTE_Y (0x18u)
#define CPU__ADDRESSING_TYPE_1_ABSOLUTE_X (0x1Cu)
#define CPU__ADDRESSING_TYPE_2_IMMEDIATE (0x00u)
#define CPU__ADDRESSING_TYPE_2_ZEROPAGE (0x04u)
#define CPU__ADDRESSING_TYPE_2_ACCUMULATOR (0x08u)
#define CPU__ADDRESSING_TYPE_2_ABSOLUTE (0x0Cu)
#define CPU__ADDRESSING_TYPE_2_ZEROPAGE_XY (0x14u)
#define CPU__ADDRESSING_TYPE_2_ABSOLUTE_XY (0x1Cu)

#define CPU__OPERATION_TYPE_0_BIT (0x20u)
#define CPU__OPERATION_TYPE_0_STY (0x80u)
#define CPU__OPERATION_TYPE_0_LDY (0xA0u)
#define CPU__OPERATION_TYPE_0_CPY (0xC0u)
#define CPU__OPERATION_TYPE_0_CPX (0xE0u)
#define CPU__OPERATION_TYPE_1_ORA (0x00u)
#define CPU__OPERATION_TYPE_1_AND (0x20u)
#define CPU__OPERATION_TYPE_1_EOR (0x40u)
#define CPU__OPERATION_TYPE_1_ADC (0x60u)
#define CPU__OPERATION_TYPE_1_STA (0x80u)
#define CPU__OPERATION_TYPE_1_LDA (0xA0u)
#define CPU__OPERATION_TYPE_1_CMP (0xC0u)
#define CPU__OPERATION_TYPE_1_SBC (0xE0u)
#define CPU__OPERATION_TYPE_2_ASL (0x00u)
#define CPU__OPERATION_TYPE_2_ROL (0x20u)
#define CPU__OPERATION_TYPE_2_LSR (0x40u)
#define CPU__OPERATION_TYPE_2_ROR (0x60u)
#define CPU__OPERATION_TYPE_2_STX (0x80u)
#define CPU__OPERATION_TYPE_2_LDX (0xA0u)
#define	CPU__OPERATION_TYPE_2_DEC (0xC0u)
#define CPU__OPERATION_TYPE_2_INC (0xE0u)

#define CPU__OPCODE_IMPLIED_BRK (0x00u)
#define CPU__OPCODE_IMPLIED_PHP (0x08u)
#define CPU__OPCODE_IMPLIED_CLC (0x18u)
#define CPU__OPCODE_IMPLIED_JSR (0x20u)
#define CPU__OPCODE_IMPLIED_PLP (0x28u)
#define CPU__OPCODE_IMPLIED_SEC (0x38u)
#define CPU__OPCODE_IMPLIED_RTI (0x40u)
#define CPU__OPCODE_IMPLIED_PHA (0x48u)
#define CPU__OPCODE_IMPLIED_JMP (0x4Cu)
#define CPU__OPCODE_IMPLIED_CLI (0x58u)
#define CPU__OPCODE_IMPLIED_RTS (0x60u)
#define CPU__OPCODE_IMPLIED_PLA (0x68u)
#define CPU__OPCODE_IMPLIED_JMPI (0x6Cu)
#define CPU__OPCODE_IMPLIED_SEI (0x78u)
#define CPU__OPCODE_IMPLIED_DEY (0x88u)
#define CPU__OPCODE_IMPLIED_TXA (0x8Au)
#define CPU__OPCODE_IMPLIED_TYA (0x98u)
#define CPU__OPCODE_IMPLIED_TXS (0x9Au)
#define CPU__OPCODE_IMPLIED_TAY (0xA8u)
#define CPU__OPCODE_IMPLIED_TAX (0xAAu)
#define CPU__OPCODE_IMPLIED_CLV (0xB8u)
#define CPU__OPCODE_IMPLIED_TSX (0xBAu)
#define CPU__OPCODE_IMPLIED_INY (0xC8u)
#define CPU__OPCODE_IMPLIED_DEX (0xCAu)
#define CPU__OPCODE_IMPLIED_CLD (0xD8u)
#define CPU__OPCODE_IMPLIED_INX (0xE8u)
#define CPU__OPCODE_IMPLIED_NOP (0xEAu)
#define CPU__OPCODE_IMPLIED_SED (0xF8u)

#define CPU__INSTRUCTION_TYPE_BRANCH (0x10u)

static constexpr uint8_t CPU_operationDuration[] =
{ // invalid opcodes take one cycle
	7, 6, 1, 1, 1, 3, 5, 1, 3, 2, 2, 1, 1, 4, 6, 1, 2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1,
	6, 6, 1, 1, 3, 3, 5, 1, 4, 2, 2, 1, 4, 4, 6, 1,	2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1,
	6, 6, 1, 1, 1, 3, 5, 1, 3, 2, 2, 1, 3, 4, 6, 1,	2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1,
	6, 6, 1, 1, 1, 3, 5, 1, 4, 2, 2, 1, 5, 4, 6, 1,	2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1,
	1, 6, 1, 1, 3, 3, 3, 1, 2, 1, 2, 1, 4, 4, 4, 1,	2, 6, 1, 1, 4, 4, 4, 1, 2, 5, 2, 1, 1, 5, 1, 1,
	2, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,	2, 5, 1, 1, 4, 4, 4, 1, 2, 4, 2, 1, 4, 4, 4, 1,
	2, 6, 1, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,	2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1,
	2, 6, 1, 1, 3, 3, 5, 1, 2, 2, 2, 2, 4, 4, 6, 1,	2, 5, 1, 1, 1, 4, 6, 1, 2, 4, 1, 1, 1, 4, 7, 1
};

static uint16_t CPU_cyclesToSkip;

static uint16_t CPU_registerPC;
static uint8_t CPU_registerSP;
static uint8_t CPU_registerA;
static uint8_t CPU_registerX;
static uint8_t CPU_registerY;
static uint8_t CPU_flags;
static uint8_t CPU_interruptRequests;

#define CPU__stack_push(data) MB::writeMainBus(CPU_registerSP-- | 0x0100, (uint8_t)(data))
#define CPU__stack_pop() MB::readMainBus(++CPU_registerSP | 0x0100)

#define CPU__read_16_bits(address_of_lsb) ((uint16_t)MB::readMainBus(address_of_lsb) | ((uint16_t)(MB::readMainBus((address_of_lsb) + 1)) << 8))

#define CPU__set_flag_zero(value) CPU_flags = ((uint8_t)(value) & 0xFF) ? (CPU_flags & ~CPU__FLAG_ZERO) : (CPU_flags | CPU__FLAG_ZERO)
#define CPU__set_flag_negative(value) CPU_flags = ((value) & 0x80) ? (CPU_flags | CPU__FLAG_NEGATIVE) : (CPU_flags & ~CPU__FLAG_NEGATIVE)

namespace CPU
{
	void reset()
	{
		CPU_cyclesToSkip = 1;

		CPU_registerPC = CPU__read_16_bits(CPU__VECTOR_RESET);
		CPU_registerSP = CPU__STACK_POINTER_INITIAL_VALUE;
		CPU_registerA = 0x00;
		CPU_registerX = 0x00;
		CPU_registerY = 0x00;

		CPU_flags = CPU__FLAG_DEFAULT_HIGH | CPU__FLAG_INHIBIT;

		CPU_interruptRequests = INTERRUPT_SOURCE_NONE;
	}

	void step()
	{
		if (CPU_interruptRequests != INTERRUPT_SOURCE_NONE)
		{
			CPU::causeInterrupt(INTERRUPT_IRQ);
		}

		if (--CPU_cyclesToSkip)
		{
			return;
		}

		uint8_t opcode = MB::readMainBus(CPU_registerPC);
		++CPU_registerPC;

		CPU_cyclesToSkip += CPU_operationDuration[opcode];

		if (CPU_cyclesToSkip == 1)
		{ // invalid opcode detected
			return;
		}

		switch (opcode)
		{ // executing implied instructions
			case CPU__OPCODE_IMPLIED_BRK:
			{
				causeInterrupt(INTERRUPT_BRK);

				break;
			}

			case CPU__OPCODE_IMPLIED_PHP:
			{
				CPU__stack_push(CPU_flags | CPU__FLAG_BREAK);

				break;
			}

			case CPU__OPCODE_IMPLIED_CLC:
			{
				CPU_flags &= ~CPU__FLAG_CARRY;

				break;
			}

			case CPU__OPCODE_IMPLIED_JSR:
			{
				CPU__stack_push((CPU_registerPC + 1) >> 8);
				CPU__stack_push(CPU_registerPC + 1);

				CPU_registerPC = CPU__read_16_bits(CPU_registerPC);

				break;
			}

			case CPU__OPCODE_IMPLIED_PLP:
			{
				CPU_flags = CPU__stack_pop();

				break;
			}

			case CPU__OPCODE_IMPLIED_SEC:
			{
				CPU_flags |= CPU__FLAG_CARRY;

				break;
			}

			case CPU__OPCODE_IMPLIED_RTI:
			{
				CPU_flags = CPU__stack_pop();

				CPU_registerPC = CPU__stack_pop();
				CPU_registerPC |= CPU__stack_pop() << 8;

				break;
			}

			case CPU__OPCODE_IMPLIED_PHA:
			{
				CPU__stack_push(CPU_registerA);

				break;
			}

			case CPU__OPCODE_IMPLIED_JMP:
			{
				CPU_registerPC = CPU__read_16_bits(CPU_registerPC);

				break;
			}

			case CPU__OPCODE_IMPLIED_CLI:
			{
				CPU_flags &= ~CPU__FLAG_INHIBIT;

				break;
			}

			case CPU__OPCODE_IMPLIED_RTS:
			{
				CPU_registerPC = CPU__stack_pop();
				CPU_registerPC |= CPU__stack_pop() << 8;
				++CPU_registerPC;

				break;
			}

			case CPU__OPCODE_IMPLIED_PLA:
			{
				CPU_registerA = CPU__stack_pop();

				CPU__set_flag_zero(CPU_registerA);
				CPU__set_flag_negative(CPU_registerA);

				break;
			}

			case CPU__OPCODE_IMPLIED_JMPI:
			{
				uint16_t address = CPU__read_16_bits(CPU_registerPC);
				uint16_t page = address & CPU__PAGE_MASK;

				CPU_registerPC = MB::readMainBus(address);
				CPU_registerPC |= (MB::readMainBus(page | ((address + 1) & ~CPU__PAGE_MASK))) << 8;

				break;
			}

			case CPU__OPCODE_IMPLIED_SEI:
			{
				CPU_flags |= CPU__FLAG_INHIBIT;

				break;
			}

			case CPU__OPCODE_IMPLIED_DEY:
			{
				--CPU_registerY;

				CPU__set_flag_zero(CPU_registerY);
				CPU__set_flag_negative(CPU_registerY);

				break;
			}

			case CPU__OPCODE_IMPLIED_TXA:
			{
				CPU_registerA = CPU_registerX;

				CPU__set_flag_zero(CPU_registerA);
				CPU__set_flag_negative(CPU_registerA);

				break;
			}

			case CPU__OPCODE_IMPLIED_TYA:
			{
				CPU_registerA = CPU_registerY;

				CPU__set_flag_zero(CPU_registerA);
				CPU__set_flag_negative(CPU_registerA);

				break;
			}

			case CPU__OPCODE_IMPLIED_TXS:
			{
				CPU_registerSP = CPU_registerX;

				break;
			}

			case CPU__OPCODE_IMPLIED_TAY:
			{
				CPU_registerY = CPU_registerA;

				CPU__set_flag_zero(CPU_registerY);
				CPU__set_flag_negative(CPU_registerY);

				break;
			}

			case CPU__OPCODE_IMPLIED_TAX:
			{
				CPU_registerX = CPU_registerA;

				CPU__set_flag_zero(CPU_registerX);
				CPU__set_flag_negative(CPU_registerX);

				break;
			}

			case CPU__OPCODE_IMPLIED_CLV:
			{
				CPU_flags &= ~CPU__FLAG_OVERFLOW;

				break;
			}

			case CPU__OPCODE_IMPLIED_TSX:
			{
				CPU_registerX = CPU_registerSP;

				CPU__set_flag_zero(CPU_registerX);
				CPU__set_flag_negative(CPU_registerX);

				break;
			}

			case CPU__OPCODE_IMPLIED_INY:
			{
				++CPU_registerY;

				CPU__set_flag_zero(CPU_registerY);
				CPU__set_flag_negative(CPU_registerY);

				break;
			}

			case CPU__OPCODE_IMPLIED_DEX:
			{
				--CPU_registerX;

				CPU__set_flag_zero(CPU_registerX);
				CPU__set_flag_negative(CPU_registerX);

				break;
			}

			case CPU__OPCODE_IMPLIED_CLD:
			{
				CPU_flags &= ~CPU__FLAG_DECIMAL;

				break;
			}

			case CPU__OPCODE_IMPLIED_INX:
			{
				++CPU_registerX;

				CPU__set_flag_zero(CPU_registerX);
				CPU__set_flag_negative(CPU_registerX);

				break;
			}

			case CPU__OPCODE_IMPLIED_NOP:
			{
				break;
			}

			case CPU__OPCODE_IMPLIED_SED:
			{
				CPU_flags |= CPU__FLAG_DECIMAL;

				break;
			}

			default:
			{
				if ((opcode & CPU__BRANCK_MASK) == CPU__INSTRUCTION_TYPE_BRANCH)
				{ // executing branch instructions
					bool willBranch = (opcode & CPU__BRANCH_CONDITION_BIT) ? true : false;

					switch (opcode & CPU__BRANCH_CONDITION_MASK)
					{ // using EQU operation
						case CPU__BRANCH_FLAG_NEGATIVE:
						{
							willBranch = !(willBranch ^ ((CPU_flags & CPU__FLAG_NEGATIVE) ? true : false));
							break;
						}

						case CPU__BRANCH_FLAG_OVERFLOW:
						{
							willBranch = !(willBranch ^ ((CPU_flags & CPU__FLAG_OVERFLOW) ? true : false));
							break;
						}

						case CPU__BRANCH_FLAG_CARRY:
						{
							willBranch = !(willBranch ^ ((CPU_flags & CPU__FLAG_CARRY) ? true : false));
							break;
						}

						case CPU__BRANCH_FLAG_ZERO:
						{
							willBranch = !(willBranch ^ ((CPU_flags & CPU__FLAG_ZERO) ? true : false));
							break;
						}
					}

					if (willBranch)
					{
						CPU_cyclesToSkip += 1;

						int8_t offset = MB::readMainBus(CPU_registerPC);
						++CPU_registerPC;

						uint16_t newPC = CPU_registerPC + offset;

						if ((CPU_registerPC & CPU__PAGE_MASK) != (newPC & CPU__PAGE_MASK))
						{ // page crossed when updating PC
							CPU_cyclesToSkip += 2;
						}

						CPU_registerPC = newPC;
					}
					else
					{
						++CPU_registerPC;
					}
				}
				else
				{
					if ((opcode & CPU__INSTRUCTION_TYPE_MASK) == CPU__INSTRUCTION_TYPE_0)
					{
						uint16_t address = 0;

						switch (opcode & CPU__ADDRESSING_MODE_MASK)
						{
							case CPU__ADDRESSING_TYPE_0_IMMEDIATE:
							{
								address = CPU_registerPC;
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_0_ZEROPAGE:
							{
								address = MB::readMainBus(CPU_registerPC);
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_0_ABSOLUTE:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								break;
							}

							case CPU__ADDRESSING_TYPE_0_ZEROPAGE_X:
							{
								address = (MB::readMainBus(CPU_registerPC) + CPU_registerX) & ~CPU__PAGE_MASK;
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_0_ABSOLUTE_X:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								if ((address & CPU__PAGE_MASK) != ((address + CPU_registerX) & CPU__PAGE_MASK))
								{
									CPU_cyclesToSkip += 1;
								}

								address += CPU_registerX;

								break;
							}
						}

						switch (opcode & CPU__OPERATION_MASK)
						{
							case CPU__OPERATION_TYPE_0_BIT:
							{
								uint8_t operand = MB::readMainBus(address);

								CPU_flags &= ~0xC0;
								CPU_flags |= operand & 0xC0;

								CPU__set_flag_zero(CPU_registerA & operand);

								break;
							}

							case CPU__OPERATION_TYPE_0_STY:
							{
								MB::writeMainBus(address, CPU_registerY);

								break;
							}

							case CPU__OPERATION_TYPE_0_LDY:
							{
								CPU_registerY = MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerY);
								CPU__set_flag_negative(CPU_registerY);

								break;
							}

							case CPU__OPERATION_TYPE_0_CPY:
							{
								uint16_t difference = CPU_registerY - MB::readMainBus(address);

								if (difference & 0x0100)
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}

								CPU__set_flag_zero(difference);
								CPU__set_flag_negative(difference);

								break;
							}

							case CPU__OPERATION_TYPE_0_CPX:
							{
								uint16_t difference = CPU_registerX - MB::readMainBus(address);

								if (difference & 0x0100)
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}

								CPU__set_flag_zero(difference);
								CPU__set_flag_negative(difference);

								break;
							}
						}
					}
					else if ((opcode & CPU__INSTRUCTION_TYPE_MASK) == CPU__INSTRUCTION_TYPE_1)
					{
						uint16_t address = 0;

						switch (opcode & CPU__ADDRESSING_MODE_MASK)
						{
							case CPU__ADDRESSING_TYPE_1_INDIRECT_X:
							{
								uint8_t baseAddress = MB::readMainBus(CPU_registerPC) + CPU_registerX; // uint8 because it is read from page 0
								CPU_registerPC += 1;

								address = MB::readMainBus(baseAddress) | ((uint16_t)MB::readMainBus((baseAddress + 1) & ~CPU__PAGE_MASK) << 8);

								break;
							}

							case CPU__ADDRESSING_TYPE_1_ZEROPAGE:
							{
								address = MB::readMainBus(CPU_registerPC);
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_IMMEDIATE:
							{
								address = CPU_registerPC;
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_ABSOLUTE:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_INDIRECT_Y:
							{
								uint8_t baseAddress = MB::readMainBus(CPU_registerPC);
								CPU_registerPC += 1;

								address = MB::readMainBus(baseAddress) | ((uint16_t)MB::readMainBus((baseAddress + 1) & ~CPU__PAGE_MASK) << 8);

								if ((address & CPU__PAGE_MASK) != ((address + CPU_registerY) & CPU__PAGE_MASK))
								{
									CPU_cyclesToSkip += 1;
								}

								address += CPU_registerY;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_ZEROPAGE_X:
							{
								address = (MB::readMainBus(CPU_registerPC) + CPU_registerX) & ~CPU__PAGE_MASK;
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_ABSOLUTE_Y:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								if ((opcode & CPU__OPERATION_MASK) != CPU__OPERATION_TYPE_1_STA)
								{
									if ((address & CPU__PAGE_MASK) != ((address + CPU_registerY) & CPU__PAGE_MASK))
									{
										CPU_cyclesToSkip += 1;
									}
								}

								address += CPU_registerY;

								break;
							}

							case CPU__ADDRESSING_TYPE_1_ABSOLUTE_X:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								if ((opcode & CPU__OPERATION_MASK) != CPU__OPERATION_TYPE_1_STA)
								{
									if ((address & CPU__PAGE_MASK) != ((address + CPU_registerX) & CPU__PAGE_MASK))
									{
										CPU_cyclesToSkip += 1;
									}
								}

								address += CPU_registerX;

								break;
							}
						}

						switch (opcode & CPU__OPERATION_MASK)
						{
							case CPU__OPERATION_TYPE_1_ORA:
							{
								CPU_registerA |= MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_AND:
							{
								CPU_registerA &= MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_EOR:
							{
								CPU_registerA ^= MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_ADC:
							{
								uint8_t parameter = MB::readMainBus(address);
								uint16_t sum = CPU_registerA + parameter + ((CPU_flags & CPU__FLAG_CARRY) ? 1 : 0);

								if (sum & 0x0100)
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}

								if ((CPU_registerA ^ sum) & (parameter ^ sum) & 0x80)
								{
									CPU_flags |= CPU__FLAG_OVERFLOW;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_OVERFLOW;
								}

								CPU_registerA = (uint8_t)sum;

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_STA:
							{
								MB::writeMainBus(address, CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_LDA:
							{
								CPU_registerA = MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}

							case CPU__OPERATION_TYPE_1_CMP:
							{
								uint16_t difference = CPU_registerA - MB::readMainBus(address);

								if (difference & 0x0100)
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}

								CPU__set_flag_zero(difference);
								CPU__set_flag_negative(difference);

								break;
							}

							case CPU__OPERATION_TYPE_1_SBC:
							{
								uint16_t parameter = MB::readMainBus(address);
								uint16_t difference = CPU_registerA - parameter - ((CPU_flags & CPU__FLAG_CARRY) ? 0 : 1);

								if (difference & 0x0100)
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}

								if ((CPU_registerA ^ difference) & (~parameter ^ difference) & 0x80)
								{
									CPU_flags |= CPU__FLAG_OVERFLOW;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_OVERFLOW;
								}

								CPU_registerA = (uint8_t)difference;

								CPU__set_flag_zero(CPU_registerA);
								CPU__set_flag_negative(CPU_registerA);

								break;
							}
						}
					}
					else if ((opcode & CPU__INSTRUCTION_TYPE_MASK) == CPU__INSTRUCTION_TYPE_2)
					{
						uint16_t address = 0;

						switch (opcode & CPU__ADDRESSING_MODE_MASK)
						{
							case CPU__ADDRESSING_TYPE_2_IMMEDIATE:
							{
								address = CPU_registerPC;
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_2_ZEROPAGE:
							{
								address = MB::readMainBus(CPU_registerPC);
								CPU_registerPC += 1;

								break;
							}

							case CPU__ADDRESSING_TYPE_2_ACCUMULATOR:
							{
								break;
							}

							case CPU__ADDRESSING_TYPE_2_ABSOLUTE:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								break;
							}

							case CPU__ADDRESSING_TYPE_2_ZEROPAGE_XY:
							{
								address = MB::readMainBus(CPU_registerPC);
								CPU_registerPC += 1;

								uint8_t index = (((opcode & CPU__OPERATION_MASK) == CPU__OPERATION_TYPE_2_LDX) || ((opcode & CPU__OPERATION_MASK) == CPU__OPERATION_TYPE_2_STX)) ? CPU_registerY : CPU_registerX;

								address += index;
								address &= ~CPU__PAGE_MASK;

								break;
							}

							case CPU__ADDRESSING_TYPE_2_ABSOLUTE_XY:
							{
								address = CPU__read_16_bits(CPU_registerPC);
								CPU_registerPC += 2;

								uint8_t index = (((opcode & CPU__OPERATION_MASK) == CPU__OPERATION_TYPE_2_LDX) || ((opcode & CPU__OPERATION_MASK) == CPU__OPERATION_TYPE_2_STX)) ? CPU_registerY : CPU_registerX;

								if ((address & CPU__PAGE_MASK) != ((address + index) & CPU__PAGE_MASK))
								{
									CPU_cyclesToSkip += 1;
								}

								address += index;

								break;
							}
						}

						switch (opcode & CPU__OPERATION_MASK)
						{
							case CPU__OPERATION_TYPE_2_ASL:
							{
								uint8_t parameter = ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR) ? CPU_registerA : MB::readMainBus(address);

								if (parameter & 0x80)
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}

								parameter <<= 1;

								CPU__set_flag_zero(parameter);
								CPU__set_flag_negative(parameter);

								if ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR)
								{
									CPU_registerA = parameter;
								}
								else
								{
									MB::writeMainBus(address, parameter);
								}

								break;
							}

							case CPU__OPERATION_TYPE_2_ROL:
							{
								uint8_t parameter = ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR) ? CPU_registerA : MB::readMainBus(address);

								uint8_t oldCarry = CPU_flags & CPU__FLAG_CARRY;

								if (parameter & 0x80)
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}

								parameter <<= 1;

								if (oldCarry)
								{
									parameter |= 0x01;
								}

								CPU__set_flag_zero(parameter);
								CPU__set_flag_negative(parameter);

								if ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR)
								{
									CPU_registerA = parameter;
								}
								else
								{
									MB::writeMainBus(address, parameter);
								}

								break;
							}

							case CPU__OPERATION_TYPE_2_LSR:
							{
								uint8_t parameter = ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR) ? CPU_registerA : MB::readMainBus(address);

								if (parameter & 0x01)
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}

								parameter >>= 1;

								CPU__set_flag_zero(parameter);

								if ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR)
								{
									CPU_registerA = parameter;
								}
								else
								{
									MB::writeMainBus(address, parameter);
								}

								break;
							}

							case CPU__OPERATION_TYPE_2_ROR:
							{
								uint8_t parameter = ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR) ? CPU_registerA : MB::readMainBus(address);

								uint8_t oldCarry = CPU_flags & CPU__FLAG_CARRY;

								if (parameter & 0x01)
								{
									CPU_flags |= CPU__FLAG_CARRY;
								}
								else
								{
									CPU_flags &= ~CPU__FLAG_CARRY;
								}

								parameter >>= 1;

								if (oldCarry)
								{
									parameter |= 0x80;
								}

								CPU__set_flag_zero(parameter);
								CPU__set_flag_negative(parameter);

								if ((opcode & CPU__ADDRESSING_MODE_MASK) == CPU__ADDRESSING_TYPE_2_ACCUMULATOR)
								{
									CPU_registerA = parameter;
								}
								else
								{
									MB::writeMainBus(address, parameter);
								}

								break;
							}

							case CPU__OPERATION_TYPE_2_STX:
							{
								MB::writeMainBus(address, CPU_registerX);

								break;
							}

							case CPU__OPERATION_TYPE_2_LDX:
							{
								CPU_registerX = MB::readMainBus(address);

								CPU__set_flag_zero(CPU_registerX);
								CPU__set_flag_negative(CPU_registerX);

								break;
							}

							case CPU__OPERATION_TYPE_2_DEC:
							{
								uint8_t parameter = MB::readMainBus(address);
								--parameter;

								CPU__set_flag_zero(parameter);
								CPU__set_flag_negative(parameter);

								MB::writeMainBus(address, parameter);

								break;
							}

							case CPU__OPERATION_TYPE_2_INC:
							{
								uint8_t parameter = MB::readMainBus(address);
								++parameter;

								CPU__set_flag_zero(parameter);
								CPU__set_flag_negative(parameter);

								MB::writeMainBus(address, parameter);

								break;
							}
						}
					}
					else
					{
						debug("Invalid opcode: " << std::hex << opcode, DEBUG_LEVEL_WARNING);
					}
				}
			}
		}
	}

	void pullInterruptPin(uint8_t source)
	{
		CPU_interruptRequests |= source;
	}

	void releaseInterruptPin(uint8_t source)
	{
		CPU_interruptRequests &= ~source;
	}

	void causeInterrupt(uint8_t interrupt)
	{
		if ((CPU_flags & CPU__FLAG_INHIBIT) && (interrupt == INTERRUPT_IRQ))
		{
			return;
		}

		if (interrupt == INTERRUPT_BRK)
		{
			++CPU_registerPC;
			CPU_flags |= CPU__FLAG_BREAK;
		}
		else
		{
			CPU_flags &= ~CPU__FLAG_BREAK;
		}

		CPU__stack_push(CPU_registerPC >> 8);
		CPU__stack_push(CPU_registerPC);
		CPU__stack_push(CPU_flags);

		CPU_flags |= CPU__FLAG_INHIBIT;

		switch (interrupt)
		{
			case INTERRUPT_IRQ:
			case INTERRUPT_BRK:
			{
				CPU_registerPC = CPU__read_16_bits(CPU__VECTOR_IRQ);

				break;
			}

			case INTERRUPT_NMI:
			{
				CPU_registerPC = CPU__read_16_bits(CPU__VECTOR_NMI);

				break;
			}
		}

		CPU_cyclesToSkip += 7;
	}

	void skipCyclesForDMA()
	{
		CPU_cyclesToSkip += CPU__DMA_DURATION;
	}
}