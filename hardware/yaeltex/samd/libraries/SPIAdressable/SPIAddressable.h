/*
 * Copyright (c) 2023, YAELTEX
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


#ifndef _SPIAddressable_H
#define _SPIAddressable_H

#if (ARDUINO >= 100) 
# include <Arduino.h>
#else
# include <WProgram.h>
#endif

#include <SPI.h>


// LEGACY MCP23S17 registers
#define REGISTER_COUNT  16
#define REGISTER_OFFSET REGISTER_COUNT

// LEGACY MCP23S17 opcodes and commands
#define OPCODER       (0b00000001)  // Opcode for MCP23S17 with LSB (bit0) set to read (1)
#define OPCODEW       (0b00000000)  // Opcode for MCP23S17 with LSB (bit0) set to write (0)
#define ADDR_ENABLE   (0b00000001)  // Configuration register for MCP23S17, the only thing we change is enabling hardware addressing
#define ADDR_DISABLE  (0b00000000)  // Configuration register for MCP23S17, the only thing we change is disabling hardware addressing
#define SEQOP_ENABLE  (0b00100000)  // Configuration register for MCP23S17, the only thing we change is enabling sequential operation

#define OPCODE_MASK               0b00000001
#define ADDRESS_MASK              0b11111110
#define SPI_SLAVE_DEFAULT_ADDRESS 0b00000111

// AVAILABLE BASE ADDRESSES
enum {
    FIXED_ELEMENTS_BASE_ADDRESS = 0,
    _BASE_ADDRESS_1,    //unused
    MCP23017_BASE_ADDRESS,
    INF_POT_BASE_ADDRESS_A,
    INF_POT_BASE_ADDRESS_B,
    _BASE_ADDRESS_5,    //unused
    _BASE_ADDRESS_6,    //unused
    _BASE_ADDRESS_7,    //unused
    _BASE_ADDRESS_8,    //unused
    _BASE_ADDRESS_9,    //unused
    _BASE_ADDRESS_10,   //unused
    _BASE_ADDRESS_11,   //unused
    _BASE_ADDRESS_12,   //unused
    _BASE_ADDRESS_13,   //unused
    _BASE_ADDRESS_14,   //unused
    _BASE_ADDRESS_15,   //unused
};

// FIXED_ELEMENTS ADDRESSES
enum {
    MAIN_FEEDBACK_ADDRESS = 0, //reserved
    ANALOG_EXPANDER_ADDRESS,
    _SELF_ADDRESS_2,    //unused
    _SELF_ADDRESS_3,    //unused
    _SELF_ADDRESS_4,    //unused
    _SELF_ADDRESS_5,    //unused
    _SELF_ADDRESS_6,    //unused
    _SELF_ADDRESS_7     //unused
};

// AVAILABLE CONTROL REGISTERS
enum {
    _0_REG,           //unused
    _1_REG,           //unused
    _2_REG,           //unused
    _3_REG,           //unused
    _4_REG,           //unused
    _5_REG,           //unused
    _6_REG,           //unused
    _7_REG,           //unused
    _8_REG,           //unused
    _9_REG,           //unused
    _10_REG,          //unused
    _11_REG,          //unused
    _12_REG,          //unused
    _13_REG,          //unused
    _14_REG,          //unused
    _ADDRESSING_REG   //reserved
};

class SPIAdressableBUS {     
    public:
        SPIAdressableBUS();
        void begin(SPIClass *, SPISettings, uint8_t);
        void DisableHWAddress(uint8_t);

        SPIClass *port; /*! This points to a valid SPI object created from the Arduino SPI library. */
        SPISettings settings;
        uint8_t cs;    /*! Chip select pin */
};

class SPIAddressableElement {     
    public:
        SPIAddressableElement();
        virtual void begin(SPIAdressableBUS *, uint8_t);
		virtual void begin(SPIAdressableBUS *, uint8_t, uint8_t);
    protected:
        SPIAdressableBUS *spiBUS; /*! This points to a valid SPI object created from the Arduino SPI library. */
        uint8_t base;  /*! base address */
        uint8_t addr;  /*! 3-bit chip address */   

        void enableAddressing(uint8_t nextAddress);
        void readChunk(uint8_t index, void *data, uint8_t size);
        void writeChunk(uint8_t index, void *data, uint8_t size);
};

#endif //_SPIAddressable_H
