/*
 * CodeSegments.c
 *
 *  Created on: 16 Apr 2014
 *      Author: rjhender
 */

#include "CodeSegments.h"
#include "InstructionSetMIPS4.h"
#include "InstructionSet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-------------------------------------------------------------------

static code_segment_data_t segmentData;
static uint8_t* CodeSegBounds;

//-------------------------------------------------------------------

static uint32_t CountRegisers(uint32_t *bitfields)
{
	int x, y;
	uint32_t c = 0;

	for (y=0; y < 3; y ++)
	{
		for (x=0; x < 32; x++)
		{
			if (bitfields[y] & (1<<x)) c++;
		}
	}
	return c;
}

//-------------------------------------------------------------------

code_seg_t* newSegment()
{
	code_seg_t* newSeg = malloc(sizeof(code_seg_t));

	memset(newSeg, 0,sizeof(code_seg_t));

	return newSeg;
}

uint32_t delSegment(code_seg_t* codeSegment)
{
	uint32_t ret = 0;

	freeIntermediateInstructions(codeSegment);
	free(codeSegment);

	return ret;
}

void freeIntermediateInstructions(code_seg_t* codeSegment)
{
	Instruction_t *prevInstruction;
	Instruction_t *nextInstruction;

	//remove any existing Intermediate code
	if (codeSegment->Intermcode)
	{
		prevInstruction = codeSegment->Intermcode;

		while (prevInstruction)
		{
			nextInstruction = prevInstruction->nextInstruction;
			free(prevInstruction);
			prevInstruction = nextInstruction;
		}
	}
	codeSegment->Intermcode = NULL;
}

