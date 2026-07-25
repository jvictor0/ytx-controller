/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2021 - Yaeltex

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

#include "headers/EncoderInputs.h"

//----------------------------------------------------------------------------------------------------
// ENCODER FUNCTIONS
//----------------------------------------------------------------------------------------------------
void EncoderInputs::Init(uint8_t maxBanks, uint8_t numberOfEncoders, SPIAdressableBUS *spiBUS){
  nBanks = 0;
  nEncoders = 0;
  nModules = 0;    
  
  if(!maxBanks || !numberOfEncoders  || (numberOfEncoders > MAX_ENCODER_AMOUNT)) return;    // If number of encoders is zero, return;

  // CHECK WHETHER AMOUNT OF DIGITAL INPUTS IN MODULES COMBINED MATCH THE AMOUNT OF DIGITAL INPUTS IN CONFIG
  // AMOUNT OF DIGITAL MODULES
  uint8_t encodersInConfig = 0;
  
  for (int nMod = 0; nMod < MAX_ENCODER_MODS; nMod++) {
    uint8_t moduleType = GetModuleType(nMod);

    if (moduleType) {
      modulesInConfig.encoders++;

      switch (moduleType) {
        case EncoderModuleTypes::E41H:
        case EncoderModuleTypes::E41V:
        case EncoderModuleTypes::E41H_D:
        case EncoderModuleTypes::E41V_D:{
            encodersInConfig += defE41module.components.nEncoders;
          } break;
        default: break;
      }
    }
  }
  
  if (encodersInConfig != numberOfEncoders) {
    SERIALPRINTLN(F("Error in config: Number of encoders does not match modules in config"));
    SERIALPRINT(F("nEncoders: ")); SERIALPRINTLN(numberOfEncoders);
    SERIALPRINT(F("Modules: ")); SERIALPRINTLN(encodersInConfig);
    return;
  } else {
    // SERIALPRINTLN(F("nEncoders and module config match"));
  }

  nBanks = maxBanks;
  nEncoders = numberOfEncoders;
  nModules = nEncoders/4;     // HARDCODE: N° of encoders in module

//  SERIALPRINT(F("N ENCODERS MODULES: "));
//  SERIALPRINTLN(nModules);
  
  // First dimension is an array of pointers (banks), each pointing to N encoder structs - https://www.eskimo.com/~scs/cclass/int/sx9b.html
  eBankData = (encoderBankData**) memHost->AllocateRAM(nBanks*sizeof(encoderBankData*));
  encMData = (moduleData*) memHost->AllocateRAM(nModules*sizeof(moduleData));

 // SERIALPRINT(F("Size of encoder data "));
 // SERIALPRINTLN(sizeof(encoderData));

  // Reset to bootloader if there isn't enough RAM to fit this data
  if(FreeMemory() < ( nBanks*nEncoders*sizeof(encoderBankData) + 
                      nEncoders*sizeof(encoderData) + 800)){
    SERIALPRINTLN("NOT ENOUGH RAM / ENCODERS -> REBOOTING TO BOOTLOADER...");
    
    delay(500);

    SelfReset(RESET_TO_BOOTLOADER);
  }
  
  eHwData = (encoderData*) memHost->AllocateRAM(nEncoders*sizeof(encoderData));

  // Second dimension is an array for each bank
  for (int b = 0; b < nBanks; b++){
    #if defined(DISABLE_ENCODER_BANKS)
      // Allocate only first bank and reference other banks to firs bank
      if(b==0){
        eBankData[b] = (encoderBankData*) memHost->AllocateRAM(nEncoders*sizeof(encoderBankData));
      }
      else{
        eBankData[b] = eBankData[0];
      }
    #else
      // Allocate all banks
      eBankData[b] = (encoderBankData*) memHost->AllocateRAM(nEncoders*sizeof(encoderBankData));
    #endif
  }

  for (int b = 0; b < nBanks; b++){
    // printPointer(eBankData[b]);
    currentBank = memHost->LoadBank(b);
    for(int e = 0; e < nEncoders; e++){
      uint16_t minValue = encoder[e].rotaryConfig.parameter[rotary_minMSB]<<7 | 
                          encoder[e].rotaryConfig.parameter[rotary_minLSB];
      uint16_t maxValue = encoder[e].rotaryConfig.parameter[rotary_maxMSB] << 7 |
                          encoder[e].rotaryConfig.parameter[rotary_maxLSB];
      if(IS_ENCODER_SW_7_BIT(e)){
        minValue &= 0x7F;
        maxValue &= 0x7F;
      }
      uint16_t centerValue = 0;

      // get center value
      if(!(abs(maxValue-minValue)%2)){
        centerValue = (minValue + maxValue)/2;
      }else{
        centerValue = (minValue + maxValue)/2 + 1;
      }
      
      eBankData[b][e].encoderValue          = (encoder[e].rotaryFeedback.mode == encoderRotaryFeedbackMode::fb_pivot) ? centerValue : 0;
      eBankData[b][e].encoderValue2cc       = 0;
      eBankData[b][e].encoderShiftValue     = 0;
      eBankData[b][e].pulseCounter          = 0;
      eBankData[b][e].switchLastValue       = 0;
      eBankData[b][e].switchInputState      = 0;
      eBankData[b][e].switchInputStatePrev  = false;
      eBankData[b][e].shiftRotaryAction     = false;
      eBankData[b][e].encFineAdjust         = false;
      eBankData[b][e].doubleCC              = false;
       // NEW FEATURE: SENSITIVITY CONTROL FOR DIGITAL BUTTONS ///////////////////////////
      eBankData[b][e].buttonSensitivityControlOn = false;
      ///////////////////////////////////////////////////////////////////////////////////
    }
  }
  currentBank = memHost->LoadBank(0);
  for(int e = 0; e < nEncoders; e++){
    eHwData[e].encoderDirection       = 0;
    eHwData[e].encoderDirectionPrev   = 0;
    eHwData[e].encoderValuePrev       = eBankData[0][e].encoderValue;
    eHwData[e].encoderValuePrev2cc    = 0;
    eHwData[e].encoderChange          = false;
    eHwData[e].millisUpdatePrev       = 0;
    eHwData[e].switchHWState          = 0;
    eHwData[e].switchHWStatePrev      = 0;
    eHwData[e].nextJump               = 1;
    eHwData[e].currentSpeed           = 0;
    eHwData[e].encoderState           = RFS_START;
    eHwData[e].debounceSwitchPressed  = 0;
    eHwData[e].clickCount             = 0;
    eHwData[e].changed                = 0;
    eHwData[e].lastSwitchBounce       = millis();
    eHwData[e].a                      = 0;
    eHwData[e].b                      = 0;
    eHwData[e].thisEncoderBank        = 0;
    eHwData[e].bankShifted            = false;
    eHwData[e].statesAcc              = 0;
  }
  // SET PROGRAM CHANGE TO 0 FOR ALL CHANNELS
  for (int c = 0; c < 16; c++) {
    currentProgram[MIDI_USB][c] = 0;
    currentProgram[MIDI_HW][c] = 0;
  }

  // DISABLE HARDWARE ADDRESSING FOR ALL CHIPS - ONLY NEEDED FOR RESET
  spiBUS->DisableHWAddress(MCP23017_BASE_ADDRESS);
  spiBUS->DisableHWAddress(INF_POT_BASE_ADDRESS_A);
  spiBUS->DisableHWAddress(INF_POT_BASE_ADDRESS_B);

  encodersModule = (void**)memHost->AllocateRAM(nModules*sizeof(void**));

  for (int n = 0; n < nModules; n++){ 
    uint8_t moduleType = GetModuleType(n);
    // Set encoder orientations and detent info
    if(moduleType != EncoderModuleTypes::ENCODER_NONE){
      encMData[n].orientation = (moduleType-1)%2;    // 0 -> HORIZONTAL , 1 -> VERTICAL
      encMData[n].detent =  (moduleType == EncoderModuleTypes::E41H_D) ||
                            (moduleType == EncoderModuleTypes::E41V_D);   
    }else{
      SetStatusLED(STATUS_BLINK, 1, STATUS_FB_ERROR);
      return;
    }
    InitRotaryModule(spiBUS,n);
  } 

  begun = true;
  return;
}

uint8_t EncoderInputs::GetModuleType(uint8_t moduleNo){
  if(moduleNo<(config->inputs.encoderCount/4))
    return (moduleNo<8 ? config->hwMapping.encoder[moduleNo] : (config->hwMapping.encoder[moduleNo-8]>>4))&0x0F;
  else
    return 0;
}

void EncoderInputs::InitRotaryModule(SPIAdressableBUS *bus, uint8_t moduleNo){
  uint8_t address;
  uint8_t nextAddress;
  uint8_t moduleType = GetModuleType(moduleNo);
  uint8_t nextModuleType = GetModuleType(moduleNo+1);

  if(moduleNo<4){
    //legacy addresing
    address = (moduleNo % 8);
    nextAddress = (moduleNo+1 % 8);
  }else{
    //extended addressing 
    static uint8_t lastMCPmodule = 4;
    static uint8_t lastYTXmodule = 4;
    
    if(moduleType==EncoderModuleTypes::E41H || moduleType==EncoderModuleTypes::E41V){
      address = lastYTXmodule % 16;
      lastYTXmodule++;
    }else{
      address = lastMCPmodule % 8;
      lastMCPmodule++;
    }

    if(nextModuleType==EncoderModuleTypes::E41H || nextModuleType==EncoderModuleTypes::E41V)
      nextAddress = lastYTXmodule % 16;
    else
      nextAddress = lastMCPmodule % 8;
  }

  encMData[moduleNo].state = 0;
  encMData[moduleNo].statePrev = 0;

  switch(moduleType){
    case EncoderModuleTypes::E41H:
    case EncoderModuleTypes::E41V:{
        SPIEndlessPot *newModule = new SPIEndlessPot;
        newModule->begin(bus, address, nextAddress);

        SPIEndlessPotParameters parameters;
        parameters.sampleInterval = 1000;
        parameters.hysteresis = 50;
        parameters.expFilter = 0.25;
        //SET CONFIG MODULE DATA 
        newModule->configure(&parameters);
        encodersModule[moduleNo] = (void*)newModule;
      } break;
    case EncoderModuleTypes::E41H_D:
    case EncoderModuleTypes::E41V_D:{
        SPIGPIOExpander *newModule = new SPIGPIOExpander;
        newModule->begin(bus, address, nextAddress);
        encodersModule[moduleNo] = (void*)newModule;

        ReadModule(moduleNo);
        for (int e = 0; e < 4; e++){
          uint8_t pinState = 0;
          if (encMData[e].state & (1 << defE41module.encPins[e%(defE41module.components.nEncoders)][0])){
            pinState |= 2; // Get new state for pin A
            eHwData[e].a = 1; // Get new state for pin A
          }else  eHwData[e].a = 0;
          
          if (encMData[e].state & (1 << defE41module.encPins[e%(defE41module.components.nEncoders)][1])){
            pinState |= 1; // Get new state for pin B
            eHwData[e].b = 1; // Get new state for pin B
          }else  eHwData[e].b = 0;
      
          eHwData[e].encoderState = pinState;

          eHwData[e].switchHWState = !(encMData[moduleNo].state & (1 << defE41module.encSwitchPins[e%(defE41module.components.nEncoders)]));
          eHwData[e].switchHWStatePrev = eHwData[e].switchHWState;
        } 
      } break;
    default: break;
  }
}

