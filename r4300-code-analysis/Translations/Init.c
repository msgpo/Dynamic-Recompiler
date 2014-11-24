/*
 * Init.c
 *
 *  Created on: 24 Nov 2014
 *      Author: rjhender
 */

#include "Translate.h"

void Translate_init(code_seg_t* const codeSegment)
{
	int x;
	Instruction_t*newInstruction;
	Instruction_t*prevInstruction = NULL;

	freeIntermediateInstructions(codeSegment);

	//now build new Intermediate code
	for (x=0; x < codeSegment->MIPScodeLen; x++)
	{
		// Filter out No-ops
		//if (0 != *(codeSegment->MIPScode + x))
		{
			newInstruction = newEmptyInstr();

			mips_decode(*(codeSegment->MIPScode + x), newInstruction);

#if CACHE_REG_AS_64BIT == 1
			if (newInstruction->Rd1.regID < REG_CO) newInstruction->Rd1.regID *= 2;
			if (newInstruction->Rd2.regID < REG_CO) newInstruction->Rd2.regID *= 2;
			if (newInstruction->R1.regID < REG_CO) newInstruction->R1.regID *= 2;
			if (newInstruction->R2.regID < REG_CO) newInstruction->R2.regID *= 2;
			if (newInstruction->R3.regID < REG_CO) newInstruction->R3.regID *= 2;
#endif

#if defined(USE_INSTRUCTION_INIT_REGS)
		memcpy(&newInstruction->Rd1_init,&newInstruction->Rd1, sizeof(reg_t));
		memcpy(&newInstruction->Rd2_init,&newInstruction->Rd2, sizeof(reg_t));
		memcpy(&newInstruction->R1_init,&newInstruction->R1, sizeof(reg_t));
		memcpy(&newInstruction->R2_init,&newInstruction->R2, sizeof(reg_t));
		memcpy(&newInstruction->R3_init,&newInstruction->R3, sizeof(reg_t));
#endif

#if defined(USE_INSTRUCTION_COMMENTS)
	sprintf_mips(newInstruction->comment, (uint32_t)(codeSegment->MIPScode + x), *(codeSegment->MIPScode + x));
#endif
			if (NULL == prevInstruction)
			{
				codeSegment->Intermcode = newInstruction;

			}
			else
			{
				prevInstruction->nextInstruction = newInstruction;
			}
			prevInstruction = newInstruction;
		}

	}

	return;
}