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

void WriteLog(char* msg)
{
	FILE* fd;
	fd = fopen("log.txt", "a");
	fprintf(fd, "%s\n", msg);
	fclose(fd);
}

typedef struct fname{
	char name[256];
}f_name;

typedef struct flist{
	f_name fname[256];
	int cnt;
}f_list;

f_list filelist;

void ClearFileList(){
	filelist.cnt =0;
}


int HasExtension(char *filename){
	if(filename[strlen(filename)-4] == '.'){
		return 1;
	}
	return 0;
}


void GetExtension(const char *srcfile,char *outext){
	if(HasExtension((char *)srcfile)){
		strcpy(outext,srcfile + strlen(srcfile) - 3);
	}else{
		strcpy(outext,"");
	}
}

enum {
	EXT_NDS = 1,
	EXT_GZ = 2,
	EXT_ZIP = 4,
	EXT_UNKNOWN = 8,
};

const struct {
	char *szExt;
	int nExtId;
} stExtentions[] = {
	{"nds",EXT_NDS},
//	{"gz",EXT_GZ},
//	{"zip",EXT_ZIP},
	{NULL, EXT_UNKNOWN}
};

int getExtId(const char *szFilePath) {
	char *pszExt;

	if ((pszExt = strrchr(szFilePath, '.'))) {
		pszExt++;
		int i;
		for (i = 0; stExtentions[i].nExtId != EXT_UNKNOWN; i++) {
			if (!strcasecmp(stExtentions[i].szExt,pszExt)) {
				return stExtentions[i].nExtId;
			}
		}
	}

	return EXT_UNKNOWN;
}


void GetFileList(const char *root) {
DIR *dir; struct dirent *ent;
if ((dir = opendir (root)) != NULL) {
  while ((ent = readdir (dir)) != NULL) {
    if(getExtId(ent->d_name)!= EXT_UNKNOWN){
				strcpy(filelist.fname[filelist.cnt].name,ent->d_name);
				filelist.cnt++;
				}
  }
  closedir (dir);
} else {
  /* could not open directory */

  return;
}
}

char ROM_PATH[256];

int selpos=0, oldpos = -1, oldpage = 0;
void DisplayFileList()
{
		/*DrawRom(root, &filelist, selpos, true);
	return;*/

	static const int MAX_ROM = filelist.cnt;
	static const int ROM_SHOWN = 20;

	const int CURR_PAGE = (selpos / ROM_SHOWN);

	//const int HOW_MANY = (MAX_ROM - (CURR_PAGE * 15));

	const int index = CURR_PAGE * ROM_SHOWN;

	if (selpos == oldpos) return;

	//printf("Curr page: %d, index: %d \n", CURR_PAGE,index);

	//pspDebugScreenClear();
	//DrawRom(root, &filelist, selpos, true);

	bool max_reached = false;

	oldpos = selpos;
	
	/*if (oldpage != CURR_PAGE)
		pspDebugScreenClear();

	oldpage = CURR_PAGE;*/


	if (CURR_PAGE > 0)
		pspDebugScreenPrintf("\nBack\n");
	else 
		pspDebugScreenPrintf("\n\n");


	for (int c = index;c < index + ROM_SHOWN;c++) {

		if (c >= MAX_ROM) {
			pspDebugScreenPrintf("\n");
			max_reached = true;
			continue;
		}

		if (selpos == c) {
			pspDebugScreenSetTextColor(0x0000ffff);
		}
		else {
			pspDebugScreenSetTextColor(0xffffffff);
		}

		pspDebugScreenPrintf("\n%s", filelist.fname[c].name);
	}

	pspDebugScreenSetTextColor(0xffffffff);

	if(!max_reached)
		pspDebugScreenPrintf("\n\nNext Page");
	else
		pspDebugScreenPrintf("\n\n\n");
}

void ROM_CHOOSER()
{
	SceCtrlData pad,oldPad;

	ClearFileList();
 
	GetFileList("ROMS");

    pspDebugScreenSetXY(0,0);

	int cnt;
	long tm;
	while(1){
    sceDisplayWaitVblankStart();
    pspDebugScreenSetXY(0, 0);
	pspDebugScreenPrintf(" NooDS PSP\n");
    pspDebugScreenPrintf(" press CROSS for launch your game\n");
    pspDebugScreenPrintf(" press TRIANGLE to boot nds firmware\n");
    pspDebugScreenPrintf(" press SQUARE now for exit\n");
		DisplayFileList();
		if(sceCtrlPeekBufferPositive(&pad, 1))
		{
			if (pad.Buttons != oldPad.Buttons)
			{
				if(pad.Buttons & PSP_CTRL_SQUARE){
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
				}

				if(pad.Buttons & PSP_CTRL_LTRIGGER)
				{
					memory--;
				}
			
				if(pad.Buttons & PSP_CTRL_UP){
					selpos--;
					if(selpos < 0)selpos=0;
				}
				if(pad.Buttons & PSP_CTRL_DOWN){
					selpos++;
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

bool CPU_HACK = true;

void Manager(){ 

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlPeekBufferPositive(&pad, 1); 

	uint16_t TouchX = psp_render->GetRealTouchX(), TouchY = psp_render->GetRealTouchY();

	pspDebugScreenSetXY(0, 1);
	pspDebugScreenPrintf("FPS:   %d   ME:    %d    ",core->getFps(),core->getMEFps());

	if (pad.Buttons & PSP_CTRL_SELECT && pad.Buttons & PSP_CTRL_DOWN){
		CPU_HACK = false;
	}

	if (pad.Buttons & PSP_CTRL_SELECT && pad.Buttons & PSP_CTRL_UP){
		CPU_HACK = true;
	}

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
    psp_render = new Draw();

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
 
  ROM_CHOOSER();

  core = new Core(rom_filename);

  pspDebugScreenClear();

  running = true;
  runCore(nullptr);
  
  return 0;
}