void EncoderInputs::ReadModule(uint8_t moduleNo){
  uint8_t moduleType = GetModuleType(moduleNo);
  switch(moduleType){
    case EncoderModuleTypes::E41H:
    case EncoderModuleTypes::E41V:{
        encMData[moduleNo].state = ((SPIEndlessPot*)(encodersModule[moduleNo]))->readModule();
      } break;
    case EncoderModuleTypes::E41H_D:
    case EncoderModuleTypes::E41V_D:{
        encMData[moduleNo].state = ((SPIGPIOExpander*)(encodersModule[moduleNo]))->digitalRead(); 
      } break;
    default: break;
  }
}

bool EncoderInputs::ModuleHasActivity(uint8_t moduleNo){
  bool activity = false;
  uint8_t moduleType = GetModuleType(moduleNo);
  switch(moduleType){
    case EncoderModuleTypes::E41H:
    case EncoderModuleTypes::E41V:{
        if(encMData[moduleNo].state&0x00F0)
          activity = true;
      } break;
    case EncoderModuleTypes::E41H_D:
    case EncoderModuleTypes::E41V_D:{
        if( encMData[moduleNo].state != encMData[moduleNo].statePrev){
          activity = true;
        } 
      } break;
    default: break;
  }

  if(activity)
    encMData[moduleNo].statePrev = encMData[moduleNo].state;
  
  return activity;
}

void EncoderInputs::Read(){
  static uint32_t antMillisEncoders=0;

  if(!begun) return;    // If didn't go through INIT, return;

  if(!nBanks || !nEncoders || !nModules) return;    // If number of encoders is zero, return;

  if(!(millis()-antMillisEncoders>0))return; //minimum 1ms beetwen rotary reads

  antMillisEncoders=millis();

  // The priority system adds to a priority list up to 2 modules to read if they are being used.
  // If the list is empty, the first two modules that are detected to have a change are added to the list
  // If the list is not empty, the only modules that are read via SPI are the ones that are on the priority list
  if(priorityCount && (millis()-priorityTime > PRIORITY_ELAPSE_TIME_MS)){
    priorityCount = 0;
//    SERIALPRINT(F("Priority list emptied"));
  }

  if(priorityCount >= 1){
    ReadModule(priorityList[0]);
  }

  if(priorityCount == 2){
    ReadModule(priorityList[1]);
  }else{  
    // READ ALL MODULE'S STATE
    for (int n = 0; n < nModules; n++){  
      if(priorityCount >= 1 && n == priorityList[0]){
        // Skip module if it's already listed as priority module
      }
      else{
        ReadModule(n);
      } 

      #if defined(PRINT_MODULE_STATE_ENC)
      for (int i = 0; i < 16; i++) {
        SERIALPRINT( (encMData[n].state >> (15 - i)) & 0x01, BIN);
        if (i == 9 || i == 6) SERIALPRINT(F(" "));
      }
      SERIALPRINT(F("\t")); 
      #endif
    }
    #if defined(PRINT_MODULE_STATE_ENC)
    SERIALPRINT(F("\n")); 
    #endif
  } 

  uint8_t encNo = 0; 
  uint8_t nEncodInMod = 0;

  for(uint8_t moduleNo = 0; moduleNo < nModules; moduleNo++){
    uint8_t moduleType = GetModuleType(moduleNo);
    switch(moduleType){
      case EncoderModuleTypes::E41H:
      case EncoderModuleTypes::E41V:
      case EncoderModuleTypes::E41H_D:
      case EncoderModuleTypes::E41V_D:{
          nEncodInMod = defE41module.components.nEncoders;              // HARDCODE: CHANGE WHEN DIFFERENT MODULES ARE ADDED
        } break;
      default: break;
    }

    if(ModuleHasActivity(moduleNo)){
      // READ N° OF ENCODERS IN ONE MODULE
      for(int n = 0; n < nEncodInMod; n++){
        if(RotaryHasConfiguration(encNo+n)){
          eHwData[encNo+n].encoderDirection = DecodeRotaryDirection(moduleNo, encNo+n);

          if(eHwData[encNo+n].encoderDirection){
            if(testEncoders){

              SERIALPRINT(F("STATES: "));SERIALPRINT(eHwData[encNo+n].statesAcc);
              SERIALPRINT(F("\t <- ENC "));SERIALPRINT(encNo+n);
              SERIALPRINT(F("\t DIR "));SERIALPRINT(eHwData[encNo+n].encoderDirection);
              SERIALPRINTLN();
              
              eHwData[encNo+n].statesAcc = 0;
            }
            // Check if current module is in read priority list
            AddToPriority(moduleNo); 
            // Execute rotary action
            EncoderAction(moduleNo,encNo+n);
            // Reset direction
            eHwData[encNo+n].encoderDirection = 0; 
          }
        }
      }
    }
    // Switch check occurs every time, not only when module state change, in order to detect simple and double clicks
    for(int n = 0; n < nEncodInMod; n++){  
      SwitchCheck(moduleNo, encNo+n);
    }
    encNo = encNo + nEncodInMod;    // Next module
  }
}

void EncoderInputs::SwitchCheck(uint8_t moduleNo, uint8_t encNo){  
  // Get switch state from module state
  uint8_t moduleType = GetModuleType(moduleNo);

  switch(moduleType){
    case EncoderModuleTypes::E41H:
    case EncoderModuleTypes::E41V:{
         eHwData[encNo].switchHWState = !((encMData[moduleNo].state>>8)&(1<<(encNo%(defE41module.components.nEncoders))));
      } break;
    case EncoderModuleTypes::E41H_D:
    case EncoderModuleTypes::E41V_D:{
        eHwData[encNo].switchHWState = !(encMData[moduleNo].state & (1 << defE41module.encSwitchPins[encNo%(defE41module.components.nEncoders)])); 
      } break;
    default: break;
  }
  
  // Get current millis
  uint32_t now = millis();
  int8_t clicks = 0;
  bool momentary =  encoder[encNo].switchConfig.action == switchActions::switch_momentary       ||  // IF switch mode is MESSAGE & msg type is key, PC#, PC+ or PC- set as momentary
                    ((encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_key ||     
                    encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc    ||
                    encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc_m  || 
                    encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc_p) &&
                    (encoder[encNo].switchConfig.mode == switch_mode_message));

  
  // multiple and long click algorithm from https://github.com/marcobrianza/ClickButton/blob/master/ClickButton.cpp

  // If the switch changed, due to noise or a button press, reset the debounce timer
  if(eHwData[encNo].switchHWState != eHwData[encNo].switchHWStatePrev) {
    eHwData[encNo].lastSwitchBounce = now;
    if(testEncoderSwitch){
      SERIALPRINT(eHwData[encNo].switchHWState ? F("PRESSED") : F("RELEASED"));
      SERIALPRINT(F(" <- ENC "));SERIALPRINTLN(encNo); 
    }
  }

  // debounce the button (Check if a stable, changed state has occured)
  if (((now - eHwData[encNo].lastSwitchBounce) > SWITCH_DEBOUNCE_WAIT) && eHwData[encNo].switchHWState != eHwData[encNo].debounceSwitchPressed){
    eHwData[encNo].debounceSwitchPressed = eHwData[encNo].switchHWState;
    // eHwData[encNo].lastSwitchBounce = now;
    if (CheckIfBankShifter(encNo, eHwData[encNo].debounceSwitchPressed)){
      // IF IT IS BANK SHIFTER, RETURN, DON'T DO ACTION FOR THIS SWITCH
      // SERIALPRINTLN(F("IS SHIFTER"));
      return;
    }
    if (eHwData[encNo].debounceSwitchPressed || (momentary && !eHwData[encNo].clickCount)){ 
      eHwData[encNo].clickCount++;
    }
  }

  // If state is still the same as the previous state, there was no change
  if(eHwData[encNo].switchHWStatePrev == eHwData[encNo].switchHWState) {
    eHwData[encNo].changed = false;
  }
  eHwData[encNo].switchHWStatePrev = eHwData[encNo].switchHWState;
  
  // If the button released state is stable, report nr of clicks and start new cycle
  // if double click is not configured, don't wait for it

  if (eHwData[encNo].clickCount > 0 &&    // If there was a press and window time for double click expired or it isn't configured for a double click action, then flag change
    ((now - eHwData[encNo].lastSwitchBounce > DOUBLE_CLICK_WAIT) || encoder[encNo].switchConfig.doubleClick == switchDoubleClickModes::switch_doubleClick_none)){
    
    // positive count for released buttons
    clicks = eHwData[encNo].clickCount;
    eHwData[encNo].clickCount = 0;
    eHwData[encNo].changed = true;
  }
  
  // Check for "long click"
  if (eHwData[encNo].debounceSwitchPressed && (now - eHwData[encNo].lastSwitchBounce > LONG_CLICK_WAIT)){
    // negative count for long clicks
    // clicks = 0 - eHwData[encNo].clickCount;      // Change to this when implementing long press
    clicks = -1;                                   // set count to prevent long press being ignored
    eHwData[encNo].clickCount = 0;  
    eHwData[encNo].changed = true;
  }



  if(eHwData[encNo].changed){
    if (encoder[encNo].switchConfig.doubleClick != switchDoubleClickModes::switch_doubleClick_none && !momentary){
      eHwData[encNo].debounceSwitchPressed = !eHwData[encNo].switchHWState;
    }

    if(clicks == 1){   
      if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_none) return;
      
      // SINGLE CLICK ACTION 
      if(momentary){   
        eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState = eHwData[encNo].debounceSwitchPressed;
      }else{  // Input is toggle
        if (eHwData[encNo].debounceSwitchPressed)
          eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState = !eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
      } 

      SwitchAction(moduleNo, encNo, clicks);
    }else if(clicks == 2){    // DOUBLE CLICK ACTION
      // Get min, max and msgType
      uint16_t minValue = 0, maxValue = 0;
      uint8_t minValue2 = 0, maxValue2 = 0; 

      uint8_t msgType = 0;

      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
        minValue = encoder[encNo].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
        maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
        msgType = encoder[encNo].switchConfig.message;
      }else{
        minValue =  encoder[encNo].rotaryConfig.parameter[rotary_minMSB]<<7 | 
                    encoder[encNo].rotaryConfig.parameter[rotary_minLSB];

        maxValue =  encoder[encNo].rotaryConfig.parameter[rotary_maxMSB]<<7 | 
                    encoder[encNo].rotaryConfig.parameter[rotary_maxLSB];
        msgType = encoder[encNo].rotaryConfig.message;
      }

      minValue2 = encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
      maxValue2 = encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];

      if(!( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
            msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb)){
        minValue = minValue & 0x7F;
        maxValue = maxValue & 0x7F;
      }

      // SET THE ENCODER AND 2CC TO MIN, CENTER, OR MAX VALUES
      if(encoder[encNo].switchConfig.doubleClick == switchDoubleClickModes::switch_doubleClick_2min){
        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = minValue;
        }else{
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = minValue;
        }

        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC){
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = minValue2;
        }
      }else if(encoder[encNo].switchConfig.doubleClick == switchDoubleClickModes::switch_doubleClick_2max){
        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = maxValue;
        }else{
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = maxValue;
        }
        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC){
          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = maxValue2;
        }
      }else if(encoder[encNo].switchConfig.doubleClick == switchDoubleClickModes::switch_doubleClick_2center){
        // Get center value for even and odd ranges differently
        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
          if(!(abs(maxValue-minValue)%2)){
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = (minValue + maxValue)/2;
          }else{
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = (minValue + maxValue)/2 + 1;
          }
        }else{
          if(!(abs(maxValue-minValue)%2)){
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = (minValue + maxValue)/2;
          }else{
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = (minValue + maxValue)/2 + 1;
          }
        }
          
        if(eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC){
          if(!(abs(maxValue2-minValue2)%2)){
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = (minValue2 + maxValue2)/2;
          }else{
            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = (minValue2 + maxValue2)/2 + 1;
          }
        }
      }
      SendRotaryMessage(moduleNo, encNo);
    }
  }
}


