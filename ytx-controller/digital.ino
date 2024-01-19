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

#include "headers/DigitalInputs.h"

//----------------------------------------------------------------------------------------------------
// DIGITAL INPUTS FUNCTIONS
//----------------------------------------------------------------------------------------------------

void DigitalInputs::Init(uint8_t maxBanks, uint16_t numberOfDigital, SPIAdressableBUS* lowBUS, SPIAdressableBUS* highBUS) {
  nBanks = 0;
  nDigitals = 0;
  nModules = 0;
  
  if (!maxBanks || maxBanks == 0xFF || !numberOfDigital || numberOfDigital > MAX_DIGITAL_AMOUNT) return; // If number of digitals is zero or 0xFF (eeprom clean), return;
  
  // CHECK WHETHER AMOUNT OF DIGITAL INPUTS IN MODULES COMBINED MATCH THE AMOUNT OF DIGITAL INPUTS IN CONFIG
  // AMOUNT OF DIGITAL PORTS/MODULES
  for (int nPort = 0; nPort < DIGITAL_PORTS; nPort++) {
    for (int nMod = 0; nMod < MODULES_PER_PORT; nMod++) {
      switch (config->hwMapping.digital[nPort][nMod]) {
        case DigitalModuleTypes::ARC41:
        case DigitalModuleTypes::RB41: {
            amountOfDigitalInConfig[nPort] += defRB41module.components.nDigital;
          } break;
        case DigitalModuleTypes::RB42: {
            amountOfDigitalInConfig[nPort] += defRB42module.components.nDigital;
          } break;
        case DigitalModuleTypes::RB82: {
            amountOfDigitalInConfig[nPort] += defRB82module.components.nDigital;
          } break;
        default: break;
      }
    }
  }

  // If amount of digitals based on module count and amount on config match, continue
  if ((amountOfDigitalInConfig[0] + amountOfDigitalInConfig[1]) != numberOfDigital) {
    SERIALPRINTLN(F("Error in config: Number of digitales does not match modules in config"));
    SERIALPRINT(F("nDigitals: ")); SERIALPRINTLN(numberOfDigital);
    SERIALPRINT(F("In Modules: ")); SERIALPRINTLN(amountOfDigitalInConfig[0]+amountOfDigitalInConfig[1]);
    return;
  } else{
    // SERIALPRINT("Amount of digitales in port 1: "); SERIALPRINTLN(amountOfDigitalInConfig[DIGITAL_PORT_1]);
    // SERIALPRINT("Amount of digitales in port 2: "); SERIALPRINTLN(amountOfDigitalInConfig[DIGITAL_PORT_2]);
    // SERIALPRINTLN(F("nDigitals and module config match"));
  }

 // Take data in EEPROM as valid and set class parameters
  nBanks = maxBanks;
  nDigitals = numberOfDigital;
  nModules = modulesInConfig.digital[0] + modulesInConfig.digital[1];
  
  buttonVelocity = VELOCITY_SESITIVITY_OFF;
  overrideVelocity = false;

  // First dimension is an array of pointers, each pointing to a column - https://www.eskimo.com/~scs/cclass/int/sx9b.html
  dBankData = (digitalBankData**) memHost->AllocateRAM(nBanks * sizeof(digitalBankData*));
  digMData = (moduleData*) memHost->AllocateRAM(nModules * sizeof(moduleData));
 
  // Reset to bootloader if there isn't enough RAM
  if(FreeMemory() < ( nBanks*nDigitals*sizeof(digitalBankData) + 
                      nDigitals*sizeof(digitalHwData) + 800)){
    SERIALPRINTLN("NOT ENOUGH RAM / DIGITAL -> REBOOTING TO BOOTLOADER...");
    delay(500);
    config->board.bootFlag = 1;                                            
    byte bootFlagState = 0;
    eep.read(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));
    bootFlagState |= 1;
    eep.write(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));

    SelfReset(RESET_TO_CONTROLLER);
  }

  // Allocate RAM for digital controls data
  dHwData = (digitalHwData*) memHost->AllocateRAM(nDigitals * sizeof(digitalHwData));

  // Second dimension is an array for each bank
  for (int b = 0; b < nBanks; b++) {
    #if defined(DISABLE_DIGITAL_BANKS)
      // Allocate only first bank and reference other banks to firs bank
      if(b==0){
        dBankData[b] = (digitalBankData*) memHost->AllocateRAM(nDigitals * sizeof(digitalBankData));
      }
      else{
        dBankData[b] = dBankData[0];
      }
    #else
      // Allocate all banks
      dBankData[b] = (digitalBankData*) memHost->AllocateRAM(nDigitals * sizeof(digitalBankData));
    #endif
  }

  // Init all the data
  for (int d = 0; d < nDigitals; d++) {
    dHwData[d].digitalHWState = 0;
    dHwData[d].digitalHWStatePrev = 0;
    dHwData[d].doubleClick = 0;
    dHwData[d].localStartUpEnabled = false;
    //digital[d].feedback.lowIntensityOff = true;
  }

  // Set all elements in arrays to 0
  for (int b = 0; b < nBanks; b++) {
    for (int d = 0; d < nDigitals; d++) {
      dBankData[b][d].digitalInputState = 0;
      dBankData[b][d].digitalInputStatePrev = 0;
      dBankData[b][d].lastValue = 0;
    }
  }
  
  // SET PROGRAM CHANGE TO 0 FOR ALL CHANNELS
  for (int c = 0; c < 16; c++) {
    currentProgram[MIDI_USB][c] = 0;
    currentProgram[MIDI_HW][c] = 0;
  }

  if(lowBUS != NULL){
    // DISABLE HARDWARE ADDRESSING FOR ALL CHIPS - ONLY NEEDED FOR RESET
    lowBUS->DisableHWAddress(MCP23017_BASE_ADDRESS);
  }
  if(highBUS != NULL){
    // DISABLE HARDWARE ADDRESSING FOR ALL CHIPS - ONLY NEEDED FOR RESET
    highBUS->DisableHWAddress(MCP23017_BASE_ADDRESS);
  }

  digitalsModule = (SPIGPIOExpander**)memHost->AllocateRAM(nModules*sizeof(SPIGPIOExpander**));
  
  // Addressing for MCP IC's
  for (int moduleNo = 0; moduleNo < nModules; moduleNo++) {
    byte chipSelect = 0;
    byte address = moduleNo % 8;
    byte nextAddress = (address+1) % 8;

    // Begin and initialize each SPI IC
    digitalsModule[moduleNo] = new SPIGPIOExpander;
    
    if (moduleNo < 8){
      digitalsModule[moduleNo]->begin(lowBUS, address, nextAddress);
    }
    else{
      digitalsModule[moduleNo]->begin(highBUS, address, nextAddress);
    }
    // if there are more than 1 modules, chain address
    /*
     * MODULE 0 - ADDRESS PINS FOR MODULE 1 -> 001
     * MODULE 1 - ADDRESS PINS FOR MODULE 2 -> 010
     * MODULE 2 - ADDRESS PINS FOR MODULE 3 -> 011 ...
     */
  }

  // Re-address chips to prevent addressing bug when resetting
  for (int moduleNo = 0; moduleNo < nModules; moduleNo++) {
    byte address = moduleNo % 8;
    if (nModules > 1)
      SetNextAddress(moduleNo, address + 1);
  }

  // SERIALPRINTLN(F("After begin and HW address"));
  // readAllRegs();
  // SERIALPRINTLN();
  // while(1);

  // First module's buttons start at index 0
  digMData[0].digitalIndexStart = 0;
  
  // Scan interval to spread the modules along the total scan time
  individualScanInterval = DIGITAL_SCAN_INTERVAL/nModules;
  generalMillis = millis();
  
  // Init every modules data
  for (int moduleNo = 0; moduleNo < nModules; moduleNo++) {
    digMData[moduleNo].type = config->hwMapping.digital[moduleNo / 8][moduleNo % 8];
    
    digMData[moduleNo].state = 0;
    digMData[moduleNo].statePrev = 0;
    digMData[moduleNo].antMillisScan = millis();

    // If there are more than 1 modules, get start index for each. Since modules have N number of digitals, proper indexing
    // of each digital input requires to have a preset start index for each module.
    if (nModules > 1) {
      if (moduleNo < nModules - 1) {   // need start address from module 1 up to module nModules-1. First run gets amount of digitals in module 0 
                                    // and sets start index on module 1, second run gets amnt on module 1 and sets module 2 and so on...
        // GET START INDEX FOR EACH MODULE
        switch (digMData[moduleNo].type) {
          case DigitalModuleTypes::ARC41:
          case DigitalModuleTypes::RB41: {
              digMData[moduleNo + 1].digitalIndexStart = digMData[moduleNo].digitalIndexStart + defRB41module.components.nDigital;
            } break;
          case DigitalModuleTypes::RB42: {
              digMData[moduleNo + 1].digitalIndexStart = digMData[moduleNo].digitalIndexStart + defRB42module.components.nDigital;
            } break;
          case DigitalModuleTypes::RB82: {
              digMData[moduleNo + 1].digitalIndexStart = digMData[moduleNo].digitalIndexStart + defRB82module.components.nDigital;
            } break;
          default: break;
        }
      }
    }
    // Initialize all pins, based on which module is in each position    
    for (int i = 0; i < 16; i++) {
      if (digMData[moduleNo].type == DigitalModuleTypes::RB41 || digMData[moduleNo].type == DigitalModuleTypes::ARC41) {
        for ( int j = 0; j < defRB41module.components.nDigital; j++) {
          if (i == defRB41module.rb41pins[j])
            digitalsModule[moduleNo]->pullUp(i, HIGH);
        }
      } else if (digMData[moduleNo].type == DigitalModuleTypes::RB42) {
        for ( int j = 0; j < defRB42module.components.nDigital; j++) {
          if (i == defRB42module.rb42pins[j])
            digitalsModule[moduleNo]->pullUp(i, HIGH);
        }
      } else if (digMData[moduleNo].type == DigitalModuleTypes::RB82) {    // Initialize RB82 rows as INPUTS, and columns as OUTPUTS
        for (int rowIndex = 0; rowIndex < RB82_ROWS; rowIndex++) {
          digitalsModule[moduleNo]->pullUp(defRB82module.rb82pins[ROWS][rowIndex], LOW);
          digitalsModule[moduleNo]->pinMode(defRB82module.rb82pins[ROWS][rowIndex], INPUT);
        }
        for (int colIndex = 0; colIndex < RB82_COLS; colIndex++) {
          digitalsModule[moduleNo]->pullUp(defRB82module.rb82pins[COLS][colIndex], LOW);
          digitalsModule[moduleNo]->pinMode(defRB82module.rb82pins[COLS][colIndex], OUTPUT);
        }
      }
    }
    // get initial state for each module
    digMData[moduleNo].state = digitalsModule[moduleNo]->digitalRead();
  }

  begun = true;
}

