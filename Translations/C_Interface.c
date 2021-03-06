/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *                                                                         *
 *  Dynamic-Recompiler - Turns MIPS code into ARM code                       *
 *  Original source: http://github.com/ricrpi/Dynamic-Recompiler             *
 *  Copyright (C) 2015  Richard Hender                                       *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation, either version 3 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "Translate.h"
#include "CodeSegments.h"
#include "InstructionSetMIPS4.h"
#include "InstructionSetARM6hf.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"
#include "mem_state.h"

#include "Debugger.h"	// RWD
#include "DebugDefines.h"

uint32_t bMemoryInlineLookup= 0U;

void cc_interrupt()
{
	uint32_t* dmaPI = (uint32_t*)(MMAP_PI) ;

	Cause |= 0x8000;

	//printf("cc_interrupt() called\n");

	if (*(dmaPI+2))
	{
		printf("PI DMA 'RAM to Cartridge' detected\n");
		memcpy((void*)*(dmaPI+1),(void*)*(dmaPI+0),*(dmaPI+2));
		*(dmaPI+3) = 0;
	}

	if (*(dmaPI+3))
	{

		printf("PI DMA 'Cartridge to RAM' detected\n");
		memcpy((void*)*(dmaPI+0),(void*)*(dmaPI+1),*(dmaPI+3));
		*(dmaPI+3) = 0;
	}


}

uint32_t virtual_address(unsigned int* addr)
{
	if (!addr)
	{
		printf("virtual_address (nil)\n");
	}
	else
	{
		printf("virtual_address 0x%08x = 0x%08x\n",(uint32_t)addr, *addr);
	}

	return 0x80000000 + (uint32_t)addr;
}

void p_r_a(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3
		, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7
		, uint32_t r8, uint32_t r9, uint32_t r10, uint32_t r11
		, uint32_t r12, uint32_t r13, uint32_t r14, uint32_t r15)
{
	printf( "\tr0 0x%08x\t r8  0x%08x\n"
			"\tr1 0x%08x\t r9  0x%08x\n"
			"\tr2 0x%08x\t r10 0x%08x\n"
			"\tr3 0x%08x\t fp  0x%08x\n"
			"\tr4 0x%08x\t ip  0x%08x\n"
			"\tr5 0x%08x\t sp  0x%08x\n"
			"\tr6 0x%08x\t lr  0x%08x\n"
			"\tr7 0x%08x\t pc  0x%08x\n"
			, r0, r8
			, r1, r9
			, r2, r10
			, r3, r11
			, r4, r12
			, r5, r13
			, r6, r14
			, r7, r15);
}

/*
 * Function to Compile MIPS code at 'address' and then return the ARM_PC counter to jump to
 */
size_t branchUnknown(code_seg_t* code_seg, size_t* address)
{
	//volatile code_seg_t* code_seg 	= segmentData.dbgCurrentSegment;
	Instruction_t* 	ins = newEmptyInstr();
	size_t ARMAddress;
	uint32_t MIPSaddress;
	Instruction_e op;

	// find out it the end of the segment is a JUMP or BRANCH
	op = ops_type(code_seg->MIPScode[code_seg->MIPScodeLen - 1]);

	//Get MIPS condition code and link
	mips_decode(code_seg->MIPScode[code_seg->MIPScodeLen - 1], ins);
	ins->outputAddress = &code_seg->MIPScode[code_seg->MIPScodeLen - 1];

	if (op & OPS_BRANCH){
		MIPSaddress =  (uint32_t)(code_seg->MIPScode + code_seg->MIPScodeLen - 1) + 4*ops_BranchOffset(code_seg->MIPScode + code_seg->MIPScodeLen - 1);
	}
	else if (op == MIPS_J || op == MIPS_JAL)
	{
		MIPSaddress =  (((size_t)code_seg->MIPScode + code_seg->MIPScodeLen * 4)&0xF0000000U) + ops_JumpAddress(&code_seg->MIPScode[code_seg->MIPScodeLen - 1]);
	}
	else if (op == MIPS_JR || op == MIPS_JALR)
	{
		MIPSaddress = address;
	}

#if SHOW_BRANCHUNKNOWN_STEPS
	printf("branchUnknown(0x%08x) called from Segment 0x%08x\n", MIPSaddress, (uint32_t)code_seg);

	printf_Intermediate(ins,1);
#endif

	code_seg_t* tgtSeg = getSegmentAt(MIPSaddress);

#if SHOW_BRANCHUNKNOWN_STEPS
	printf("tgtSeg = 0x%08x\n", (uint32_t)tgtSeg);
#endif

	// 1. Need to generate the ARM assembler for target code_segment. Use 'addr' and code Seg map.
	// 2. Then we need to patch the code_segment branch we came from. Do we need it to be a link?
	// 3. return the address to branch to.

	// 1.
	if (NULL != tgtSeg)
	{
		if (NULL == tgtSeg->ARMEntryPoint)
		{
#if SHOW_BRANCHUNKNOWN_STEPS
			printf("Translating pre-existing CodeSegment\n");
#endif
			Translate(tgtSeg);
		}
		else
		{
#if SHOW_BRANCHUNKNOWN_STEPS
			printf("Patch pre-existing CodeSegment\n");
#endif
		}

		ARMAddress = (size_t)tgtSeg->ARMEntryPoint;
	}
	else
	{
#if SHOW_BRANCHUNKNOWN_STEPS
		printf("Creating new CodeSegment for 0x%08x\n", MIPSaddress);
#endif
		tgtSeg = CompileCodeAt((uint32_t*)MIPSaddress);
		ARMAddress = (size_t)tgtSeg->ARMEntryPoint;
	}

	code_seg->pBranchNext = tgtSeg;

	// 2.
	if (((op & OPS_BRANCH) == OPS_BRANCH)
			|| (op == MIPS_J)
			|| (op == MIPS_JAL))
	{
#if USE_HOST_MANAGED_BRANCHING
		if ((op & OPS_LINK) == OPS_LINK)
		{
			if ((op & OPS_LIKELY) == OPS_LIKELY)
			{
				//Set instruction to ARM_BRANCH for new target
				InstrBL(ins, AL, ARMAddress, 1);
			}
			else
			{
				//Set instruction to ARM_BRANCH for new target
				InstrBL(ins, ins->cond, ARMAddress, 1);
			}

		}
		else
#endif
		{
			//Set instruction to ARM_BRANCH for new target
			InstrB(ins, ins->cond, ARMAddress, 1);
		}

		addToCallers(code_seg, tgtSeg);
		ins->outputAddress = (size_t)address;

#if SHOW_BRANCHUNKNOWN_STEPS
		printf_Intermediate(ins,1);
#endif

		uint32_t* 	out 		= address;

		*out = arm_encode(ins, (size_t)out);
		__clear_cache(out, &out[1]);
	}
#if USE_HOST_MANAGED_BRANCHING == 0
	else if (op == MIPS_JR)
	{
		ins = tgtSeg->Intermcode;
		Instruction_t branchIns = ins;

		while (ins != NULL)
		{
			if (ins == ARM_B)
			{
				branchIns = ins;
			}
			ins = ins->nextInstruction;
		}

		ARMAddress = (size_t)branchIns->outputAddress + 4U;
	}
#endif

	// 3.
	return ARMAddress;
}