void EncoderInputs::SwitchAction(uint8_t moduleNo, uint8_t encNo, int8_t clicks, bool initDump) { // clicks is here to know if it is a long press
  bool newSwitchState = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
  // SERIALPRINT(F("Encoder switch ")); SERIALPRINT(encNo); SERIALPRINT(F(" in bank ")); SERIALPRINT(eHwData[encNo].thisEncoderBank); 
  // SERIALPRINT(F(": New switch state: "));SERIALPRINT(eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState);
  // SERIALPRINT(F("\tPrev switch state: "));SERIALPRINTLN(eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev);
  if(newSwitchState != eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev || clicks < 0 || initDump){
    eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev = newSwitchState;  // update previous
    
    if(initDump){
      if (CheckIfBankShifter(encNo, eHwData[encNo].debounceSwitchPressed)){
        return;
      }
    }

    // Get config parameters for switch action
    uint16_t paramToSend = encoder[encNo].switchConfig.parameter[switch_parameter_MSB] << 7 |
                           encoder[encNo].switchConfig.parameter[switch_parameter_LSB];
    uint8_t channelToSend = encoder[encNo].switchConfig.channel + 1;
    uint16_t minValue = encoder[encNo].switchConfig.parameter[switch_minValue_MSB] << 7 |
                        encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
    uint16_t maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB] << 7 |
                        encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
  
    uint16_t valueToSend = 0;
    bool updateSwitchFb = false;
    bool programFb = false;

    if(IS_ENCODER_SW_7_BIT(encNo)){
      minValue &= 0x7F;
      maxValue &= 0x7F;
    }

    if (newSwitchState) {
      valueToSend = maxValue;
    } else {
      valueToSend = minValue;
    }

    if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_quick_shift){ // QUICK SHIFT TO BANK #
      byte nextBank = 0;
      updateSwitchFb = true;
      if(eHwData[encNo].bankShifted){
        nextBank = currentBank;
      }else{
        nextBank = paramToSend&0x7F;    // if bank is not shifted, get bank to switch to, from param
      }
      // Check just in case
      if(nextBank >= config->banks.count) return; // Limit to banks count - 1. Don't go on if otherwise
      
      // Swap switch state data between current and next bank
      bool auxState                                                       = eBankData[nextBank][encNo].switchInputState;
      bool auxStatePrev                                                   = eBankData[nextBank][encNo].switchInputStatePrev;
      bool auxValue                                                       = eBankData[nextBank][encNo].switchLastValue;
      eBankData[nextBank][encNo].switchInputState                         = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
      eBankData[nextBank][encNo].switchInputStatePrev                     = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev;
      eBankData[nextBank][encNo].switchLastValue                          = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState     = auxState;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev = auxStatePrev;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue      = auxValue;

      eHwData[encNo].thisEncoderBank = nextBank;
      eHwData[encNo].bankShifted = !eHwData[encNo].bankShifted;

      // SHIFT BANK (PARAMETER LSB) ONLY FOR THIS ENCODER 
      memHost->LoadBankSingleSection(nextBank, ytxIOBLOCK::Encoder, encNo, QSTB_LOAD); // Flag true QSTB
      
      // Scan for changes present in buffer
      ScanMidiBufferAndUpdate(nextBank, QSTB_LOAD, encNo);

      // UPDATE FEEDBACK FOR NEW BANK
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                          encNo, 
                                          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue, 
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation,
                                          NO_SHIFTER, NO_BANK_UPDATE);
      
    }else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_quick_shift_note){ // QUICK SHIFT TO BANK # + NOTE
      // SHIFT BANK (PARAMETER MSB) ONLY FOR THIS ENCODER 
      byte nextBank = 0;
      updateSwitchFb = true;
      if(eHwData[encNo].bankShifted){
        nextBank = currentBank;
      }else{
        nextBank = paramToSend&0x7F;    // if bank is not shifted, get bank to switch to, from param
      }
      // Check just in case
      if(nextBank >= config->banks.count) return; // Limit to banks count - 1. Don't go on if otherwise
      
      // Swap switch state data between current and next bank
      bool auxState                                                       = eBankData[nextBank][encNo].switchInputState;
      bool auxStatePrev                                                   = eBankData[nextBank][encNo].switchInputStatePrev;
      bool auxValue                                                       = eBankData[nextBank][encNo].switchLastValue;
      eBankData[nextBank][encNo].switchInputState                         = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
      eBankData[nextBank][encNo].switchInputStatePrev                     = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev;
      eBankData[nextBank][encNo].switchLastValue                          = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState     = auxState;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev = auxStatePrev;
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue      = auxValue;

      eHwData[encNo].thisEncoderBank = nextBank;
      eHwData[encNo].bankShifted = !eHwData[encNo].bankShifted;

      // SHIFT BANK (PARAMETER LSB) ONLY FOR THIS ENCODER 
      memHost->LoadBankSingleSection(nextBank, ytxIOBLOCK::Encoder, encNo, QSTB_LOAD); // Flag true QSTB
      
      // Scan for changes present in buffer
      ScanMidiBufferAndUpdate(nextBank, QSTB_LOAD, encNo);
            
      // UPDATE FEEDBACK FOR NEW BANK
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                          encNo, 
                                          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue, 
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation,
                                          NO_SHIFTER, NO_BANK_UPDATE);
                                          
      // SEND NOTE
      if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB))
        MIDI.sendNoteOn( (paramToSend >> 7) & 0x7f, valueToSend & 0x7f, channelToSend);
      if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW))
        MIDIHW.sendNoteOn( (paramToSend >> 7) & 0x7f, valueToSend & 0x7f, channelToSend);

    }else if (encoder[encNo].switchConfig.mode == switchModes::switch_mode_shift_rot){  // SHIFT ROTARY ACTION
      eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction = newSwitchState;
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
        eHwData[encNo].encoderValuePrev = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue;
        feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                            encNo, 
                                            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue, 
                                            encMData[moduleNo].orientation, 
                                            NO_SHIFTER, NO_BANK_UPDATE);           
      }else{
        eHwData[encNo].encoderValuePrev = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
        feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                            encNo, 
                                            eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue, 
                                            encMData[moduleNo].orientation, 
                                            NO_SHIFTER, NO_BANK_UPDATE);
      }

      updateSwitchFb = true;
    }else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_fine){ // ENCODER FINE ADJUST
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encFineAdjust = newSwitchState;
      updateSwitchFb = true;
    }else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){ // DOUBLE CC
      eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC = newSwitchState;
      updateSwitchFb = true;
/////////////// NEW FEATURE: SENSITIVITY CONTROL FOR DIGITAL BUTTONS /////////////////////////////////////////////////////////
    }else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_velocity){ // BUTTON VELOCITY
      eBankData[eHwData[encNo].thisEncoderBank][encNo].buttonSensitivityControlOn = !eBankData[eHwData[encNo].thisEncoderBank][encNo].buttonSensitivityControlOn;

      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].buttonSensitivityControlOn){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
        eHwData[encNo].encoderValuePrev = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = digitalHw.GetButtonVelocity();

      }else{
        digitalHw.SetButtonVelocity(VELOCITY_SESITIVITY_OFF);
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
        eHwData[encNo].encoderValuePrev = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
      }
      
      // UPDATE FEEDBACK FOR NEW BANK
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                          encNo, 
                                          eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue, 
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation,
                                          NO_SHIFTER, NO_BANK_UPDATE);
      updateSwitchFb = true;
    }
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_message){  // SEND NORMAL MIDI MESSAGE
      switch (encoder[encNo].switchConfig.message) {
        case switchMessageTypes::switch_msg_note: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB)){
              if(valueToSend) MIDI.sendNoteOn( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
              else            MIDI.sendNoteOff( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
            }
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW)){
              if(valueToSend) MIDIHW.sendNoteOn( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
              else            MIDIHW.sendNoteOff( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
            }
          } break;
        case switchMessageTypes::switch_msg_cc: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB))
              MIDI.sendControlChange( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW))
              MIDIHW.sendControlChange( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
          } break;
        case switchMessageTypes::switch_msg_pc: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB) && newSwitchState){
              MIDI.sendProgramChange( paramToSend & 0x7f, channelToSend);
              currentProgram[MIDI_USB][channelToSend - 1] = paramToSend & 0x7f;
            }

            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW) && newSwitchState){
              MIDIHW.sendProgramChange( paramToSend & 0x7f, channelToSend);
              currentProgram[MIDI_HW][channelToSend - 1] = paramToSend & 0x7f; 
            }
          } break;
        case switchMessageTypes::switch_msg_pc_m: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB)) {
              uint8_t programToSend = minValue + currentProgram[MIDI_USB][channelToSend - 1];
              if ((programToSend > minValue) && newSwitchState) {
                currentProgram[MIDI_USB][channelToSend - 1]--;
                programToSend--;
                MIDI.sendProgramChange(programToSend, channelToSend);
                programFb = true;
              }else if(!newSwitchState)   programFb = true;
            }
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW)) {
              uint8_t programToSend = minValue + currentProgram[MIDI_HW][channelToSend - 1];
              if ((programToSend > minValue) && newSwitchState) {
                currentProgram[MIDI_HW][channelToSend - 1]--;
                programToSend--;
                MIDIHW.sendProgramChange(programToSend, channelToSend);
                programFb = true;
              }else if(!newSwitchState)   programFb = true;
            }
          } break;
        case switchMessageTypes::switch_msg_pc_p: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB)) {
              uint8_t programToSend = minValue + currentProgram[MIDI_USB][channelToSend - 1];
              if ((programToSend < maxValue) && newSwitchState) {
                currentProgram[MIDI_USB][channelToSend - 1]++;
                programToSend++;
                MIDI.sendProgramChange(programToSend, channelToSend);
                programFb = true;
              }else if(!newSwitchState)   programFb = true;
            }
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW)) {
              uint8_t programToSend = minValue + currentProgram[MIDI_HW][channelToSend - 1];
              if ((programToSend < maxValue) && newSwitchState) {
                currentProgram[MIDI_HW][channelToSend - 1]++;
                programToSend++;
                MIDIHW.sendProgramChange(programToSend, channelToSend);
                programFb = true;
              }else if(!newSwitchState)   programFb = true;
            }
          } break;
        case switchMessageTypes::switch_msg_nrpn: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB)) {
              MIDI.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
              MIDI.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
              MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
              MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
            }
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW)) {
              MIDIHW.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
              MIDIHW.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
              MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
              MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
            }
          } break;
        case switchMessageTypes::switch_msg_rpn: {
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB)) {
              MIDI.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
              MIDI.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
              MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
              MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
            }
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW)) {
              MIDIHW.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
              MIDIHW.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
              MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
              MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
            }
          } break;
        case switchMessageTypes::switch_msg_pb: {
            int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_USB))
              MIDI.sendPitchBend( valuePb, channelToSend);
            if (encoder[encNo].switchConfig.midiPort & (1<<MIDI_HW))
              MIDIHW.sendPitchBend( valuePb, channelToSend);
          } break;
        case switchMessageTypes::switch_msg_key: {
          uint8_t modifier = 0;
          
          if(valueToSend == maxValue){
            switch(encoder[encNo].switchConfig.parameter[switch_modifier]){
              case 0:{                              }break;
              case 1:{  modifier = KEY_LEFT_ALT;    }break;
              case 2:{  modifier = KEY_LEFT_CTRL;   }break;
              case 3:{  modifier = KEY_LEFT_SHIFT;  }break;
              default: break;
            }
            if(modifier)   // if different than 0, press modifier
              YTXKeyboard->press(modifier);
            if (encoder[encNo].switchConfig.parameter[switch_key])
              YTXKeyboard->press(encoder[encNo].switchConfig.parameter[switch_key]);
          }else{
            YTXKeyboard->releaseAll();
          }
        } break;
        default: break;
      }
      SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);
    }
    
    if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Encoder, encNo) != lastComponentInfoId)){
      SendComponentInfo(ytxIOBLOCK::Encoder, encNo);
    }

    // SERIALPRINTLN(encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_key ? F("KEY") : F("NOT KEY"));

    if( encoder[encNo].switchFeedback.source & feedbackSource::fb_src_local                       || 
        encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc                  ||    
        (encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc_m && programFb) ||
        (encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_pc_p && programFb) ||
        encoder[encNo].switchConfig.message == switchMessageTypes::switch_msg_key                 ||
        testEncoderSwitch                                                                         ||
        updateSwitchFb){
      uint16_t fbValue = 0;
      if(encoder[encNo].switchFeedback.source == fb_src_local && encoder[encNo].switchFeedback.localBehaviour == fb_lb_always_on){
        fbValue = true;
      }else{
        if(programFb) fbValue = newSwitchState;
        else          fbValue = valueToSend;
      } 
      eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue = valueToSend;
      feedbackHw.SetChangeEncoderFeedback(FB_ENC_SWITCH, encNo, fbValue, encMData[ENC_MODULE_NUMBER(encNo)].orientation, NO_SHIFTER, NO_BANK_UPDATE);   
    }
  }
}

