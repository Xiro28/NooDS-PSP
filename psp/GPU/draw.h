#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <stdio.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

const uint32_t SCREEN_SZ  = 256*192*2;
const uint8_t  SLICE_SIZE = 64;

extern bool CPU_HACK;

class Draw
{
    public:
        Draw();
        ~Draw();

        void SetScreenBuffer(bool upperScr,uint32_t * buff);

        void DrawFrame(uint16_t * top,uint16_t * bottom);
        void DrawFPS();
    
    private:
        const uint32_t    FRAMESIZE	 = 0x44000;			//in byte
        const uint32_t    VRAM_START = 0x4000000;
        const uint32_t    VRAM_SIZE  = 0x00200000;

        int        TexFilter;
        int        PixelFormat;
        int        TexFormat;
        int        TexColor;

        void       *DisplayBuffer;
        void       *DrawBuffer;
        void       *VramOffset;
        void       *VramChunkOffset;

        //uint16_t          * upper, * lower;

};

extern Draw * psp_render;

