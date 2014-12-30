/*
 * 32bit.c
 *
 *  Created on: 23 Oct 2014
 *      Author: rjhender
 */

#include "Translate.h"

/*
 * Function to turn 64bit registers into multiple 32-bit registers
 *
 */
void Translate_ALU(code_seg_t* const codeSegment)
{
	Instruction_t*ins;
	Instruction_t*new_ins;
	regID_t Rd1, R1, R2;
	ins = codeSegment->Intermcode;

#if defined(USE_INSTRUCTION_COMMENTS)
	currentTranslation = "ALU";
#endif

	while (ins)
	{
		Rd1 = ins->Rd1.regID;
		R1 = ins->R1.regID;
		R2 = ins->R2.regID;
		int32_t imm = ins->immediate;
		switch (ins->instruction)
		{
			case SLL:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->shift = imm;
					ins->shiftType = LOGICAL_LEFT;
				}break;
			case SRL:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->shift = imm;
					ins->shiftType = LOGICAL_RIGHT;
				}break;
			case SRA:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->shift = imm;
					ins->shiftType = ARITHMETIC_RIGHT;
				}break;
			case SLLV:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->R3.regID = R2;
					ins->shiftType = LOGICAL_LEFT;
				}break;
			case SRLV:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->R3.regID = R2;
					ins->shiftType = LOGICAL_RIGHT;
				}break;
			case SRAV:
				{
					Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
					ins->R3.regID = R2;
					ins->shiftType = ARITHMETIC_RIGHT;
				}break;
			case DSLLV:
				/*
				 *		Rd1 W        Rd1            R1 W           R1               R2 W          R2
				 * [FF FF FF FF | FF FF FF FE] = [FF FF FF FF | FF FF FF FF] << [00 00 00 00 | 00 00 00 3F]
				 *
				 *
				 */

				// 1. Work out lower Word
				Instr(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1);
				ins->shiftType = LOGICAL_LEFT;
				ins->R3.regID = R2;

				// 2. Work out upper word
				new_ins = newInstr(ARM_MOV, AL, Rd1 | REG_WIDE, REG_NOT_USED, R1 | REG_WIDE);
				new_ins->shiftType = LOGICAL_LEFT;
				new_ins->R3.regID = R2;
				ADD_LL_NEXT(new_ins, ins);

				// 3. Work out lower shifted to upper
				new_ins = newInstrIS(ARM_RSB, AL, REG_TEMP_SCRATCH2, REG_NOT_USED, R2, 32);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstr(ARM_ORR, PL, Rd1 | REG_WIDE, REG_NOT_USED, R1);
				new_ins->shiftType = LOGICAL_RIGHT;
				new_ins->R3.regID = REG_TEMP_SCRATCH2;
				ADD_LL_NEXT(new_ins, ins);

				// 4. Work out R1 << into Rd1 W (i.e. where R2 > 32) If this occurs then Step 1 and 2 didn't do anything
				new_ins = newInstrIS(ARM_SUB, AL, REG_TEMP_SCRATCH1, REG_NOT_USED, R1, 32);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstr(ARM_ORR, PL, Rd1 | REG_WIDE, R1, REG_TEMP_SCRATCH1);
				ADD_LL_NEXT(new_ins, ins);

				break;
			case DSRLV:
				/*
				 *
				 * [7F FF FF FF | FF FF FF FF] = [FF FF FF FF | FF FF FF FF] >> [00 00 00 00 | 00 00 00 3F]
				 *
				 *
				 */

				//Work out lower Word
				Instr(ins, ARM_MOV, AL, ins->Rd1.regID, REG_NOT_USED, ins->R1.regID);
				ins->shiftType = LOGICAL_RIGHT;
				ins->R3.regID = R2;

				//Work out upper word
				new_ins = newInstr(ARM_MOV, AL, ins->Rd1.regID| REG_WIDE, REG_NOT_USED, ins->Rd1.regID | REG_WIDE);
				new_ins->shiftType = LOGICAL_RIGHT;
				new_ins->R3.regID = ins->R2.regID;
				ADD_LL_NEXT(new_ins, ins);

				//Work out upper shifted to lower
				new_ins = newInstrIS(ARM_SUB, AL, REG_TEMP_SCRATCH1, REG_NOT_USED, ins->R1.regID, 32);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstr(ARM_MOV, PL, REG_TEMP_SCRATCH2, ins->R1.regID, REG_NOT_USED);
				new_ins->shiftType = LOGICAL_RIGHT;
				new_ins->R3.regID = REG_TEMP_SCRATCH1;
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstr(ARM_ORR, PL, ins->Rd1.regID| REG_WIDE, ins->Rd1.regID | REG_WIDE, REG_TEMP_SCRATCH1);
				ADD_LL_NEXT(new_ins, ins);
				break;
			case DSRAV: break;
			case MULT: break;
			case MULTU: break;
			case DIV:					//TODO DIV uses signed!
				/*
				 *  clz  r3, r0                 r3 ← CLZ(r0) Count leading zeroes of N
					clz  r2, r1                 r2 ← CLZ(r1) Count leading zeroes of D
					sub  r3, r2, r3             r3 ← r2 - r3.
												 This is the difference of zeroes
												 between D and N.
												 Note that N >= D implies CLZ(N) <= CLZ(D)
					add r3, r3, #1              Loop below needs an extra iteration count

					mov r2, #0                  r2 ← 0
					b .Lloop_check4
					.Lloop4:
					  cmp r0, r1, lsl r3        Compute r0 - (r1 << r3) and update cpsr
					  adc r2, r2, r2            r2 ← r2 + r2 + C.
												  Note that if r0 >= (r1 << r3) then C=1, C=0 otherwise
					  subcs r0, r0, r1, lsl r3  r0 ← r0 - (r1 << r3) if C = 1 (this is, only if r0 >= (r1 << r3) )
					.Lloop_check4:
					  subs r3, r3, #1           r3 ← r3 - 1
					  bpl .Lloop4               if r3 >= 0 (N=0) then branch to .Lloop1

					mov r0, r2
				 */
				{
					// Count leading Zeros of N
					Instr(ins, ARM_CLZ, AL, REG_TEMP_SCRATCH3, R1, REG_NOT_USED);

					// Count leading Zeros of D
					new_ins = newInstr(ARM_CLZ, AL, REG_TEMP_SCRATCH2, R2, REG_NOT_USED);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstr(ARM_MOV, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, R1);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstr(ARM_SUB, AL, REG_TEMP_SCRATCH3, REG_TEMP_GEN2, REG_TEMP_SCRATCH3);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_ADD, AL, REG_TEMP_SCRATCH3, REG_TEMP_GEN3, REG_NOT_USED, 1);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_MOV, AL, REG_TEMP_SCRATCH2, REG_NOT_USED, REG_NOT_USED, 0);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrB(AL, 4, 0);
					new_ins->I = 0;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstr(ARM_CMP, AL, REG_NOT_USED, R1, REG_TEMP_SCRATCH0);
					new_ins->R3.regID = REG_TEMP_GEN3;
					new_ins->shiftType = LOGICAL_LEFT;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_ADC, AL, REG_TEMP_SCRATCH2, REG_TEMP_SCRATCH2, REG_TEMP_SCRATCH2, 0);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstr(ARM_SUB, CS, REG_TEMP_SCRATCH0, REG_TEMP_SCRATCH0, R2);
					new_ins->R3.regID = REG_TEMP_GEN3;
					new_ins->shiftType = LOGICAL_LEFT;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrIS(ARM_SUB, AL, REG_TEMP_SCRATCH2, REG_NOT_USED, REG_NOT_USED, 1);
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrB(PL, -4, 0);
					new_ins->I = 0;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstr(ARM_MOV, AL, Rd1, REG_NOT_USED, REG_TEMP_SCRATCH0);
					ADD_LL_NEXT(new_ins, ins);

					TRANSLATE_ABORT();
				}
				break;
			case DIVU:
			case DMULT:
			case DMULTU:
			case DDIV:
			case DDIVU:
				TRANSLATE_ABORT();
				break;
			case ADD:	// TRAP
			case ADDU:
				{
					Instr(ins, ARM_ADD, AL, Rd1, R1, R2);
				} break;
			case AND:
				{
					Instr(ins, ARM_AND, AL, Rd1, R1, R2);

					//new_ins = newInstr(ARM_AND, AL, Rd1 | REG_WIDE, R1| REG_WIDE, R2 | REG_WIDE);
				//	ADD_LL_NEXT(new_ins, ins);
				}break;
			case OR:
				{
					Instr(ins, ARM_ORR, AL, Rd1, R1, R2);

					new_ins = newInstr(ARM_ORR, AL, Rd1 | REG_WIDE, R1| REG_WIDE, R2 | REG_WIDE);
					ADD_LL_NEXT(new_ins, ins);
				}
				break;
			case XOR:
				{
					Instr(ins, ARM_EOR, AL, Rd1, R1, R2);

					new_ins = newInstr(ARM_EOR, AL, Rd1 | REG_WIDE, R1| REG_WIDE, R2 | REG_WIDE);
					ADD_LL_NEXT(new_ins, ins);
				}
				break;
			case NOR:
				Instr(ins, ARM_MVN, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, R2);

				new_ins = newInstr(ARM_ORR, AL, Rd1, REG_TEMP_SCRATCH0, R2);
				ADD_LL_NEXT(new_ins, ins);
				break;
			case SLT:
				Instr(ins, ARM_CMP, AL, REG_NOT_USED, R1, R2);

				new_ins = newInstrI(ARM_MOV, LT, Rd1, REG_NOT_USED, REG_NOT_USED, 1);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, GE, Rd1, REG_NOT_USED, REG_NOT_USED, 0);
				ADD_LL_NEXT(new_ins, ins);
				break;
			case SLTU:
				Instr(ins, ARM_CMP, AL, REG_NOT_USED, R1, R2);

				new_ins = newInstrI(ARM_MOV, CC, Rd1, REG_NOT_USED, REG_NOT_USED, 1);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, CS, Rd1, REG_NOT_USED, REG_NOT_USED, 0);
				ADD_LL_NEXT(new_ins, ins);
				break;
			case DADD:	// TRAP
			case DADDU:
				{
					InstrS(ins, ARM_ADD, AL, Rd1, R1, R2);

					new_ins = newInstr(ARM_ADC, AL, Rd1 | REG_WIDE, R1| REG_WIDE, R2 | REG_WIDE);
					ADD_LL_NEXT(new_ins, ins);
				}
				break;
			case TGE:
			case TGEU:
			case TLT:
			case TLTU:
			case TEQ:
			case TNE:
				TRANSLATE_ABORT();
				break;
			case DSLL:
				{
					assert(imm < 32);

					InstrI(ins, ARM_MOV, AL, Rd1 | REG_WIDE, REG_NOT_USED, R1| REG_WIDE, imm);
					new_ins->shiftType = LOGICAL_LEFT;

					new_ins = newInstrI(ARM_ORR, AL, Rd1 | REG_WIDE, Rd1 | REG_WIDE, R1, 32 - imm);
					new_ins->shiftType = LOGICAL_RIGHT;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_MOV, AL, Rd1 , REG_NOT_USED, R1, imm);
					ins->shiftType = LOGICAL_LEFT;
					ADD_LL_NEXT(new_ins, ins);
				}break;
			case DSRL:
				{
					assert(imm < 32);

					InstrI(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1, imm);
					ins->shiftType = LOGICAL_RIGHT;

					new_ins = newInstrI(ARM_ORR, AL, Rd1 , Rd1 , R1| REG_WIDE, 32 - imm);
					new_ins->shiftType = LOGICAL_LEFT;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_MOV, AL, Rd1 | REG_WIDE, REG_NOT_USED, R1| REG_WIDE, imm);
					new_ins->shiftType = LOGICAL_RIGHT;
					ADD_LL_NEXT(new_ins, ins);
				}break;
			case DSRA:
				{
					assert(imm < 32);

					InstrI(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1, imm);
					ins->shiftType = LOGICAL_RIGHT;

					new_ins = newInstrI(ARM_ORR, AL, Rd1 , Rd1 , R1| REG_WIDE, 32 - imm);
					new_ins->shiftType = LOGICAL_LEFT;
					ADD_LL_NEXT(new_ins, ins);

					new_ins = newInstrI(ARM_MOV, AL, Rd1 | REG_WIDE, REG_NOT_USED, R1| REG_WIDE, imm);
					new_ins->shiftType = ARITHMETIC_RIGHT;
					ADD_LL_NEXT(new_ins, ins);
				}break;
			case DSLL32:
				{
					InstrI(ins, ARM_MOV, AL, Rd1 |REG_WIDE , REG_NOT_USED, R1, imm);
					ins->shiftType = LOGICAL_LEFT;
				}
				break;
			case DSRL32:
				{
					InstrI(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1|REG_WIDE , imm);
					ins->shiftType = LOGICAL_RIGHT;
				}
				break;
			case DSRA32:
				{
					InstrIS(ins, ARM_MOV, AL, Rd1 , REG_NOT_USED, R1|REG_WIDE , imm);
					ins->shiftType = ARITHMETIC_RIGHT;

					new_ins = newInstrI(ARM_MVN, MI, Rd1 | REG_WIDE, REG_NOT_USED, REG_NOT_USED, 0);
					ADD_LL_NEXT(new_ins, ins);
				}
				break;
			case TGEI:
			case TGEIU:
			case TLTI:
			case TLTIU:
			case TEQI:
			case TNEI:
				TRANSLATE_ABORT();
				break;
			case ADDI:	// TRAP
			case ADDIU:
				{
					if (imm < 0)
					{
						InstrI(ins, ARM_SUB, AL, Rd1, R1, REG_NOT_USED, (-imm));

						if (imm < -255)
						{
							new_ins = newInstrI(ARM_SUB, AL, Rd1, Rd1, REG_NOT_USED, (-imm)&0xFF00);
							ADD_LL_NEXT(new_ins, ins);
						}
					}
					else if (imm > 0)
					{
						if (imm > 255)
						{
							if (0 == R1){
								InstrI(ins, ARM_MOV, AL, Rd1, REG_NOT_USED, REG_NOT_USED, (imm&0xFF));

								new_ins = newInstrI(ARM_ADD, AL, Rd1, Rd1, REG_NOT_USED, imm&0xFF00);
								ADD_LL_NEXT(new_ins, ins);
							}
							else
							{
								InstrI(ins, ARM_ADD, AL, Rd1, R1, REG_NOT_USED, (imm&0xFF));

								new_ins = newInstrI(ARM_ADD, AL, Rd1, Rd1, REG_NOT_USED, imm&0xFF00);
								ADD_LL_NEXT(new_ins, ins);
							}
						}
						else
						{
							InstrI(ins, ARM_ADD, AL, Rd1, R1, REG_NOT_USED, (imm&0xFF));
						}
					}
					else if (Rd1 == R1) // imm = 0
					{
						Instr(ins, NO_OP, AL, REG_NOT_USED, REG_NOT_USED, REG_NOT_USED);
					}
				}
				break;
			case SLTI:
				if (imm < 0)	//TODO no idea if this is right
				{
					InstrI(ins, ARM_MVN, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xff);

					if (imm < 255)
					{
						new_ins = newInstrI(ARM_SUB, AL, Rd1, R1, REG_NOT_USED, (-imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				else
				{
					InstrI(ins, ARM_MOV, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xff);

					if (imm > 255)
					{
						new_ins = newInstrI(ARM_ADD, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}

				new_ins = newInstr(ARM_CMP, AL, REG_NOT_USED, R1, REG_TEMP_SCRATCH0);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, LT, Rd1, REG_NOT_USED, REG_NOT_USED, 1);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, GE, Rd1, REG_NOT_USED, REG_NOT_USED, 0);
				ADD_LL_NEXT(new_ins, ins);

				break;
			case SLTIU:
				if (imm < 0)	//TODO no idea if this is right
				{
					InstrI(ins, ARM_MVN, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xff);

					if (imm < 255)
					{
						new_ins = newInstrI(ARM_SUB, AL, Rd1, R1, REG_NOT_USED, (-imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				else
				{
					InstrI(ins, ARM_MOV, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xff);

					if (imm > 255)
					{
						new_ins = newInstrI(ARM_ADD, AL, REG_TEMP_SCRATCH0, REG_NOT_USED, REG_NOT_USED, (imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}

				new_ins = newInstr(ARM_CMP, AL, REG_NOT_USED, R1, REG_TEMP_SCRATCH0);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, CC, Rd1, REG_NOT_USED, REG_NOT_USED, 1);
				ADD_LL_NEXT(new_ins, ins);

				new_ins = newInstrI(ARM_MOV, CS, Rd1, REG_NOT_USED, REG_NOT_USED, 0);
				ADD_LL_NEXT(new_ins, ins);

				break;
			case ANDI:
				if (imm < 0)
				{
					InstrI(ins, ARM_BIC, AL, Rd1, R1, REG_NOT_USED, (-imm)&0xff);

					if (imm < 255)
					{
						new_ins = newInstrI(ARM_BIC, AL, Rd1, R1, REG_NOT_USED, (-imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				else
				{
					InstrI(ins, ARM_BIC, AL, Rd1, R1, REG_NOT_USED, (imm&0xff));

					if (imm > 255)
					{
						new_ins = newInstrI(ARM_BIC, AL, Rd1, R1, REG_NOT_USED, (imm)&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				break;
			case ORI:
				ins->immediate = ins->immediate&0xff;

				if (imm < 0)
				{
					InstrI(ins, ARM_ORR, AL, Rd1, R1, REG_NOT_USED, 0xFF000000);

					new_ins = newInstrI(ARM_ORR, AL, Rd1, R1, REG_NOT_USED, 0x00FF0000);
					ADD_LL_NEXT(new_ins, ins);

					if ((imm)&0x0000FF00)
					{
						new_ins = newInstrI(ARM_ORR, AL, Rd1, R1, REG_NOT_USED, (imm)&0x0000FF00);
						ADD_LL_NEXT(new_ins, ins);
					}

					new_ins = newInstrI(ARM_ORR, AL, Rd1, R1, REG_NOT_USED, (imm)&0x000000FF);
					ADD_LL_NEXT(new_ins, ins);
				}
				else
				{
					InstrI(ins, ARM_ORR, AL, Rd1, R1, REG_NOT_USED, (imm&0xFF));

					if (imm > 255)
					{
						new_ins = newInstrI(ARM_ORR, AL, Rd1, R1, REG_NOT_USED, imm&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				break;
			case XORI:
				ins->immediate = ins->immediate&0xff;

				if (imm < 0)
				{
					InstrI(ins, ARM_EOR, AL, Rd1, R1, REG_NOT_USED, 0xFF000000);

					new_ins = newInstrI(ARM_EOR, AL, Rd1, R1, REG_NOT_USED, 0x00FF0000);
					ADD_LL_NEXT(new_ins, ins);

					if ((imm)&0x0000FF00)
					{	new_ins = newInstrI(ARM_EOR, AL, Rd1, R1, REG_NOT_USED, (imm)&0x0000FF00);
						ADD_LL_NEXT(new_ins, ins);
					}

					new_ins = newInstrI(ARM_EOR, AL, Rd1, R1, REG_NOT_USED, (imm)&0x000000FF);
					ADD_LL_NEXT(new_ins, ins);
				}
				else
				{
					InstrI(ins, ARM_EOR, AL, Rd1, R1, REG_NOT_USED, (imm&0xFF));

					if (imm > 255)
					{
						new_ins = newInstrI(ARM_EOR, AL, Rd1, R1, REG_NOT_USED, imm&0xFF00);
						ADD_LL_NEXT(new_ins, ins);
					}
				}
				break;
			case DADDI:
			case DADDIU:
				TRANSLATE_ABORT();
					break;
		default: break;
		}

		ins = ins->nextInstruction;
	}
}