/*
    Copyright 2019-2020 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstring>

#include <pspkernel.h>

#include "core.h"
#include "settings.h"
#include "melib.h"
#include "psp/GPU/draw.h"

Core * exCore;

Core::Core(std::string ndsPath, std::string gbaPath):
    cartridge(this), cp15(this), divSqrt(this), dma { Dma(this, 0), Dma(this, 1) }, gpu(this), gpu2D { Gpu2D(this, 0),
    Gpu2D(this, 1) }, gpu3D(this), gpu3DRenderer(this), input(this), interpreter { Interpreter(this, 0), Interpreter(this, 1) },
    ipc(this), memory(this), rtc(this), spi(this), spu(this), timers { Timers(this, 0), Timers(this, 1) }, wifi(this)
{

    J_Init(false);

    exCore = this;

    // Load the NDS BIOS and firmware unless directly booting a GBA ROM
    if (ndsPath != "" || gbaPath == "" || !Settings::getDirectBoot())
    {
        memory.loadBios();
        spi.loadFirmware();
    }

    if (ndsPath != "")
    {
        // Load an NDS ROM
        cartridge.loadNdsRom(ndsPath);
        
        // Prepare to boot the NDS ROM directly if direct boot is enabled
        if (Settings::getDirectBoot())
        {
            // Set some registers as the BIOS/firmware would
            cp15.write(1, 0, 0, 0x0005707D); // CP15 Control
            cp15.write(9, 1, 0, 0x0300000A); // Data TCM base/size
            cp15.write(9, 1, 1, 0x00000020); // Instruction TCM size
            memory.write<uint8_t>(0,  0x4000247,   0x03); // WRAMCNT
            memory.write<uint8_t>(0,  0x4000300,   0x01); // POSTFLG (ARM9)
            memory.write<uint8_t>(1,  0x4000300,   0x01); // POSTFLG (ARM7)
            memory.write<uint16_t>(0, 0x4000304, 0x0001); // POWCNT1
            memory.write<uint16_t>(1, 0x4000504, 0x0200); // SOUNDBIAS

            // Set some memory values as the BIOS/firmware would
            memory.write<uint32_t>(0, 0x27FF800, 0x00001FC2); // Chip ID 1
            memory.write<uint32_t>(0, 0x27FF804, 0x00001FC2); // Chip ID 2
            memory.write<uint16_t>(0, 0x27FF850,     0x5835); // ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FF880,     0x0007); // Message from ARM9 to ARM7
            memory.write<uint16_t>(0, 0x27FF884,     0x0006); // ARM7 boot task
            memory.write<uint32_t>(0, 0x27FFC00, 0x00001FC2); // Copy of chip ID 1
            memory.write<uint32_t>(0, 0x27FFC04, 0x00001FC2); // Copy of chip ID 2
            memory.write<uint16_t>(0, 0x27FFC10,     0x5835); // Copy of ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FFC40,     0x0001); // Boot indicator

            cartridge.directBoot();
            interpreter[0].directBoot();
            interpreter[1].directBoot();
            spi.directBoot();
        }
    }
}

void Core::runGbaFrame()
{
}


int PC_Core(int addr){

    if (exCore->CurrVcount >= 192) return 1;

   // for (int k = 0; k < 192;k++)
   {
        exCore->gpu2D[exCore->SwapDisplayRender].drawScanline(exCore->CurrVcount );
        exCore->gpu2D[!exCore->SwapDisplayRender].drawScanline(exCore->CurrVcount );
        exCore->dma[0].trigger(2);
    }

    return 1;
}

int ME_Core(int addr){
   for (int k = 0; k < 192;k++)
   {
        exCore->gpu2D[exCore->SwapDisplayRender].drawScanline(k);
        exCore->dma[0].trigger(2);
    }

    return 1;
}

bool first = true;
uint8_t frameskip = 0;

void Core::runNdsFrame()
{
   sceKernelDcacheWritebackInvalidateAll();

    if (ME_JobReturnValue() || first){
        SwapDisplayRender = !SwapDisplayRender;
        J_EXECUTE_ME_ONCE(ME_Core,0);

        if (SwapDisplayRender) SkipFrame = !SkipFrame; 

        first = false;
    }

    for (int i = 0; i < 263; i++) // 263 scanlines
    {
       // CurrVcount = i;

        for (int j = 0; j < 1065; j++) // 355 dots per scanline * 3
        {
            // Run the ARM9 at twice the speed of the ARM7
            {              
                if (interpreter[0].shouldRun()) interpreter[0].runCycle();    

                if (timers[0].shouldTick())     timers[0].tick(1);
                if (dma[0].shouldTransfer())    dma[0].transfer();
                
                if (interpreter[0].shouldRun()) interpreter[0].runCycle();

                if (timers[0].shouldTick())     timers[0].tick(1);
                if (dma[0].shouldTransfer())    dma[0].transfer();
            }

            // Run the ARM7 
            if (interpreter[1].shouldRun()) interpreter[1].runCycle();
            
            if (timers[1].shouldTick())     timers[1].tick(2);
            if (dma[1].shouldTransfer())    dma[1].transfer();

            if (gpu3D.shouldRun()) gpu3D.runCycle();

            //Stop the loop if both halted
            if (CPU_HACK && !interpreter[0].shouldRun() && !interpreter[1].shouldRun())
                break;
        }
        
        //PC_Core(0);
        gpu.scanline256();
        gpu.scanline355();
    }

    // Copy the completed sub-framebuffers to the main framebuffer
    if (!SkipFrame && gpu.readPowCnt1() & BIT(0)) // LCDs enabled
    {

        sceKernelDcacheWritebackInvalidateAll();
        if (gpu.readPowCnt1() & BIT(15)) // Display swap
        {
            psp_render->DrawFrame(gpu2D[0].getFramebuffer(0),gpu2D[1].getFramebuffer(0));
        }
        else
        {
            psp_render->DrawFrame(gpu2D[1].getFramebuffer(0),gpu2D[0].getFramebuffer(0));
        }

         MEfpsCount++;
    }

    fpsCount++;

    // Update the FPS and reset the counter every second
    auto tm_now = std::chrono::steady_clock::now();
    std::chrono::duration<double> fpsTime = tm_now - lastFpsTime;
    if (fpsTime.count() >= 1.0f)
    {
        fps = fpsCount;
        MEfps = MEfpsCount;
        fpsCount = 0;
        MEfpsCount = 0;
        lastFpsTime = tm_now;
    }

    
}

void Core::enterGbaMode()
{
    // Switch to GBA mode
    interpreter[1].enterGbaMode();
    runFunc = &Core::runGbaFrame;
    gbaMode = true;

    // Set VRAM blocks A and B to plain access mode
    // This is used by the GPU to access the VRAM borders
    memory.write<uint8_t>(0, 0x4000240, 0x80); // VRAMCNT_A
    memory.write<uint8_t>(0, 0x4000241, 0x80); // VRAMCNT_B
}
