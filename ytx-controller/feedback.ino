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

#include "headers/FeedbackClass.h"

//----------------------------------------------------------------------------------------------------
// FEEDBACK FUNCTIONS
//----------------------------------------------------------------------------------------------------
void FeedbackClass::Init(uint8_t maxBanks, uint8_t maxEncoders, uint16_t maxDigital, uint16_t maxIndependent) {
  nBanks = maxBanks;
  nEncoders = maxEncoders;
  nDigitals = maxDigital;
  nIndependent = maxIndependent;

  if(!nBanks) return;    // If number of encoders is zero, return;
  
  feedbackUpdateWriteIdx = 0;
  feedbackUpdateReadIdx = 0;
  fbItemsToSend = 0;
  fbMessagesSent = 0;
  waitingMoreData = false;
  antMillisWaitMoreData = 0;

  for(int f = 0; f < FEEDBACK_UPDATE_BUFFER_SIZE; f++){
    feedbackUpdateBuffer[f].type = 0;
    feedbackUpdateBuffer[f].indexChanged = 0;
    feedbackUpdateBuffer[f].newValue = 0;
    feedbackUpdateBuffer[f].newOrientation = 0;
    feedbackUpdateBuffer[f].isShifter = 0;
    feedbackUpdateBuffer[f].updatingBank = false;
    feedbackUpdateBuffer[f].rotaryValueToColor = false;
    feedbackUpdateBuffer[f].valueToIntensity = false;
  }
  
  flagBlinkStatusLED = 0;
  blinkCountStatusLED = 0;
  statusLEDfbType = 0;
  lastStatusLEDState = LOW;
  millisStatusPrev = 0;
    
  // First dimension is an array of pointers, each pointing to a column - https://www.eskimo.com/~scs/cclass/int/sx9b.html

  // ESTO ADELANTE DEL MALLOC DE LOS ENCODERS HACE QUE FUNCIONEN LOS ENCODERS PARA 1 BANCO CONFIGURADO
  // EL PRIMER PUNTERO QUE LLAMA A MALLOC, CAMBIA LA DIRECCIÓN A LA QUE APUNTA DESPUÉS DE LA INICIALIZACIÓN,
  // EN LA SEGUNDA VUELTA DE LOOP, SOLO CUANDO HAY 1 BANCO CONFIGURADO
  if(nDigitals){
    digFbData = (digFeedbackData**) memHost->AllocateRAM(nBanks*sizeof(digFeedbackData*));
  }
  if(nEncoders){
    encFbData = (encFeedbackData**) memHost->AllocateRAM(nBanks*sizeof(encFeedbackData*));
  }

  // Reset to bootloader if there isn't enough RAM
  if(FreeMemory() < nBanks*nEncoders*sizeof(encFeedbackData) + nBanks*nDigitals*sizeof(digFeedbackData) + 800){
    SERIALPRINTLN("NOT ENOUGH RAM / FEEDBACK -> REBOOTING TO BOOTLOADER...");
    delay(500);
    config->board.bootFlag = 1;                                            
    byte bootFlagState = 0;
    eep.read(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));
    bootFlagState |= 1;
    eep.write(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));

    SelfReset(RESET_TO_CONTROLLER);
  }

  for (int b = 0; b < nBanks; b++) {
    if(nEncoders){
      #if defined(DISABLE_ENCODER_BANKS)
        // Allocate only first bank and reference other banks to firs bank
        if(b==0){
          encFbData[b] = (encFeedbackData*) memHost->AllocateRAM(nEncoders*sizeof(encFeedbackData));
        }else{
          encFbData[b] = encFbData[0];
        }
      #else
        // Allocate all banks
        encFbData[b] = (encFeedbackData*) memHost->AllocateRAM(nEncoders*sizeof(encFeedbackData));
      #endif

      for (int e = 0; e < nEncoders; e++) {
        encFbData[b][e].encRingState = 0;
        encFbData[b][e].encRingStatePrev = 0;
        encFbData[b][e].vumeterValue = 0;
        encFbData[b][e].colorIndexRotary = 127;
        encFbData[b][e].colorIndexSwitch = 0;  
        encFbData[b][e].rotIntensityFactor = MAX_INTENSITY;
        encFbData[b][e].swIntensityFactor = MAX_INTENSITY;
      }
    }
    
    if(nDigitals){
      #if defined(DISABLE_DIGITAL_BANKS)
        // Allocate only first bank and reference other banks to firs bank
        if(b==0){
          digFbData[b] = (digFeedbackData*) memHost->AllocateRAM(nDigitals*sizeof(digFeedbackData));
        }else{
          digFbData[b] = digFbData[0];
        }
      #else
        // Allocate all banks
        digFbData[b] = (digFeedbackData*) memHost->AllocateRAM(nDigitals*sizeof(digFeedbackData));
      #endif

      for (uint16_t d = 0; d < nDigitals; d++) {
        digFbData[b][d].colorIndexPrev = 127;
        digFbData[b][d].digIntensityFactor = MAX_INTENSITY;
      }
    }
  } 
}

void FeedbackClass::InitFb(){
  // POWER MANAGEMENT - READ FROM POWER PIN, IF POWER SUPPLY IS PRESENT AND SET LED BRIGHTNESS ACCORDINGLY
  feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
  delay(10);
    
  if(digitalRead(externalVoltagePin)){
    if(nEncoders >= 28)  currentBrightness = BRIGHTNESS_WOP_32_ENC;
    else                 currentBrightness = BRIGHTNESS_WOP;
  }else{
    currentBrightness = BRIGHTNESS_WITH_POWER;
  }
  // Set External ISR for the power adapter detector pin
  attachInterrupt(digitalPinToInterrupt(externalVoltagePin), ChangeBrightnessISR, CHANGE);
                            
  InitAuxController(false);

  begun = true;
}

