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
#ifndef SPIAddressableSlave_h
#define SPIAddressableSlave_h

#include <Arduino.h>
#include "pins_arduino.h"
#include "wiring_private.h"

#include "SPIAddressable.h"

// #define FRAMEWORK // comment this line to use direct registrer manipulation

// #define SERIALPRINT(a)        {Serial.print(a);     }
// #define SERIALPRINTLN(a)      {Serial.println(a);   }
// #define SERIALPRINTF(a, f)    {Serial.print(a,f);   }
// #define SERIALPRINTLNF(a, f)  {Serial.println(a,f); }


#ifdef SPI_SLAVE

#define CONCAT_(a,b) a ## b
#define CONCAT(a,b) CONCAT_(a,b)

#define SERCOM SERCOM4
#define IRQ(sercom_n) CONCAT(sercom_n,_IRQn)

typedef void (*voidFuncPtr)(void);

class SPIAddressableSlave {
  public:
    // Constructors //
    SPIAddressableSlave();

    // Public methods //
    void begin(int,int,int);
    void hook();
    void wiring(int*,int*);
    void takeMISObus();
    void leaveMISObus();
    void resetInternalState();
    void getAddress();
    void setNextAddress(int);
    void setAddress(int);
    uint8_t* getUserRegistersPointer();
    uint8_t* getControlRegistersPointer();
    void setTransmissionCompleteCallback(voidFuncPtr);
    void setConfigurationCompleteCallback(voidFuncPtr);
    uint8_t getChecksum(const uint8_t *data, uint32_t size);

    volatile uint8_t  base;
    volatile uint8_t  registersCount;
    volatile uint8_t  ctrlRegistersCount;
    volatile uint8_t  usrRegistersCount;
    volatile uint32_t myAddress;
    volatile uint32_t nextAddress;

    volatile uint32_t state;

    volatile uint32_t registerIndex;
    volatile uint32_t receivedBytes;
    volatile uint8_t* registers;
    volatile uint8_t* controlRegister;
    volatile uint8_t* userDataRegister;

    volatile bool isAddressEnable;
    volatile bool isTransmissionComplete;
    volatile bool isConfigurationComplete;
    volatile bool updateAddressingMode;
    
  private:
    void SercomInit();

    bool isWired;
    int inputAddressPin[3];
    int outputAddressPin[3];

    void (*transmissionCompleteCallback)(void);
    void (*configurationCompleteCallback)(void);
};

extern SPIAddressableSlave SPIAddressableSlaveModule;
#endif // SPI_SLAVE

enum machineSates
{
  GET_OPCODE = 0,
  GET_REG_INDEX,
  GET_TRANSFER
};

#ifdef FRAMEWORK
  enum spi_transfer_mode
  {
    SERCOM_SPI_TRANSFER_MODE_0 = 0,
    SERCOM_SPI_TRANSFER_MODE_1 = SERCOM_SPI_CTRLA_CPHA,
    SERCOM_SPI_TRANSFER_MODE_2 = SERCOM_SPI_CTRLA_CPOL,
    SERCOM_SPI_TRANSFER_MODE_3 = SERCOM_SPI_CTRLA_CPHA | SERCOM_SPI_CTRLA_CPOL,
  };

  enum spi_frame_format
  {
    SERCOM_SPI_FRAME_FORMAT_SPI_FRAME      = SERCOM_SPI_CTRLA_FORM(0),
    SERCOM_SPI_FRAME_FORMAT_SPI_FRAME_ADDR = SERCOM_SPI_CTRLA_FORM(2),
  };

  enum spi_character_size
  {
    SERCOM_SPI_CHARACTER_SIZE_8BIT = SERCOM_SPI_CTRLB_CHSIZE(0),
    SERCOM_SPI_CHARACTER_SIZE_9BIT = SERCOM_SPI_CTRLB_CHSIZE(1),
  };

  enum spi_data_order
  {
    SERCOM_SPI_DATA_ORDER_LSB = SERCOM_SPI_CTRLA_DORD,
    SERCOM_SPI_DATA_ORDER_MSB   = 0,
  };
#else
  #define MISO_PORT PORTA
  #define MISO_PIN  12
#endif//#ifdef FRAMEWORK

#endif