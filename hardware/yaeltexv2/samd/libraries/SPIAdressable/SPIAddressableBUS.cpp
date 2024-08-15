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

enum {
    IOCONA_ADDRESS_B0 = 10,// ADDRESS FOR IOCONA, for IOCON.BANK = 0
    IOCONB_ADDRESS_B0 = 11,// ADDRESS FOR IOCONB, for IOCON.BANK = 0
    IOCONA_ADDRESS_B1 = 5, // ADDRESS FOR IOCONA, for IOCON.BANK = 1 
    IOCONB_ADDRESS_B1 = 15 // ADDRESS FOR IOCONB, for IOCON.BANK = 1
};

SPIAdressableBUS::SPIAdressableBUS() {}

void SPIAdressableBUS::begin(SPIClass *_spi, SPISettings _spiSettings, uint8_t _cs) { 
    port = _spi;
    cs = _cs;
    settings = _spiSettings;

    pinMode(cs, OUTPUT);
}

void SPIAdressableBUS::DisableHWAddress(uint8_t base){
  byte cmd = 0;
  // DISABLE HARDWARE ADDRESSING FOR ALL CHIPS - ONLY NEEDED FOR RESET
  base = base<<4;
  
  for (int n = 0; n < 8; n++) {
    cmd = OPCODEW | ((base | (n & 0b111)) << 1);
    port->beginTransaction(settings);
    ::digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IOCONA_ADDRESS_B0);                     
    port->transfer(ADDR_DISABLE);
    ::digitalWrite(cs, HIGH);
    port->endTransaction();
    
    port->beginTransaction(settings);
    ::digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IOCONB_ADDRESS_B0);                     
    port->transfer(ADDR_DISABLE);
    ::digitalWrite(cs, HIGH);
    port->endTransaction();

    port->beginTransaction(settings);
    ::digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IOCONA_ADDRESS_B1);                          
    port->transfer(ADDR_DISABLE); 
    ::digitalWrite(cs, HIGH);
    port->endTransaction();

    port->beginTransaction(settings);
    ::digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IOCONB_ADDRESS_B1);                           
    port->transfer(ADDR_DISABLE); 
    ::digitalWrite(cs, HIGH);
    port->endTransaction();
  }

  delay(10); // wait disable addressing ready
}


