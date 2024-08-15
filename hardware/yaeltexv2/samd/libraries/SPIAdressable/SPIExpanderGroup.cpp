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

#include "SPIGPIOExpander.h"

SPIExpanderGroup::SPIExpanderGroup(){}

void SPIExpanderGroup::readAllRegs (){
  byte cmd = OPCODER;
    for (uint8_t i = 0; i < MCP23017_REGISTER_COUNT; i++) {
      port->beginTransaction(settings);
        digitalWrite(cs, LOW);
        port->transfer(cmd);
        port->transfer(i);
        digitalWrite(cs, HIGH);
      port->endTransaction();
    }
}

void SPIExpanderGroup::SetAllAsOutput(){
  byte cmd = OPCODEW;
  port->beginTransaction(settings);
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IODIRA);
    port->transfer(0x00);
    digitalWrite(cs, HIGH);
  port->endTransaction();
  delayMicroseconds(5);
  port->beginTransaction(settings);
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(IODIRB);
    port->transfer(0x00);
    digitalWrite(cs, HIGH);
  port->endTransaction();
}

void SPIExpanderGroup::InitPinsGhostModules(){
  byte cmd = OPCODEW;
  port->beginTransaction(settings);
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(OLATA);
    port->transfer(0xFF);
    digitalWrite(cs, HIGH);
  port->endTransaction();
  delayMicroseconds(5);
  port->beginTransaction(settings);
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(OLATB);
    port->transfer(0xFF);
    digitalWrite(cs, HIGH);
  port->endTransaction();
}

void SPIExpanderGroup::SetPullUps(){
  byte cmd = OPCODEW;
  port->beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(GPPUA);
    port->transfer(0xFF);
    digitalWrite(cs, HIGH);
  port->endTransaction();
  delayMicroseconds(5);
  port->beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));
    digitalWrite(cs, LOW);
    port->transfer(cmd);
    port->transfer(GPPUB);
    port->transfer(0xFF);
    digitalWrite(cs, HIGH);
  port->endTransaction();
}
