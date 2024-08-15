/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2020 - Yaeltex

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifndef MEMORY_HOST_H
#define MEMORY_HOST_H

#include <extEEPROM.h>

class memoryHost
{
  public:
    memoryHost(extEEPROM *,uint8_t blocks);
    
    void ConfigureBlock(uint8_t,uint16_t,uint16_t,bool,bool allocate=true);
    void DisarmBlocks(void);

    void LayoutBanks(bool AllocateRAM=true);
    uint8_t LoadBank(uint8_t);
    void SaveBank(uint8_t);
    void SaveBlockToEEPROM(uint8_t);

    int8_t GetCurrentBank();

    uint8_t LoadBankSingleSection(uint8_t, uint8_t, uint16_t, bool);
    
    void ReadFromEEPROM(uint8_t,uint8_t,uint16_t,void *, bool);
    void PrintEEPROM(uint8_t,uint8_t,uint16_t);
    void WriteToEEPROM(uint8_t,uint8_t,uint16_t,void *);
    
    void* Block(uint8_t);
    void* GetSectionAddress(uint8_t,uint16_t);
    uint16_t SectionSize(uint8_t);
    uint16_t SectionCount(uint8_t);
    
    uint handleSaveControllerState(uint);
    uint SaveControllerState(uint);
    void LoadControllerState(void);
    bool IsCtrlStateMemNew(void);
    void SetFwConfigChangeFlag(void);
    bool FwConfigVersionChanged(void);
    void OnFwConfigVersionChange(void);
    void SetNewMemFlag(void);

    void* AllocateRAM(uint16_t);
    void FreeRAM(void*);

    
  private:
    uint8_t blocksCount;
    blockDescriptor *descriptors;       

    extEEPROM *externalMemory;
    uint16_t eepIndex;      // Points to the start of the bank area
    
    void *bankChunk;
    uint16_t bankSize;
    int8_t bankNow;  
};


/*
    ----------EXT MEMORY MAP----------

    ----------------------------------

    ----------END OF EEPROM-----------

    ---------------------------------- 
    | MIDI BUFFER                     | 
    ----------------------------------
    | ELEMENTS                        | 
    ----------------------------------
    | GENERAL SETTINGS                | 
    ----------------------------------

*/

#endif //MEMORY_HOST_H