#if defined(TEST_BRANCH_TO_C)
uint32_t test_callCode(uint32_t r0)
{
	return r0 + 0x1000;
}
#endif
/*
 * Function Called to Begin Running Dynamic compiled code.
 */
code_seg_t* Generate_CodeStart(code_segment_data_t* seg_data)
{
	code_seg_t* 	code_seg 		= newSegment();
	Instruction_t* 	newInstruction;
	Instruction_t* 	ins 			= NULL;

	seg_data->dbgCurrentSegment = code_seg;

	code_seg->Type = SEG_START;

	newInstruction 		= newInstrPUSH(AL, REG_HOST_STM_EABI2 );
	code_seg->Intermcode = ins = newInstruction;

	regID_t base;
	int32_t offset;

#if defined(TEST_BRANCHING_FORWARD)
	newInstruction 		= newInstrI(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_NOT_USED, 0);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_B, AL, REG_NOT_USED, REG_NOT_USED, REG_NOT_USED, 3);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x1);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x2);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x4);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x8);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x10);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x20);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x40);
	ADD_LL_NEXT(newInstruction, ins);

	// return back to debugger
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

#elif defined(TEST_BRANCHING_BACKWARD)

// Jump forwards to the Landing Pad
	newInstruction 		= newInstrI(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_NOT_USED, 0);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_B, AL, REG_NOT_USED, REG_NOT_USED, REG_NOT_USED, 10);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x1);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x2);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x4);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x8);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x10);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x20);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0x40);
	ADD_LL_NEXT(newInstruction, ins);

// return back to debugger
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

// Landing pad
	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrI(ARM_ADD, AL, REG_HOST_R0, REG_HOST_R0, REG_NOT_USED,0);
	ADD_LL_NEXT(newInstruction, ins);

// Now jump backwards
	newInstruction 		= newInstrI(ARM_B, AL, REG_NOT_USED, REG_NOT_USED, REG_NOT_USED, -10);
	ADD_LL_NEXT(newInstruction, ins);

#elif defined(TEST_BRANCH_TO_C)

	newInstruction 		= newInstrI(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_NOT_USED, 0);
	ADD_LL_NEXT(newInstruction, ins);

	addLiteral(code_seg, &base, &offset,(uint32_t)&test_callCode);
	assert(base == REG_HOST_PC);

	newInstruction 		= newInstrI(ARM_LDR_LIT, AL, REG_HOST_R1, REG_NOT_USED, base, offset);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_LR, REG_NOT_USED, REG_HOST_PC);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_R1);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstrPOP(AL, REG_HOST_STM_EABI2 );
	ADD_LL_NEXT(newInstruction, ins);

	// Return back to Debugger
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

