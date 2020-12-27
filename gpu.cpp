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

#include "gpu.h"
#include "core.h"
#include "settings.h"


Gpu::Gpu(Core *core): core(core)
{
    // Mark the thread as not drawing to start
    drawing = false;
}

Gpu::~Gpu()
{
}

uint32_t Gpu::rgb6ToRgb8(uint32_t color)
{
    // Convert an RGB6 value to an RGB8 value
    uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
    uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
    uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

uint16_t Gpu::rgb6ToRgb5(uint32_t color)
{
    // Convert an RGB6 value to an RGB5 value
    uint8_t r = ((color >>  0) & 0x3F) / 2;
    uint8_t g = ((color >>  6) & 0x3F) / 2;
    uint8_t b = ((color >> 12) & 0x3F) / 2;
    return BIT(15) | (b << 10) | (g << 5) | r;
}

uint32_t *Gpu::getFrame(bool gbaCrop)
{
    uint32_t *out = nullptr;

    // If a new frame is ready, get it, convert it to RGB8 format, and crop it if needed
    // If a new frame isn't ready yet, nothing will be returned
    // In that case, frontends should reuse the previous frame (or skip redrawing) to avoid repeated conversion
  
    /*if (gbaCrop)
    {
        int offset = (powCnt1 & BIT(15)) ? 0 : (256 * 192); // Display swap
        out = new uint32_t[240 * 160];
        for (int y = 0; y < 160; y++)
            for (int x = 0; x < 240; x++)
                out[y * 240 + x] = rgb6ToRgb8(framebuffer[offset + (y + 16) * 256 + (x + 8)]);
    }else
    {
        out = framebuffer;
    }
    */

    return out;
}

void Gpu::gbaScanline240()
{
}

void Gpu::gbaScanline308()
{
}

void Gpu::scanline256()
{

    // Draw 3D scanlines 48 lines in advance, if the current 3D is dirty
    // If the 3D parameters haven't changed since the last frame, there's no need to draw it again
    // Bit 0 of the dirty variable represents invalidation, and bit 1 represents a frame currently drawing
    if (dirty3D && (core->gpu2D[0].readDispCnt() & BIT(3)) && ((vCount + 48) % 263) < 192)
    {
        if (vCount == 215) dirty3D = BIT(1);
        core->gpu3DRenderer.drawScanline((vCount + 48) % 263);
        if (vCount == 143) dirty3D &= ~BIT(1);
    }

    for (int i = 0; i < 2; i++)
    {
        // Set the H-blank flag
        dispStat[i] |= BIT(1);

        // Trigger an H-blank IRQ if enabled
        if (dispStat[i] & BIT(4))
            core->interpreter[i].sendInterrupt(1);
    }
}

void Gpu::scanline355()
{
    for (int i = 0; i < 2; i++)
    {
        // Check if the current scanline matches the V-counter
        if (vCount == ((dispStat[i] >> 8) | ((dispStat[i] & BIT(7)) << 1)))
        {
            // Set the V-counter flag
            dispStat[i] |= BIT(2);

            // Trigger a V-counter IRQ if enabled
            if (dispStat[i] & BIT(5))
                core->interpreter[i].sendInterrupt(2);
        }
        else if (dispStat[i] & BIT(2))
        {
            // Clear the V-counter flag on the next line
            dispStat[i] &= ~BIT(2);
        }

        // Clear the H-blank flag
        dispStat[i] &= ~BIT(1);
    }

    // Move to the next scanline
    switch (++vCount)
    {
        case 192: // End of visible scanlines
        {
            for (int i = 0; i < 2; i++)
            {
                // Set the V-blank flag
                dispStat[i] |= BIT(0);

                // Trigger a V-blank IRQ if enabled
                if (dispStat[i] & BIT(3))
                    core->interpreter[i].sendInterrupt(0);

                // Trigger V-blank DMA transfers
                core->dma[i].trigger(1);
            }

            // Swap the buffers of the 3D engine if needed
            if (core->gpu3D.shouldSwap())
                core->gpu3D.swapBuffers();

            ready = true;
            break;
        }

        case 262: // Last scanline
        {
            // Clear the V-blank flag
            for (int i = 0; i < 2; i++)
                dispStat[i] &= ~BIT(0);
            break;
        }

        case 263: // End of frame
        {
            // Start the next frame
            vCount = 0;
            break;
        }
    }

    // Signal that the next scanline has started
  /*  if (vCount < 192)
        drawing = true;*/
}

void Gpu::drawGbaThreaded()
{
}

void Gpu::drawThreaded()
{
    while (running)
    {
        // Wait until the next scanline starts
        while (!drawing)
        {
            if (!running) return;
        }

        // Draw the scanlines
        core->gpu2D[0].drawScanline(vCount);
        core->gpu2D[1].drawScanline(vCount);

        // Signal that the scanline is finished
        drawing = false;
    }
}

void Gpu::writeDispStat(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the DISPSTAT registers
    mask &= 0xFFB8;
    dispStat[cpu] = (dispStat[cpu] & ~mask) | (value & mask);
}

void Gpu::writeDispCapCnt(uint32_t mask, uint32_t value)
{
    // Write to the DISPCAPCNT register
    mask &= 0xEF3F1F1F;
    dispCapCnt = (dispCapCnt & ~mask) | (value & mask);
}

void Gpu::writePowCnt1(uint16_t mask, uint16_t value)
{
    // Write to the POWCNT1 register
    mask &= 0x820F;
    powCnt1 = (powCnt1 & ~mask) | (value & mask);
}
