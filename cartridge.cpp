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

#include "cartridge.h"
#include "core.h"
#include "settings.h"

FILE *ndsRomFile;

Cartridge::~Cartridge()
{
    // Write the save before exiting
    writeSave();

    // Free the ROM and save memory
    if (ndsRomFile)  fclose(ndsRomFile);
    if (ndsSave) delete[] ndsSave;
    if (gbaRom)  delete[] gbaRom;
    if (gbaSave) delete[] gbaSave;
}

void Cartridge::loadNdsRom(std::string path)
{
    // Attempt to load an NDS ROM
    ndsRomName = path;
    ndsRomFile = fopen(ndsRomName.c_str(), "rb");
    if (!ndsRomFile) return;

    fseek(ndsRomFile, 0, SEEK_END);
    ndsRomSize = ftell(ndsRomFile);
    fseek(ndsRomFile, 0, SEEK_SET);

    fread(RomHeader, sizeof(uint8_t), 0x1000, ndsRomFile);

    fseek(ndsRomFile, 0x4000, SEEK_SET);
    fread(SecureArea, sizeof(uint8_t), 0x800, ndsRomFile);
    fseek(ndsRomFile, 0, SEEK_SET);

    //WriteLog("Done");

    // Attempt to load the ROM's save file
    ndsSaveName = path.substr(0, path.rfind(".")) + ".sav";
    FILE *ndsSaveFile = fopen(ndsSaveName.c_str(), "rb");
    if (ndsSaveFile)
    {
        fseek(ndsSaveFile, 0, SEEK_END);
        ndsSaveSize = ftell(ndsSaveFile);
        fseek(ndsSaveFile, 0, SEEK_SET);
        ndsSave = new uint8_t[ndsSaveSize];
        fread(ndsSave, sizeof(uint8_t), ndsSaveSize, ndsSaveFile);
        fclose(ndsSaveFile);
    }
    else
    {
        // The save size is unknown, so just assume FLASH 512KB and hope the user will change it if it doesn't work
        ndsSaveSize = 0x80000;
        ndsSave = new uint8_t[ndsSaveSize];
        memset(ndsSave, 0xFF, ndsSaveSize * sizeof(uint8_t));
    }

}

void Cartridge::loadGbaRom(std::string path)
{
    // Attempt to load a GBA ROM
    gbaRomName = path;
    FILE *gbaRomFile = fopen(gbaRomName.c_str(), "rb");
    if (!gbaRomFile) return;
    fseek(gbaRomFile, 0, SEEK_END);
    gbaRomSize = ftell(gbaRomFile);
    fseek(gbaRomFile, 0, SEEK_SET);
    gbaRom = new uint8_t[gbaRomSize];
    fread(gbaRom, sizeof(uint8_t), gbaRomSize, gbaRomFile);
    fclose(gbaRomFile);

    // Attempt to load the ROM's save file
    gbaSaveName = path.substr(0, path.rfind(".")) + ".sav";
    FILE *gbaSaveFile = fopen(gbaSaveName.c_str(), "rb");
    if (gbaSaveFile)
    {
        fseek(gbaSaveFile, 0, SEEK_END);
        gbaSaveSize = ftell(gbaSaveFile);
        fseek(gbaSaveFile, 0, SEEK_SET);
        gbaSave = new uint8_t[gbaSaveSize];
        fread(gbaSave, sizeof(uint8_t), gbaSaveSize, gbaSaveFile);
        fclose(gbaSaveFile);
    }
    else
    {
        const std::string saveStrs[] = { "EEPROM_V", "SRAM_V", "FLASH_V", "FLASH512_V", "FLASH1M_V" };
        int match = -1;

        // Unlike the DS, a GBA cart's save type can be reliably detected by searching for strings in the ROM
        // Search the ROM for a save string so a new save of that type can be created
        for (unsigned int i = 0; i < gbaRomSize; i += 4)
        {
            for (unsigned int j = 0; j < 5; j++)
            {
                match = j;
                for (unsigned int k = 0; k < saveStrs[j].length(); k++)
                {
                    if (i + k >= gbaRomSize || gbaRom[i + k] != saveStrs[j][k])
                    {
                        match = -1;
                        break;
                    }
                }
                if (match != -1) break;
            }
            if (match != -1) break;
        }

        // Create a new GBA save of the detected type
        if (match != -1)
        {
            switch (match)
            {
                case 0: // EEPROM
                {
                    // EEPROM can be either 0.5KB or 8KB, so it must be guessed based on how the game uses it
                    gbaSaveSize = -1;
                    return;
                }

                case 1: // SRAM 32KB
                {
                    gbaSaveSize = 0x8000;
                    printf("Detected SRAM 32KB save type\n");
                    break;
                }

                case 2: case 3: // FLASH 64KB
                {
                    gbaSaveSize = 0x10000;
                    printf("Detected FLASH 64KB save type\n");
                    break;
                }

                case 4: // FLASH 128KB
                {
                    gbaSaveSize = 0x20000;
                    printf("Detected FLASH 128KB save type\n");
                    break;
                }
            }

            gbaSave = new uint8_t[gbaSaveSize];
            memset(gbaSave, 0xFF, gbaSaveSize * sizeof(uint8_t));
        }
    }
}