bool EncoderInputs::RotaryHasConfiguration(uint8_t encNo){
  return (bool)(encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_none);
}

int EncoderInputs::DecodeRotaryDirection(uint8_t moduleNo, uint8_t encNo){
  int direction = 0;

  bool programChangeEncoder = (encoder[encNo].rotaryConfig.message == rotaryMessageTypes::rotary_msg_pc_rel);
  bool isKey = (encoder[encNo].rotaryConfig.message == rotaryMessageTypes::rotary_msg_key);
  
  uint8_t moduleType = GetModuleType(moduleNo);

  switch(moduleType){
    case EncoderModuleTypes::E41H:
    case EncoderModuleTypes::E41V:{
      bool getDirection=false;
      uint8_t prescaler;

      if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_1 ||
         encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_1 ||
         encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_2 ||
         encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_3 || 
         programChangeEncoder || isKey){
        prescaler = 4;
      }else if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_2 ||
               encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_3){
        prescaler = 1;
      }

      if(++eHwData[encNo].encoderState>=prescaler){
        eHwData[encNo].encoderState = 0;
        getDirection=true;
      }  

      if(getDirection){
        uint8_t elementRotary = encNo - 4*moduleNo;
        //HAS MOVE?
        if(encMData[moduleNo].state&(1<<(4+elementRotary))){
          encMData[moduleNo].state &= ~(1<<(4+elementRotary));
          //WHAT DIRECTION?
          if(encMData[moduleNo].state&(1<<elementRotary))
            direction = 1;
          else
            direction = -1;
        }
      }
    } break;
    case EncoderModuleTypes::E41H_D:
    case EncoderModuleTypes::E41V_D:{
        // ENCODER CHANGED?
        uint8_t pinState = 0;
        
        if (encMData[moduleNo].state & (1 << defE41module.encPins[encNo%(defE41module.components.nEncoders)][0])){  // If the pin A for this encoder is HIGH
          pinState |= 2;        // Save new state for pin A
        }
        
        if (encMData[moduleNo].state & (1 << defE41module.encPins[encNo%(defE41module.components.nEncoders)][1])){
          pinState |= 1;        // Get new state for pin B
        } 

        if(testEncoders){
          if(encMData[moduleNo].detent){
            eHwData[encNo].encoderState = pgm_read_byte(&fullStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);
          }else{
            eHwData[encNo].encoderState = pgm_read_byte(&quarterStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);
          }
        
          if(eHwData[encNo].encoderState){
            eHwData[encNo].statesAcc++;
          }
        }else{
          // Check state in table
          if(encMData[moduleNo].detent && !eBankData[eHwData[encNo].thisEncoderBank][encNo].encFineAdjust){
            // IF DETENTED ENCODER AND FINE ADJUST IS NOT SELECTED - SELECT FROM TABLE BASED ON SPEED CONFIGURATION
            if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_1 ||
               encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_1 ||
               encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_2 ||
               encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_variable_speed_3 || 
               programChangeEncoder || isKey){
              eHwData[encNo].encoderState = pgm_read_byte(&fullStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);
            }else if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_2){
              eHwData[encNo].encoderState = pgm_read_byte(&quarterStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);  
            }else if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_3){
              eHwData[encNo].encoderState = pgm_read_byte(&quarterStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);  
            }
          }else if(encMData[moduleNo].detent && eBankData[eHwData[encNo].thisEncoderBank][encNo].encFineAdjust){
            // IF FINE ADJUST AND DETENTED ENCODER - FULL STEP TABLE
            eHwData[encNo].encoderState = pgm_read_byte(&fullStepTable[eHwData[encNo].encoderState & 0x0f][pinState]);
          }else{
            // IF NON DETENTED ENCODER - QUARTER STEP TABLE
            eHwData[encNo].encoderState = pgm_read_byte(&quarterStepTable[eHwData[encNo].encoderState & 0x0f][pinState]); 
          }
        } 

        // if at a valid state, check direction
        switch (eHwData[encNo].encoderState & 0x30) {
          case DIR_CW:{
              direction = 1; 
          }   break;
          case DIR_CCW:{
              direction = -1;
          }   break;
          case DIR_NONE:
          default:
              break;
        }
      } break;
    default: break;
  }

  return direction;
}
void EncoderInputs::EncoderAction(uint8_t moduleNo, uint8_t encNo){
  bool programChangeEncoder = (encoder[encNo].rotaryConfig.message == rotaryMessageTypes::rotary_msg_pc_rel);
  bool isKey = (encoder[encNo].rotaryConfig.message == rotaryMessageTypes::rotary_msg_key);

  ///////////////////////////////////////////////  
  //////// ENCODER SPEED  ///////////////////////
  ///////////////////////////////////////////////
  
  //  SPEED CONFIG 
  if (!eBankData[eHwData[encNo].thisEncoderBank][encNo].encFineAdjust){  
    // FINE ADJUST INACTIVE

    //FIXED SPEEDS
    if ((encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_1) ||
        (encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_2) ||
         programChangeEncoder ||
         isKey){
      eHwData[encNo].nextJump = 1;
    }else if (encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_3){
      eHwData[encNo].nextJump = 2;
    }else{
    //VARIABLE SPEEDS
      uint8_t *millisSpeedInterval;

      uint8_t speedIndex = encoder[encNo].rotBehaviour.speed == rot_variable_speed_1 ? 0 :
                           encoder[encNo].rotBehaviour.speed == rot_variable_speed_2 ? 1 :
                           encoder[encNo].rotBehaviour.speed == rot_variable_speed_3 ? 2 : 0;
                           
      if(encMData[moduleNo].detent){
        millisSpeedInterval = detentMillisSpeedThresholds[speedIndex];
      }else{
        millisSpeedInterval = detentMillisSpeedThresholds[speedIndex];
      }

      unsigned long timeLastChange = millis() - eHwData[encNo].millisUpdatePrev;

      if(eHwData[encNo].currentSpeed < ENCODER_MAX_SPEED){
        if(timeLastChange < millisSpeedInterval[eHwData[encNo].currentSpeed+1]){  // If quicker than next ms, go to next speed
          eHwData[encNo].currentSpeed++;  
          eHwData[encNo].nextJump = encoderAccelSpeed[speedIndex][eHwData[encNo].currentSpeed];
        }
      }

      if(eHwData[encNo].currentSpeed > 0){
        if(timeLastChange > millisSpeedInterval[eHwData[encNo].currentSpeed-1]){  // If slower than prev ms, go to prev speed
          eHwData[encNo].currentSpeed--;  
          eHwData[encNo].nextJump = encoderAccelSpeed[speedIndex][eHwData[encNo].currentSpeed];
        }
      }
    }
  }else{ 
    // FINE ADJUST ACTIVE
    if(encoder[encNo].rotBehaviour.speed == encoderRotarySpeed::rot_fixed_speed_1){
      //HALVE SPEED ON FIXED 1 SPEED CONFIG 
      if(++eBankData[eHwData[encNo].thisEncoderBank][encNo].pulseCounter >= SLOW_SPEED_COUNT){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].pulseCounter = 0;
        eHwData[encNo].nextJump = 1;
      }else{
        eHwData[encNo].nextJump = 0;
      }
    }else{
      //FORCE MINIMUM JUMP ON OTHERS SPEEDS CONFIGS
      eHwData[encNo].nextJump = 1;
    }
  }

  eHwData[encNo].millisUpdatePrev = millis();
  
  //SERIALPRINTLN(F("CHANGE!"));
  SendRotaryMessage(moduleNo, encNo);
}