// #define PRINT_MODULE_STATE_DIG

void DigitalInputs::Read(void) {
  if (!nBanks || !nDigitals || !nModules) return;  // if no banks, no digital inputs or no modules are configured, exit here

  if(!begun) return;    // If didn't go through INIT, return;
  
  byte nButtonsInModule = 0;
  static byte moduleNo = 0;

  // DIGITAL MODULES ARE SCANNED EVERY "DIGITAL_SCAN_INTERVAL" EACH 
  // BUT EVERY "individualScanInterval" THE PROGRAM READS ONE MODULE
  // IF THERE ARE N MODULES,  individualScanInterval = DIGITAL_SCAN_INTERVAL/N (ms)
  if (millis() - generalMillis > individualScanInterval) {
    generalMillis = millis();
//    SERIALPRINT(F("Reading module: ")); SERIALPRINT(moduleNo);  SERIALPRINT(F(" at millis(): ")); SERIALPRINT(millis()); 
//    SERIALPRINT(F("\tElapsed time since last read: ")); SERIALPRINTLN(millis() - digMData[moduleNo].antMillisScan);
    digMData[moduleNo].antMillisScan = millis();
    
    // FOR EACH MODULE IN CONFIG, READ DIFFERENTLY
    if (digMData[moduleNo].type == DigitalModuleTypes::RB82) {   // FOR RB82
      // MATRIX MODULES
     // iterate the columns
      #if defined(PRINT_MODULE_STATE_DIG)
        for (int i = 0; i < 16; i++) {
          SERIALPRINT( (digMData[moduleNo].state >> (15 - i)) & 0x01, BIN);
          if (i == 9 || i == 6) SERIALPRINT(F(" "));
        }
        if(moduleNo == nModules - 1) SERIALPRINT(F("\n"));
        else                      SERIALPRINT(F("\t"));
      #endif

      // Cycle for all columns
      for (int colIndex = 0; colIndex < RB82_COLS; colIndex++) {
        // col: set to output to low
        byte colPin = defRB82module.rb82pins[COLS][colIndex];

        // Set column output low
        digitalsModule[moduleNo]->digitalWrite(colPin, LOW);

        // 1- Set PullUp on all rows
        uint16_t pullUpState = 0;
        for (int rowIndex = 0; rowIndex < RB82_ROWS; rowIndex++) {
          byte rowPin = defRB82module.rb82pins[ROWS][rowIndex];
          pullUpState |= (1 << rowPin);
        }
        digitalsModule[moduleNo]->writeWord(GPPUA, pullUpState);   
        
        // 2- Read state of a whole row and update new state
        digMData[moduleNo].state = digitalsModule[moduleNo]->digitalRead();  // READ ENTIRE ROW
        // row: interate through the rows
        for (int rowIndex = 0; rowIndex < RB82_ROWS; rowIndex++) {
          uint8_t rowPin = defRB82module.rb82pins[ROWS][rowIndex];
          uint8_t mapIndex = defRB82module.buttonMapping[rowIndex][colIndex];
          dHwData[digMData[moduleNo].digitalIndexStart + mapIndex].digitalHWState = !((digMData[moduleNo].state >> rowPin) & 0x1); // Check state for each input

          // 3- Check if input changed state      
          CheckIfChanged(digMData[moduleNo].digitalIndexStart + mapIndex);
        }
        
        // disable the column
        digitalsModule[moduleNo]->digitalWrite(colPin, HIGH);
      }
    } else {
      digMData[moduleNo].state = digitalsModule[moduleNo]->digitalRead();  // READ ENTIRE MODULE

      #if defined(PRINT_MODULE_STATE_DIG)
        for (int i = 0; i < 16; i++) {
          SERIALPRINT( (digMData[moduleNo].state >> (15 - i)) & 0x01, BIN);
          if (i == 9 || i == 6) SERIALPRINT(F(" "));
        }
        if(moduleNo == nModules - 1) SERIALPRINT(F("\n"));
        else                      SERIALPRINT(F("\t"));
      #endif

      if ( digMData[moduleNo].state != digMData[moduleNo].statePrev) {  // if module state changed
        digMData[moduleNo].statePrev = digMData[moduleNo].state;  // update state
        
        // Get number of digital inputs in module
        if (digMData[moduleNo].type == DigitalModuleTypes::RB42) {
          nButtonsInModule = defRB42module.components.nDigital;
        } else if (digMData[moduleNo].type == DigitalModuleTypes::RB41 || digMData[moduleNo].type == DigitalModuleTypes::ARC41) {
          nButtonsInModule = defRB41module.components.nDigital;
        }

        for (int nBut = 0; nBut < nButtonsInModule; nBut++) { // check each digital input. nBut is a local index inside the module
          byte indexDigital = digMData[moduleNo].digitalIndexStart + nBut; // global digital index is start index for module plus local index in module
          
          // get pin for each individual input based on module type
          byte pin = 0;
          switch(digMData[moduleNo].type){
            case DigitalModuleTypes::RB42:{
              pin = defRB42module.rb42pins[nBut];
            }break;
            case DigitalModuleTypes::ARC41:
            case DigitalModuleTypes::RB41:{
              pin = defRB41module.rb41pins[nBut];
            }break;
            default: return; break;
          }

          dHwData[indexDigital].digitalHWState = !((digMData[moduleNo].state >> pin) & 0x0001);  // read the state of each input
          
          CheckIfChanged(indexDigital);   // check changes in digital input
        }
      }
    }
    moduleNo++;
    if (moduleNo >= nModules)  moduleNo = 0;    // If last module was scanned, start over
  }
}


