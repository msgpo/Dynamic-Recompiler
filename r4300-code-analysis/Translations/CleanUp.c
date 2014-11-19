/*
 * CleanUp.c
 *
 *  Created on: 17 Nov 2014
 *      Author: rjhender
 */

#include "Translate.h"

void Translate_CleanUp(code_seg_t* const codeSegment)
{
	Instruction_t*ins;
	ins = codeSegment->Intermcode;

#if defined(USE_INSTRUCTION_COMMENTS)
	currentTranslation = "CleanUp";
#endif

	while (ins)
	{
		if (NO_OP == ins->instruction)
		{
			ins = InstrFree(codeSegment, ins);
		}
		else if (SLL == ins->instruction && ins->Rd1.regID == 0)	// MIPS NO_OP
		{
			ins = InstrFree(codeSegment, ins);
		}
		else if (LUI == ins->instruction)
		{
			ins = InstrFree(codeSegment, ins);
		}
		else if (CACHE == ins->instruction)
		{
			ins = InstrFree(codeSegment, ins);
		}
		/*else if (ins->instruction == ARM_ADD
				&& ins->Rd1 == ins->R1
				&& ins->R2 == REG_NOT_USED
				&& ins->immediate == 0)
		{
			ins = InstrFree(codeSegment, ins);
		}*/
		else
		{
			ins = ins->nextInstruction;
		}
	}
}
