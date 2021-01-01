#include <pspgu.h>
#include <pspgum.h>

#include <malloc.h>

#include "GUI.h"

#define TEXTURE_FLAGS (GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D)

const int POS_APP[6][2] = {
    {  70 , 50},
    { 220 , 50},
    { 370 , 50},

    { 70 , 170},
    {220 , 170},
    {370 , 170}
};

struct DispVertex
{
	u16 u, v;
	s16 x, y, z;
};

void* glist = memalign(16, 2048);

Icon menu [2][3];

int oldPage = 0, oldPos = -1;

char * selectionToChar(int selection)
{
    switch (selection)
    {
        case 1:  return  "  0.5KB"; //  0.5KB
        case 2:  return  "    8KB"; //    8KB
        case 3:  return  "   32KB"; //   32KB
        case 4:  return  "   64KB"; //   64KB
        case 5:  return  "  128KB"; //  128KB
        case 6:  return  "  256KB"; //  256KB
        case 7:  return  "  512KB"; //  512KB
        case 8:  return  " 1024KB"; // 1024KB
        case 9:  return  " 8192KB"; // 8192KB
        default: return  "    0KB"; // None
    }
}

bool getRomIcon(const char * filename, int i, int j)
{
    char rompath[256];

    char title[0xD];

	strcpy(rompath, "ROMS/");
	strcat(rompath, filename);

    // Attempt to open the ROM
    FILE *rom = fopen(rompath, "rb");
    if (!rom) return false;

    fread(title, sizeof(char), 0xC, rom);

    fseek(rom, 0, SEEK_SET);

    // Get the icon offset
    uint32_t offset;
    fseek(rom, 0x68, SEEK_SET);
    fread(&offset, sizeof(uint32_t), 1, rom);

    // Get the icon data
    uint8_t data[512];
    fseek(rom, 0x20 + offset, SEEK_SET);
    fread(data, sizeof(uint8_t), 512, rom);

    // Get the icon palette
    uint16_t palette[16];
    fseek(rom, 0x220 + offset, SEEK_SET);
    fread(palette, sizeof(uint16_t), 16, rom);

    fclose(rom);

    // Get each pixel's 5-bit palette color and convert it to 8-bit
    u16 tiles[32 * 32];
    for (int i = 0; i < 32 * 32; i++)
    {
        uint8_t index = (i & 1) ? ((data[i / 2] & 0xF0) >> 4) : (data[i / 2] & 0x0F);
        uint8_t r = index ? ((palette[index] >>  0) & 0x1F) : 0xFFFF;
        uint8_t g = index ? ((palette[index] >>  5) & 0x1F) : 0xFFFF;
        uint8_t b = index ? ((palette[index] >> 10) & 0x1F) : 0xFFFF;
        tiles[i] = (1 << 15) | (b << 10) | (g << 5) | r;
    } 
    
    u16 texture[32 * 32];

    // Rearrange the pixels from 8x8 tiles to a 32x32 texture
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            for (int k = 0; k < 4; k++)
                memcpy(&texture[256 * i + 32 * j + 8 * k], &tiles[256 * i + 8 * j + 64 * k], 8 * sizeof(u16));
        }
    }

    menu[i][j].MEMSetIcon(texture);
    menu[i][j].SetIconName(title);

    return true;
}

void GetRomIcon(const f_list * files, int page){

    for (int i = 0; i < 2;i++){
        for(int j = 0; j < 3; j++){
           if (!getRomIcon(files->fname[((page-1)*6) + i*3 + j].name,i,j)) return;
        }
    }
    
}

void DrawIcon(u16 x, u16 y, u8 line, u8 sprX, bool PointerOn) {

	sceGuColor(0xffffffff);

	struct DispVertex* vertices = (struct DispVertex*)sceGuGetMemory(2 * sizeof(struct DispVertex));

	sceGuTexMode(GU_PSM_5551, 0, 0, 0);
	sceGuTexImage(0, ICON_SZ, ICON_SZ, ICON_SZ, menu[line][sprX].GetIconData());
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexWrap(GU_CLAMP,GU_CLAMP);

	vertices[0].u = 0;
	vertices[0].v = 0;
	vertices[0].x = x;
	vertices[0].y = PointerOn ? y - 20 : y;
	vertices[0].z = 0;

	vertices[1].u = ICON_SZ;
	vertices[1].v = ICON_SZ;
	vertices[1].x = x + ICON_SZ + 15;
	vertices[1].y = (PointerOn ? y - 20 : y) + ICON_SZ + 15;
	vertices[1].z = 0;

	sceKernelDcacheWritebackInvalidateAll();
	sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);

}

void GUI_Init(){
}

void DrawROMList(const f_list * files, int page, int pos) {

    sceGuStart(GU_DIRECT, glist);

    sceGuEnable(GU_TEXTURE_2D);

    if (oldPage != page){
        oldPage = page;
        GetRomIcon(files, page);
    }

    if (pos != oldPos)
        sceGuClear(GU_COLOR_BUFFER_BIT);

    oldPos = pos;

    for (int i = 0; i + ((page-1)*6) < files->cnt && i < 6; i++){

        const int r = (i+1)/4; 
        const int c = i%3; 

        DrawIcon(POS_APP[i][0],POS_APP[i][1], r, c, (i==pos));
    }

    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

void ShowSettings(){

    sceGuStart(GU_DIRECT, glist);

    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}