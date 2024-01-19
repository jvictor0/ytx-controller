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

#include "headers/Defines.h"

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

/*! \brief Encode System Exclusive messages.
 SysEx messages are encoded to guarantee transmission of data bytes higher than
 127 without breaking the MIDI protocol. Use this static method to convert the
 data you want to send.
 \param inData The data to encode.
 \param outSysEx The output buffer where to store the encoded message.
 \param inLength The length of the input buffer.
 \return The length of the encoded output buffer.
 @see decodeSysEx
 Code inspired from Ruin & Wesen's SysEx encoder/decoder - http://ruinwesen.com
 */
uint16_t encodeSysEx(uint8_t* inData, uint8_t* outSysEx, uint16_t inLength)
{
    uint16_t outLength   = 0;     // Num bytes in output array.
    uint16_t count      = 0;     // Num 7bytes in a block.
    outSysEx[0]         = 0;

    for (uint16_t i = 0; i < inLength; ++i)
    {
        const uint8_t data = inData[i];
        const uint8_t msb  = data >> 7;
        const uint8_t body = data & 0x7f;

        outSysEx[0] |= (msb << count);
        outSysEx[1 + count] = body;

        if (count++ == 6)
        {
            outSysEx   += 8;
            outLength  += 8;
            outSysEx[0] = 0;
            count       = 0;
        }
    }
    return outLength + count + (count != 0 ? 1 : 0);
}

/*! \brief Decode System Exclusive messages.
 SysEx messages are encoded to guarantee transmission of data bytes higher than
 127 without breaking the MIDI protocol. Use this static method to reassemble
 your received message.
 \param inSysEx The SysEx data received from MIDI in.
 \param outData    The output buffer where to store the decrypted message.
 \param inLength The length of the input buffer.
 \return The length of the output buffer.
 @see encodeSysEx @see getSysExArrayLength
 Code inspired from Ruin & Wesen's SysEx encoder/decoder - http://ruinwesen.com
 */
uint16_t decodeSysEx(uint8_t* inSysEx, uint8_t* outData, uint16_t inLength)
{
    uint16_t count      = 0;
    uint8_t msbStorage  = 0;

    for (uint16_t i = 0; i < inLength; ++i)
    {
        if ((i % 8) == 0)
        {
            msbStorage = inSysEx[i];
        }
        else
        {
            outData[count++] = inSysEx[i] | ((msbStorage & 1) << 7);
            msbStorage >>= 1;
        }
    }
    return count;
}

void handleSystemExclusiveUSB(byte *message, unsigned size){
  // SERIALPRINT(F("SysEx arrived via USB"));
  handleSystemExclusive(message, size, MIDI_USB);
}
void handleSystemExclusiveHW(byte *message, unsigned size){
  // SERIALPRINT(F("SysEx arrived via HW"));
  handleSystemExclusive(message, size, MIDI_HW);
}