void EncoderInputs::SendRotaryMessage(uint8_t moduleNo, uint8_t encNo, bool initDump){
  uint16_t paramToSend = 0;
  uint8_t channelToSend = 0, portToSend = 0;
  uint16_t minValue = 0, maxValue = 0; 
  uint8_t msgType = 0;
  
  uint16_t paramToSend2 = 0;
  uint8_t channelToSend2 = 0, portToSend2 = 0;
  uint16_t minValue2 = 0, maxValue2 = 0; 
  
  bool is14bits = false;

  bool isAbsolute = (encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_absolute) ||            // Set absolute mode if it is configured as such
                    ((encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_cc) &&  // Or if it isn't a CC. Relative only for CC and VU CC encoders
                     (encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_vu_cc));   
  
  // Get config info for this encoder
  if((eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction || 
     eBankData[eHwData[encNo].thisEncoderBank][encNo].buttonSensitivityControlOn) && !initDump){
    // IF SHIFT ROTARY ACTION FLAG IS ACTIVATED GET CONFIG FROM SWITCH CONFIG (SHIFT CONFIG)
    paramToSend = encoder[encNo].switchConfig.parameter[switch_parameter_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_parameter_LSB];
    channelToSend = encoder[encNo].switchConfig.channel + 1;
    minValue = encoder[encNo].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
    maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
    msgType = encoder[encNo].switchConfig.message;
    portToSend = encoder[encNo].switchConfig.midiPort;
  }else{
    paramToSend = encoder[encNo].rotaryConfig.parameter[rotary_MSB]<<7 | encoder[encNo].rotaryConfig.parameter[rotary_LSB];
    channelToSend = encoder[encNo].rotaryConfig.channel + 1;
    minValue = encoder[encNo].rotaryConfig.parameter[rotary_minMSB]<<7 | encoder[encNo].rotaryConfig.parameter[rotary_minLSB];
    maxValue = encoder[encNo].rotaryConfig.parameter[rotary_maxMSB]<<7 | encoder[encNo].rotaryConfig.parameter[rotary_maxLSB];
    msgType = encoder[encNo].rotaryConfig.message;
    portToSend = encoder[encNo].rotaryConfig.midiPort;
  }

  if(eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC && !initDump){
    paramToSend2 = encoder[encNo].switchConfig.parameter[switch_parameter_LSB];
    channelToSend2 = encoder[encNo].switchConfig.channel + 1;
    minValue2 = encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
    maxValue2 = encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
    portToSend2 = encoder[encNo].switchConfig.midiPort; 
  }

  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else if(msgType == rotary_msg_key){
    minValue = 0;
    maxValue = S_SPOT_SIZE;
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }
  
  //bool invert = false;
  if(eHwData[encNo].encoderDirection != eHwData[encNo].encoderDirectionPrev) 
    eHwData[encNo].nextJump = 1;    // If direction changed, set speed to minimum

  int8_t normalDirection = eHwData[encNo].encoderDirection;
  int8_t doubleCCdirection = eHwData[encNo].encoderDirection;

  if(minValue > maxValue){    // If minValue is higher, invert behaviour
    normalDirection *= -1;   // Invert position
    //invert = true;                        // 
    uint16_t aux = minValue;              // Swap min and max values, to treat them equally henceforth
    minValue = maxValue;
    maxValue = aux;
  }
  
  uint8_t speedMultiplier = 1;

  // INCREASE SPEED FOR 14 BIT VALUES
  if(is14bits && eHwData[encNo].nextJump > 1){
    int16_t maxMinDiff = maxValue - minValue;
    // :O - magic to get speed multiplier - If speed is high, it shifts (divides) the range just a little, 
    //                                      High range means higher multiplier
    //                                      if speed is low, it shifts range a lot, getting a lower speed multiplier 
    speedMultiplier = abs(maxMinDiff) >> (14-eHwData[encNo].nextJump);    
    // only valid if multiplier is > 0
    if(!speedMultiplier) speedMultiplier = 1;
    
  }

  // GET NEW ENCODER VALUE based on speed, direction and speed multiplier (14 bits config)
  if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction && encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_absolute && !initDump){
    eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue += eHwData[encNo].nextJump*speedMultiplier*normalDirection;           // New value
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue > maxValue){
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = maxValue;
    } 
    // if below min, stay in min
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue < minValue) {
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = minValue;
    }
  }else if(isAbsolute){
    if(msgType == rotary_msg_pc_rel)
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue += normalDirection;           // New value - speed 1 for Program Change Encoders
    else 
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue += eHwData[encNo].nextJump*speedMultiplier*normalDirection;           // New value

    if(msgType != rotary_msg_key){
      // If overflows max, stay in max
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue > maxValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = maxValue;
      }
      // if below min, stay in min
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue < minValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = minValue;
      }
    }else{
      // If keystroke, when it overflows max, go to min
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue > maxValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = minValue;
        
      }
      // if below min, go to max
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue < minValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = maxValue;
      }
    }
      
  } 
  

  // If double CC is ON, process value for it
  if(eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC && !initDump){
    if(minValue2 > maxValue2){    // If minValue is higher, invert behaviour
      doubleCCdirection = eHwData[encNo].encoderDirection * (-1);   // Invert position
      //invert = true;                        // 
      uint16_t aux = minValue2;              // Swap min and max values, to treat them equally henceforth
      minValue2 = maxValue2;
      maxValue2 = aux;
    }
    eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc += eHwData[encNo].nextJump*doubleCCdirection;    // New value
    // If overflows max, stay in max
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc >= maxValue2) eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = maxValue2;
    // if below min, stay in min
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc < minValue2) eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = minValue2;
  }

  // update previous direction
  eHwData[encNo].encoderDirectionPrev = eHwData[encNo].encoderDirection;   

  bool updateFb = false;
  // If value changed 
  uint16_t valueToSend = 0;
    
  // VALUE TO SEND UPDATE BASE ON HW MODE CONFIGURATION
  // ABSOLUTE ENCODER OR NOT CC
  if(isAbsolute){
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction && !initDump){
      valueToSend = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue;
    }else{
      valueToSend = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
    }  
    
  // BINARY OFFSET RELATIVE ENCODER  
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_binaryOffset){
    // Current speed is the offset, encoderDirection adds or takes from 64
    valueToSend = 64 + eHwData[encNo].nextJump*eHwData[encNo].encoderDirection;   // Positive values 065 (+1) - 127 (+63) 
                                                                                  // Negative values 063 (-1) - 000 (-64)
  // 2's COMPLEMENT RELATIVE ENCODER    
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_complement2){
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = eHwData[encNo].nextJump;      // positive values 001 (+1) - 064 (+64)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 128 - eHwData[encNo].nextJump;  // negative values 127 (-1) - 065 (-63)
    }
  // SIGNED BIT RELATIVE ENCODER      
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit){   // Positive values have MSB in 1
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = 64 + eHwData[encNo].nextJump;   // positive values 065 (+1) - 127 (+63)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = eHwData[encNo].nextJump;      // negative values 001 (-1) - 063 (-63)
    }
  // SIGNED BIT 2 RELATIVE ENCODER
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit2){  // Positive values have MSB in 0
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = eHwData[encNo].nextJump;      // positive values 001 (+1) - 063 (+63)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 64 + eHwData[encNo].nextJump;   // negative values 065 (-1) - 127 (-63)
    }
  // SINGLE VALUE RELATIVE ENCODER  
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_singleValue){
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = 96;                             // Single value increment
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 97;                             // Single value decrement
    }
  }else{
    valueToSend = 0;
  }
  

  if((valueToSend != eHwData[encNo].encoderValuePrev) || (msgType == rotaryMessageTypes::rotary_msg_key) ||
     (msgType == rotaryMessageTypes::rotary_msg_note) || !isAbsolute || initDump){     

    if(isAbsolute)  eHwData[encNo].encoderValuePrev = valueToSend;
    
    if(!is14bits){
      paramToSend &= 0x7F;
      valueToSend &= 0x7F;
    }
    
    // NEW FEATURE: SENSITIVITY CONTROL FOR DIGITAL BUTTONS ///////////////////////////
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].buttonSensitivityControlOn){
      digitalHw.SetButtonVelocity(valueToSend);
    }
    ///////////////////////////////////////////////////////////////////////////////////
    else{
      switch(msgType){
        case rotaryMessageTypes::rotary_msg_note:{
          if(eHwData[encNo].encoderDirection < 0){        // Rotating left
            valueToSend = minValue;                       // Set velocity as minValue when rotating left
          }else if(eHwData[encNo].encoderDirection > 0){  // Rotating right
            valueToSend = maxValue;                       // Set velocity as maxValue when rotating right
          }
          if(portToSend & (1<<MIDI_USB))
            MIDI.sendNoteOn( paramToSend, valueToSend, channelToSend);
          if(portToSend & (1<<MIDI_HW))
            MIDIHW.sendNoteOn( paramToSend, valueToSend, channelToSend);
        }break;
        case rotaryMessageTypes::rotary_msg_cc:
        case rotaryMessageTypes::rotary_msg_vu_cc:{
          if(portToSend & (1<<MIDI_USB))
            MIDI.sendControlChange( paramToSend, valueToSend, channelToSend);
          if(portToSend & (1<<MIDI_HW))
            MIDIHW.sendControlChange( paramToSend, valueToSend, channelToSend);
        }break;
        case rotaryMessageTypes::rotary_msg_pc_rel:{
          if(portToSend & (1<<MIDI_USB)){
            MIDI.sendProgramChange( valueToSend, channelToSend);
            currentProgram[MIDI_USB][channelToSend - 1] = valueToSend;
          }
         
          if(portToSend & (1<<MIDI_HW)){
            MIDIHW.sendProgramChange( valueToSend, channelToSend);
            currentProgram[MIDI_HW][channelToSend - 1] = valueToSend;
          }
        }break;

        case rotaryMessageTypes::rotary_msg_nrpn:{
          if(portToSend & (1<<MIDI_USB)){
            MIDI.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
            MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
          }
          if(portToSend & (1<<MIDI_HW)){
            MIDIHW.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
            MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
          }
        }break;
        case rotaryMessageTypes::rotary_msg_rpn:{
          if(portToSend & (1<<MIDI_USB)){
            MIDI.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
            MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
          }
          if(portToSend & (1<<MIDI_HW)){
            MIDIHW.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
            MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
          }
        }break;
        case rotaryMessageTypes::rotary_msg_pb:{
          int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
          if(portToSend & (1<<MIDI_USB))
            MIDI.sendPitchBend( valuePb, channelToSend);    
          if(portToSend & (1<<MIDI_HW))
            MIDIHW.sendPitchBend( valuePb, channelToSend);    
        }break;
        case rotaryMessageTypes::rotary_msg_key:{
          uint8_t key = 0;
          uint8_t modifier = 0;

          if(eHwData[encNo].encoderDirection < 0){       // turned left
            // SERIALPRINTLN(F("Key Left action triggered"));
            // SERIALPRINT(F("Modifier left: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_modifierLeft]);
            // SERIALPRINT(F("Key left: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_keyLeft]);
            if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
              key = encoder[encNo].switchConfig.parameter[rotary_keyLeft];  
              modifier = encoder[encNo].switchConfig.parameter[rotary_modifierLeft];  
            }else{
              key = encoder[encNo].rotaryConfig.parameter[rotary_keyLeft];  
              modifier = encoder[encNo].rotaryConfig.parameter[rotary_modifierLeft];  
            }
            
            switch(modifier){
              case 0:{}break;
              case 1:{  modifier = KEY_LEFT_ALT;   }break;
              case 2:{  modifier = KEY_LEFT_CTRL;  }break;
              case 3:{  modifier = KEY_LEFT_SHIFT; }break;
              default: break;
            }
            if(modifier)   // if different than 0, press modifier
              YTXKeyboard->press(modifier);
            if(key)  
              YTXKeyboard->press(key);
          }else if(eHwData[encNo].encoderDirection > 0){ //turned right
            // SERIALPRINTLN(F("Key Right action triggered"));
            // SERIALPRINT(F("Modifier right: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_modifierRight]);
            // SERIALPRINT(F("Key right: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_keyRight]);
            if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
              key = encoder[encNo].switchConfig.parameter[rotary_keyRight];  
              modifier = encoder[encNo].switchConfig.parameter[rotary_modifierRight];  
            }else{
              key = encoder[encNo].rotaryConfig.parameter[rotary_keyRight];  
              modifier = encoder[encNo].rotaryConfig.parameter[rotary_modifierRight];  
            }

            switch(modifier){
              case 1:{  modifier = KEY_LEFT_ALT;   }break;
              case 2:{  modifier = KEY_LEFT_CTRL;  }break;
              case 3:{  modifier = KEY_LEFT_SHIFT; }break;
              default: break;
            }
            if(modifier)   // if different than 0, press modifier
              YTXKeyboard->press(modifier);
            if(key)
              YTXKeyboard->press(key);
          }
          millisKeyboardPress = millis()+KEYBOARD_MILLIS;
          keyboardReleaseFlag = true; 
        }break;
      }
    }
      
    
    SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);

    if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Encoder, encNo) != lastComponentInfoId)){
      SendComponentInfo(ytxIOBLOCK::Encoder, encNo);
    }
    
    // aprox 90 us / 4 rings de 16 leds   // 120 us / 8 enc // 200 us / 16 enc
    
    
    if(encoder[encNo].rotaryFeedback.source & feedbackSource::fb_src_local) updateFb = true;
    //}
  }

  if( eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC && 
     (eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc != eHwData[encNo].encoderValuePrev2cc)){
    eHwData[encNo].encoderValuePrev2cc = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
    uint16_t valueToSend2cc = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
    
    if(!initDump){
      if(portToSend2 & (1<<MIDI_USB))
        MIDI.sendControlChange( paramToSend2, valueToSend2cc, channelToSend2);
      if(portToSend2 & (1<<MIDI_HW))
        MIDIHW.sendControlChange( paramToSend2, valueToSend2cc, channelToSend2);
    }
      
    SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);
    
    updateFb = true;      
  }
  if(updateFb && isAbsolute){
    if(encoder[encNo].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc){
      feedbackHw.SetChangeEncoderFeedback(FB_ENC_VUMETER, encNo, feedbackHw.GetVumeterValue(encNo), encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE);
      feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC,         encNo, valueToSend,                       encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE);
    }else{
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, encNo, valueToSend, encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE);           
      if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){
        feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, encNo, eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc, 
                                                           encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE); 
      }
    }
  }
}

