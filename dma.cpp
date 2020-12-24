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

#include "dma.h"
#include "core.h"

void Dma::transfer()
{
    for (int i = 0; i < 4; i++)
    {
        // Only transfer on active channels
        if (!(active & BIT(i)))
            continue;

        // Deactivate the channel now that it's being processed
        active &= ~BIT(i);

        int dstAddrCnt = (dmaCnt[i] & 0x00600000) >> 21;
        int srcAddrCnt = (dmaCnt[i] & 0x01800000) >> 23;
        int mode       = (dmaCnt[i] & 0x38000000) >> 27;
        int gxFifoCount = 0;

        if (dmaCnt[i] & BIT(26)) // Whole word transfer
        {
            for (unsigned int j = 0; j < wordCounts[i]; j++)
            {
                // Transfer a word
                core->memory.write<uint32_t>(cpu, dstAddrs[i], core->memory.read<uint32_t>(cpu, srcAddrs[i]));

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddrs[i] += 4;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddrs[i] -= 4;

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddrs[i] += 4;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddrs[i] -= 4;

                // In GXFIFO mode, only send 112 words at a time
                if (mode == 7 && ++gxFifoCount == 112)
                    break;
            }
        }
        else // Half-word transfer
        {
            for (unsigned int j = 0; j < wordCounts[i]; j++)
            {
                // Transfer a half-word
                core->memory.write<uint16_t>(cpu, dstAddrs[i], core->memory.read<uint16_t>(cpu, srcAddrs[i]));

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddrs[i] += 2;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddrs[i] -= 2;

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddrs[i] += 2;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddrs[i] -= 2;

                // In GXFIFO mode, only send 112 words at a time
                if (mode == 7 && ++gxFifoCount == 112)
                    break;
            }
        }

        if (mode == 7)
        {
            // In GXFIFO mode, keep the channel active if the FIFO is still half empty
            if (core->gpu3D.readGxStat() & BIT(25))
                active |= BIT(i);

            // Don't end a GXFIFO transfer if there are still words left
            wordCounts[i] -= gxFifoCount;
            if (wordCounts[i] > 0)
                continue;
        }

        if ((dmaCnt[i] & BIT(25)) && mode != 0) // Repeat
        {
            // Reload the internal registers on repeat
            wordCounts[i] = dmaCnt[i] & 0x001FFFFF;
            if (dstAddrCnt == 3) // Increment and reload
                dstAddrs[i] = dmaDad[i];
        }
        else
        {
            // End the transfer
            dmaCnt[i] &= ~BIT(31);
            active &= ~BIT(i);
        }

        // Trigger an end of transfer IRQ if enabled
        if (dmaCnt[i] & BIT(30))
            core->interpreter[cpu].sendInterrupt(8 + i);
    }
}

void Dma::trigger(int mode, uint8_t channels)
{
    // ARM7 DMAs don't use the lowest mode bit, so adjust accordingly
    if (cpu == 1) mode <<= 1;

    // Activate channels that are set to the triggered mode
    for (int i = 0; i < 4; i++)
    {
        if ((channels & BIT(i)) && (dmaCnt[i] & BIT(31)) && ((dmaCnt[i] & 0x38000000) >> 27) == mode)
            active |= BIT(i);
    }
}

void Dma::disable(int mode, uint8_t channels)
{
    // ARM7 DMAs don't use the lowest mode bit, so adjust accordingly
    if (cpu == 1) mode <<= 1;

    // Deactivate channels that are set to the disabled mode
    // This usually isn't necessary, because most modes transfer once on a trigger event and then deactivate
    // However, there are edge cases, such as having two GXFIFO DMAs and the first one fills the FIFO more than half
    for (int i = 0; i < 4; i++)
    {
        if ((channels & BIT(i)) && (dmaCnt[i] & BIT(31)) && ((dmaCnt[i] & 0x38000000) >> 27) == mode)
            active &= ~BIT(i);
    }
}

void Dma::writeDmaSad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMASAD registers
    mask &= ((cpu == 0 || channel != 0) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaSad[channel] = (dmaSad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaDad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMADAD registers
    mask &= ((cpu == 0 || channel == 3) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaDad[channel] = (dmaDad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaCnt(int channel, uint32_t mask, uint32_t value)
{
    uint32_t old = dmaCnt[channel];

    // Write to one of the DMACNT registers
    mask &= ((cpu == 0) ? 0xFFFFFFFF : (channel == 3 ? 0xF7E0FFFF : 0xF7E03FFF));
    dmaCnt[channel] = (dmaCnt[channel] & ~mask) | (value & mask);

    // Deactivate the channel if it's disabled or the mode changed
    if (!(dmaCnt[channel] & BIT(31)) || (dmaCnt[channel] & 0x38000000) != (old & 0x38000000))
        active &= ~BIT(channel);

    // In GXFIFO mode, activate the channel immediately if the FIFO is already half empty
    // All other modes are only triggered at the moment when the event happens
    // For example, if a word from the DS cart is ready before starting a DMA, the DMA will not be triggered
    if ((dmaCnt[channel] & BIT(31)) && ((dmaCnt[channel] & 0x38000000) >> 27) == 7 && (core->gpu3D.readGxStat() & BIT(25)))
        active |= BIT(channel);

    // Don't reload the internal registers unless the enable bit changed from 0 to 1
    if ((old & BIT(31)) || !(dmaCnt[channel] & BIT(31)))
        return;

    // Reload the internal registers
    dstAddrs[channel] = dmaDad[channel];
    srcAddrs[channel] = dmaSad[channel];
    wordCounts[channel] = dmaCnt[channel] & 0x001FFFFF;

    // Activate the channel if it's set to immediate mode
    // Reloading seems to be the only trigger for this, so an enabled channel changed to immediate will never transfer
    // This also means that repeating doesn't work; in this case, the enabled bit is cleared after only one transfer
    if (((dmaCnt[channel] & 0x38000000) >> 27) == 0)
        active |= BIT(channel);
}
