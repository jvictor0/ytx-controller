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
    addr = 0;
    spiBUS = _spiBUS;
    base = ANALOG_EXPANDER_BASE_ADDRESS<<4;

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

uint16_t SPIAnalogExpander::analogRead(uint32_t n) {
    uint16_t value;       
    uint8_t cmd = OPCODER | ((base | (addr & 0b111)) << 1);
  spiBUS->port->beginTransaction(spiBUS->settings);
    ::digitalWrite(spiBUS->cs, LOW);
    spiBUS->port->transfer(cmd);
    spiBUS->port->transfer(REGISTER_OFFSET+n*sizeof(uint16_t));//index of analog value register
    spiBUS->port->transfer(0xFF);//dummy 
    value = (uint16_t)(spiBUS->port->transfer(0xFF));
    value += (uint16_t)(spiBUS->port->transfer(0xFF))<<8;
    ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();
  return value;
}