void EncoderInputs::SendRotaryAltMessage(uint8_t moduleNo, uint8_t encNo, bool initDump){
  uint16_t paramToSend = 0;
  uint8_t channelToSend = 0, portToSend = 0;
  uint16_t minValue = 0, maxValue = 0; 
  uint8_t msgType = 0;
  
  uint16_t paramToSend2 = 0;
  uint8_t channelToSend2 = 0, portToSend2 = 0;
  uint16_t minValue2 = 0, maxValue2 = 0; 
  
  bool is14bits = false;

  bool isAbsolute = (encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_absolute) ||            // Set absolute mode if it is configured as such
                    ((encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_cc) &&  // Or if it isn't a CC. Relative only for CC and VU CC encoders
                     (encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_vu_cc));   
  
  // GET SWITCH CONFIG
  paramToSend = encoder[encNo].switchConfig.parameter[switch_parameter_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_parameter_LSB];
  channelToSend = encoder[encNo].switchConfig.channel + 1;
  minValue = encoder[encNo].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
  maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
  msgType = (encoder[encNo].switchConfig.mode == switchModes::switch_mode_shift_rot) ? encoder[encNo].switchConfig.message : rotaryMessageTypes::rotary_msg_cc;
  portToSend = encoder[encNo].switchConfig.midiPort;

  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else if(msgType == rotary_msg_key){
    minValue = 0;
    maxValue = S_SPOT_SIZE;
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }
  
  //bool invert = false;
  if(eHwData[encNo].encoderDirection != eHwData[encNo].encoderDirectionPrev) 
    eHwData[encNo].nextJump = 1;    // If direction changed, set speed to minimum

  int8_t normalDirection = eHwData[encNo].encoderDirection;
  int8_t doubleCCdirection = eHwData[encNo].encoderDirection;

  if(minValue > maxValue){    // If minValue is higher, invert behaviour
    normalDirection *= -1;   // Invert position
    //invert = true;                        // 
    uint16_t aux = minValue;              // Swap min and max values, to treat them equally henceforth
    minValue = maxValue;
    maxValue = aux;
  }
  
  uint8_t speedMultiplier = 1;

  // INCREASE SPEED FOR 14 BIT VALUES
  if(is14bits && eHwData[encNo].nextJump > 1){
    int16_t maxMinDiff = maxValue - minValue;
    // :O - magic to get speed multiplier - If speed is high, it shifts (divides) the range just a little, 
    //                                      High range means higher multiplier
    //                                      if speed is low, it shifts range a lot, getting a lower speed multiplier 
    speedMultiplier = abs(maxMinDiff) >> (14-eHwData[encNo].nextJump);    
    // only valid if multiplier is > 0
    if(!speedMultiplier) speedMultiplier = 1;
    
  }

  // GET NEW ENCODER VALUE based on speed, direction and speed multiplier (14 bits config)
  if((encoder[encNo].switchConfig.mode == switchModes::switch_mode_shift_rot) && encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_absolute && !initDump){
    eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue += eHwData[encNo].nextJump*speedMultiplier*normalDirection;           // New value
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue > maxValue){
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = maxValue;
    } 
    // if below min, stay in min
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue < minValue) {
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue = minValue;
    }
  }else if(isAbsolute){
    if(msgType == rotary_msg_pc_rel)
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue += normalDirection;           // New value - speed 1 for Program Change Encoders
    else 
      eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue += eHwData[encNo].nextJump*speedMultiplier*normalDirection;           // New value

    if(msgType != rotary_msg_key){
      // If overflows max, stay in max
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue > maxValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = maxValue;
      }
      // if below min, stay in min
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue < minValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = minValue;
      }
    }else{
      // If keystroke, when it overflows max, go to min
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue > maxValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = minValue;
        
      }
      // if below min, go to max
      if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue < minValue){
        eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue = maxValue;
      }
    }
      
  } 
  
  // If double CC is ON, process value for it
  if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc && !initDump){
    eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc += eHwData[encNo].nextJump*doubleCCdirection;    // New value
    // If overflows max, stay in max
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc >= maxValue2) eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = maxValue2;
    // if below min, stay in min
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc < minValue2) eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc = minValue2;
  }

  // update previous direction
  eHwData[encNo].encoderDirectionPrev = eHwData[encNo].encoderDirection;   

  bool updateFb = false;
  // If value changed 
  uint16_t valueToSend = 0;
    
  // VALUE TO SEND UPDATE BASE ON HW MODE CONFIGURATION
  // ABSOLUTE ENCODER OR NOT CC
  if(isAbsolute){
    if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_shift_rot){
      valueToSend = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue;
    }else if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){
      valueToSend = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
    }else{
      valueToSend = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
    }  
    
  // BINARY OFFSET RELATIVE ENCODER  
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_binaryOffset){
    // Current speed is the offset, encoderDirection adds or takes from 64
    valueToSend = 64 + eHwData[encNo].nextJump*eHwData[encNo].encoderDirection;   // Positive values 065 (+1) - 127 (+63) 
                                                                                  // Negative values 063 (-1) - 000 (-64)
  // 2's COMPLEMENT RELATIVE ENCODER    
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_complement2){
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = eHwData[encNo].nextJump;      // positive values 001 (+1) - 064 (+64)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 128 - eHwData[encNo].nextJump;  // negative values 127 (-1) - 065 (-63)
    }
  // SIGNED BIT RELATIVE ENCODER      
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit){   // Positive values have MSB in 1
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = 64 + eHwData[encNo].nextJump;   // positive values 065 (+1) - 127 (+63)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = eHwData[encNo].nextJump;      // negative values 001 (-1) - 063 (-63)
    }
  // SIGNED BIT 2 RELATIVE ENCODER
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit2){  // Positive values have MSB in 0
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = eHwData[encNo].nextJump;      // positive values 001 (+1) - 063 (+63)
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 64 + eHwData[encNo].nextJump;   // negative values 065 (-1) - 127 (-63)
    }
  // SINGLE VALUE RELATIVE ENCODER  
  }else if(encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_rel_singleValue){
    if(eHwData[encNo].encoderDirection > 0){        // Rotating right
      valueToSend = 96;                             // Single value increment
    }else if(eHwData[encNo].encoderDirection < 0){  // Rotating left
      valueToSend = 97;                             // Single value decrement
    }
  }else{
    valueToSend = 0;
  }
  

  if((valueToSend != eHwData[encNo].encoderValuePrev) || (msgType == rotaryMessageTypes::rotary_msg_key) ||
     (msgType == rotaryMessageTypes::rotary_msg_note) || !isAbsolute || initDump){     
    
    if(isAbsolute)  eHwData[encNo].encoderValuePrev = valueToSend;
    
    if(!is14bits){
      paramToSend &= 0x7F;
      valueToSend &= 0x7F;
    }  
    
    switch(msgType){
      case rotaryMessageTypes::rotary_msg_note:{
        if(eHwData[encNo].encoderDirection < 0){        // Rotating left
          valueToSend = minValue;                       // Set velocity as minValue when rotating left
        }else if(eHwData[encNo].encoderDirection > 0){  // Rotating right
          valueToSend = maxValue;                       // Set velocity as maxValue when rotating right
        }
        if(portToSend & (1<<MIDI_USB))
          MIDI.sendNoteOn( paramToSend, valueToSend, channelToSend);
        if(portToSend & (1<<MIDI_HW))
          MIDIHW.sendNoteOn( paramToSend, valueToSend, channelToSend);
      }break;
      case rotaryMessageTypes::rotary_msg_cc:
      case rotaryMessageTypes::rotary_msg_vu_cc:{
        if(portToSend & (1<<MIDI_USB))
          MIDI.sendControlChange( paramToSend, valueToSend, channelToSend);
        if(portToSend & (1<<MIDI_HW))
          MIDIHW.sendControlChange( paramToSend, valueToSend, channelToSend);
      }break;
      case rotaryMessageTypes::rotary_msg_pc_rel:{
        if(portToSend & (1<<MIDI_USB)){
          MIDI.sendProgramChange( valueToSend, channelToSend);
          currentProgram[MIDI_USB][channelToSend - 1] = valueToSend;
        }
       
        if(portToSend & (1<<MIDI_HW)){
          MIDIHW.sendProgramChange( valueToSend, channelToSend);
          currentProgram[MIDI_HW][channelToSend - 1] = valueToSend;
        }
      }break;

      case rotaryMessageTypes::rotary_msg_nrpn:{
        if(portToSend & (1<<MIDI_USB)){
          MIDI.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
          MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
        }
        if(portToSend & (1<<MIDI_HW)){
          MIDIHW.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
          MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
        }
      }break;
      case rotaryMessageTypes::rotary_msg_rpn:{
        if(portToSend & (1<<MIDI_USB)){
          MIDI.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
          MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
        }
        if(portToSend & (1<<MIDI_HW)){
          MIDIHW.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
          MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
        }
      }break;
      case rotaryMessageTypes::rotary_msg_pb:{
        int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
        if(portToSend & (1<<MIDI_USB))
          MIDI.sendPitchBend( valuePb, channelToSend);    
        if(portToSend & (1<<MIDI_HW))
          MIDIHW.sendPitchBend( valuePb, channelToSend);    
      }break;
      case rotaryMessageTypes::rotary_msg_key:{
        uint8_t key = 0;
        uint8_t modifier = 0;

        if(eHwData[encNo].encoderDirection < 0){       // turned left
          if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
            key = encoder[encNo].switchConfig.parameter[rotary_keyLeft];  
            modifier = encoder[encNo].switchConfig.parameter[rotary_modifierLeft];  
          }else{
            key = encoder[encNo].rotaryConfig.parameter[rotary_keyLeft];  
            modifier = encoder[encNo].rotaryConfig.parameter[rotary_modifierLeft];  
          }
          
          switch(modifier){
            case 0:{}break;
            case 1:{  modifier = KEY_LEFT_ALT;   }break;
            case 2:{  modifier = KEY_LEFT_CTRL;  }break;
            case 3:{  modifier = KEY_LEFT_SHIFT; }break;
            default: break;
          }
          if(modifier)   // if different than 0, press modifier
            YTXKeyboard->press(modifier);
          if(key)  
            YTXKeyboard->press(key);
        }else if(eHwData[encNo].encoderDirection > 0){ //turned right
          // SERIALPRINTLN(F("Key Right action triggered"));
          // SERIALPRINT(F("Modifier right: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_modifierRight]);
          // SERIALPRINT(F("Key right: "));SERIALPRINTLN(encoder[encNo].rotaryConfig.parameter[rotary_keyRight]);
          if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
            key = encoder[encNo].switchConfig.parameter[rotary_keyRight];  
            modifier = encoder[encNo].switchConfig.parameter[rotary_modifierRight];  
          }else{
            key = encoder[encNo].rotaryConfig.parameter[rotary_keyRight];  
            modifier = encoder[encNo].rotaryConfig.parameter[rotary_modifierRight];  
          }

          switch(modifier){
            case 1:{  modifier = KEY_LEFT_ALT;   }break;
            case 2:{  modifier = KEY_LEFT_CTRL;  }break;
            case 3:{  modifier = KEY_LEFT_SHIFT; }break;
            default: break;
          }
          if(modifier)   // if different than 0, press modifier
            YTXKeyboard->press(modifier);
          if(key)
            YTXKeyboard->press(key);
        }
        millisKeyboardPress = millis()+KEYBOARD_MILLIS;
        keyboardReleaseFlag = true; 
      }break;
    }
  }
      
  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);

  if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Encoder, encNo) != lastComponentInfoId)){
    SendComponentInfo(ytxIOBLOCK::Encoder, encNo);
  }
  
  if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){
    eHwData[encNo].encoderValuePrev2cc = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
    uint16_t valueToSend2cc = eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;

    if(isAbsolute){
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, encNo, valueToSend, encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE);           
      if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){
        feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, encNo, eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc, 
                                                           encMData[moduleNo].orientation, NO_SHIFTER, NO_BANK_UPDATE); 
      }
   
    }
  }
}

