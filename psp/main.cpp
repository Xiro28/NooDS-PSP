/* main.c - this file is part of DeSmuME
 *
 * Copyright (C) 2006-2015 DeSmuME Team
 * Copyright (C) 2007 Pascal Giard (evilynux)
 * Used under fair use by the DSonPSP team, 2019
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <psppower.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspsuspend.h>
#include <pspkernel.h>

PSP_MODULE_INFO("NooDSPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

#include <unistd.h>
#include "dirent.h"

#include "../common/screen_layout.h"
#include "../core.h"
#include "../settings.h"
#include "../gpu.h"

#include "GUI.h"

#include "GPU/draw.h"

#include "melib.h"

ScreenLayout layout; 
Core *core;

int memory = 7;

const u16 pspKEY[12] =
  { PSP_CTRL_CIRCLE,    //A
	PSP_CTRL_CROSS,     //B
	PSP_CTRL_SELECT,	//Select
	PSP_CTRL_START,		//Start
	PSP_CTRL_RIGHT,		//Right
	PSP_CTRL_LEFT,		//Left
	PSP_CTRL_UP,		//Up
	PSP_CTRL_DOWN,		//Down
	PSP_CTRL_RTRIGGER,	//R
	PSP_CTRL_LTRIGGER,	//L
	PSP_CTRL_TRIANGLE,  //X
	PSP_CTRL_SQUARE     //Y
  };

char rom_filename[256];

Draw * psp_render;
f_list filelist;

int selpos=0;

void WriteLog(char* msg)
{
	FILE* fd;
	fd = fopen("log.txt", "a");
	fprintf(fd, "%s\n", msg);
	fclose(fd);
}

void ROM_CHOOSER()
{
	SceCtrlData pad,oldPad;

	ClearFileList(&filelist);
 
	GetFileList(filelist, "ROMS");

    pspDebugScreenSetXY(0,0);

	int cnt;
	long tm;
	while(1){

		DrawROMList(&filelist,(selpos/6) + 1,selpos%6);

		if(sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if(pad.Buttons & PSP_CTRL_HOME){
			      sceKernelExitGame();
				}

				if(pad.Buttons & PSP_CTRL_CROSS)
				{
				 sprintf(rom_filename,"ROMS/%s\0",filelist.fname[selpos].name);
				 break;
				}

				if(pad.Buttons & PSP_CTRL_TRIANGLE)
				{
				 sprintf(rom_filename,"","");
				 break;
				}

				if(pad.Buttons & PSP_CTRL_RTRIGGER)
				{
					memory++;
					if (memory > 9) memory = 9;
				}

				if(pad.Buttons & PSP_CTRL_LTRIGGER)
				{
					memory--;
					if (memory <= 0) memory = 0;
				}

				if(pad.Buttons & PSP_CTRL_LEFT){
					//selpos -= 24;
					selpos -= 1;
					if(selpos < 0)selpos=0;
				}
				if(pad.Buttons & PSP_CTRL_RIGHT){
					//selpos += 24;
					selpos += 1;
					if(selpos >= filelist.cnt -1)selpos=filelist.cnt-1;
				}
			
				if(pad.Buttons & PSP_CTRL_UP){
					selpos-= 3;
					if(selpos < 0)selpos=0;
				}
				if(pad.Buttons & PSP_CTRL_DOWN){
					selpos += 3;
					if(selpos >= filelist.cnt -1)selpos=filelist.cnt-1;
				}

			}
			oldPad = pad;
		}
	}
}

bool running = false;
bool skipframe = true;
bool done = true;

void Manager(){ 

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlPeekBufferPositive(&pad, 1); 

	uint16_t TouchX = psp_render->GetRealTouchX(), TouchY = psp_render->GetRealTouchY();

	/*pspDebugScreenSetXY(0, 1);
	pspDebugScreenPrintf("FPS:   %d   ME:    %d    ",core->getFps(),core->getMEFps());*/

	if (pad.Lx < 10) {
		--TouchX; --TouchX;
		--TouchX; --TouchX;
	}

	if (pad.Lx > 245) {
		++TouchX; ++TouchX;
		++TouchX; ++TouchX;
	}

	if (pad.Ly < 10) {
		--TouchY; --TouchY;
		--TouchY; --TouchY;
	}

	if (pad.Ly > 245) {
		++TouchY; ++TouchY;
		++TouchY; ++TouchY;
	}

	psp_render->SetTouchCoord(TouchX,TouchY);
	
	if (pad.Buttons & PSP_CTRL_RTRIGGER && pad.Buttons & PSP_CTRL_CIRCLE) {
	  	core->input.pressScreen();
        core->spi.setTouch(psp_render->GetTouchX(), psp_render->GetTouchY());
		return;
	  }else{
		core->input.releaseScreen();
        core->spi.clearTouch();
	  }

	if(pad.Buttons & PSP_CTRL_HOME){
		KILL_ME();
		sceKernelExitGame();
	}

	for(int i=0;i<12;i++) {
    if (pad.Buttons & pspKEY[i])
        core->input.pressKey(i);
    else
        core->input.releaseKey(i);
    }
	
}

int selectionToSize(int selection)
{
    switch (selection)
    {
        case 1:  return    0x200; //  0.5KB
        case 2:  return   0x2000; //    8KB
        case 3:  return   0x8000; //   32KB
        case 4:  return  0x10000; //   64KB
        case 5:  return  0x20000; //  128KB
        case 6:  return  0x40000; //  256KB
        case 7:  return  0x80000; //  512KB
        case 8:  return 0x100000; // 1024KB
        case 9:  return 0x800000; // 8192KB
        default: return        0; // None
    }
}


void runCore(void *args)
{
	core->cartridge.resizeNdsSave(selectionToSize(memory));
        
    // Run the emulator
    while (running){
        core->runFrame();
        Manager();
    }
}

int screenFilter = 0;
int showFpsCounter = 1;

int main() {

  pspDebugScreenInitEx((void*)(0x44000000), PSP_DISPLAY_PIXEL_FORMAT_5551, 1);
  scePowerSetClockFrequency(333, 333, 166);
  
  // Define the platform settings
  std::vector<Setting> platformSettings =
  {
      Setting("screenFilter",   &screenFilter,   false),
      Setting("showFpsCounter", &showFpsCounter, false)
  };

  psp_render = new Draw();

  GUI_Init();
 
  ROM_CHOOSER();

  core = new Core(rom_filename);

  pspDebugScreenClear();

  running = true;
  runCore(nullptr);
  
  return 0;
}