void DigitalInputs::CheckIfChanged(uint8_t indexDigital) {

  if ( dHwData[indexDigital].digitalHWState != dHwData[indexDigital].digitalHWStatePrev) {             // and bounce time elapsed   // REVIEW BOUNCE ROUTINE
    dHwData[indexDigital].digitalHWStatePrev = dHwData[indexDigital].digitalHWState;    // update state

    // HW-ID for digital inputs starts after encoders
    if (CheckIfBankShifter(indexDigital+config->inputs.encoderCount, dHwData[indexDigital].digitalHWState)){
      // IF IT IS BANK SHIFTER, RETURN, DON'T DO ACTION FOR THIS SWITCH
      //SERIALPRINTLN(F("IS SHIFTER"));
      return;
    }  

    if(digital[indexDigital].actionConfig.message == digitalMessageTypes::digital_msg_none) return;   // if digital input is disabled in config, return


    bool momentary =  digital[indexDigital].actionConfig.action == switch_momentary                        ||  // IF key, PC#, PC+ or PC- treat as momentary
                      digital[indexDigital].actionConfig.message == digitalMessageTypes::digital_msg_key   ||     
                      digital[indexDigital].actionConfig.message == digitalMessageTypes::digital_msg_pc    ||
                      digital[indexDigital].actionConfig.message == digitalMessageTypes::digital_msg_pc_m  || 
                      digital[indexDigital].actionConfig.message == digitalMessageTypes::digital_msg_pc_p;
    
    if(momentary || testDigital){   
        dBankData[currentBank][indexDigital].digitalInputState = dHwData[indexDigital].digitalHWState;  
      }else{  // TOGGLE
        if (dHwData[indexDigital].digitalHWState)
          dBankData[currentBank][indexDigital].digitalInputState = !dBankData[currentBank][indexDigital].digitalInputState;
      } 
    // Then do whatever the configuration says this input should do
    DigitalAction(indexDigital, dBankData[currentBank][indexDigital].digitalInputState);
  }
}

