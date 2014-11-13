/*
 * Branching.c
 *
 *  Created on: 23 Oct 2014
 *      Author: rjhender
 */

#include "Translate.h"
#include "InstructionSet_ascii.h"
#include "InstructionSetARM6hf.h"
#include "memory.h"


static void getTgtAddress(code_seg_t* const codeSegment, uint32_t instructionCount, int32_t offset, size_t* tgt_address, uint8_t* branchAbsolute)
{
	code_seg_t* 	BranchToSeg = getSegmentAt(codeSegment->MIPScode + codeSegment->MIPScodeLen + offset);

	if (BranchToSeg)
	{
		if (BranchToSeg == codeSegment)	//loops to self
		{
			*tgt_address = -instructionCount -1;
			*branchAbsolute = 0;
		}
		else if (BranchToSeg->ARMEntryPoint)
		{
			*tgt_address = (size_t)BranchToSeg->ARMEntryPoint;
			*branchAbsolute = 1;
		}
		else
		{
			*tgt_address = *((size_t*)(MMAP_FP_BASE + FUNC_GEN_BRANCH_UNKNOWN));
			*branchAbsolute = 1;
		}
	}
	else // No segment Found
	{
		*tgt_address = *((size_t*)(MMAP_FP_BASE + FUNC_GEN_BRANCH_UNKNOWN));
		*branchAbsolute = 1;
	}
}

void Translate_Branch(code_seg_t* const codeSegment)
{
	Instruction_t*	ins;
	Instruction_t*	new_ins;
	Instruction_e	instruction;
	int32_t 		offset;
	size_t 			tgt_address;
	uint8_t 		branchAbsolute 		= 1;
	uint32_t 		instructionCount 	= 0;

	ins = 			codeSegment->Intermcode;

#if defined(USE_INSTRUCTION_COMMENTS)
	currentTranslation = "Branching";
#endif

	while (ins)
	{
		instruction = ins->instruction;
		switch (instruction)
		{
			case BEQ:
			case BEQL:
				offset = ins->offset;
				if (0 == ins->R2.regID)
				{
					InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

					new_ins = newInstrI(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0);
					ADD_LL_NEXT(new_ins, ins);
				}
				else
				{
					Instr(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, ins->R2.regID | REG_WIDE);

					new_ins = newInstr(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , ins->R2.regID);
					ADD_LL_NEXT(new_ins, ins);
				}

				getTgtAddress(codeSegment, instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(EQ, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case BNE:
			case BNEL:

				offset = ins->offset;
				if (0 == ins->R2.regID)
				{
					InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

					new_ins = newInstrI(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0);
					ADD_LL_NEXT(new_ins, ins);
				}
				else
				{
					Instr(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, ins->R2.regID | REG_WIDE);

					new_ins = newInstr(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , ins->R2.regID);
					ADD_LL_NEXT(new_ins, ins);
				}

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(NE, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case J:
			case JAL:

				offset = ins->offset;

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(AL, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;
				ADD_LL_NEXT(new_ins, ins);
				break;

			case JR:
			case JALR:
				// we need to lookup the code segment we should be branching to according to the value in the register
				// work out address in code segment lookup table

				Instr(ins, ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, ins->R1.regID);

				tgt_address = *((size_t*)(MMAP_FP_BASE + FUNC_GEN_BRANCH_UNKNOWN));

				new_ins = newInstrB(AL, tgt_address, 1);
				if (instruction & OPS_LINK) new_ins->Ln = 1;
				ADD_LL_NEXT(new_ins, ins);

				break;

			case BLTZ:
			case BLTZL:
			case BLTZAL:
			case BLTZALL:
				offset = ins->offset;
				if (0 == ins->R2.regID)
				{
					InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

					//new_ins = newInstrI(ARM_CMP, PL, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0);
					//ADD_LL_NEXT(new_ins, ins);
				}
				else
				{
					Instr(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, ins->R2.regID | REG_WIDE);

					//new_ins = newInstrI(ARM_CMP, PL, REG_NOT_USED, ins->R1.regID , ins->R2.regID);
					//ADD_LL_NEXT(new_ins, ins);
				}

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(MI, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case BGEZ:
			case BGEZL:
			case BGEZAL:
			case BGEZALL:

				offset = ins->offset;

				InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

				new_ins = newInstrI(ARM_CMP, PL, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0); // if not minus
				ADD_LL_NEXT(new_ins, ins);

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(GEZ, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case BLEZ:
			case BLEZL:

				offset = ins->offset;

				InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

				new_ins = newInstrI(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0); // if upper 64 bit is EQ then check lower
				ADD_LL_NEXT(new_ins, ins);

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(LEZ, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case BGTZ:
			case BGTZL:

				offset = ins->offset;

				InstrI(ins, ARM_CMP, AL, REG_NOT_USED, ins->R1.regID | REG_WIDE, REG_NOT_USED, 0);

				new_ins = newInstrI(ARM_CMP, EQ, REG_NOT_USED, ins->R1.regID , REG_NOT_USED, 0); // upper is 0 what about lower
				ADD_LL_NEXT(new_ins, ins);

				getTgtAddress(codeSegment,instructionCount, offset, &tgt_address, &branchAbsolute);

				new_ins = newInstrB(HI, tgt_address, branchAbsolute);
				if (instruction & OPS_LINK) new_ins->Ln = 1;

				ADD_LL_NEXT(new_ins, ins);

				break;

			case BC1F:
			case BC1T:
			case BC1FL:
			case BC1TL:
				printf("Cannot handle this type of branch '%s' yet in Translate.c:%d\n", Instruction_ascii[STRIP(ins->instruction)], __LINE__);

				break;

		default:
			break;
		}

		ins = ins->nextInstruction;
		instructionCount++;
	}
}