void handleSystemExclusive(byte *message, unsigned size, bool midiSrc)
{
    
    static bool waitingAckAfterGet = false;

    if(testSysex){
      SERIALPRINTLN(F("******************************************************************"));
      SERIALPRINT(F("Received message size: "));
      SERIALPRINTLN(size);

      SERIALPRINTLN(F("RAW MESSAGE"));
      SERIALPRINTLN();
      for(int i = 0; i<size; i++){
       SERIALPRINTF(message[i], HEX);
       SERIALPRINT(F(" "));
       // if(i>0 && !(i%16)) SERIALPRINTLN();
      }
      SERIALPRINTLN();
    }

    if(message[ytxIOStructure::ID1] == 'y' && 
       message[ytxIOStructure::ID2] == 't' && 
       message[ytxIOStructure::ID3] == 'x')
    { 
      int8_t error = 0;
      
      if(message[ytxIOStructure::MESSAGE_TYPE] == ytxIOMessageTypes::configurationMessages)
      {
         // SERIALPRINT(F("CONFIG OK"));
          void *destination;
          uint16_t decodedPayloadSize;
          uint16_t section = (uint16_t) (message[ytxIOStructure::SECTION_MSB]<<7) | message[ytxIOStructure::SECTION_LSB];
          uint16_t encodedPayloadSize = size - ytxIOStructure::SECTION_LSB-2; //ignore headers and F0 F7

          receivingConfig = true;
          antMicrosSysex = millis();

          Watchdog.disable();
          Watchdog.enable(WATCHDOG_RESET_CONFIG);
          //SERIALPRINTLN("WD CONFIG");

          if(message[ytxIOStructure::BANK] < MAX_BANKS){
            if(message[ytxIOStructure::BLOCK] < BLOCKS_COUNT){
              
              if(testSysex){
                SERIALPRINT(F("SECTION RECEIVED: "));SERIALPRINTLN(section);
                SERIALPRINT(F("MAX SECTIONS: "));SERIALPRINTLN(memHost->SectionCount(message[ytxIOStructure::BLOCK]));
              }

              if(section < memHost->SectionCount(message[ytxIOStructure::BLOCK]))
              {              
                if(message[ytxIOStructure::WISH] == ytxIOWish::SET){
                  uint8_t decodedPayload[MAX_SECTION_SIZE];
                  decodedPayloadSize = decodeSysEx(&message[ytxIOStructure::DATA],decodedPayload,encodedPayloadSize);
                  
                  if(testSysex){
                    SERIALPRINTLN();
                    
                    SERIALPRINT(F("\nDECODED DATA SIZE: "));SERIALPRINT(decodedPayloadSize);SERIALPRINTLN(F(" BYTES"));
                    SERIALPRINT(F("EXPECTED DATA SIZE: "));SERIALPRINT(memHost->SectionSize(message[ytxIOStructure::BLOCK]));SERIALPRINTLN(F(" BYTES"));
                  
                    for(int i = 0; i<decodedPayloadSize; i++){
                      SERIALPRINTF(decodedPayload[i], HEX);
                      SERIALPRINT(F("\t"));
                      if(i>0 && !(i%16)) SERIALPRINTLN();
                    }
                    SERIALPRINTLN();  
                  }
                  
                  if(decodedPayloadSize == memHost->SectionSize(message[ytxIOStructure::BLOCK])){
                    if(testSysex){
                      SERIALPRINTLN(F("\n Bloque recibido. Tamaño concuerda. Escribiendo en EEPROM..."));
                    }
                    
                    static bool newMemReset = false;

                    if(message[ytxIOStructure::BLOCK] == ytxIOBLOCK::Configuration){
                      ytxConfigurationType* payload = (ytxConfigurationType *) decodedPayload;
                      // SERIALPRINT(F("\n Block 0 received"));

                      if(memcmp(&config->inputs,&payload->inputs,sizeof(config->inputs))){
                        // SERIALPRINT(F("\n Input config changed"));

                        enableProcessing = false;
                        validConfigInEEPROM = false;

                        //reconfigure
                        memHost->DisarmBlocks();
                                                // BLOCK NUMBER             // HOW MANY SECTIONS            // SECTION SIZE               // UNIQUE?    // ALLOCATE RAM? default: true
                        memHost->ConfigureBlock(ytxIOBLOCK::Configuration,  1,                              sizeof(ytxConfigurationType), true,         false);
                        memHost->ConfigureBlock(ytxIOBLOCK::ColorTable,     1,                              sizeof(colorRangeTable),      true,         false);
                        memHost->ConfigureBlock(ytxIOBLOCK::Encoder,        payload->inputs.encoderCount,   sizeof(ytxEncoderType),       false);
                        memHost->ConfigureBlock(ytxIOBLOCK::Analog,         payload->inputs.analogCount,    sizeof(ytxAnalogType),        false);
                        memHost->ConfigureBlock(ytxIOBLOCK::Digital,        payload->inputs.digitalCount,   sizeof(ytxDigitalType),       false);
                        memHost->ConfigureBlock(ytxIOBLOCK::Feedback,       payload->inputs.feedbackCount,  sizeof(ytxFeedbackType),      false);
                        memHost->LayoutBanks(false);  

                        // Reset controller state memory if true
                        newMemReset = true;
                      }
                      if(config->banks.count != payload->banks.count){ // Check for a change in the number of banks
                        // Reset controller state memory if true
                        newMemReset = true;
                      }
                      if(newMemReset) memHost->SetNewMemFlag();
                      
                    }
                    // if(validConfigInEEPROM && (message[ytxIOStructure::BANK] != currentBank)){    // CHECK
                    //   destination = memHost->GetSectionAddress(message[ytxIOStructure::BLOCK],section);
                    //   memcpy(destination,decodedPayload,decodedPayloadSize);
                    // }
                    if(testSysex){
                      SERIALPRINT(F("BANK RECEIVED: "));SERIALPRINT(message[ytxIOStructure::BANK]);
                      SERIALPRINT(F("\tBLOCK RECEIVED: "));SERIALPRINT(message[ytxIOStructure::BLOCK]);
                      SERIALPRINT(F("\tSECTION RECEIVED: "));SERIALPRINT(section);
                      SERIALPRINT(F("\tSIZE OF SECTION: "));SERIALPRINT(memHost->SectionSize(message[ytxIOStructure::BLOCK]));
                    }

                    memHost->WriteToEEPROM( message[ytxIOStructure::BANK], 
                                            message[ytxIOStructure::BLOCK],
                                            section, 
                                            decodedPayload);

                    if(!newMemReset) {
                      memHost->LoadBankSingleSection( message[ytxIOStructure::BANK], 
                                                      message[ytxIOStructure::BLOCK], 
                                                      section, 
                                                      false);
                    }
                  }
                  else{
                    error = ytxIOStatus::sizeError;
                  }
                 
                }
                else if(message[ytxIOStructure::WISH] == ytxIOWish::GET)
                {
                    if(testSysex){
                      SERIALPRINTLN(F("GET REQUEST RECEIVED"));
                      SERIALPRINT(F("REQUESTED BANK: ")); SERIALPRINT(message[ytxIOStructure::BANK]);
                      SERIALPRINT(F("\tREQUESTED BLOCK: ")); SERIALPRINT(message[ytxIOStructure::BLOCK]);
                      SERIALPRINT(F("\tREQUESTED SECTION: ")); SERIALPRINTLN(section);
                    }

                    uint8_t sysexBlock[MAX_SECTION_SIZE+48];
                    uint8_t sectionData[MAX_SECTION_SIZE];
                    
                    sysexBlock[ytxIOStructure::ID1] = 'y';
                    sysexBlock[ytxIOStructure::ID2] = 't';
                    sysexBlock[ytxIOStructure::ID3] = 'x';
                    sysexBlock[ytxIOStructure::MESSAGE_STATUS] = 0; //MESSAGE PART
                    sysexBlock[ytxIOStructure::WISH] = ytxIOWish::SET;
                    sysexBlock[ytxIOStructure::MESSAGE_TYPE] = ytxIOMessageTypes::configurationMessages;
                    sysexBlock[ytxIOStructure::BANK] = message[ytxIOStructure::BANK]; //bank
                    sysexBlock[ytxIOStructure::BLOCK] = message[ytxIOStructure::BLOCK]; //block
                    sysexBlock[ytxIOStructure::SECTION_MSB] = (section>>7)&0x7F;  //section msb
                    sysexBlock[ytxIOStructure::SECTION_LSB] = section & 0x7F;     //section lsb

                    memHost->ReadFromEEPROM(message[ytxIOStructure::BANK],message[ytxIOStructure::BLOCK], section, sectionData, false);

                    uint16_t sysexSize = encodeSysEx(sectionData, &sysexBlock[ytxIOStructure::DATA], memHost->SectionSize(message[ytxIOStructure::BLOCK]));
                    
                    sendSysExYTX(MIDI_USB, ytxIOStructure::SECTION_LSB + sysexSize, &sysexBlock[1], false);

                    waitingAckAfterGet = true;

                    if(testSysex){
                      PrintSysex(ytxIOStructure::SECTION_LSB + sysexSize, sysexBlock);
                    }
                }else
                  error = ytxIOStatus::wishError;
              }
              else
                error = ytxIOStatus::sectionError;
            }
            else
              error = ytxIOStatus::blockError;
          }
          else
            error = ytxIOStatus::bankError;
                
          if(!error)    SendAck();  
          else          SendError(error, message);

      }else if(message[ytxIOStructure::MESSAGE_TYPE] == ytxIOMessageTypes::specialRequests){
        if(testSysex){
          SERIALPRINTLN(F("SPECIAL REQUEST RECEIVED"));
        }
        if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::handshakeRequest){
          SendAck();
        }else if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::bootloaderMode){
          if(testSysex){
            SERIALPRINTLN(F("REQUEST: BOOTLOADER MODE"));
          }
          feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
          
          config->board.bootFlag = 1;                                            
          byte bootFlagState = 0;
          eep.read(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));
          bootFlagState |= 1;
          eep.write(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));

          SelfReset(RESET_TO_CONTROLLER);
        }else if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::fwVersion){
          if(testSysex){
            SERIALPRINTLN(F("REQUEST: FIRMWARE VERSION"));
          }

          uint8_t statusMsgSize = MSG_SIZE_FW_HW;
          uint8_t sysexBlock[statusMsgSize];
          
          // sysexBlock[ytxIOStructure::START] = 0xF0;
          sysexBlock[ytxIOStructure::ID1-1]             = 'y';
          sysexBlock[ytxIOStructure::ID2-1]             = 't';
          sysexBlock[ytxIOStructure::ID3-1]             = 'x';
          sysexBlock[ytxIOStructure::MESSAGE_STATUS-1]  = validTransaction;
          sysexBlock[ytxIOStructure::WISH-1]            = SET;
          sysexBlock[ytxIOStructure::MESSAGE_TYPE-1]    = ytxIOMessageTypes::specialRequests;
          sysexBlock[ytxIOStructure::REQUEST_ID-1]      = ytxIOSpecialRequests::fwVersion;
          sysexBlock[ytxIOStructure::FW_HW_MAJ-1]       = FW_VERSION_MAJOR;
          sysexBlock[ytxIOStructure::FW_HW_MIN-1]       = FW_VERSION_MINOR;
          // sysexBlock[ytxIOStructure::FW_HW_MIN+1] = 0xF7;

          sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);
          if(testSysex){
            PrintSysex(statusMsgSize, sysexBlock);
          }
        }else if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::hwVersion){
          if(testSysex){
            SERIALPRINTLN(F("REQUEST: HARDWARE VERSION"));
          }

          uint8_t statusMsgSize = MSG_SIZE_FW_HW;
          uint8_t sysexBlock[statusMsgSize];
          
          // sysexBlock[ytxIOStructure::START] = 0xF0;
          sysexBlock[ytxIOStructure::ID1-1] = 'y';
          sysexBlock[ytxIOStructure::ID2-1] = 't';
          sysexBlock[ytxIOStructure::ID3-1] = 'x';
          sysexBlock[ytxIOStructure::MESSAGE_STATUS-1] = validTransaction;
          sysexBlock[ytxIOStructure::WISH-1] = SET;
          sysexBlock[ytxIOStructure::MESSAGE_TYPE-1] = ytxIOMessageTypes::specialRequests;
          sysexBlock[ytxIOStructure::REQUEST_ID-1] = ytxIOSpecialRequests::hwVersion;
          sysexBlock[ytxIOStructure::FW_HW_MAJ-1] = HW_VERSION_MAJOR;
          sysexBlock[ytxIOStructure::FW_HW_MIN-1] = HW_VERSION_MINOR;
          // sysexBlock[ytxIOStructure::FW_HW_MIN+1] = 0xF7;

          sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);

          if(testSysex){
            PrintSysex(statusMsgSize, sysexBlock);
          }
        }else if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::reboot){                
          if(testSysex){
            SERIALPRINTLN(F("REQUEST: REBOOT"));
          } 
           
          feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
          // delay(5);

          SendAck();
          SelfReset(RESET_TO_CONTROLLER);
        }else if(message[ytxIOStructure::REQUEST_ID] == ytxIOSpecialRequests::enableTesting){                
          if(testSysex){
            SERIALPRINTLN(F("REQUEST: ENABLE CDC"));
          } 
          
          // /* First tap */
          // CDC_ENABLE_DATA = CDC_ENABLE_MAGIC;
          cdcMagicData = CDC_ENABLE_MAGIC;
          SelfReset(RESET_TO_CONTROLLER);
        }

      }else if(message[ytxIOStructure::MESSAGE_TYPE] == ytxIOMessageTypes::componentInfoMessages){
       autoSelectMode = message[ytxIOStructure::CAPTURE_STATE];
        // autoSelectMode  = !autoSelectMode;
        if(testSysex){
            SERIALPRINT(F("COMPONENT INFO "));SERIALPRINTLN(autoSelectMode ? F("ENABLED") : F("DISABLED"));
        }
        SendAck();
      }else{
        error = ytxIOStatus::msgTypeError;
        SendError(error, message);
      }
    }else{
      // If it is not meant to get to a yaeltex controller, route it accordingly
      if(testSysex){
        SERIALPRINTLN(F("NOT YTX SYSEX MESSAGE"));  
      }

      if(midiSrc == MIDI_HW){  // IN FROM MIDI HW
        if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_USB){    // Send to MIDI USB port
          // SERIALPRINT(F("HW -> USB"));
          sendSysExYTX(MIDI_USB, size, message, true);
        }
        if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_HW){     // Send to MIDI DIN port
          // SERIALPRINT(F("HW -> HW"));
          sendSysExYTX(MIDI_HW, size, message, true);
        }
      }else{        // IN FROM MIDI USB
        if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
          // SERIALPRINT(F("USB -> USB"));
          sendSysExYTX(MIDI_USB, size, message, true);
        }
        if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
          // SERIALPRINT(F("USB -> HW"));
          sendSysExYTX(MIDI_HW, size, message, true);
        }
      }
    }

}