void EncoderInputs::SetEncoderValue(uint8_t bank, uint8_t encNo, uint16_t value){
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;
  
  // Get config info for this encoder
  minValue = encoder[encNo].rotaryConfig.parameter[rotary_minMSB]<<7 | encoder[encNo].rotaryConfig.parameter[rotary_minLSB];
  maxValue = encoder[encNo].rotaryConfig.parameter[rotary_maxMSB]<<7 | encoder[encNo].rotaryConfig.parameter[rotary_maxLSB];
  msgType = encoder[encNo].rotaryConfig.message;

  // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else{
    minValue = minValue & 0x7F; 
    maxValue = maxValue & 0x7F;
  }

  bool invert = false;
  if(minValue > maxValue){    // If minValue is higher, invert behaviour
    invert = true;
  }

  // If bank is shifted, get current bank for the individual encoder
  if(IsBankShifted(encNo)){
    bank = eHwData[encNo].thisEncoderBank;
  }

  if      (value > (invert ? minValue : maxValue))  eBankData[bank][encNo].encoderValue = (invert ? minValue : maxValue);
  else if (value < (invert ? maxValue : minValue))  eBankData[bank][encNo].encoderValue = (invert ? maxValue : minValue);
  else{
    eBankData[bank][encNo].encoderValue = value;
  } 
  // update prev value
  eHwData[encNo].encoderValuePrev = value;
  
  if ((bank == (IsBankShifted(encNo) ? eHwData[encNo].thisEncoderBank : currentBank)) && !eBankData[bank][encNo].shiftRotaryAction){
    if(encoder[encNo].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_vu_cc){
      feedbackHw.SetChangeEncoderFeedback(FB_ENC_VUMETER, 
                                          encNo, 
                                          feedbackHw.GetVumeterValue(encNo),   
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                          NO_SHIFTER, 
                                          NO_BANK_UPDATE,
                                          NO_COLOR_CHANGE,                // it's not color change message
                                          NO_VAL_TO_INT,
                                          EXTERNAL_FEEDBACK);
      feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC,         
                                          encNo, 
                                          eBankData[bank][encNo].encoderValue, 
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                          NO_SHIFTER, 
                                          NO_BANK_UPDATE,
                                          NO_COLOR_CHANGE,                // it's not color change message
                                          NO_VAL_TO_INT,
                                          EXTERNAL_FEEDBACK);
    }else{
      feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                          encNo, 
                                          eBankData[bank][encNo].encoderValue, 
                                          encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                          NO_SHIFTER, 
                                          NO_BANK_UPDATE,
                                          NO_COLOR_CHANGE,                // it's not color change message
                                          NO_VAL_TO_INT,
                                          EXTERNAL_FEEDBACK);
      if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){
        feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, 
                                            encNo, 
                                            eBankData[bank][encNo].encoderValue2cc, 
                                            encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                            NO_SHIFTER, 
                                            NO_BANK_UPDATE,
                                            NO_COLOR_CHANGE,                // it's not color change message
                                            NO_VAL_TO_INT,
                                            EXTERNAL_FEEDBACK);
      }  
    }  
  }
}

void EncoderInputs::SetEncoderShiftValue(uint8_t bank, uint8_t encNo, uint16_t value){
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;
  
  // Get config info for this encoder
  // HOW TO KNOW IF INCOMING MIDI MESSAGE IS FOR DOUBLE CC OR SHIFTED ACTION???
  
  minValue = encoder[encNo].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
  maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
  msgType = encoder[encNo].switchConfig.message;


  // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
  if( msgType == rotary_msg_nrpn || msgType == rotary_msg_rpn || msgType == rotary_msg_pb || 
      msgType == switch_msg_nrpn || msgType == switch_msg_rpn || msgType == switch_msg_pb){
    is14bits = true;      
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }

  bool invert = false;
  if(minValue > maxValue){    // If minValue is higher, invert behaviour
    invert = true;
  }
  // 
  if(value > (invert ? minValue : maxValue)){
    eBankData[bank][encNo].encoderShiftValue = (invert ? minValue : maxValue);
  }
  else if(value < (invert ? maxValue : minValue)){
    eBankData[bank][encNo].encoderShiftValue = (invert ? maxValue : minValue);
  }
  else{
    eBankData[bank][encNo].encoderShiftValue = value;
  }
  // update prev value
  eHwData[encNo].encoderValuePrev = value;
    
  if (bank == currentBank && eBankData[bank][encNo].shiftRotaryAction){
    feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                        encNo, 
                                        eBankData[bank][encNo].encoderShiftValue, 
                                        encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                        NO_SHIFTER, 
                                        NO_BANK_UPDATE,
                                        NO_COLOR_CHANGE,                // it's not color change message
                                        NO_VAL_TO_INT,
                                        EXTERNAL_FEEDBACK);
    
  }
}

void EncoderInputs::SetEncoder2cc(uint8_t bank, uint8_t encNo, uint16_t value){
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool is14bits = false;
  
  minValue = encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
  maxValue = encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];

  bool invert = false;
  if(minValue > maxValue){    // If minValue is higher, invert behaviour
    invert = true;
  }
  // 
  if(value > (invert ? minValue : maxValue)){
    eBankData[bank][encNo].encoderValue2cc = (invert ? minValue : maxValue);
  }
  else if(value < (invert ? maxValue : minValue)){
    eBankData[bank][encNo].encoderValue2cc = (invert ? maxValue : minValue);
  }
  else{
    eBankData[bank][encNo].encoderValue2cc = value;
  }
  // update prev value
  eHwData[encNo].encoderValuePrev2cc = value;
    
  if (bank == currentBank){
    feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                        encNo, 
                                        eBankData[bank][encNo].encoderValue, 
                                        encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                        NO_SHIFTER, 
                                        NO_BANK_UPDATE,
                                        NO_COLOR_CHANGE,                // it's not color change message
                                        NO_VAL_TO_INT,
                                        EXTERNAL_FEEDBACK);
    feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, 
                                        encNo, 
                                        eBankData[bank][encNo].encoderValue2cc, 
                                        encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                        NO_SHIFTER, 
                                        NO_BANK_UPDATE,
                                        NO_COLOR_CHANGE,                // it's not color change message
                                        NO_VAL_TO_INT,
                                        EXTERNAL_FEEDBACK);
  }
}

void EncoderInputs::SetEncoderSwitchValue(uint8_t bank, uint8_t encNo, uint16_t newValue){

  eBankData[bank][encNo].switchLastValue = newValue & 0x3FFF;   // lastValue is 14 bit

  if( newValue > 0 )
    eBankData[bank][encNo].switchInputState = true;  
  else
    eBankData[bank][encNo].switchInputState = false;  

  // if input is toggle, update prev state so next time a user presses, it will toggle correctly.
  if(encoder[encNo].switchConfig.action == switchActions::switch_toggle){
    eBankData[bank][encNo].switchInputStatePrev = eBankData[bank][encNo].switchInputState;
  }
  
  if (bank == currentBank){
    uint16_t fbValue = 0;
    // If local behaviour is always on, set value to true always
    if(encoder[encNo].switchFeedback.source & fb_src_local && encoder[encNo].switchFeedback.localBehaviour == fb_lb_always_on){
      fbValue = true;
    }else{
      fbValue = eBankData[bank][encNo].switchLastValue;
    }
    feedbackHw.SetChangeEncoderFeedback(FB_ENC_SWITCH, 
                                        encNo, 
                                        fbValue, 
                                        encMData[ENC_MODULE_NUMBER(encNo)].orientation, 
                                        NO_SHIFTER, 
                                        NO_BANK_UPDATE,
                                        NO_COLOR_CHANGE,                // it's not color change message
                                        NO_VAL_TO_INT,
                                        EXTERNAL_FEEDBACK);
  }

}

void EncoderInputs::SetProgramChange(uint8_t port,uint8_t channel, uint8_t program){
  currentProgram[port][channel] = program&0x7F;
  return;
}

void EncoderInputs::SetBankForEncoders(uint8_t newBank){
  for(int encNo = 0; encNo < nEncoders; encNo++){
    if(IsBankShifted(encNo)){
      eHwData[encNo].bankShifted = false;
      if(newBank == currentBank){
        eBankData[newBank][encNo].switchInputState                          = 0;
        eBankData[newBank][encNo].switchInputStatePrev                      = 0;
        eBankData[newBank][encNo].switchLastValue                           = 0;
      }else{
        // Swap back switch state data
        bool auxState                                                       = eBankData[newBank][encNo].switchInputState;
        bool auxStatePrev                                                   = eBankData[newBank][encNo].switchInputStatePrev;
        bool auxValue                                                       = eBankData[newBank][encNo].switchLastValue;
        eBankData[newBank][encNo].switchInputState                          = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
        eBankData[newBank][encNo].switchInputStatePrev                      = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev;
        eBankData[newBank][encNo].switchLastValue                           = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue;
        eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState     = auxState;
        eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputStatePrev = auxStatePrev;
        eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue      = auxValue;  
      }
      
    }
    eHwData[encNo].thisEncoderBank = newBank;
    bool isAbsolute = (encoder[encNo].rotBehaviour.hwMode == rotaryModes::rot_absolute) ||          // Set absolute mode if it is configured as such
                      (encoder[encNo].rotaryConfig.message != rotaryMessageTypes::rotary_msg_cc);   // Or if it isn't a CC. Relative only for CC encoders
    
    if(isAbsolute)
      eHwData[encNo].encoderValuePrev = eBankData[newBank][encNo].encoderValue;
  }
}

