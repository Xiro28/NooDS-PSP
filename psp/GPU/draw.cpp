#include <pspgu.h>
#include <pspgum.h>
#include <pspvfpu.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <pspdisplay.h>

#include <iterator>

#include <cstring>
#include <malloc.h>


#include "draw.h"
#include "pspDmac.h"

#define VRAM_START 0x4000000
#define GU_VRAM_WIDTH      512
#define TEXTURE_FLAGS (GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D)

//const int padding_top = (1024 * 24);

//void* DISP_POINTER = reinterpret_cast<void*>(VRAM_START + 0);
void* frameBuffer =  reinterpret_cast<void*>(0);

void* list = memalign(16, 2048);

struct PSPVertex
{
	u16 u, v;
	s16 x, y, z;
};


Draw::Draw()
{
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
 	
	sceGuDrawBuffer(GU_PSM_5551, frameBuffer, BUF_WIDTH/2);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, frameBuffer , BUF_WIDTH);

	sceGuClearColor(0xFF404040);
    sceGuDisable(GU_SCISSOR_TEST);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	

	sceGuDisplay(GU_TRUE);

	sceGuSwapBuffers();

}

void Draw::SetScreenBuffer(bool upperScr,uint32_t * buff)
{
}

void Draw::DrawFPS()
{

}


const int sw = 254;
static const int scale = (int)((float)sw * (float)SLICE_SIZE) / (float)256;
void DrawSliced(int dx){

    for (int start = 0, end = sw; start < end; start += SLICE_SIZE, dx += scale) {

		struct PSPVertex* vertices = (struct PSPVertex*)sceGuGetMemory(2 * sizeof(struct PSPVertex));
		int width = (start + SLICE_SIZE) < end ? SLICE_SIZE : end - start;

		vertices[0].u = start;
		vertices[0].v = 0;
		vertices[0].x = dx;
		vertices[0].y = 40;
		vertices[0].z = 0;

		vertices[1].u = start + width;
		vertices[1].v = 192;
		vertices[1].x = dx + scale;
		vertices[1].y = 192+40;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);
  }
}

void Draw::DrawTouchPointer(){

	struct PSPVertex* vertices = (struct PSPVertex*)sceGuGetMemory(2 * sizeof(struct PSPVertex));

	static const int sz_pointer = 3;

	vertices[0].u = 0;
	vertices[0].v = 0;
	vertices[0].x = TX;
	vertices[0].y = TY;
	vertices[0].z = 0;

	vertices[1].u = sz_pointer;
	vertices[1].v = sz_pointer;
	vertices[1].x = TX + sz_pointer;
	vertices[1].y = TY + sz_pointer;
	vertices[1].z = 0;

	sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);

}

void Draw::DrawFrame(uint16_t * top,uint16_t * bottom)
{
	sceGuStart(GU_DIRECT, list);

	//sceGuSwapBuffers();

	sceGuEnable(GU_TEXTURE_2D);

    sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, 256, 256, 256, top);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);

	sceGuColor(0xffffffff);

	DrawSliced(0);

	sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, 256, 256, 256, bottom);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);

	DrawSliced(240);

	sceGuDisable(GU_TEXTURE_2D);

	DrawTouchPointer();

	sceGuFinish();

	sceKernelDcacheWritebackAll();

	sceGuDisplay(GU_TRUE);
}