void SendComponentInfo(uint8_t componentType, uint16_t index){
  
  uint8_t statusMsgSize = MSG_SIZE_CMP_INFO;
  uint8_t sysexBlock[statusMsgSize];

  // sysexBlock[ytxIOStructure::START] = 0xF0;
  sysexBlock[ytxIOStructure::ID1-1] = 'y';
  sysexBlock[ytxIOStructure::ID2-1] = 't';
  sysexBlock[ytxIOStructure::ID3-1] = 'x';
  sysexBlock[ytxIOStructure::MESSAGE_STATUS-1] = 0;
  sysexBlock[ytxIOStructure::WISH-1] = 1;
  sysexBlock[ytxIOStructure::MESSAGE_TYPE-1] = ytxIOMessageTypes::componentInfoMessages;
  sysexBlock[ytxIOStructure::BANK-1] = currentBank;
  sysexBlock[ytxIOStructure::BLOCK-1] = componentType;
  uint16_t section = GetHardwareID(componentType, index);
  sysexBlock[ytxIOStructure::SECTION_MSB-1] = (section>>7)&0x7F;  //section msb
  sysexBlock[ytxIOStructure::SECTION_LSB-1] = section & 0x7F;     //section lsb
  // sysexBlock[ytxIOStructure::SECTION_LSB+1] = 0xF7;

  lastComponentInfoId = section;
  
  sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);

  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_CONFIG_OUT);

  if(testSysex){
    PrintSysex(statusMsgSize, sysexBlock);
  }
  
}