void FeedbackClass::InitAuxController(bool resetHappened){
  bool okToContinue = false;
  byte initFrameIndex = 0;

  // SEND INITIAL VALUES AND LED BRIGHTNESS TO SAMD11
  #define INIT_FRAME_SIZE 7
  byte initFrameArray[INIT_FRAME_SIZE] = {INIT_VALUES, 
                                          nEncoders,
                                          nIndependent,   // CHANGE TO AMOUNT OF ANALOG WITH FEEDBACK
                                          amountOfDigitalInConfig[0],
                                          amountOfDigitalInConfig[1],
                                          currentBrightness,
                                          resetHappened ? 0 : config->board.rainbowOn};
  do{
    SendCommand(initFrameArray[initFrameIndex++]); 

    if(initFrameIndex == INIT_FRAME_SIZE) okToContinue = true;

  }while(!okToContinue);
}

void FeedbackClass::Update() {

  if(!begun) return;    // If didn't go through INIT, return;

  if((waitingMoreData && (millis()-antMillisWaitMoreData > MAX_WAIT_MORE_DATA_MS)) || (fbItemsToSend >= MSG_BUFFER_AUX)){
    waitingMoreData = false;
  }

  if(waitingMoreData || fbShowInProgress || fbItemsToSend == 0) return;

  if(!sendingFbData){
    SendCommand(BURST_INIT);
    sendingFbData = true;
  } 

  while (fbItemsToSend && fbMessagesSent < MSG_BUFFER_AUX) {    

    uint8_t fbUpdateType = feedbackUpdateBuffer[feedbackUpdateReadIdx].type;
    uint8_t fbUpdateQueueIndex = feedbackUpdateReadIdx;
    
    IncreaseBufferIndex(READ_INDEX);
    // SERIALPRINTLN(fbItemsToSend);
    
    switch(fbUpdateType){
      case FB_ENCODER:
      case FB_ENC_2CC:
      case FB_ENC_SWITCH:
      case FB_ENC_VAL_TO_COLOR:
      case FB_ENC_VAL_TO_INT:
      case FB_ENC_SHIFT:
      case FB_ENC_SW_VAL_TO_INT:
      case FB_ENC_VUMETER:{
        FillFrameWithEncoderData(fbUpdateQueueIndex);
        SendDataIfReady();
        // SERIALPRINTLN("Encoder feedback update");
      }break;
      case FB_DIGITAL:
      case FB_DIG_VAL_TO_INT:{
        FillFrameWithDigitalData(fbUpdateQueueIndex);
        SendDataIfReady();
        // SERIALPRINTLN("Digital feedback update");
      }break;
      case FB_ANALOG:{
        
      }break;
      case FB_INDEPENDENT:{
        
      }break;
      case FB_BANK_CHANGED:{
        // A bank change consists of several burst of data:
        // First we update the encoders and encoder switches
        // Then the DIGITAL 1 port 
        // Then, if necessary, the DIGITAL 2 port
        // 9ms para cambiar el banco - 32 encoders, 0 dig, 0 analog - 16/7/2009
        updatingBankFeedback = true;

        // Update all rotary encoders
        for(uint8_t n = 0; n < nEncoders; n++){
          // If it's configured as vumeter encoder, send vumeter feedback first and then value indicator
          if(encoder[n].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc){
            SetChangeEncoderFeedback(FB_ENC_VUMETER, n, encFbData[currentBank][n].vumeterValue, 
                                                        encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);   // HARDCODE: N° of encoders in module / is   
            SetChangeEncoderFeedback(FB_ENC_2CC, n, encoderHw.GetEncoderValue(n), 
                                                encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);   // HARDCODE: N° of encoders in module / is 
          }else{
            SetChangeEncoderFeedback(FB_ENCODER, n, encoderHw.GetEncoderValue(n), 
                                                    encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);   // HARDCODE: N° of encoders in module / is 
            // If it's a double CC encoder, update second CC after first
            if(encoder[n].switchConfig.mode == switchModes::switch_mode_2cc){
              SetChangeEncoderFeedback(FB_ENC_2CC, n, encoderHw.GetEncoderValue2(n), 
                                                  encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);   // HARDCODE: N° of encoders in module / is                                                   
            }            
          }
        }
        // Update all encoder switches that aren't shifters
        for(uint8_t n = 0; n < nEncoders; n++){
          bool isShifter = false;
          // Is it a shifter?
          if(config->banks.count > 1){
            for(int bank = 0; bank < config->banks.count; bank++){
              byte bankShifterIndex = config->banks.shifterId[bank];
              if(GetHardwareID(ytxIOBLOCK::Encoder, n) == bankShifterIndex){
                isShifter = true;
              }
            }
          }
            
          if(!isShifter)
            SetChangeEncoderFeedback(FB_ENC_SWITCH, n, encoderHw.GetEncoderSwitchValue(n), 
                                                           encoderHw.GetModuleOrientation(n/4), NO_SHIFTER, BANK_UPDATE);  // HARDCODE: N° of encoders in module                                                
        }
        SetBankChangeFeedback(FB_BANK_DIGITAL1);
        // feedbackHw.SendCommand(BURST_END);
        // sendingFbData = false;
      }break;  
      case FB_BANK_DIGITAL1:{
        // Update all digitals that aren't shifters
        if(amountOfDigitalInConfig[DIGITAL_PORT_2] > 0){   // If there are digitals on the second port
          for(uint16_t n = 0; n < amountOfDigitalInConfig[DIGITAL_PORT_1]; n++){  
            bool isShifter = false;
            // Is it a shifter?
            if(config->banks.count > 1){
              for(int bank = 0; bank < config->banks.count; bank++){
                byte bankShifterIndex = config->banks.shifterId[bank];
                if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex){
                  isShifter = true;
                }
              }
            }

            if(!isShifter) {
              SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
            }
          }
          SetBankChangeFeedback(FB_BANK_DIGITAL2);
        }else{
          for(uint16_t n = 0; n < nDigitals; n++){
            bool isShifter = false;

            // Is it a shifter?
            if(config->banks.count > 1){
              for(int bank = 0; bank < config->banks.count; bank++){
                byte bankShifterIndex = config->banks.shifterId[bank];
                if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex){
                  isShifter = true;
                }
              }
            }

            if(!isShifter) {
              SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
            }
          }
          if(!nDigitals) SendDataIfReady();

          // Set shifters feedback
          SetShifterFeedback();
        }
        // if(bankUpdateFirstTime){
        //   // SetBankChangeFeedback(FB_BANK_CHANGED);        // Double update banks
        //   bankUpdateFirstTime = false;
        // }
        updatingBankFeedback = false;
      }break;
      case FB_BANK_DIGITAL2:{  
        for(uint16_t n = amountOfDigitalInConfig[DIGITAL_PORT_1]; n < nDigitals; n++){
          bool isShifter = false;
          if(config->banks.count > 1){
            for(int bank = 0; bank < config->banks.count; bank++){
              byte bankShifterIndex = config->banks.shifterId[bank];
              if(GetHardwareID(ytxIOBLOCK::Digital, n) == bankShifterIndex){
                isShifter = true;
              }
            }
          }

          if(!isShifter) {
            SetChangeDigitalFeedback(n, digitalHw.GetDigitalValue(n), digitalHw.GetDigitalState(n), NO_SHIFTER, BANK_UPDATE);
          }
        }
        
        SetShifterFeedback();

        updatingBankFeedback = false;
      }break;
      default: break;
    }
    
    fbMessagesSent++;
  }

  Serial.write(BURST_END);    // Signal end of burst
  sendingFbData = false;      // Flag end of data send
  
    // if(fbMessagesSent == MSG_BUFFER_AUX){
    //                  // SEND BANK END if burst mode was enabled
       
    //   SERIALPRINT(fbMessagesSent); SERIALPRINTLN(" END B");

    // }
    // else if((feedbackUpdateWriteIdx == feedbackUpdateReadIdx)){
    //        // SEND BANK END if burst mode was enabled
    //   SERIALPRINT(fbMessagesSent); SERIALPRINTLN(" END A");
    //   // SERIALPRINTLN("BURST_END");
    // }
    
  fbMessagesSent = 0;         // Restart message counter

}