/*
 * SetNextAddress for module addressing
 */
void DigitalInputs::SetNextAddress(uint8_t moduleNo, uint8_t addr) {
  for (int i = 0; i < 3; i++) {
    digitalsModule[moduleNo]->pinMode(defRB41module.nextAddressPin[i], OUTPUT); 
  }
  for (int i = 0; i < 3; i++) {
    digitalsModule[moduleNo]->digitalWrite(defRB41module.nextAddressPin[i], (addr >> i) & 1);
  }
}

void DigitalInputs::DigitalAction(uint16_t dInput, uint16_t state, bool initDump) {
  // Check if new state is different from previous state
  if(dBankData[currentBank][dInput].digitalInputState != dBankData[currentBank][dInput].digitalInputStatePrev || initDump){
    dBankData[currentBank][dInput].digitalInputStatePrev = dBankData[currentBank][dInput].digitalInputState;  // update previous
    
    if(initDump){
      if(CheckIfBankShifter(dInput+config->inputs.encoderCount, dHwData[dInput].digitalHWState)){
        return;
      }
    }

    // Get config parameters for digital action / message
    uint16_t paramToSend = digital[dInput].actionConfig.parameter[digital_MSB] << 7 |
                           digital[dInput].actionConfig.parameter[digital_LSB];
    byte channelToSend = digital[dInput].actionConfig.channel + 1;
    uint16_t minValue = digital[dInput].actionConfig.parameter[digital_minMSB] << 7 |
                        digital[dInput].actionConfig.parameter[digital_minLSB];
    uint16_t maxValue = digital[dInput].actionConfig.parameter[digital_maxMSB] << 7 |
                        digital[dInput].actionConfig.parameter[digital_maxLSB];
  
    uint16_t valueToSend = 0;
    bool programFb = false;

    // if OFF or ON, set to MIN and MAX value respectively
    
    if (state) {
      valueToSend = (overrideVelocity ? buttonVelocity : maxValue);
    } else {
      valueToSend = minValue;
    }

    if(IS_DIGITAL_7_BIT(dInput)){
      minValue &= 0x7F;
      maxValue &= 0x7F;
    }
  //    SERIALPRINT(dInput);SERIALPRINT(F(" -> "));
  //    SERIALPRINT(F("Param: "));SERIALPRINT(paramToSend);
  //    SERIALPRINT(F(" Valor: "));SERIALPRINT(valueToSend);
  //    SERIALPRINT(F(" Canal: "));SERIALPRINT(channelToSend);
  //    SERIALPRINT(F(" Min: "));SERIALPRINT(minValue);
  //    SERIALPRINT(F(" Max: "));SERIALPRINTLN(maxValue);
    switch (digital[dInput].actionConfig.message) {
      case digitalMessageTypes::digital_msg_note: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB))
          MIDI.sendNoteOn( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW))
          MIDIHW.sendNoteOn( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
      } break;
      case digitalMessageTypes::digital_msg_cc: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB))
          MIDI.sendControlChange( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW))
          MIDIHW.sendControlChange( paramToSend & 0x7f, valueToSend & 0x7f, channelToSend);
      } break;
      case digitalMessageTypes::digital_msg_pc: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB) && valueToSend && state){
          MIDI.sendProgramChange( paramToSend & 0x7f, channelToSend);
          currentProgram[MIDI_USB][channelToSend - 1] = paramToSend & 0x7f;
        }
        if ((digital[dInput].actionConfig.midiPort & (1<<MIDI_HW)) && valueToSend && state){
          MIDIHW.sendProgramChange( paramToSend & 0x7f, channelToSend);
          currentProgram[MIDI_HW][channelToSend - 1] = paramToSend & 0x7f;
        }
      } break;
      case digitalMessageTypes::digital_msg_pc_m: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB)) {
          uint8_t programToSend = minValue + currentProgram[MIDI_USB][channelToSend - 1];
          if ((programToSend > minValue) && state) {
            currentProgram[MIDI_USB][channelToSend - 1]--;
            programToSend--;
            MIDI.sendProgramChange(programToSend, channelToSend);
            programFb = true;
          }else if(!state)   programFb = true;
        }
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW)) {
          uint8_t programToSend = minValue + currentProgram[MIDI_HW][channelToSend - 1];
          if ((programToSend > minValue) && state) {
            currentProgram[MIDI_HW][channelToSend - 1]--;
            programToSend--;
            MIDIHW.sendProgramChange(programToSend, channelToSend);
            programFb = true;
          }else if(!state)   programFb = true;
        }
      } break;
      case digitalMessageTypes::digital_msg_pc_p: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB)) {
          uint8_t programToSend = minValue + currentProgram[MIDI_USB][channelToSend - 1];
          if ((programToSend < maxValue) && state) {
            currentProgram[MIDI_USB][channelToSend - 1]++;
            programToSend++;
            MIDI.sendProgramChange(programToSend, channelToSend);
            programFb = true;
          }else if(!state)   programFb = true;
        }
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW)) {
          uint8_t programToSend = minValue + currentProgram[MIDI_HW][channelToSend - 1];
          if ((programToSend < maxValue) && state) {
            currentProgram[MIDI_HW][channelToSend - 1]++;
            programToSend++;
            MIDIHW.sendProgramChange(programToSend, channelToSend);
            programFb = true;
          }else if(!state)   programFb = true;
        }
      } break;
      case digitalMessageTypes::digital_msg_nrpn: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB)) {
          MIDI.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
          MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
        }
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW)) {
          MIDIHW.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
          MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
        }
      } break;
      case digitalMessageTypes::digital_msg_rpn: {
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB)) {
          MIDI.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
          MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
        }
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW)) {
          MIDIHW.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
          MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
          MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);
        }
      } break;
      case digitalMessageTypes::digital_msg_pb: {
        int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_USB))
          MIDI.sendPitchBend( valuePb, channelToSend);
        if (digital[dInput].actionConfig.midiPort & (1<<MIDI_HW))
          MIDIHW.sendPitchBend( valuePb, channelToSend);
      } break;
      case digitalMessageTypes::digital_msg_key: {
        uint8_t modifier = 0;
        if(valueToSend == maxValue){
          switch(digital[dInput].actionConfig.parameter[digital_modifier]){
            case 0:{                              }break;
            case 1:{  modifier = KEY_LEFT_ALT;    }break;
            case 2:{  modifier = KEY_LEFT_CTRL;   }break;
            case 3:{  modifier = KEY_LEFT_SHIFT;  }break;
            default: break;
          }
          
          if(modifier)   // if different than 0, press modifier
            YTXKeyboard->press(modifier);
          if (digital[dInput].actionConfig.parameter[digital_key])
            YTXKeyboard->press(digital[dInput].actionConfig.parameter[digital_key]);  
        }else{
          YTXKeyboard->releaseAll();
        }
          
      } break;
    }
    // STATUS LED SET BLINK
    SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);

    if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Digital, dInput) != lastComponentInfoId)){
      SendComponentInfo(ytxIOBLOCK::Digital, dInput);
    }
    
    if(testDigital){
      SERIALPRINT(valueToSend != minValue ? F("PRESSED") : F("RELEASED"));
      SERIALPRINT(F("\t <- DIG "));SERIALPRINTLN(dInput);
    }

    // Check if feedback is local, or if action is keyboard (no feedback)
    if (digital[dInput].feedback.source & feedbackSource::fb_src_local          || 
        digital[dInput].actionConfig.message == digital_msg_pc                  ||
        (digital[dInput].actionConfig.message == digital_msg_pc_m) && programFb ||
        (digital[dInput].actionConfig.message == digital_msg_pc_p) && programFb ||
        digital[dInput].actionConfig.message == digital_msg_key                 || 
        dHwData[dInput].localStartUpEnabled                                     ||
        testDigital) {      
     // SET INPUT FEEDBACK
      uint16_t fbValue = 0;
      bool fbState = 0;
      // If local behaviour is always on, set value to true always
      if(digital[dInput].feedback.source & fb_src_local && digital[dInput].feedback.localBehaviour == fb_lb_always_on){
        fbValue = maxValue;
        fbState = true;
      }else{
        // Otherwise set to reflect input value
        if(digital[dInput].feedback.valueToColor && dBankData[currentBank][dInput].digitalInputState){
          fbValue = feedbackHw.GetCurrentDigitalFeedbackData(currentBank, dInput)->colorIndexPrev;
        }else{
          fbValue = valueToSend;
        }
        fbState = dBankData[currentBank][dInput].digitalInputState;
      }
      
      dBankData[currentBank][dInput].lastValue = valueToSend;
      // Set feedback for update
      feedbackHw.SetChangeDigitalFeedback(dInput, fbValue, fbState, NO_SHIFTER, NO_BANK_UPDATE);
    }
    //SERIALPRINT(F("Digital input state: ")); SERIALPRINT();
  }
}

