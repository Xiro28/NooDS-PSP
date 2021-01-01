#ifndef GUI_H
#define GUI_H

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>

#include <stdio.h>
#include <cstring>

#include <unistd.h>
#include "dirent.h"

#include "include/pspDmac.h"

#define ICON_SZ 32

class Icon {

public:

	u16* GetIconData() {
		return data;
	}

	char* GetIconName() {
		return RomName;
	}

	char* GetDevName() {
		return Developer;
	}

	char* GetFileName() {
		return Filename;
	}

	void SetIconPixel(u8 X, u8 Y, u16 pixel) {
		data[X + (Y * ICON_SZ)] = pixel;
	}

	void SetIconName(const char* Name) {
		
		if (*Name == '.')
			strcpy(RomName, "Homebrew");
		else
			strcpy(RomName, Name);
		
		RomName[11] = 0;
	}
	void SetDevName(const char* Name) {
		strcpy(Developer, Name);
		Developer[63] = 0;
	}
	void SetFileName(const char* Name) {
		strcpy(Filename, Name);
		Filename[127] = 0;
	}

	void ClearIcon(u16 color) {
		memset(data, color, ICON_SZ * ICON_SZ);
	}

	void MEMSetIcon(u16* buff) {
		sceKernelDcacheWritebackInvalidateAll();
		sceDmacMemcpy(data, buff, ICON_SZ * ICON_SZ * 2);
		sceKernelDcacheWritebackInvalidateAll();
	}

private:
	char RomName[12];
	char Developer[64];
	char Filename[128];
	__attribute__((aligned(16))) u16 data[ICON_SZ * ICON_SZ];
};


typedef struct fname{
	char name[256];
}f_name;

typedef struct flist{
	f_name fname[256];
	int cnt;
}f_list;

static void ClearFileList(f_list * filelist){
	filelist->cnt =0;
}


static int HasExtension(char *filename){
	if(filename[strlen(filename)-4] == '.'){
		return 1;
	}
	return 0;
}


static void GetExtension(const char *srcfile,char *outext){
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

static int getExtId(const char *szFilePath) {
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


static void GetFileList(f_list &filelist, const char *root) {
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

void DrawROMList(const f_list * files, int page, int pos);
void GUI_Init();


#endif