uint16_t EncoderInputs::GetEncoderValue(uint8_t encNo){
  if(encNo < nEncoders){
    if(eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction){
      return eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue;
    }else{
      return eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue;
    }
    
  }   
}

EncoderInputs::encoderBankData* EncoderInputs::GetCurrentEncoderStateData(uint8_t bank, uint8_t encNo){
  if(begun){
    return &eBankData[bank][encNo];
  }else{
    return NULL;
  }
}

uint16_t EncoderInputs::GetEncoderValue2(uint8_t encNo){
  if(encNo < nEncoders){
    return eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderValue2cc;
  }   
}

uint16_t EncoderInputs::GetEncoderShiftValue(uint8_t encNo){
  if(encNo < nEncoders){
    return eBankData[eHwData[encNo].thisEncoderBank][encNo].encoderShiftValue;
  }   
}

uint16_t EncoderInputs::GetEncoderSwitchValue(uint8_t encNo){
  uint16_t retValue = 0;
  if(encNo < nEncoders){
    if((encoder[encNo].switchFeedback.source & fb_src_local) && encoder[encNo].switchFeedback.localBehaviour == fb_lb_always_on){
      retValue =  encoder[encNo].switchConfig.parameter[switch_maxValue_MSB] << 7 |
                  encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
      if(IS_ENCODER_SW_7_BIT(encNo)){
        retValue &= 0x7F;
      }
    }else{
      retValue = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchLastValue;
    }   
//    SERIALPRINT(F("Get encoder switch ")); SERIALPRINT(encNo); SERIALPRINT(F(" value: "));SERIALPRINTLN(eBankData[currentBank][encNo].switchLastValue);
    return retValue;
  }       
}

bool EncoderInputs::GetEncoderSwitchState(uint8_t encNo){
  uint16_t retValue = 0;
  if(encNo < nEncoders){
    if(encoder[encNo].switchFeedback.source & fb_src_local && encoder[encNo].switchFeedback.localBehaviour == fb_lb_always_on){
      retValue = 1;
    }else{
      retValue = eBankData[eHwData[encNo].thisEncoderBank][encNo].switchInputState;
    }   
//    SERIALPRINT(F("Get encoder switch ")); SERIALPRINT(encNo); SERIALPRINT(F(" state: "));SERIALPRINTLN(eBankData[currentBank][encNo].switchInputState);
    return retValue;
  }       
}

uint8_t EncoderInputs::GetThisEncoderBank(uint8_t encNo){
  return eHwData[encNo].thisEncoderBank;
}

bool EncoderInputs::EncoderShiftedBufferMatch(uint16_t bufferIndex){
  for(int encNo = 0; encNo < nEncoders; encNo++){
    if(IsBankShifted(encNo)){
      
      // First check if we should update value and feedback in shifted bank
      bool valueUpdated = QSTBUpdateValue(eHwData[encNo].thisEncoderBank, 
                                          encNo,
                                          midiMsgBuf7[bufferIndex].message, 
                                          midiMsgBuf7[bufferIndex].channel, 
                                          midiMsgBuf7[bufferIndex].parameter, 
                                          midiMsgBuf7[bufferIndex].value, 
                                          midiMsgBuf7[bufferIndex].port);
      if(valueUpdated) midiMsgBuf7[bufferIndex].banksToUpdate &= ~(1 << eHwData[encNo].thisEncoderBank);

    }
  }
  // Then check if message is present in currentBank, so it is kept in buffer and it can be updated when encoder returns to current bank
  if((midiMsgBuf7[bufferIndex].banksToUpdate >> currentBank) & 0x1){
    return true;    // If message is present in currentBank 
  }
  return false;
}

bool EncoderInputs::IsShiftActionOn(uint8_t encNo){
  if(encNo < nEncoders)
    return  eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction;
}

bool EncoderInputs::IsFineAdj(uint8_t encNo){
  if(encNo < nEncoders)
    return  eBankData[eHwData[encNo].thisEncoderBank][encNo].shiftRotaryAction;
}

bool EncoderInputs::IsBankShifted(uint8_t encNo){
  if(encNo < nEncoders)
    return  eHwData[encNo].bankShifted;
}

bool EncoderInputs::IsDoubleCC(uint8_t encNo){
  if(encNo < nEncoders)
    return  eBankData[eHwData[encNo].thisEncoderBank][encNo].doubleCC;
}


uint8_t EncoderInputs::GetModuleOrientation(uint8_t moduleNo){
  if(moduleNo < nModules)
    return encMData[moduleNo].orientation;
}

bool EncoderInputs::EncodersInMotion(void){
  return (priorityCount > 0);
}

bool EncoderInputs::CaptureEncoderDiagnostics(uint8_t encNo, uint16_t durationMs, encoderDiagnosticData* out){
  if(!begun || !out || encNo >= nEncoders) return false;

  memset(out, 0, sizeof(encoderDiagnosticData));

  uint8_t moduleNo = ENC_MODULE_NUMBER(encNo);
  if(moduleNo >= nModules) return false;

  uint8_t moduleType = GetModuleType(moduleNo);
  out->encoder = encNo;
  out->module = moduleNo;
  out->moduleType = moduleType;
  out->pinA = defE41module.encPins[encNo%(defE41module.components.nEncoders)][0];
  out->pinB = defE41module.encPins[encNo%(defE41module.components.nEncoders)][1];
  out->minSampleMicros = 0x3FFF;

  if(moduleType != EncoderModuleTypes::E41H_D && moduleType != EncoderModuleTypes::E41V_D) return false;

  if(durationMs < 10) durationMs = 10;
  if(durationMs > 5000) durationMs = 5000;

  bool havePrev = false;
  uint8_t prevPinState = 0;
  uint32_t startMillis = millis();
  uint32_t lastWatchdogReset = startMillis;
  uint32_t lastControllerService = startMillis;
  uint32_t prevMicros = micros();

  while((uint16_t)(millis() - startMillis) < durationMs){
    uint32_t nowMillis = millis();
    if(nowMillis != lastControllerService){
      lastControllerService = nowMillis;
      for(uint8_t message = 0; message < USB_MIDI_MESSAGES_PER_LOOP; message++){
        if(!MIDI.read()){
          break;
        }
      }
      ServiceAuxControllerReset();
      bool pauseFeedbackForMemoryRecovery = ServiceAuxMemoryError();
      RecoverStaleAuxShow();
      if(!pauseFeedbackForMemoryRecovery &&
         (enableProcessing || feedbackHw.AuxRecoveryInProgress())){
        feedbackHw.Update();
      }
    }

    if(nowMillis - lastWatchdogReset > WATCHDOG_CHECK_MS){
      Watchdog.reset();
      lastWatchdogReset = nowMillis;
    }

    uint32_t nowMicros = micros();
    uint32_t sampleDelta = nowMicros - prevMicros;
    prevMicros = nowMicros;

    if(sampleDelta > 0x3FFF) sampleDelta = 0x3FFF;
    if(sampleDelta && sampleDelta < out->minSampleMicros) out->minSampleMicros = sampleDelta;
    if(sampleDelta > out->maxSampleMicros) out->maxSampleMicros = sampleDelta;

    ReadModule(moduleNo);
    uint16_t stateA = encMData[moduleNo].state;
    ReadModule(moduleNo);
    uint16_t stateB = encMData[moduleNo].state;
    if(stateA != stateB && out->readMismatches < 0x3FFF) out->readMismatches++;

    uint8_t pinState = 0;
    if(stateB & (1 << out->pinA)) pinState |= 2;
    if(stateB & (1 << out->pinB)) pinState |= 1;

    out->lastState = pinState;
    if(out->samples < 0x3FFF) out->samples++;
    if(out->stateCount[pinState] < 0x3FFF) out->stateCount[pinState]++;

    if(havePrev){
      uint8_t transition = (prevPinState << 2) | pinState;
      if(out->transitionCount[transition] < 0x3FFF) out->transitionCount[transition]++;

      uint8_t delta = prevPinState ^ pinState;
      if(delta == 3){
        if(out->invalidTransitions < 0x3FFF) out->invalidTransitions++;
      }else if(delta == 1 || delta == 2){
        bool cw =  (prevPinState == 0 && pinState == 2) ||
                   (prevPinState == 2 && pinState == 3) ||
                   (prevPinState == 3 && pinState == 1) ||
                   (prevPinState == 1 && pinState == 0);
        bool ccw = (prevPinState == 0 && pinState == 1) ||
                   (prevPinState == 1 && pinState == 3) ||
                   (prevPinState == 3 && pinState == 2) ||
                   (prevPinState == 2 && pinState == 0);

        if(cw && out->cwTransitions < 0x3FFF) out->cwTransitions++;
        if(ccw && out->ccwTransitions < 0x3FFF) out->ccwTransitions++;
      }
    }

    prevPinState = pinState;
    havePrev = true;
  }

  if(out->minSampleMicros == 0x3FFF) out->minSampleMicros = 0;
  return true;
}


void EncoderInputs::AddToPriority(uint8_t nModule){
  if (!priorityCount){
    priorityCount++;
    priorityList[0] = nModule;
    priorityTime = millis();
  }
  else if(priorityCount == 1 && nModule != priorityList[0]){
    priorityCount++;
    priorityList[1] = nModule;
    priorityTime = millis();
  }
}

uint8_t EncoderInputs::GetEncoderBrightness(uint8_t index){
  uint8_t brightness=0;
  uint8_t encAcc=0;
  for(int i=0;i<nModules;i++){
    uint8_t encPerModule;
    uint8_t moduleType = GetModuleType(i);
    switch (moduleType) {
      case EncoderModuleTypes::E41H:
      case EncoderModuleTypes::E41V:
      case EncoderModuleTypes::E41H_D:
      case EncoderModuleTypes::E41V_D:{
          encPerModule = defE41module.components.nEncoders;
          break;
      }
    }

    if(index>=encAcc && index<(encAcc+encPerModule)){

      switch (moduleType) {
        case EncoderModuleTypes::E41H:
        case EncoderModuleTypes::E41V:
          if(IsPowerConnected()){
            brightness = 100;
            // brightness = 200;
          }else{
            if(nEncoders<32){
              brightness = BRIGHTNESS_WOP;
            }else{
              brightness = 38;
            }
          }
          break;
        case EncoderModuleTypes::E41H_D:
        case EncoderModuleTypes::E41V_D:
          if(IsPowerConnected()){
            brightness = BRIGHTNESS_WITH_POWER;
          }else{
            if(nEncoders<32){
              brightness = BRIGHTNESS_WOP;
            }else{
              brightness = BRIGHTNESS_WOP_32_ENC;
            }     
          }
          break;
      }
      break;
    }
    encAcc += encPerModule;
  }
  return brightness;    
}

/*
  Check for possible alternate functions activated before a change in configuration and clear them
*/
void EncoderInputs::RefreshData(uint8_t b, uint8_t e){
  if(encoder[e].switchConfig.mode != switchModes::switch_mode_shift_rot){
    eBankData[b][e].shiftRotaryAction = false;  
    eBankData[b][e].encoderShiftValue     = 0;  
  }

  if(encoder[e].switchConfig.mode != switchModes::switch_mode_fine){
    eBankData[b][e].encFineAdjust = false;          
  }

  if(encoder[e].switchConfig.mode != switchModes::switch_mode_2cc){
    eBankData[b][e].doubleCC = false;
    eBankData[b][e].encoderValue2cc       = 0;
  }

  if(encoder[e].switchConfig.mode != switchModes::switch_mode_velocity){
    eBankData[b][e].buttonSensitivityControlOn = false;  
  }
}