//uint8_t _arm[1<<20];

void Cartridge::directBoot()
{
    // Extract some information about the initial ARM9 code from the header
    uint32_t offset9    = U8TO32(RomHeader, 0x20);
    uint32_t entryAddr9 = U8TO32(RomHeader, 0x24);
    uint32_t ramAddr9   = U8TO32(RomHeader, 0x28);
    uint32_t size9      = U8TO32(RomHeader, 0x2C);
    printf("ARM9 code ROM offset:    0x%X\n", offset9);
    printf("ARM9 code entry address: 0x%X\n", entryAddr9);
    printf("ARM9 RAM address:        0x%X\n", ramAddr9);
    printf("ARM9 code size:          0x%X\n", size9);

    // Extract some information about the initial ARM7 code from the header
    uint32_t offset7    = U8TO32(RomHeader, 0x30);
    uint32_t entryAddr7 = U8TO32(RomHeader, 0x34);
    uint32_t ramAddr7   = U8TO32(RomHeader, 0x38);
    uint32_t size7      = U8TO32(RomHeader, 0x3C);
    printf("ARM7 code ROM offset:    0x%X\n", offset7);
    printf("ARM7 code entry address: 0x%X\n", entryAddr7);
    printf("ARM7 RAM address:        0x%X\n", ramAddr7);
    printf("ARM7 code size:          0x%X\n", size7);

    /*char buff[256];
    sprintf(buff,"0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X",offset9, entryAddr9, ramAddr9, size9, offset7, entryAddr7, ramAddr7,size7);
    WriteLog(buff);*/

    uint8_t data;

    // Load the ROM header into memory
    for (uint32_t i = 0; i < 0x170; i++)
        core->memory.write<uint8_t>(0, 0x27FFE00 + i, RomHeader[i]);
    
    fseek(ndsRomFile, offset9, SEEK_SET);

    // Load the initial ARM9 code into memory
    for (uint32_t i = 0; i < size9; i++){
        fread(&data, sizeof(uint8_t), 1, ndsRomFile);
        core->memory.write<uint8_t>(0, ramAddr9 + i, data);
    }

    fseek(ndsRomFile, 0, SEEK_SET);
    fseek(ndsRomFile, offset7, SEEK_SET);

    // Load the initial ARM7 code into memory
    for (uint32_t i = 0; i < size7; i++){
        fread(&data, sizeof(uint8_t), 1, ndsRomFile);
        core->memory.write<uint8_t>(1, ramAddr7 + i, data);
    }

    fseek(ndsRomFile, 0, SEEK_SET);

}