void FeedbackClass::SetShifterFeedback(){
  if(config->banks.count > 1){    // If there is more than one bank
    for(int bank = 0; bank < config->banks.count; bank++){
      byte bankShifterIndex = config->banks.shifterId[bank];

      if(currentBank == bank){
        if(bankShifterIndex < config->inputs.encoderCount){              
          SetChangeEncoderFeedback(FB_ENC_SWITCH, bankShifterIndex, true, encoderHw.GetModuleOrientation(bankShifterIndex/4), IS_SHIFTER, NO_BANK_UPDATE);  // HARDCODE: N° of encoders in module     
        }else{
          // DIGITAL
          SetChangeDigitalFeedback(bankShifterIndex - (config->inputs.encoderCount), true, true, IS_SHIFTER, NO_BANK_UPDATE);
        }
      }else{
        if(bankShifterIndex < config->inputs.encoderCount){
//              SERIALPRINTLN(F("FB SWITCH BANK OFF"));
          SetChangeEncoderFeedback(FB_ENC_SWITCH, bankShifterIndex, false, encoderHw.GetModuleOrientation(bankShifterIndex/4), IS_SHIFTER, NO_BANK_UPDATE);  // HARDCODE: N° of encoders in module     
        }else{
          // DIGITAL
          SetChangeDigitalFeedback(bankShifterIndex - (config->inputs.encoderCount), false, false, IS_SHIFTER, NO_BANK_UPDATE);
        }
      }
    }
  }
}