code_segment_data_t* GenerateCodeSegmentData(uint32_t* ROM, uint32_t size)
{
	int x,y;
	uint32_t segmentCount = 0;

	//find the number of segments

	int32_t iStart = 0;
	
	code_seg_t* prevCodeSeg = NULL;
	code_seg_t* nextCodeSeg;

	segmentData.StaticSegments = NULL;

	CodeSegBounds = malloc(size/sizeof(*CodeSegBounds));
	memset(CodeSegBounds, BLOCK_INVALID, size/sizeof(*CodeSegBounds));
	
	/*
	 * Generate Code block validity
	 *
	 * Scan memory for code blocks and ensure there are no invalid instructions in them.
	 *
	 * */
	for (x=64/4; x< size/4; x++)
	{
		uint32_t bValidBlock = 1;
		
		for (y = x; y < size/4; y++)
		{
			Instruction_e op;
			op = ops_type(ROM[y]);

			//we are not in valid code
			if (INVALID == op)
			{
				bValidBlock = 0;
				break;
			}
			if ((op & OPS_JUMP) == OPS_JUMP)
			{
				CodeSegBounds[y] = BLOCK_END;
				//if (x < 400) printf("Segment at 0x%08X to 0x%08X (%d) j\n", (x)*4, y*4, x-6);
				break;
			}
			else if(((op & OPS_CALL) == OPS_CALL)
				|| ((op & OPS_BRANCH) == OPS_BRANCH)	//MIPS does not have an unconditional branch
				)
			{
				CodeSegBounds[y] = BLOCK_BRANCH_CONT;
				//if (x < 400) printf("Segment at 0x%08X to 0x%08X (%d) b\n", (x)*4, y*4, x-6);
				break;
			}
		}

		//mark block as valid
		if (bValidBlock)
		{
			if (y - x > 0) memset(&CodeSegBounds[x], BLOCK_PART, y - x );
			x = y;
		}
	}

	/*
	 * Build CodeSegments using map
	 *
	 * There may be branches within a segment that branch to other code segments.
	 * If this happens then the segment needs to be split.
	 * */
	iStart = 64/4;
	for (x=64/4; x< size/4; x++)
	{
		//if in invalid block then continue scanning
		if (CodeSegBounds[x] == BLOCK_INVALID){
			iStart = x+1;
			continue;
		}

		if (CodeSegBounds[x] == BLOCK_PART)
		{
			if (CodeSegBounds[x-1] == BLOCK_INVALID) CodeSegBounds[x] = BLOCK_START;
			else if (CodeSegBounds[x-1] == BLOCK_END) CodeSegBounds[x] = BLOCK_START;
			else if (CodeSegBounds[x-1] == BLOCK_BRANCH_CONT) CodeSegBounds[x] = BLOCK_START_CONT;
			else if (CodeSegBounds[x-1] == BLOCK_LOOPS) CodeSegBounds[x] = BLOCK_START_CONT;
			continue;
		}

		//we have reached the end of the segment
		if (CodeSegBounds[x] == BLOCK_END)
		{
			int32_t offset =  ops_JumpAddressOffset(ROM[x]);

			nextCodeSeg = newSegment();
			if (!segmentCount) segmentData.StaticSegments = nextCodeSeg;

			nextCodeSeg->MIPScode = &ROM[iStart];
			nextCodeSeg->MIPScodeLen = x - iStart +1;
			nextCodeSeg->ARMcodeLen = 0;
			nextCodeSeg->MIPSnextInstructionIndex = &ROM[iStart + offset];
			nextCodeSeg->blockType = BLOCK_END;

			if (ops_type(ROM[x]) == JR) //only JR can set PC to the Link Register (or other register!)
			{
				nextCodeSeg->MIPSReturnRegister = (ROM[x]>>21)&0x1f;
			}
			else
			{
				nextCodeSeg->MIPSReturnRegister = 0;
			}

			nextCodeSeg->nextCodeSegmentLinkedList = NULL;
			if (prevCodeSeg) prevCodeSeg->nextCodeSegmentLinkedList = nextCodeSeg;
			prevCodeSeg = nextCodeSeg;
			segmentCount++;

			iStart = x+1;
		}

		//check to see if instruction is a branch and if it stays local, to segment
		else if (CodeSegBounds[x] == BLOCK_BRANCH_CONT)
		{
			//nextSegment = findNextSegment(x, size);

			int32_t offset =  ops_JumpAddressOffset(ROM[x]);

			nextCodeSeg = newSegment();
			if (!segmentCount) segmentData.StaticSegments = nextCodeSeg;

			//is this a loop currently in a segment?
			if (offset < 0 && (x + offset > iStart) )
			{
				nextCodeSeg->MIPScode = &ROM[(iStart)];
				nextCodeSeg->MIPScodeLen = x + offset - iStart;

				nextCodeSeg->blockType = BLOCK_CONTINUES;
				CodeSegBounds[x + offset] = BLOCK_CONTINUES;

				nextCodeSeg->nextCodeSegmentLinkedList = NULL;
				if (prevCodeSeg) prevCodeSeg->nextCodeSegmentLinkedList = nextCodeSeg;
				prevCodeSeg = nextCodeSeg;

				segmentCount++;

				nextCodeSeg = newSegment();

				nextCodeSeg->MIPScode = &ROM[x + offset];
				nextCodeSeg->MIPScodeLen = -offset + 1;
				nextCodeSeg->MIPSnextInstructionIndex = &ROM[x + offset]; //&ROM[x + 1];

				nextCodeSeg->blockType = BLOCK_LOOPS;
				CodeSegBounds[x] = BLOCK_LOOPS;
			}

			else //must branch to another segment
			{
				nextCodeSeg->MIPScode = &ROM[iStart];
				nextCodeSeg->MIPScodeLen = x - iStart+1;
				nextCodeSeg->MIPSnextInstructionIndex = &ROM[x + offset];
				nextCodeSeg->ARMcodeLen = 0;
				nextCodeSeg->MIPSReturnRegister = 0;

				nextCodeSeg->blockType = BLOCK_BRANCH_CONT;
			}

			nextCodeSeg->nextCodeSegmentLinkedList = NULL;
			if (prevCodeSeg) prevCodeSeg->nextCodeSegmentLinkedList = nextCodeSeg;
			prevCodeSeg = nextCodeSeg;

			segmentCount++;

			iStart = x+1;
		}
	}

	//update the count of segments
	segmentData.count = segmentCount;


	/*
	 * Generate Segment Linkage
	 */
	nextCodeSeg = segmentData.StaticSegments;
	while (nextCodeSeg != NULL)
	{
		nextCodeSeg->MIPSRegistersUsed[0] = 0;
		nextCodeSeg->MIPSRegistersUsed[1] = 0;
		nextCodeSeg->MIPSRegistersUsed[2] = 0;
		for (x=0; x < nextCodeSeg->MIPScodeLen; x++)
		{
			 ops_regs_input(*(nextCodeSeg->MIPScode + x), &nextCodeSeg->MIPSRegistersUsed[0], &nextCodeSeg->MIPSRegistersUsed[1], &nextCodeSeg->MIPSRegistersUsed[2]);
			 ops_regs_output(*(nextCodeSeg->MIPScode + x), &nextCodeSeg->MIPSRegistersUsed[0], &nextCodeSeg->MIPSRegistersUsed[1], &nextCodeSeg->MIPSRegistersUsed[2]);

		}

		code_seg_t* tempCodeSeg = segmentData.StaticSegments;
		// build links between segments

		nextCodeSeg->pCodeSegmentTargets[0] = 0;
		uint32_t tgtIndex = 0;

		if (!nextCodeSeg->MIPSReturnRegister)
		{

			while (tempCodeSeg != NULL)
			{
				if (tempCodeSeg->MIPScode == nextCodeSeg->MIPSnextInstructionIndex)
				{
					nextCodeSeg->pCodeSegmentTargets[tgtIndex++] = tempCodeSeg;
					break;
				}
				tempCodeSeg = tempCodeSeg->nextCodeSegmentLinkedList;
			}

			if (nextCodeSeg->blockType == BLOCK_CONTINUES
					|| nextCodeSeg->blockType == BLOCK_BRANCH_CONT)
			{
				tempCodeSeg = segmentData.StaticSegments;

				while (tempCodeSeg != NULL)
				{
					if (tempCodeSeg->MIPScode == (nextCodeSeg->MIPScode + nextCodeSeg->MIPScodeLen))
					{
						nextCodeSeg->pCodeSegmentTargets[tgtIndex++] = tempCodeSeg;
						break;
					}
					tempCodeSeg = tempCodeSeg->nextCodeSegmentLinkedList;
				}
			}
		}


		nextCodeSeg->MIPSRegistersUsedCount = CountRegisers(nextCodeSeg->MIPSRegistersUsed);
		nextCodeSeg = nextCodeSeg->nextCodeSegmentLinkedList;
	}

	return &segmentData;
}