void Cartridge::writeSave()
{
    // Update the NDS save file if necessary
    if (ndsSaveDirty)
    {
        FILE *ndsSaveFile = fopen(ndsSaveName.c_str(), "wb");
        if (ndsSaveFile)
        {
            if (ndsSaveSize > 0)
                fwrite(ndsSave, sizeof(uint8_t), ndsSaveSize, ndsSaveFile);
            fclose(ndsSaveFile);
            ndsSaveDirty = false;
        }
    }

    // Update the GBA save file if necessary
    if (gbaSaveDirty)
    {
        FILE *gbaSaveFile = fopen(gbaSaveName.c_str(), "wb");
        if (gbaSaveFile)
        {
            if (gbaSaveSize > 0)
                fwrite(gbaSave, sizeof(uint8_t), gbaSaveSize, gbaSaveFile);
            fclose(gbaSaveFile);
            gbaSaveDirty = false;
        }
    }
}

void Cartridge::trimRom(uint8_t **rom, int *romSize, std::string *romName)
{
    // Starting from the end, reduce the ROM size until a non-filler word is found
    int newSize;
    for (newSize = *romSize & ~3; newSize > 0; newSize -= 4)
    {
        if (U8TO32(*rom, newSize - 4) != 0xFFFFFFFF)
            break;
    }

    if (newSize < *romSize)
    {
        // Update the ROM in memory
        *romSize = newSize;
        uint8_t *newRom = new uint8_t[newSize];
        memcpy(newRom, *rom, newSize * sizeof(uint8_t));
        delete[] *rom;
        *rom = newRom;

        // Update the ROM file
        FILE *romFile = fopen(romName->c_str(), "wb");
        if (romFile)
        {
            if (newSize > 0)
                fwrite(*rom, sizeof(uint8_t), newSize, romFile);
            fclose(romFile);
        }
    }
}

void Cartridge::resizeSave(int newSize, uint8_t **save, int *saveSize, bool *saveDirty)
{
}

uint8_t Cartridge::gbaEepromRead()
{
    return 0;
}

void Cartridge::gbaEepromWrite(uint8_t value)
{
}

uint8_t Cartridge::gbaSramRead(uint32_t address)
{
    return 0xFF;
}

void Cartridge::gbaSramWrite(uint32_t address, uint8_t value)
{
}

