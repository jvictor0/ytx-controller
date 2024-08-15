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
 *  3. Neither the name of YAELTEX nor the names of its contributors may be used
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

#include "SPIAddressable.h"

SPIAddressableElement::SPIAddressableElement() {}

/*! The begin function takes three parameters.  The first is an SPI class
 *  pointer.  This is the address of an SPI object (either the default
 *  SPI object on the Arduino, or an object made using the DSPIx classes
 *  on the chipKIT).  The second parameter is the chip select pin number
 *  to use when communicating with the chip.  The third is the internal
 *  address number of the chip.  This is controlled by the three Ax pins
 *  on the chip.
 *  It also performs the initial configuration of the IO expander chip.
 *  Not only does it set up the SPI communications, but it also configures the chip
 *  for address-based communication and sets the default parameters and registers
 *  to sensible values.
 *
 *  Example:
 *
 *      myModule.begin(&SPIBUS, 0);
 *
 */

void SPIAddressableElement::begin(SPIAdressableBUS *_spiBUS, uint8_t _addr) {}

void SPIAddressableElement::begin(SPIAdressableBUS *_spiBUS, uint8_t _base, uint8_t _addr) {
    spiBUS = _spiBUS;
    addr = _addr;
    base = _base;
}

void SPIAddressableElement::enableAddressing(uint8_t _next_addr){
    uint8_t cmd = OPCODEW | ((base | (addr & 0b111)) << 1);
  spiBUS->port->beginTransaction(spiBUS->settings);
    ::digitalWrite(spiBUS->cs, LOW);
    spiBUS->port->transfer(cmd);
    spiBUS->port->transfer(0x0F);
    spiBUS->port->transfer(((_next_addr & 0b111) << 1)|ADDR_ENABLE);
    ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();
}

void SPIAddressableElement::readChunk(uint8_t index, void *data, uint8_t size){
    uint8_t *data_ = (uint8_t*)data;
    uint8_t cmd = OPCODER | ((base | (addr & 0b111)) << 1);
  spiBUS->port->beginTransaction(spiBUS->settings);
    ::digitalWrite(spiBUS->cs, LOW);
    spiBUS->port->transfer(cmd);
    spiBUS->port->transfer(index);//index of first data register to read
    spiBUS->port->transfer(0xFF);//dummy 
    for(int i=0;i<size;i++){
        data_[i] = spiBUS->port->transfer(0xFF);
    }
    ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();
}

void SPIAddressableElement::writeChunk(uint8_t index, void *data, uint8_t size){
    uint8_t *data_ = (uint8_t*)data;
    uint8_t cmd = OPCODEW | ((base | (addr & 0b111)) << 1);
  spiBUS->port->beginTransaction(spiBUS->settings);
    ::digitalWrite(spiBUS->cs, LOW);
    spiBUS->port->transfer(cmd);
    spiBUS->port->transfer(index);//index of first data register to write
    for(int i=0;i<size;i++){
        spiBUS->port->transfer(data_[i]);
    }
    ::digitalWrite(spiBUS->cs, HIGH);
  spiBUS->port->endTransaction();
}

uint8_t SPIAddressableElement::getChecksum(const uint8_t *data, uint32_t size){
    uint8_t checksum = 0;

    for (uint32_t i = 0; i < size; ++i) {
        checksum += data[i];
    }

    return checksum;
}