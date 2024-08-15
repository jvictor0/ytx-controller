/*
 * Copyright (c) 2023, YAELTEX Technologies
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 * 
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 * 
 *  3. Neither the name of YAELTEX Technologies nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without 
 *     specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _SPIGPIOExpander_H
#define _SPIGPIOExpander_H

#if (ARDUINO >= 100) 
# include <Arduino.h>
#else
# include <WProgram.h>
#endif

#include <SPI.h>

#include "SPIAddressable.h"

// registers
enum {
    IODIRA,     IODIRB,
    IPOLA,      IPOLB,
    GPINTENA,   GPINTENB,
    DEFVALA,    DEFVALB,
    INTCONA,    INTCONB,
    IOCONA,     IOCONB,
    GPPUA,      GPPUB,
    INTFA,      INTFB,
    INTCAPA,    INTCAPB,
    GPIOA,      GPIOB,
    OLATA,      OLATB,
    MCP23017_REGISTER_COUNT
};

#define MCP23017_INT_ERR 255

class SPIGPIOExpander : public SPIAddressableElement{
    private:
        uint8_t _reg[MCP23017_REGISTER_COUNT];   /*! Local mirrors of the 22 internal registers of the MCP23S17 chip */        

        void readRegister(uint8_t index); 
        void writeRegister(uint8_t index);
        void readAll();
        void writeAll();
        
    public:
        SPIGPIOExpander();
        void begin(SPIAdressableBUS *bus, uint8_t address, uint8_t nextAddress) override;
        void pinMode(uint8_t pin, uint8_t mode);
        void digitalWrite(uint8_t pin, uint8_t value);
        uint8_t digitalRead(uint8_t pin);
        uint16_t digitalRead();
        
        void writeWord(uint8_t reg, uint16_t word);
        uint8_t readPort(uint8_t port);
        uint16_t readPort();
        void updateRegisterBit(uint8_t pin, uint8_t pValue, uint8_t portAaddr, uint8_t portBaddr);
        void writePort(uint8_t port, uint8_t val);
        void writePort(uint16_t val);
        uint8_t bitForPin(uint8_t pin);
        uint8_t regForPin(uint8_t pin, uint8_t portAaddr, uint8_t portBaddr);
        void pullUp(uint8_t p, uint8_t d);
        void enableInterrupt(uint8_t pin, uint8_t type);
        void disableInterrupt(uint8_t pin);
        void setMirror(boolean m);
        uint16_t getInterruptPins();
        uint16_t getInterruptValue();
        void setInterruptLevel(uint8_t level);
        void setInterruptOD(boolean openDrain);

};

class SPIExpanderGroup : public SPIAdressableBUS{        
    public:
        SPIExpanderGroup();
        void SetAllAsOutput();
        void InitPinsGhostModules();
        void SetPullUps();
        void readAllRegs();
        void writeAllRegs(byte);
};
#endif
