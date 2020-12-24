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

struct Vertex
{
	u16 u, v;
	s16 x, y, z;
};

Draw::Draw()
{
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
 	
	sceGuDrawBuffer(GU_PSM_5551, frameBuffer, BUF_WIDTH/2);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)(sizeof(u32) *
    BUF_WIDTH * SCR_HEIGHT) , BUF_WIDTH);

	sceGuClearColor(0xFF404040);
    sceGuDisable(GU_SCISSOR_TEST);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceGuDisplay(GU_TRUE);
}

void Draw::SetScreenBuffer(bool upperScr,uint32_t * buff)
{
}

void Draw::DrawFPS()
{

}

void DrawSliced(int dx){

	const int sw = 240;

    for (int start = 0, end = sw; start < end; start += SLICE_SIZE, dx += SLICE_SIZE) {

		struct Vertex* vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));
		int width = (start + SLICE_SIZE) < end ? SLICE_SIZE : end - start;

		vertices[0].u = start;
		vertices[0].v = 0;
		vertices[0].x = dx;
		vertices[0].y = 40;
		vertices[0].z = 0;

		vertices[1].u = start + width;
		vertices[1].v = 192;
		vertices[1].x = dx + width;
		vertices[1].y = 192+40;
		vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES, TEXTURE_FLAGS, 2, NULL, vertices);
  }
}

void Draw::DrawFrame(uint16_t * top,uint16_t * bottom)
{
	sceGuStart(GU_DIRECT, list);

	sceGuClearColor(0 /*0x00ff00ff*/);
    //sceGuClear(GU_COLOR_BUFFER_BIT);

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

	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceKernelDcacheWritebackAll();

	sceGuSwapBuffers();

	sceGuDisplay(GU_TRUE);
}