void FeedbackClass::FillFrameWithEncoderData(byte updateIndex){
  // FIX FOR SHIFT ROTARY ACTION
  uint8_t colorR = 0, colorG = 0, colorB = 0;
  uint8_t colorIndex = 0;
  uint8_t ringStateIndex= false;
  bool encoderSwitchChanged = false;
  bool isRotaryShifted = false;
  bool isFb2cc = false;
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;
  bool onCenterValue = false;
  
  uint8_t indexChanged = feedbackUpdateBuffer[updateIndex].indexChanged;
  uint8_t newOrientation = feedbackUpdateBuffer[updateIndex].newOrientation;
  uint16_t newValue = feedbackUpdateBuffer[updateIndex].newValue;
  uint8_t fbUpdateType = feedbackUpdateBuffer[updateIndex].type;
  bool isShifter = feedbackUpdateBuffer[updateIndex].isShifter;
  bool bankUpdate = feedbackUpdateBuffer[updateIndex].updatingBank;
  bool rotaryValueToColor = feedbackUpdateBuffer[updateIndex].rotaryValueToColor;
  bool valueToIntensity = feedbackUpdateBuffer[updateIndex].valueToIntensity;

  // Get state for alternate switch functions
  isRotaryShifted = encoderHw.IsShiftActionOn(indexChanged);
  isFb2cc = (fbUpdateType == FB_ENC_2CC);
  
  // Get config info for this encoder
  if(fbUpdateType == FB_ENC_SWITCH || isRotaryShifted || isFb2cc){ // If encoder is shifted
    minValue = encoder[indexChanged].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[indexChanged].switchConfig.parameter[switch_minValue_LSB];
    maxValue = encoder[indexChanged].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[indexChanged].switchConfig.parameter[switch_maxValue_LSB];
    msgType = encoder[indexChanged].switchConfig.message;
  }else{
    minValue = encoder[indexChanged].rotaryConfig.parameter[rotary_minMSB]<<7 | encoder[indexChanged].rotaryConfig.parameter[rotary_minLSB];
    maxValue = encoder[indexChanged].rotaryConfig.parameter[rotary_maxMSB]<<7 | encoder[indexChanged].rotaryConfig.parameter[rotary_maxLSB];
    msgType = encoder[indexChanged].rotaryConfig.message;
  }
  
  // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else if(fbUpdateType == FB_ENCODER && msgType == rotary_msg_key){
    minValue = 0;
    maxValue = S_SPOT_SIZE;
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }

  bool invert = false;
  uint16_t lowerValue = minValue;
  uint16_t higherValue = maxValue;
  if(minValue > maxValue){
    invert = true;
    lowerValue = maxValue;
    higherValue = minValue;
  }

  uint8_t rotaryMode = encoder[indexChanged].rotaryFeedback.mode;
  
  if((fbUpdateType == FB_ENC_VUMETER && encoder[indexChanged].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc) ||
     (fbUpdateType == FB_ENCODER && msgType == rotaryMessageTypes::rotary_msg_key)){
    rotaryMode = encoderRotaryFeedbackMode::fb_spot;
  }

  if(fbUpdateType == FB_ENCODER){
    if(!rotaryValueToColor && !valueToIntensity){
      switch(rotaryMode){
        case encoderRotaryFeedbackMode::fb_spot: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/S_SPOT_SIZE;
          if(fbStep){
            for(int step = 0; step < S_SPOT_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (S_SPOT_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : S_SPOT_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, S_SPOT_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, S_SPOT_SIZE-1, 0);
          }  
                                                                           
          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&simpleSpot[newOrientation][ringStateIndex]);
        }
        break;
        case encoderRotaryFeedbackMode::fb_fill: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/FILL_SIZE;
          if(fbStep){
            for(int step = 0; step < FILL_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (FILL_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : FILL_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, FILL_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, FILL_SIZE-1, 0);
          }

          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&fill[newOrientation][ringStateIndex]);
        }
        break;
        case encoderRotaryFeedbackMode::fb_pivot: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/PIVOT_SIZE;
          if(fbStep){
            for(int step = 0; step < PIVOT_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (PIVOT_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : PIVOT_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, PIVOT_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, PIVOT_SIZE-1, 0);
          }  
          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&pivot[newOrientation][ringStateIndex]);

          uint16_t centerValue = 0;
          if((minValue+maxValue)%2)   centerValue = (minValue+maxValue+1)/2;
          else                        centerValue = (minValue+maxValue)/2;
          if(newValue == centerValue) onCenterValue = true;    // Flag center value to change color
        }
        break;
        case encoderRotaryFeedbackMode::fb_mirror: {
          float fbStep = abs(maxValue-minValue);
          fbStep =  fbStep/MIRROR_SIZE;
          if(fbStep){
            for(int step = 0; step < MIRROR_SIZE-1; step++){
              if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
                ringStateIndex = invert ? (MIRROR_SIZE-1 - step) : step;
              }else if(newValue > lowerValue + (step+1)*fbStep){
                ringStateIndex = invert ? 0 : MIRROR_SIZE-1;
              }
            }
          }else{
            if(!invert)
              ringStateIndex = mapl(newValue, lowerValue, higherValue, 0, MIRROR_SIZE-1);
            else
              ringStateIndex = mapl(newValue, lowerValue, higherValue, MIRROR_SIZE-1, 0);
          }
          encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
          encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&spread[newOrientation][ringStateIndex]);
        }
        break;
        default: break;
      }
    }
    // FEATURE NEXT STATE SHOW ON EACH ENCODER CHANGE
    // if(encFbData[currentBank][indexChanged].encRingStatePrev == encFbData[currentBank][indexChanged].encRingState){
    //   encFbData[currentBank][indexChanged].nextStateOn = true;
    //   encFbData[currentBank][indexChanged].millisStateUpdate = millis();
    // }

    // If encoder isn't shifted, use rotary feedback data to get color, otherwise use switch feedback data
    if(!isRotaryShifted){ 
      if(onCenterValue){
        colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
      }else{
        // encoder[indexChanged].rotaryFeedback.rotaryValueToColor = true;
        if(encoder[indexChanged].rotaryFeedback.rotaryValueToColor){
          // SERIALPRINTLN("FB 1");
          if(rotaryValueToColor){
            if(newValue <= 127)       // Safe guard
              encFbData[currentBank][indexChanged].colorIndexRotary = newValue;
          }
          
          colorIndex = encFbData[currentBank][indexChanged].colorIndexRotary;

          colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
          colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
          colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
        }else{
          colorR = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
          colorG = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
          colorB = pgm_read_byte(&gamma8[encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
        }
      }
    }else{
      if(onCenterValue){
        colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].switchFeedback.color[B_INDEX]]);
      }else{
        colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]]);
      }
    }
      
  }else if (fbUpdateType == FB_ENC_2CC){  // Feedback for double CC and for vumeter indicator
    uint16_t fbStep = abs(maxValue-minValue);
    fbStep =  fbStep/S_SPOT_SIZE;
    for(int step = 0; step < S_SPOT_SIZE-1; step++){
      if((newValue >= lowerValue + step*fbStep) && (newValue <= lowerValue + (step+1)*fbStep)){
        ringStateIndex = invert ? (FILL_SIZE-1 - step) : step;
      }else if(newValue > lowerValue + (step+1)*fbStep){
        ringStateIndex = invert ? 0 : FILL_SIZE-1;
      }
    }           
    encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
    if(invert)  encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&simpleSpotInv[newOrientation][ringStateIndex]);
    else        encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&simpleSpot[newOrientation][ringStateIndex]);
    
    if(encoder[indexChanged].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc){
      colorR = pgm_read_byte(&gamma8[0xF0]);  // whte for vumeter overlay indicator
      colorG = pgm_read_byte(&gamma8[0xF0]);
      colorB = pgm_read_byte(&gamma8[0xF0]);
    }else if(newValue == feedbackUpdateBuffer[(updateIndex != 0) ? (updateIndex-1) : (FEEDBACK_UPDATE_BUFFER_SIZE-1)].newValue){    // Ring buffer, catch the event where the two 2cc messages are first and last
      colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
      colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
      colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
    }else{
      colorR = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[R_INDEX]]);
      colorG = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[G_INDEX]]);
      colorB = pgm_read_byte(&gamma8[255-encoder[indexChanged].rotaryFeedback.color[B_INDEX]]);
    }
    
  }else if (fbUpdateType == FB_ENC_VUMETER){  // Feedback type for vumeter visualization on encoder ring
    ringStateIndex = mapl(newValue, 
                          minValue,           // might need to be hardcoded 0 
                          maxValue,           // might need to be hardcoded 127
                          0, 
                          FILL_SIZE - 1);
                                                    
    encFbData[currentBank][indexChanged].encRingState &= newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON;
    encFbData[currentBank][indexChanged].encRingState |= pgm_read_word(&fill[newOrientation][ringStateIndex]);
    
    colorR = 0;  // Color for this feedback type is on SAMD11
    colorG = 0;
    colorB = 0;
    
  }else if (fbUpdateType == FB_ENC_SWITCH) {  // Feedback for encoder switch  
    bool switchState    = (newValue == minValue) ? 0 : 1 ;   // if new value is minValue, state of LED is OFF, otherwise is ON
    bool is2cc          = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_2cc);
    bool isFineAdj      = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_fine);
    bool isQSTB         = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_quick_shift) || 
                          (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_quick_shift_note);
    bool isShiftRotary  = (encoder[indexChanged].switchConfig.mode == switchModes::switch_mode_shift_rot);
    bool encoderSwitchState = encoderHw.GetEncoderSwitchState(indexChanged);
    // encoder[indexChanged].switchFeedback.lowIntensityOff = true;
    bool lowI = encoder[indexChanged].switchFeedback.lowIntensityOff;

    if((is2cc || isQSTB || isFineAdj || isShiftRotary) && !isShifter){    // Any encoder special function has a white ON state
      if(encoderSwitchState)  encFbData[currentBank][indexChanged].encRingState |=  (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
      else                    encFbData[currentBank][indexChanged].encRingState &= ~(newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);

      valueToIntensity = false; // If a special feature is configured, 

      // SERIALPRINT(F("ENCODER SWITCH ")); SERIALPRINT(indexChanged); SERIALPRINT(F(" - STATE: ")); SERIALPRINTLN(encoderSwitchState);
      // White colour for switch special functions
      colorR = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      colorG = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      colorB = pgm_read_byte(&gamma8[encoderSwitchState ? 220 : 0]);
      encoderSwitchChanged = true;
    }else if(encoder[indexChanged].switchFeedback.valueToColor && !isShifter){     // If color range is configured, get color from value
      encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
      colorIndex = newValue;

      if(!valueToIntensity && (newValue <= sizeof(colorRangeTable)))       // Safe guard
        colorIndex = newValue;
      else if(valueToIntensity)
        colorIndex = encFbData[currentBank][indexChanged].colorIndexSwitch;

      if(colorIndex != encFbData[currentBank][indexChanged].colorIndexSwitch || bankUpdate || valueToIntensity){
        encoderSwitchChanged = true;
        if (!valueToIntensity) encFbData[currentBank][indexChanged].colorIndexSwitch = colorIndex;
        
        colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
        colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
        colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
      }
    }else{   // No color range, no special feature, might be normal encoder switch or shifter button
      if(encoderSwitchState || (newValue == maxValue) || (newValue && isShifter) || valueToIntensity){      // ON
        encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
        colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]]);
        colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]]);
        colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]]);    
      }else if((newValue == minValue && !valueToIntensity) || (isShifter && !newValue)){    // SHIFTER, OFF
        encFbData[currentBank][indexChanged].encRingState |= (newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);    // If it's a bank shifter, switch LED's are on
        if(lowI || isShifter){
          if(IsPowerConnected()){    // POWER SUPPLY CONNECTED
            colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
            colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
            colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);  
          }else{                     // NO POWER SUPPLY 
            colorR = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
            colorG = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
            colorB = pgm_read_byte(&gamma8[encoder[indexChanged].switchFeedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          }
        
        }else{    
          encFbData[currentBank][indexChanged].encRingState &= ~(newOrientation ? ENCODER_SWITCH_V_ON : ENCODER_SWITCH_H_ON);
          colorR = 0;
          colorG = 0;
          colorB = 0;
        } 
      }else{
        return;  // If value != MIN and != MAX, do nothing, do not send feedback message. Skips feedbackUpdateBuffer entry.
      }
      encoderSwitchChanged = true;
    }
  }

  if (fbUpdateType == FB_ENCODER 
        || fbUpdateType == FB_ENC_2CC
        || fbUpdateType == FB_ENC_VUMETER
        || updatingBankFeedback 
        || encoderSwitchChanged) {
    encFbData[currentBank][indexChanged].encRingStatePrev = encFbData[currentBank][indexChanged].encRingState;    // not being used
    
    sendSerialBufferDec[d_frameType] = (fbUpdateType == FB_ENCODER      ? ENCODER_CHANGE_FRAME        :
                                        fbUpdateType == FB_ENC_2CC      ? ENCODER_DOUBLE_FRAME        : 
                                        fbUpdateType == FB_ENC_VUMETER  ? ENCODER_VUMETER_FRAME       : 
                                        fbUpdateType == FB_ENC_SWITCH   ? ENCODER_SWITCH_CHANGE_FRAME : 255);   
    
    // value to intensity feature
    uint8_t intensityFactor = MAX_INTENSITY;

    if(fbUpdateType == FB_ENCODER && encoder[indexChanged].rotaryFeedback.valueToIntensity){
      intensityFactor = encFbData[currentBank][indexChanged].rotIntensityFactor;
    }else if(fbUpdateType == FB_ENC_SWITCH &&  encoder[indexChanged].switchFeedback.valueToIntensity){
      intensityFactor = encFbData[currentBank][indexChanged].swIntensityFactor;
    }

    if(valueToIntensity){   // if current feedback update is Value To Intensity, store value
      intensityFactor = mapl(newValue, minValue, maxValue, MIN_INTENSITY, MAX_INTENSITY);
      if(fbUpdateType == FB_ENCODER){
        encFbData[currentBank][indexChanged].rotIntensityFactor = intensityFactor;
      }else if(fbUpdateType == FB_ENC_SWITCH && encoderHw.GetEncoderSwitchState(indexChanged)){
        encFbData[currentBank][indexChanged].swIntensityFactor = intensityFactor;
      }else if(fbUpdateType == FB_ENC_SWITCH && !encoderHw.GetEncoderSwitchState(indexChanged)){
        return;
      }
    }
    
    uint8_t brightness = encoderHw.GetEncoderBrightness(indexChanged);

    sendSerialBufferDec[d_nRing] = indexChanged;
    sendSerialBufferDec[d_orientation] = newOrientation;
    sendSerialBufferDec[d_ringStateH] = encFbData[currentBank][indexChanged].encRingState >> 8;
    sendSerialBufferDec[d_ringStateL] = encFbData[currentBank][indexChanged].encRingState & 0xff;
    sendSerialBufferDec[d_R] = colorR*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    sendSerialBufferDec[d_G] = colorG*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    sendSerialBufferDec[d_B] = colorB*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
    sendSerialBufferDec[d_ENDOFFRAME] = END_OF_FRAME_BYTE;
    feedbackDataToSend = true;
  }
}

