#include "doomgeneric.h"

uint32_t* DG_ScreenBuffer = 0;


void dg_Create()
{
#ifndef __PSP__
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif

	DG_Init();
}

