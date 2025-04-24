/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "customUart.h"
#include "Arduino.h"
#include "wiring_private.h"

void dummyCallback(){
  //Do Nothing
}

customUart::customUart(SERCOM *_s, uint8_t _pinRX, uint8_t _pinTX, SercomRXPad _padRX, SercomUartTXPad _padTX)
{
  sercom = _s;
  uc_pinRX = _pinRX;
  uc_pinTX = _pinTX;
  uc_padRX = _padRX ;
  uc_padTX = _padTX;

  mReceptionCallback = dummyCallback;
}

void customUart::begin(unsigned long baudrate)
{
  begin(baudrate, SERIAL_8N1);
}

void customUart::begin(unsigned long baudrate, uint16_t config)
{
  pinPeripheral(uc_pinRX, g_APinDescription[uc_pinRX].ulPinType);
  pinPeripheral(uc_pinTX, g_APinDescription[uc_pinTX].ulPinType);

  sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
  sercom->initFrame(extractCharSize(config), LSB_FIRST, extractParity(config), extractNbStopBit(config));
  sercom->initPads(uc_padTX, uc_padRX);

  sercom->disableReceiveCompleteInterruptUART();

  sercom->enableUART();
}

void customUart::end()
{
  sercom->resetUART();
}

void customUart::flush()
{
  dummyCallback();
}

void customUart::setReceptionCallback(void (*callback)(void))
{
  sercom->disableReceiveCompleteInterruptUART();

  mReceptionCallback = callback;

  sercom->enableReceiveCompleteInterruptUART();
}

void customUart::IrqHandler()
{
  if (sercom->availableDataUART()) {
    lastReceived = sercom->readDataUART();

    mReceptionCallback();
  }

  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    // TODO: if (sercom->isBufferOverflowErrorUart()) ....
    // TODO: if (sercom->isFrameErrorUart()) ....
    // TODO: if (sercom->isParityErrorUart()) ....
    sercom->clearStatusUART();
  }
}

int customUart::available()
{
  return 1;
}

int customUart::availableForWrite()
{
  return 1;
}

int customUart::peek()
{
  return read();
}

int customUart::read()
{
  return lastReceived;
}

size_t customUart::write(const uint8_t data){
  sercom->writeDataUART(data);
  return 1;
}

size_t customUart::write9bit(const uint8_t data) { 
  sercom->writeDataUART9bit(data);
  return 1;
}

SercomNumberStopBit customUart::extractNbStopBit(uint16_t config)
{
  switch(config & HARDSER_STOP_BIT_MASK)
  {
    case HARDSER_STOP_BIT_1:
    default:
      return SERCOM_STOP_BIT_1;

    case HARDSER_STOP_BIT_2:
      return SERCOM_STOP_BITS_2;
  }
}

SercomUartCharSize customUart::extractCharSize(uint16_t config)
{
  switch(config & HARDSER_DATA_MASK)
  {
    case HARDSER_DATA_5:
      return UART_CHAR_SIZE_5_BITS;

    case HARDSER_DATA_6:
      return UART_CHAR_SIZE_6_BITS;

    case HARDSER_DATA_7:
      return UART_CHAR_SIZE_7_BITS;
      
    case HARDSER_DATA_9:
      return UART_CHAR_SIZE_9_BITS;

    case HARDSER_DATA_8:
    default:
      return UART_CHAR_SIZE_8_BITS;

  }
}

SercomParityMode customUart::extractParity(uint16_t config)
{
  switch(config & HARDSER_PARITY_MASK)
  {
    case HARDSER_PARITY_NONE:
    default:
      return SERCOM_NO_PARITY;

    case HARDSER_PARITY_EVEN:
      return SERCOM_EVEN_PARITY;

    case HARDSER_PARITY_ODD:
      return SERCOM_ODD_PARITY;
  }
}