/*
 * return input value for any digital input
 */
uint16_t DigitalInputs::GetDigitalValue(uint16_t digNo){
  uint16_t fbValue = 0;
  if(digNo < nDigitals){
    if(digital[digNo].feedback.source & fb_src_local && digital[digNo].feedback.localBehaviour == fb_lb_always_on){
      fbValue = digital[digNo].actionConfig.parameter[digital_maxMSB] << 7 |
                digital[digNo].actionConfig.parameter[digital_maxLSB];
      if(IS_DIGITAL_7_BIT(digNo)){
        fbValue &= 0x7F;
      }
    }else{
      fbValue = dBankData[currentBank][digNo].lastValue;
    }   
    return fbValue;
  }   
}

/*
 * return input state for any digital input
 */
bool DigitalInputs::GetDigitalState(uint16_t digNo){
  uint16_t fbState = 0;
  if(digNo < nDigitals){
    if(digital[digNo].feedback.source & fb_src_local || digital[digNo].feedback.localBehaviour == fb_lb_always_on){
      fbState = 1;
    }else{
      fbState = dBankData[currentBank][digNo].digitalInputState;
    }   
    return fbState;
  }   
}
/*
 * Set velocity for all buttons
 */

void DigitalInputs::SetButtonVelocity(uint8_t newVelocity){
  buttonVelocity = newVelocity;

  if(buttonVelocity == VELOCITY_SESITIVITY_OFF){   // Greater than 127
    overrideVelocity = false;
  }else if (overrideVelocity <= 127){
    overrideVelocity = true;
  }

}