#elif defined(TEST_LITERAL)	// Test Literal loading

	addLiteral(code_seg, &base, &offset,(uint32_t)MMAP_FP_BASE);
	assert(base == REG_HOST_PC);

	newInstruction 		= newInstrI(ARM_LDR_LIT, AL, REG_HOST_R0, REG_NOT_USED, base, offset);
	ADD_LL_NEXT(newInstruction, ins);

	// return back to debugger
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

#elif defined(TEST_ROR)

	newInstruction 		= newInstrI(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_NOT_USED, 0x00138000);
	ADD_LL_NEXT(newInstruction, ins);

	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

#else
	addLiteral(code_seg, &base, &offset,(uint32_t)MMAP_FP_BASE);
	assert(base == REG_HOST_PC);

	// setup the HOST_FP
	newInstruction 		= newInstrI(ARM_LDR_LIT, AL, REG_EMU_FP, REG_NOT_USED, base, offset);
	ADD_LL_NEXT(newInstruction, ins);

	// start executing recompiled code
	newInstruction 		= newInstrI(ARM_LDR, AL, REG_HOST_PC, REG_NOT_USED, REG_EMU_FP, RECOMPILED_CODE_START);
	ADD_LL_NEXT(newInstruction, ins);
#endif

	Translate_Registers(code_seg);
	Translate_Literals(code_seg);

	return code_seg;
}

code_seg_t* Generate_CodeStop(code_segment_data_t* seg_data)
{
	code_seg_t* 	code_seg 		= newSegment();
	Instruction_t* 	newInstruction;
	Instruction_t* 	ins 			= NULL;

	seg_data->dbgCurrentSegment = code_seg;

	newInstruction 		= newInstrPOP(AL, REG_HOST_STM_EABI2 );
	code_seg->Intermcode = ins = newInstruction;

	newInstruction 		= newInstrI(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_NOT_USED, 0);
	ADD_LL_NEXT(newInstruction, ins);

	// Return
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

	Translate_Registers(code_seg);

	return code_seg;
}


/*
 * This is equivalent to cc_interupt() in new_dynarec
 *
 */
code_seg_t* Generate_ISR(code_segment_data_t* seg_data)
{
	code_seg_t* 	code_seg 		= newSegment();
	Instruction_t* 	newInstruction;
	Instruction_t* 	ins 			= NULL;

	seg_data->dbgCurrentSegment = code_seg;


	// need to test if interrupts are enabled (Status Register bit 0)
	newInstruction 		= newInstrI(ARM_TST, AL, REG_NOT_USED, REG_STATUS, REG_NOT_USED, 0x01);
	code_seg->Intermcode = ins = newInstruction;

#if USE_INSTRUCTION_COMMENTS
	sprintf(newInstruction->comment, "Generate_ISR() segment 0x%08x\n", (uint32_t)code_seg);
#endif

	ins = insertCall_To_C(code_seg, ins, AL, (size_t)&cc_interrupt, REG_HOST_STM_EABI);

	// Return
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	ADD_LL_NEXT(newInstruction, ins);

#if USE_TRANSLATE_DEBUG
	Translate_Debug(code_seg);
#endif

	Translate_Registers(code_seg);

	return code_seg;
}

code_seg_t* Generate_BranchUnknown(code_segment_data_t* seg_data)
{
	code_seg_t* 	code_seg 		= newSegment();
	Instruction_t* 	newInstruction;
	Instruction_t* 	ins 			= NULL;

	seg_data->dbgCurrentSegment = code_seg;

	/* Don't think we need this instruction as function branchUnknown() can look
	 * up 'seg_data->dbgCurrentSegment' to:
	 * 1. find/compile the branch target
	 * 2. patch the branch instruction that caused us to end up here.
	 */
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_R0, REG_NOT_USED, REG_HOST_R0);
	code_seg->Intermcode = ins = newInstruction;

#if USE_INSTRUCTION_COMMENTS
	sprintf(newInstruction->comment, "Generate_BranchUnknown() segment 0x%08x\n", (uint32_t)code_seg);
#endif

	ins = insertCall_To_C(code_seg, ins, AL, (uint32_t)&branchUnknown, REG_HOST_STM_R1_3);

	// Now jump to the compiled code
	newInstruction 		= newInstrI(ARM_SUB, AL, REG_HOST_PC, REG_HOST_R0, REG_NOT_USED, 0);
	ADD_LL_NEXT(newInstruction, ins);

#if 0 && USE_TRANSLATE_DEBUG
	Translate_Debug(code_seg);
#endif

	Translate_Registers(code_seg);
	Translate_Literals(code_seg);

	return code_seg;
}

code_seg_t* Generate_MIPS_Trap(code_segment_data_t* seg_data)
{
	code_seg_t* 	code_seg 		= newSegment();
	Instruction_t* 	newInstruction;
	Instruction_t* 	ins 			= NULL;

	seg_data->dbgCurrentSegment = code_seg;

	// Return
	newInstruction 		= newInstr(ARM_MOV, AL, REG_HOST_PC, REG_NOT_USED, REG_HOST_LR);
	code_seg->Intermcode = ins = newInstruction;

#if USE_TRANSLATE_DEBUG
	Translate_Debug(code_seg);
#endif

	Translate_Registers(code_seg);

	return code_seg;
}