uint16_t GetHardwareID(uint8_t componentType, uint16_t index){

  switch (componentType) {
    case ytxIOBLOCK::Encoder: {
      return index;
    } break;
    case ytxIOBLOCK::Digital: {
      return index + config->inputs.encoderCount; 
    } break;
    case ytxIOBLOCK::Analog: {
      return index + config->inputs.encoderCount + config->inputs.digitalCount;
    } break;
    default:{
      return 0;
    }break;
  }
  
}

void PrintSysex(uint16_t msgSize, uint8_t* msg){
  SERIALPRINTLN(F("\nMessage sent: "));
  SERIALPRINT(F("Size: "));SERIALPRINTLN(msgSize);
  
  SERIALPRINTF(240, HEX); SERIALPRINT(F("\t"));

  for(int i = 0; i < msgSize; i++){
    SERIALPRINTF(msg[i], HEX);
    SERIALPRINT(F("\t"));
    if(i>0 && !(i%16)) SERIALPRINTLN();
  }
  SERIALPRINTLNF(247, HEX);
  SERIALPRINTLN();
}

void SendError(uint8_t error, byte *recvdMessage){
  uint8_t statusMsgSize = MSG_SIZE_SZ_ERROR;
  uint8_t sysexBlock[statusMsgSize];
  
  // sysexBlock[ytxIOStructure::START] = 0xF0;
  sysexBlock[ytxIOStructure::ID1-1] = 'y';
  sysexBlock[ytxIOStructure::ID2-1] = 't';
  sysexBlock[ytxIOStructure::ID3-1] = 'x';
  sysexBlock[ytxIOStructure::MESSAGE_STATUS-1] = error;

  if(error == ytxIOStatus::sizeError){
    sysexBlock[ytxIOStructure::BANK-1] = recvdMessage[ytxIOStructure::BANK];
    sysexBlock[ytxIOStructure::BLOCK-1] = recvdMessage[ytxIOStructure::BLOCK];
    sysexBlock[ytxIOStructure::SECTION_MSB-1] = recvdMessage[ytxIOStructure::SECTION_MSB];
    sysexBlock[ytxIOStructure::SECTION_LSB-1] = recvdMessage[ytxIOStructure::SECTION_LSB];
    // sysexBlock[ytxIOStructure::SECTION_LSB+1] = 0xF7;
    sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);
  }else{
    statusMsgSize = MSG_SIZE_ERROR;
    // sysexBlock[ytxIOStructure::MESSAGE_STATUS+1] = 0xF7;
    sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);
  }
  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_CONFIG_OUT);

  if(testSysex){
    SERIALPRINT(F("ERROR CODE: "));
    SERIALPRINTLNF(error, HEX);
    PrintSysex(statusMsgSize, sysexBlock);
  }
}

