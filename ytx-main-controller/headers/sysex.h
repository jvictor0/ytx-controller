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
#ifndef SYSEX_H
#define SYSEX_H

#include "Defines.h"

#define MAX_SECTION_SIZE  256

enum ytxIOStructure
{
    START,
    ID1,
    ID2,
    ID3,
    MESSAGE_STATUS,
    WISH,
    MESSAGE_TYPE,
    BANK,
    BLOCK,
    SECTION_MSB,
    SECTION_LSB,
    DATA,
    REQUEST_ID = BANK,
    CAPTURE_STATE = BANK,
    FW_HW_MAJ = BLOCK,
    FW_HW_MIN = SECTION_MSB
};

#define MSG_SIZE_ACK        4
#define MSG_SIZE_FW_HW      9  
#define MSG_SIZE_CMP_INFO   10
#define MSG_SIZE_ERROR      4  
#define MSG_SIZE_SZ_ERROR   10  



enum ytxIOWish
{
  GET,
  SET,
};

enum ytxIOMessageTypes
{
  specialRequests,
  configurationMessages,
  componentInfoMessages,
  statusMessages,
};

enum ytxIOSpecialRequests
{
  handshakeRequest = 0x01,
  fwVersion = 0x10,
  hwVersion = 0x11,
  reboot = 0x12,
  bootloaderMode = 0x13,
  selffwUpload = 0x14,
  samd11fwUpload = 0x15,
  firmwareData = 0x16,
  enableProc = 0x17,
  disbleProc = 0x18,
  eraseEEPROM = 0x19,
  enableTesting = 0x1A
};

enum ytxIOStatus
{
  noStatus,
  validTransaction,
  statusError,
  msgTypeError,
  handshakeError,
  wishError,
  bankError,
  blockError,
  sectionError,
  sizeError,
  N_ERRORS
};

#endif // SYSEX_H