void FeedbackClass::FillFrameWithDigitalData(byte updateIndex){
  uint8_t colorR = 0, colorG = 0, colorB = 0;
  bool colorIndexChanged = false;
  uint8_t colorIndex = 0; 
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;

  uint8_t indexChanged = feedbackUpdateBuffer[updateIndex].indexChanged;
  uint16_t newValue = feedbackUpdateBuffer[updateIndex].newValue;
  uint8_t fbUpdateType = feedbackUpdateBuffer[updateIndex].type;
  bool isShifter = feedbackUpdateBuffer[updateIndex].isShifter;
  bool newState = feedbackUpdateBuffer[updateIndex].newOrientation;
  bool bankUpdate = feedbackUpdateBuffer[updateIndex].updatingBank;
  bool lowI = digital[indexChanged].feedback.lowIntensityOff;
  bool valueToIntensity = feedbackUpdateBuffer[updateIndex].valueToIntensity;
  bool invert = false;
  
  minValue = digital[indexChanged].actionConfig.parameter[digital_minMSB] << 7 |
                      digital[indexChanged].actionConfig.parameter[digital_minLSB];
  maxValue = digital[indexChanged].actionConfig.parameter[digital_maxMSB] << 7 |
                      digital[indexChanged].actionConfig.parameter[digital_maxLSB];
  msgType = digital[indexChanged].actionConfig.message;

  if(IS_DIGITAL_FB_14_BIT(indexChanged)){
    is14bits = true;      
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
    newValue = newValue & 0x7F;
  }

  if(minValue > maxValue){
    invert = true;
  }
  
  //digital config is valueToColor
  if(digital[indexChanged].feedback.valueToColor && !isShifter){
    
    //feedback action is valueToIntensity
    if(valueToIntensity){
      colorIndex = digFbData[currentBank][indexChanged].colorIndexPrev;
    }else{
      //feedback action isnt valueToIntensity
      if(newValue <= sizeof(colorRangeTable)){
        colorIndex = newValue;
        
        if(newValue>0)
          digFbData[currentBank][indexChanged].colorIndexPrev = colorIndex;
      }
    }

    colorR = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][R_INDEX])]);
    colorG = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][G_INDEX])]);
    colorB = pgm_read_byte(&gamma8[pgm_read_byte(&colorRangeTable[colorIndex][B_INDEX])]);
  }else{     
    // FIXED COLOR
    if(newValue == maxValue || (newValue && isShifter) || valueToIntensity){
      colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]]);
      colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]]);
      colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]]); 
      if (invert) newValue = minValue;
    }else if((newValue == minValue && !valueToIntensity) || (isShifter && !newValue)){
      if(lowI || isShifter){
        if(IsPowerConnected()){
          colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
          colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);
          colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WP]);  
        }else{
          colorR = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[R_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          colorG = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[G_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
          colorB = pgm_read_byte(&gamma8[digital[indexChanged].feedback.color[B_INDEX]*BANK_OFF_BRIGHTNESS_FACTOR_WOP]);
        }    
      }else{
        colorR = 0;
        colorG = 0;
        colorB = 0;  
      } 
      if (invert) newValue = maxValue;
    }else{
      return;  // If value != MIN and != MAX, do nothing, do not send feedback message. Skips feedbackUpdateBuffer entry.
    }
  }
  
   // value to intensity feature
  uint8_t intensityFactor = MAX_INTENSITY;

  if(digital[indexChanged].feedback.valueToIntensity){
    intensityFactor = digFbData[currentBank][indexChanged].digIntensityFactor;
  }

  if(valueToIntensity){   // if current feedback update is Value To Intensity, store value
    intensityFactor = mapl(newValue, minValue, maxValue, MIN_INTENSITY, MAX_INTENSITY);
    if(digitalHw.GetDigitalState(indexChanged)){
      // SERIALPRINTLN("Dig value to intensity updated");
      digFbData[currentBank][indexChanged].digIntensityFactor = intensityFactor;
    }else if(!digitalHw.GetDigitalState(indexChanged)){
      // SERIALPRINTLN("Dig value to intensity not updated because OFF");
      return;
    }
  }

  uint8_t brightness = digitalHw.GetDigitalButtonBrightness(indexChanged);

  //sendSerialBufferDec[msgLength] = TX_BYTES;   // INIT SERIAL FRAME WITH CONSTANT DATA
  sendSerialBufferDec[d_frameType] = (indexChanged < amountOfDigitalInConfig[0]) ?  DIGITAL1_CHANGE_FRAME : 
                                                                                    DIGITAL2_CHANGE_FRAME;   
  sendSerialBufferDec[d_nDig] = indexChanged;
  sendSerialBufferDec[d_orientation] = 0;
  sendSerialBufferDec[d_digitalState] = (isShifter || newValue || lowI || valueToIntensity) ? 1 : 0;
  sendSerialBufferDec[d_ringStateL] = 0;
  sendSerialBufferDec[d_R] = colorR*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  sendSerialBufferDec[d_G] = colorG*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  sendSerialBufferDec[d_B] = colorB*brightness/MAX_INTENSITY*intensityFactor/MAX_INTENSITY;
  sendSerialBufferDec[d_ENDOFFRAME] = END_OF_FRAME_BYTE;
  feedbackDataToSend = true;
}