/*
 * Get current velocity for all buttons
 */

uint8_t DigitalInputs::GetButtonVelocity(void){
  return buttonVelocity;
}
/*
 * Set input value for any digital input
 */
void DigitalInputs::SetDigitalValue(uint8_t bank, uint16_t digNo, uint16_t newValue){
  dBankData[bank][digNo].lastValue = newValue & 0x3FFF;   // lastValue is 14 bit
  if(dHwData[digNo].localStartUpEnabled){
    dHwData[digNo].localStartUpEnabled = false;
  }
  if( newValue > 0 )
    dBankData[bank][digNo].digitalInputState = true;  
  else
    dBankData[bank][digNo].digitalInputState = false;  
  
  // if input is toggle, update prev state so next time a user presses, it will toggle correctly.
  if(digital[digNo].actionConfig.action == switchActions::switch_toggle){
    dBankData[bank][digNo].digitalInputStatePrev = dBankData[bank][digNo].digitalInputState;
  }
  
  if (bank == currentBank){
    /// SET INPUT FEEDBACK
      uint16_t fbValue = 0;
      bool fbState = 0;
      // If local behaviour is always on, set value to true always
      if(digital[digNo].feedback.source & fb_src_local && digital[digNo].feedback.localBehaviour == fb_lb_always_on){
        fbValue = true;
        fbState = true;
      }else{
        // Otherwise set to reflect input value
        fbValue = dBankData[bank][digNo].lastValue;
        fbState = dBankData[currentBank][digNo].digitalInputState;
      }
      
      feedbackHw.SetChangeDigitalFeedback(digNo, 
                                          fbValue, 
                                          fbState, 
                                          NO_SHIFTER, NO_BANK_UPDATE, EXTERNAL_FEEDBACK);
  }
}

DigitalInputs::digitalBankData* DigitalInputs::GetCurrentDigitalStateData(uint8_t bank, uint16_t digNo){
  if(begun){
    return &dBankData[bank][digNo];
  }else{
    return NULL;
  }
}

void DigitalInputs::SetProgramChange(uint8_t port,uint8_t channel, uint8_t program){
  currentProgram[port][channel] = program&0x7F;
  return;
}

uint8_t DigitalInputs::GetDigitalButtonBrightness(uint8_t){
  uint8_t brightness=0;

  if(IsPowerConnected()){
    brightness = BRIGHTNESS_WITH_POWER;
  }else{
    brightness = BRIGHTNESS_WOP_32_ENC;
  }
  return brightness;    
}
