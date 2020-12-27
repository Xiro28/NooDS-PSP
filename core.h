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

#ifndef CORE_H
#define CORE_H

#include <chrono>
#include <cstdint>
#include <string>

#include "cartridge.h"
#include "cp15.h"
#include "defines.h"
#include "div_sqrt.h"
#include "dma.h"
#include "gpu.h"
#include "gpu_2d.h"
#include "gpu_3d.h"
#include "gpu_3d_renderer.h"
#include "input.h"
#include "interpreter.h"
#include "ipc.h"
#include "memory.h"
#include "rtc.h"
#include "spi.h"
#include "spu.h"
#include "timers.h"
#include "wifi.h"

class Core
{
    public:
        Core(std::string ndsPath = "", std::string gbaPath = "");

        int MEfpsCount = 0;

        void runFrame() { (this->*runFunc)(); }

        bool isGbaMode() { return gbaMode; }
        int  getFps()    { return fps;     }
        int  getMEFps()    { return MEfps;     }

        bool SkipFrame = false;
        bool CheckTimer = false;
        bool CheckDMA = false;

        void enterGbaMode();

        Cartridge cartridge;
        Cp15 cp15;
        DivSqrt divSqrt;
        Dma dma[2];
        Gpu gpu;
        Gpu2D gpu2D[2];
        Gpu3D gpu3D;
        Gpu3DRenderer gpu3DRenderer;
        Input input;
        Interpreter interpreter[2];
        Ipc ipc;
        Memory memory;
        Rtc rtc;
        Spi spi;
        Spu spu;
        Timers timers[2];
        Wifi wifi;

    private:
        bool gbaMode = false;
        void (Core::*runFunc)() = &Core::runNdsFrame;

        int fps = 0, fpsCount = 0, MEfps = 0;
        std::chrono::steady_clock::time_point lastFpsTime;
        int spuTimer = 0;

        void runNdsFrame();
        void runGbaFrame();
};


void WriteLog(char* msg);

#endif // CORE_H