FeedbackClass::encFeedbackData* FeedbackClass::GetCurrentEncoderFeedbackData(uint8_t bank, uint8_t encNo){
  if(begun){
    return &encFbData[bank][encNo];
  }else{
    return NULL;
  }
}

FeedbackClass::digFeedbackData* FeedbackClass::GetCurrentDigitalFeedbackData(uint8_t bank, uint8_t digNo){
  if(begun){
    return &digFbData[bank][digNo];
  }else{
    return NULL;
  }
}

uint8_t FeedbackClass::GetVumeterValue(uint8_t encNo){
  if(encNo < nEncoders)
    return encFbData[currentBank][encNo].vumeterValue;
}

void FeedbackClass::SetChangeEncoderFeedback(uint8_t type, uint8_t encIndex, uint16_t val, uint8_t encoderOrientation, 
                                              bool isShifter, bool bankUpdate, bool encoderColorChangeMsg, bool valToIntensity, bool externalFeedback) {
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].type                         = type;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].indexChanged                 = encIndex;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].newValue                     = val;
  if(type == FB_ENC_VUMETER)  encFbData[currentBank][encIndex].vumeterValue = val;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].newOrientation               = encoderOrientation;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].isShifter                    = isShifter;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].updatingBank                 = bankUpdate;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].rotaryValueToColor           = encoderColorChangeMsg;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].valueToIntensity             = valToIntensity;

  // If feedback is from an external source (not a switch pushed or an encoder moved) wait for more data before sending
  WaitForMIDI(externalFeedback);

  IncreaseBufferIndex(WRITE_INDEX);
}