void Cartridge::encrypt(uint32_t* data)
{
    // Encrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = data[0];
    uint32_t x = data[1];

    for (int i = 0x00; i <= 0x0F; i++)
    {
        uint32_t z = encTable[i] ^ x;
        x = encTable[0x012 + ((z >> 24) & 0xFF)];
        x = encTable[0x112 + ((z >> 16) & 0xFF)] + x;
        x = encTable[0x212 + ((z >>  8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >>  0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    data[0] = x ^ encTable[0x10];
    data[1] = y ^ encTable[0x11];
}

void Cartridge::decrypt(uint32_t* data)
{
    // Decrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = data[0];
    uint32_t x = data[1];

    for (int i = 0x11; i >= 0x02; i--)
    {
        uint32_t z = encTable[i] ^ x;
        x = encTable[0x012 + ((z >> 24) & 0xFF)];
        x = encTable[0x112 + ((z >> 16) & 0xFF)] + x;
        x = encTable[0x212 + ((z >>  8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >>  0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    data[0] = x ^ encTable[0x1];
    data[1] = y ^ encTable[0x0];
}

void Cartridge::initKeycode(int level)
{
    // Initialize the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++

    for (int i = 0; i < 0x412; i++)
        encTable[i] = core->memory.read<uint32_t>(1, 0x30 + i * 4);

    uint32_t code = U8TO32(RomHeader, 0x0C);

    encCode[0] = code;
    encCode[1] = code / 2;
    encCode[2] = code * 2;

    if (level >= 1) applyKeycode();
    if (level >= 2) applyKeycode();

    encCode[1] *= 2;
    encCode[2] /= 2;

    if (level >= 3) applyKeycode();
}

void Cartridge::applyKeycode()
{
    // Apply a keycode to the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++
    
    encrypt(&encCode[1]);
    encrypt(&encCode[0]);

    for (int i = 0; i <= 0x11; i++)
    {
        uint32_t byteReverse = 0;
        for (int j = 0; j < 4; j++)
            byteReverse |= ((encCode[i % 2] >> (j * 8)) & 0xFF) << ((3 - j) * 8);

        encTable[i] ^= byteReverse;
    }

    uint64_t scratch = 0;
    uint32_t temp[2] = {0,0};

    for (int i = 0; i <= 0x410; i += 2)
    {
        encrypt(temp);
        encTable[i + 0] = temp[1];
        encTable[i + 1] = temp[0];
    }
}

void Cartridge::writeAuxSpiCnt(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the AUXSPICNT registers
    mask &= 0xE043;
    auxSpiCnt[cpu] = (auxSpiCnt[cpu] & ~mask) | (value & mask);
}

void Cartridge::writeRomCmdOutL(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (low)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask)) | (value & mask);
}

void Cartridge::writeRomCmdOutH(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (high)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
}

void Cartridge::writeAuxSpiData(bool cpu, uint8_t value)
{
    // Do nothing if there is no save
    if (ndsSaveSize == 0) return;

    if (auxWriteCount[cpu] == 0)
    {
        // On the first write, set the command byte
        if (value == 0) return;
        auxCommand[cpu] = value;
        auxAddress[cpu] = 0;
        auxSpiData[cpu] = 0;
    }
    else
    {
        switch (ndsSaveSize)
        {
            case 0x200: // EEPROM 0.5KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read from lower memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress[cpu] = value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < 0x200) ? ndsSave[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x0B: // Read from upper memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress[cpu] = 0x100 + value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < 0x200) ? ndsSave[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x02: // Write to lower memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress[cpu] = value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress[cpu] < 0x200)
                            {
                                ndsSave[auxAddress[cpu]] = value;
                                ndsSaveDirty = true;
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    case 0x0A: // Write to upper memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress[cpu] = 0x100 + value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress[cpu] < 0x200)
                            {
                                ndsSave[auxAddress[cpu]] = value;
                                ndsSaveDirty = true;
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM 0.5KB command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            case 0x2000: case 0x8000: case 0x10000: case 0x20000: // EEPROM 8KB, 64KB, 128KB; FRAM 32KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read from memory
                    {
                        if (auxWriteCount[cpu] < ((ndsSaveSize == 0x20000) ? 4 : 3))
                        {
                            // On writes 2-3, set the 2 byte address to read from (not EEPROM 128KB)
                            // EEPROM 128KB uses a 3 byte address, so it's set on writes 2-4
                            auxAddress[cpu] |= value << ((((ndsSaveSize == 0x20000) ? 3 : 2) - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 4+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < ndsSaveSize) ? ndsSave[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x02: // Write to memory
                    {
                        if (auxWriteCount[cpu] < ((ndsSaveSize == 0x20000) ? 4 : 3))
                        {
                            // On writes 2-3, set the 2 byte address to write to (not EEPROM 128KB)
                            // EEPROM 128KB uses a 3 byte address, so it's set on writes 2-4
                            auxAddress[cpu] |= value << ((((ndsSaveSize == 0x20000) ? 3 : 2) - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 4+, write data to the save
                            if (auxAddress[cpu] < ndsSaveSize)
                            {
                                ndsSave[auxAddress[cpu]] = value;
                                ndsSaveDirty = true;
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM/FRAM command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            case 0x40000: case 0x80000: case 0x100000: case 0x800000: // FLASH 256KB, 512KB, 1024KB, 8192KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read data bytes
                    {
                        if (auxWriteCount[cpu] < 4)
                        {
                            // On writes 2-4, set the 3 byte address to read from
                            auxAddress[cpu] |= value << ((3 - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 5+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < ndsSaveSize) ? ndsSave[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x0A: // Page write
                    {
                        if (auxWriteCount[cpu] < 4)
                        {
                            // On writes 2-4, set the 3 byte address to write to
                            auxAddress[cpu] |= value << ((3 - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 5+, write data to the save
                            if (auxAddress[cpu] < ndsSaveSize)
                            {
                                ndsSave[auxAddress[cpu]] = value;
                                ndsSaveDirty = true;
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    case 0x08: // IR-related
                    {
                        // If a gamecode starts with 'I', the game has an infrared port in its cartridge
                        // This shares the same SPI as FLASH memory
                        // Some games check this command as an anti-piracy measure
                        auxSpiData[cpu] = (RomHeader[0xC] == 'I') ? 0xAA : 0;
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown FLASH command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            default:
            {
                printf("Write to AUX SPI with unknown save size: 0x%X\n", ndsSaveSize);
                break;
            }
        }
    }

    // Keep track of the write count
    if (auxSpiCnt[cpu] & BIT(6)) // Keep chip selected
        auxWriteCount[cpu]++;
    else // Deselect chip
        auxWriteCount[cpu] = 0;
}

void Cartridge::writeRomCtrl(bool cpu, uint32_t mask, uint32_t value)
{
    bool transfer = false;

    // Set the release reset bit, but never clear it
    romCtrl[cpu] |= (value & BIT(29));

    // Start a transfer if the start bit changes from 0 to 1
    if (!(romCtrl[cpu] & BIT(31)) && (value & BIT(31)))
        transfer = true;

    // Write to one of the ROMCTRL registers
    mask &= 0xDF7F7FFF;
    romCtrl[cpu] = (romCtrl[cpu] & ~mask) | (value & mask);

    if (!transfer) return;

    // Determine the size of the block to transfer
    uint8_t size = (romCtrl[cpu] & 0x07000000) >> 24;
    switch (size)
    {
        case 0:  blockSize[cpu] = 0;             break;
        case 7:  blockSize[cpu] = 4;             break;
        default: blockSize[cpu] = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command
    command[cpu] = 0;
    for (int i = 0; i < 8; i++)
        command[cpu] |= ((romCmdOut[cpu] >> (i * 8)) & 0xFF) << ((7 - i) * 8);

    // Decrypt the ROM command if encryption is enabled
    if (encrypted[cpu])
    {
        initKeycode(2);
        uint32_t data[2];
        data[0] = command[cpu];
        data[1] = command[cpu]>>32;
        decrypt(data);
        command[cpu] = (data[1]<<32) | data[0];
    }

    // Handle encryption commands
    if (ndsRomFile)
    {
        if ((command[cpu] >> 56) == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            encrypted[cpu] = true;
        }
        else if (((command[cpu] >> 56) & 0xF0) == 0xA0) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            encrypted[cpu] = false;
        }
    }

    if (blockSize[cpu] == 0)
    {
        // End the transfer right away if the block size is 0
        romCtrl[cpu] &= ~BIT(23); // Word not ready
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Disable DS cartridge DMA transfers
        core->dma[cpu].disable((cpu == 0) ? 5 : 2);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Indicate that a word is ready
        romCtrl[cpu] |= BIT(23);

        // Trigger DS cartridge DMA transfers
        core->dma[cpu].trigger((cpu == 0) ? 5 : 2);

        readCount[cpu] = 0;
    }
}

uint32_t Cartridge::readRomDataIn(bool cpu)
{
    // Don't transfer if the word ready bit isn't set
    if (!(romCtrl[cpu] & BIT(23)))
        return 0;

    // Endless 0xFFs are returned on a dummy command or when no cart is inserted
    uint32_t value = 0xFFFFFFFF;

    if (ndsRomFile)
    {
        // Interpret the current ROM command
        if (command[cpu] == 0x0000000000000000) // Get header
        {
            // Return the ROM header, repeated every 0x1000 bytes
            value = U8TO32(RomHeader, readCount[cpu] % 0x1000);
        }
        else if (command[cpu] == 0x9000000000000000 || ((command[cpu] >> 56) & 0xF0) == 0x10 ||
                 command[cpu] == 0xB800000000000000) // Get chip ID
        {
            // Return the chip ID, repeated every 4 bytes
            // ROM dumps don't provide a chip ID, so use a fake one
            value = 0x00001FC2;
        }
        else if (((command[cpu] >> 56) & 0xF0) == 0x20) // Get secure area
        {
            uint32_t address = ((command[cpu] & 0x0FFFF00000000000) >> 44) * 0x1000;

            // Return data from the selected secure area block
            if (address == 0x4000 && readCount[cpu] < 0x800)
            {
                // Encrypt the first 2KB of the first block
                // The first 8 bytes of this block should contain the double-encrypted string 'encryObj'
                // This string isn't included in ROM dumps, so manually supply it
                uint32_t data[2];
                if (readCount[cpu] < 8)
                {
                   data[0] = 0x6A624F79;
                   data[1] = 0x72636E65; // encryObj
                }
                else
                {
                    data[1] = U8TO32(SecureArea, ((address + readCount[cpu]) & ~7) + 4) << 32;
                    data[0] = U8TO32(SecureArea, ((address + readCount[cpu]) & ~7));
                }

                // Encrypt the data
                initKeycode(3);
                encrypt(data);

                // Double-encrypt the 'encryObj' string
                if (readCount[cpu] < 8)
                {
                    initKeycode(2);
                    encrypt(data);
                }

                value = (((address + readCount[cpu]) & 4) ? data[0] : data[1]);
            }
            else
            {
                value = U8TO32(RomHeader, address + readCount[cpu]);
            }
        }
        else if ((command[cpu] & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
        {
            // Return ROM data from the given address
            // This command can't read the first 32KB of a ROM, so it redirects the address
            // Some games verify that the first 32KB are unreadable as an anti-piracy measure
            uint32_t address = (command[cpu] & 0x00FFFFFFFF000000) >> 24;
            if (address < 0x8000) address = 0x8000 + (address & 0x1FF);
            int addr_tot = address + readCount[cpu];
            const uint8_t dataBlockSZ = 4;

            uint32_t ROMdata;
            /*static int last_addrRead = addr_tot + (dataBlockSZ);
            static int read_status_index = -1;*/

            //if (addr_tot >= last_addrRead || read_status_index < 0)
			{
                
               // last_addrRead = addr_tot + (dataBlockSZ);

                fseek(ndsRomFile,addr_tot,SEEK_SET);
                fread(&ROMdata,sizeof(uint32_t),1,ndsRomFile);

              //  read_status_index = 0;
            }
            
			if (address + readCount[cpu] < ndsRomSize) value = ROMdata;
            //if (address + readCount[cpu] < ndsRomSize) value = U8TO32(ndsRom, address + readCount[cpu]);
        }
        else if (command[cpu] != 0x9F00000000000000) // Unknown (not dummy)
        {
            printf("ROM transfer with unknown command: 0x%llX\n", command[cpu]);
            value = 0;
        }
    }

    readCount[cpu] += 4;

    if (readCount[cpu] == blockSize[cpu])
    {
        // End the transfer when the block size has been reached
        romCtrl[cpu] &= ~BIT(23); // Word not ready
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Disable DS cartridge DMA transfers
        core->dma[cpu].disable((cpu == 0) ? 5 : 2);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Trigger DS cartridge DMA transfers until the block size is reached
        core->dma[cpu].trigger((cpu == 0) ? 5 : 2);
    }

    return value;
}