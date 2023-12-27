/*
 * Copyright (c) 2014, Majenko Technologies
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
 *  3. Neither the name of Majenko Technologies nor the names of its contributors may be used
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

#include "SPIAnalogExpander.h"

void SPIAnalogExpander::begin(SPIAdressableBUS *_spiBUS) {
    addr = ANALOG_EXPANDER_ADDRESS;
    spiBUS = _spiBUS;
    base = FIXED_ELEMENTS_BASE_ADDRESS<<4;

    enableAddressing(0);
    delay(5); // wait addresing ready
}

void SPIAnalogExpander::configure(SPIAnalogExpanderParameters *parameters){
    writeChunk(0,parameters,sizeof(SPIAnalogExpanderParameters));
    delay(5); // wait config ready
}

void SPIAnalogExpander::getActiveChannels(void) {
  uint8_t cmd = OPCODER | ((base | (addr & 0b111)) << 1);
  spiBUS->port->beginTransaction(spiBUS->settings);
  ::digitalWrite(spiBUS->cs, LOW);
  spiBUS->port->transfer(cmd);
  spiBUS->port->transfer(REGISTER_OFFSET+MAX_ANALOG_CHANNELS*sizeof(uint16_t));//index of first active channel register
  spiBUS->port->transfer(0xFF);//dummy 
  for(int i=0;i<MAX_ANALOG_CHANNELS/8;i++)
  {
    activeChannels[i] = (uint8_t)(spiBUS->port->transfer(0xFF));
  }
  ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();
}

bool SPIAnalogExpander::isActiveChannel(uint32_t n) {
  return (bool)(activeChannels[n/8] & (1<<(n%8)));
}

int16_t SPIAnalogExpander::analogRead(uint32_t n) {
    uint8_t lowByte;
    uint8_t highByte;
    uint16_t newRead; 
    uint16_t localChecksum;
    uint16_t transactionChecksum;
    uint8_t cmd = OPCODER | ((base | (addr & 0b111)) << 1);

  spiBUS->port->beginTransaction(spiBUS->settings);
    ::digitalWrite(spiBUS->cs, LOW);
    spiBUS->port->transfer(cmd);
    spiBUS->port->transfer(REGISTER_OFFSET+n*sizeof(uint16_t));//index of analog value register
    spiBUS->port->transfer(0xFF);//dummy 
    lowByte = spiBUS->port->transfer(0xFF);
    highByte = spiBUS->port->transfer(0xFF);
    ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();

    newRead = ((uint16_t)highByte)<<8 | (uint16_t)lowByte;

    localChecksum = (uint16_t)((newRead&0x00FF + (newRead>>8)&0x00FF))&0x000F;
    transactionChecksum = (newRead>>12)&0x000F;

    if(transactionChecksum==localChecksum){
      return newRead&0x0FFF;
    }else{
      return -1;
    }
}