void FeedbackClass::SetChangeDigitalFeedback(uint16_t digitalIndex, uint16_t updateValue, bool hwState, 
                                              bool isShifter, bool bankUpdate, bool externalFeedback, bool valToIntensity){
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].type               = FB_DIGITAL;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].indexChanged       = digitalIndex;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].newValue           = updateValue;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].newOrientation     = hwState;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].isShifter          = isShifter;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].updatingBank       = bankUpdate;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].valueToIntensity   = valToIntensity;
  
  WaitForMIDI(externalFeedback);

  IncreaseBufferIndex(WRITE_INDEX);

}

void FeedbackClass::SetChangeIndependentFeedback(uint8_t type, uint16_t fbIndex, uint16_t val, bool bankUpdate, bool externalFeedback){
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].type         = type;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].indexChanged = fbIndex;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].newValue     = val;
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].updatingBank = bankUpdate;

  WaitForMIDI(externalFeedback);

  IncreaseBufferIndex(WRITE_INDEX);
  // SERIALPRINTLN(fbItemsToSend);
}

void FeedbackClass::SetBankChangeFeedback(uint8_t type){
  feedbackUpdateBuffer[feedbackUpdateWriteIdx].type = type;
  
  IncreaseBufferIndex(WRITE_INDEX);
  // SERIALPRINTLN(fbItemsToSend);
}