void SendAck(){
  uint8_t statusMsgSize = MSG_SIZE_ACK;
  uint8_t sysexBlock[statusMsgSize];
  
  // sysexBlock[ytxIOStructure::START] = 0xF0;
  sysexBlock[ytxIOStructure::ID1-1] = 'y';
  sysexBlock[ytxIOStructure::ID2-1] = 't';
  sysexBlock[ytxIOStructure::ID3-1] = 'x';
  sysexBlock[ytxIOStructure::MESSAGE_STATUS-1] = 1;
  // sysexBlock[ytxIOStructure::MESSAGE_STATUS+1] = 0xF7;
  sendSysExYTX(MIDI_USB, statusMsgSize, sysexBlock, false);

  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_CONFIG_OUT);

  if(testSysex){
    // SERIALPRINTLN();
    // SERIALPRINT(micros()-antMicrosSysex); SERIALPRINT(F("<- MICROS BETWEEN MESSAGES: ")); 
    // antMicrosSysex = micros();
    PrintSysex(statusMsgSize, sysexBlock);
  }
}

void sendSysExYTX(bool port, uint16_t length, const byte* msg, bool inArrayContainsBoundaries){
  tcDisable();
  if(port == MIDI_USB){
    MIDI.sendSysEx(length, msg, inArrayContainsBoundaries);
  }else if(port == MIDI_HW){
    MIDIHW.sendSysEx(length, msg, inArrayContainsBoundaries);
  }
  tcStartCounter();
}