void FeedbackClass::IncreaseBufferIndex(bool indexType){
  if(indexType == READ_INDEX){
    if(++feedbackUpdateReadIdx >= FEEDBACK_UPDATE_BUFFER_SIZE)
      feedbackUpdateReadIdx = 0;
    fbItemsToSend--;
    // SERIALPRINT("R IDX: ");
    // SERIALPRINTLN(fbItemsToSend);
  }else if(indexType == WRITE_INDEX){
    if(++feedbackUpdateWriteIdx >= FEEDBACK_UPDATE_BUFFER_SIZE)
      feedbackUpdateWriteIdx = 0;
    fbItemsToSend++;  
    // SERIALPRINT("W IDX: ");
    // SERIALPRINTLN(fbItemsToSend);
  }
  
// SERIALPRINTLN(fbItemsToSend);

}

void FeedbackClass::WaitForMIDI(bool externalFeedback){
  if(externalFeedback){
    antMillisWaitMoreData = millis();
    waitingMoreData = true;
  }
}

bool FeedbackClass::SendingData(void){
  return sendingFbData;
}

void FeedbackClass::SendDataIfReady(){
  
  SendFeedbackData(); 
  feedbackDataToSend = false;

}

void FeedbackClass::AddCheckSum(){ 
  uint16_t sum = 2019 - checkSum(sendSerialBufferEnc, e_B+1);
  
  sum &= 0x3FFF;    // 14 bit checksum
  
  sendSerialBufferEnc[e_checkSum_MSB] = (sum >> 7) & 0x7F;
  sendSerialBufferEnc[e_checkSum_LSB] = sum & 0x7F;
}

// #define DEBUG_FB_FRAME
void FeedbackClass::SendFeedbackData(){
  uint8_t tries = 0;
  bool okToContinue = false;
  uint8_t cmd = 0;
  static uint32_t ackNotReceivedCount = 0;
  uint8_t encodedFrameSize = encodeSysEx(sendSerialBufferDec, sendSerialBufferEnc, d_ENDOFFRAME);
  
  // Adds checksum bytes to encoded frame
  AddCheckSum();

  #ifdef DEBUG_FB_FRAME
    SERIALPRINT(F("FRAME WITHOUT ENCODING:\n"));
    for(int i = 0; i <= d_B; i++){
      SERIALPRINT(i); SERIALPRINT(F(": "));SERIALPRINT(sendSerialBufferDec[i]); SERIALPRINT(F("\t"));
    } 
    SERIALPRINTLN();
  #endif
  
  do{
    if(!fbShowInProgress){
      waitingForAck = true;

      cmd = 0;
      Serial.write(NEW_FRAME_BYTE);             // SEND FRAME HEADER

      Serial.write(e_ENDOFFRAME+1);             // NEW FRAME SIZE - SIZE FOR ENCODED FRAME

      for (int i = 0; i < e_ENDOFFRAME; i++) {
        Serial.write(sendSerialBufferEnc[i]);   // FRAME BODY
      }
      Serial.write(END_OF_FRAME_BYTE);          // SEND END OF FRAME BYTE
      
      Serial.flush();    
      
      
      
      antMicrosAck = micros();

      // Wait for fb microcontroller to acknowledge message reception, or try again
      while(waitingForAck && ((micros() - antMicrosAck) < 300));      

      if(!waitingForAck) 
        okToContinue = true;
      else{
        tries++;
        // SERIALPRINT(micros() - antMicrosAck);
        // SERIALPRINTLN(" micros");  // SERIALPRINT(++ackNotReceivedCount); SERIALPRINT(" times");                  
        // SERIALPRINT("\t");                                  SERIALPRINT(sendSerialBufferDec[d_frameType]);
        // SERIALPRINT(", #");                                 SERIALPRINT(sendSerialBufferDec[d_nRing]);
        // SERIALPRINT("\t read idx: ");                       SERIALPRINT(feedbackUpdateReadIdx);
        // SERIALPRINT("\t write idx: ");                      SERIALPRINTLN(feedbackUpdateWriteIdx);
      }               
    }else{
      //delayMicros
    }
  }while(!okToContinue && tries < 20);

}

void FeedbackClass::SendCommand(uint8_t cmd){
//  SERIALPRINT(F("Command sent: "));
//  SERIALPRINTLNF(cmd, HEX);
  Serial.write(cmd);
  Serial.flush();
}
void FeedbackClass::SendResetToBootloader(){
  
  Serial.flush();
}

void * FeedbackClass::GetEncoderFBPtr(){
  return (void*) encFbData[currentBank];
}
      
