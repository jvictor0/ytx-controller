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

 #include "headers/AnalogInputs.h"

//----------------------------------------------------------------------------------------------------
// ANALOG METHODS
//----------------------------------------------------------------------------------------------------

void AnalogInputs::Init(byte maxBanks, byte numberOfAnalog, SPIAdressableBUS* spiBUS){
  nBanks = 0;
  nAnalog = 0;
  spiAnalogExpanderEnable = false;

  if(!maxBanks || !numberOfAnalog || numberOfAnalog > MAX_ANALOG_AMOUNT) return;  // If number of analog is zero, return;
  
  byte analogInConfig = 0;    // number of analog inputs in hwConfig (sum of inputs in modules) to compare with "numberOfAnalog"
  
  // CHECK WHETHER AMOUNT OF ANALOG INPUTS IN MODULES COMBINED MATCH THE AMOUNT OF ANALOG INPUTS IN CONFIG
  for (int nMux = 0; nMux < ANALOG_MUXES; nMux++) {
    for (int nMod = 0; nMod < ANALOG_MODULES_PER_MUX; nMod++) {

      int hwMapping = (nMux < 4) ? config->hwMapping.analog[nMux][nMod]&0x0F : (config->hwMapping.analog[nMux-4][nMod]>>4)&0x0F;

      switch (hwMapping) {
        case AnalogModuleTypes::P41:
        case AnalogModuleTypes::F41: {
            analogInConfig += defP41module.nAnalog;
            nMod++;                                     // A module of 4 components takes two module spaces, so jump one
          } break;
        case AnalogModuleTypes::JAL:
        case AnalogModuleTypes::JAF: {
            analogInConfig += defJAFmodule.nAnalog;
          } break;
        case AnalogModuleTypes::F21100: {
            analogInConfig += defF21100module.nAnalog;
          } break;
        default: break;
      }
    }
  }
  if (analogInConfig != numberOfAnalog) {
    SERIALPRINTLN(F("Error in config: Number of analog does not match modules in config"));
    SERIALPRINT(F("nAnalog: ")); SERIALPRINTLN(numberOfAnalog);
    SERIALPRINT(F("Modules: ")); SERIALPRINTLN(analogInConfig);
    return;
  } else {
    SerialUSB.println(F("nAnalog and module config match"));
  }

  SerialUSB.print(F("analogs: ")); SerialUSB.println(analogInConfig);

  if(analogInConfig>64){
    spiAnalogExpanderEnable = true;
  }
  
  //initialice SPI Analog Expander
  if(spiAnalogExpanderEnable){
    pinMode(A1, OUTPUT);//A1 its connected to RESET pin on spiAnalogExpander
    digitalWrite(A1, LOW);
    delay(5);
    digitalWrite(A1, HIGH);
    delay(5);
    // DISABLE HARDWARE ADDRESSING FOR ALL CHIPS - ONLY NEEDED FOR RESET
    spiBUS->DisableHWAddress(FIXED_ELEMENTS_BASE_ADDRESS);
    
    spiAnalogExpander = new SPIAnalogExpander();

    spiAnalogExpander->begin(spiBUS); // spi, chip select pin, address

    SPIAnalogExpanderParameters parameters;
    parameters.inputs = numberOfAnalog-32;
    parameters.expFilter = 0.25;
    parameters.noiseThreshold = 10;

    spiAnalogExpander->configure(&parameters); // nº of inputs, exponential filter constant, noise threshold
  }

  // Set output pins for multiplexers
  pinMode(_S0, OUTPUT);
  pinMode(_S1, OUTPUT);
  pinMode(_S2, OUTPUT);
  pinMode(_S3, OUTPUT);

  // init ADC peripheral
  FastADCsetup();

  // Take data in as valid and set class parameters
  nBanks = maxBanks;
  nAnalog = numberOfAnalog;
  
  analogMuxesWithElements = ((config->inputs.analogCount-1)/16) + 1;
  // SERIALPRINTLN("Analog ports: "); SERIALPRINTLN(analogMuxesWithElements);
  
  // analog update flags and timestamp
  updateValue = 0;
  antMillisAnalogUpdate = millis();
  
  // First dimension is an array of pointers, each pointing to a column - https://www.eskimo.com/~scs/cclass/int/sx9b.html
  aBankData = (analogBankData**) memHost->AllocateRAM(nBanks*sizeof(analogBankData*));

  // Reset to bootloader if there isn't enough RAM
  if(FreeMemory() < ( nBanks*nAnalog*sizeof(analogBankData) + 
                      nAnalog*sizeof(analogHwData) + 800)){
    SERIALPRINTLN("NOT ENOUGH RAM / ANALOG -> REBOOTING TO BOOTLOADER...");
    
    delay(500);
    config->board.bootFlag = 1;                                            
    byte bootFlagState = 0;
    eep.read(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));
    bootFlagState |= 1;
    eep.write(BOOT_FLAGS_ADDR, (byte *) &bootFlagState, sizeof(bootFlagState));

    SelfReset(RESET_TO_CONTROLLER);
  }

  aHwData = (analogHwData*) memHost->AllocateRAM(nAnalog*sizeof(analogHwData));

  // Allocate RAM for analog inputs data
  for (int b = 0; b < nBanks; b++){
    #if defined(DISABLE_ANALOG_BANKS)
      // Allocate only first bank and reference other banks to firs bank
      if(b==0){
        aBankData[b] = (analogBankData*) memHost->AllocateRAM(nAnalog*sizeof(analogBankData));
      }
      else{
        aBankData[b] = aBankData[0];
      }
    #else
      // Allocate all banks
      aBankData[b] = (analogBankData*) memHost->AllocateRAM(nAnalog*sizeof(analogBankData));
    #endif
  }
  
  // Set all elements in arrays to 0
  for(int b = 0; b < nBanks; b++){
    for(int i = 0; i < nAnalog; i++){
       aBankData[b][i].analogValue          = 0;
       aBankData[b][i].analogValuePrev      = 0;
       aBankData[b][i].hardwarePivot        = 0;
       aBankData[b][i].targetValuePivot     = 0;
       aBankData[b][i].flags.takeOverOn     = 0;
       aBankData[b][i].flags.reservedFlags  = 0;
    }
  }

  // INIT ANALOG DATA
  for(int i = 0; i < nAnalog; i++){
     if(spiAnalogExpanderEnable && i>=32){
      int spiInput = i - 32;
      aHwData[i].analogRawValue = spiAnalogExpander->analogRead(spiInput)&0x0FFF;
    }
    else{
      int mux = i < 16 ? MUX_A :  (i < 32 ? MUX_B : ( i < 48 ? MUX_C : MUX_D)) ;    // Select correct multiplexer for this input
      int muxChannel = i % NUM_MUX_CHANNELS;        
      aHwData[i].analogRawValue    = MuxAnalogRead(mux, muxChannel);
    }

    aHwData[i].analogRawValuePrev  = aHwData[i].analogRawValue;
    aHwData[i].analogDirection     = ANALOG_INCREASING;
    aHwData[i].analogDirectionRaw  = ANALOG_INCREASING;
    aHwData[i].exponentialFilter = 0;

  }

  // INIT TABLES
  minRawValue = ADC_MIN_BUS_COUNT + (ADC_MAX_COUNT*0.005); //begin with an excess of 0.5%
  maxRawValue = ADC_MAX_BUS_COUNT - (ADC_MAX_COUNT*0.005); //begin with an excess of 0.5%

  // Scale taper tables
  for(int i = i; i < FADER_TTE_PS45M_TABLE_SIZE; i++){
    TTE_PS45M_Taper[i][0] = (uint16_t)(TTE_PS45M_Taper_Template[i][0]*maxRawValue/100.0);
    TTE_PS45M_Taper[i][1] = (uint16_t)(TTE_PS45M_Taper_Template[i][1]*maxRawValue/100.0);
  }

  TTE_PS45M_Taper[0][0] = minRawValue;
  TTE_PS45M_Taper[0][1] = minRawValue;

  for(int i = i; i < FADER_ALPS_RSA0_TAPERS_TABLE_SIZE; i++){
    ALPS_RSA0_Taper[i][0] = (uint16_t)(ALPS_RSA0_Taper_Template[i][0]*maxRawValue/100.0);
    ALPS_RSA0_Taper[i][1] = (uint16_t)(ALPS_RSA0_Taper_Template[i][1]*maxRawValue/100.0);
  }

  ALPS_RSA0_Taper[0][0] = minRawValue;
  ALPS_RSA0_Taper[0][1] = minRawValue;

  begun = true;
}


void AnalogInputs::Read(){
  if(!nBanks || nBanks == 0xFF || !nAnalog || nAnalog == 0xFF) return;  // If number of analog or banks is zero or 0xFF (EEPROM cleared), return

  if(!begun) return;    // If didn't go through INIT, return;

  int aInput = 0;
  int nAnalogInMod = 0;
  bool isJoystickX = false;
  bool isJoystickY = false;
  bool isFaderModule = false;
  bool isLogFader = false;
  static byte initMuxRead = 0;
  static byte lastMuxRead = 1;

  uint8_t noiseTh = 0;

  // Scan all analog inputs to detect changes
  for (int nMux = initMuxRead; nMux < lastMuxRead; nMux++) {
    if(spiAnalogExpanderEnable && nMux==2){
      spiAnalogExpander->getActiveChannels();
    }
    for (int nMod = 0; nMod < ANALOG_MODULES_PER_MUX; nMod++) {
      int hwMapping = (nMux < 4) ? config->hwMapping.analog[nMux][nMod]&0x0F : (config->hwMapping.analog[nMux-4][nMod]>>4)&0x0F;

      isFaderModule = false; // reset flags for new component
      isLogFader = false;
      isJoystickX = false;

      // Get module type and number of analog inputs in it
      switch(hwMapping){
        case AnalogModuleTypes::P41:
        case AnalogModuleTypes::F41: {
          nAnalogInMod = defP41module.nAnalog;    // both have 4 components
          isFaderModule = (hwMapping == AnalogModuleTypes::F41) ? true : false;
        } break;
        case AnalogModuleTypes::F21100: {
          nAnalogInMod = defF21100module.nAnalog;    // have 2 components
          isFaderModule = true;
          isLogFader = true;
        } break;
        case AnalogModuleTypes::JAL:
        case AnalogModuleTypes::JAF: {
          isJoystickX = true;
          nAnalogInMod = defJAFmodule.nAnalog;    // both have 2 components
        }break;
        default:
          continue;
        break;
      }
      // Scan inputs for this module
      for(int a = 0; a < nAnalogInMod; a++){
        aInput = nMux*ANALOG_PER_MUX + nMod*ANALOG_PER_MODULES + a;  // establish which n° of analog input we're scanning 

        // SERIALPRINT("Read Analog input: "); SERIALPRINTLN(aInput);

        if(analog[aInput].message == analogMessageTypes::analog_msg_none) continue;   // check if input is disabled in config
        
        bool is14bit =  analog[aInput].message == analog_msg_nrpn || 
                        analog[aInput].message == analog_msg_rpn || 
                        analog[aInput].message == analog_msg_pb;
        
        noiseTh = is14bit ? NOISE_THRESHOLD_RAW_14BIT : NOISE_THRESHOLD_RAW_7BIT;

        if(spiAnalogExpanderEnable && aInput>=32){
          uint8_t expanderInput = aInput - 32;
          if(spiAnalogExpander->isActiveChannel(expanderInput)){
            int16_t newRead = spiAnalogExpander->analogRead(expanderInput);

            if(newRead >= 0){
              aHwData[aInput].analogRawValue = (uint16_t)newRead;
            }
          }
        }
        else{
          byte mux = aInput < 16 ? MUX_A :  (aInput < 32 ? MUX_B : ( aInput < 48 ? MUX_C : MUX_D)) ;    // Select correct multiplexer for this input
          byte muxChannel = aInput % NUM_MUX_CHANNELS;
        
        
          aHwData[aInput].analogRawValue = MuxAnalogRead(mux, muxChannel);         // Read analog value from MUX_A and channel 'aInput'

          // exponential average filter
          aHwData[aInput].analogRawValue = FilterGetNewExponentialAverage(aInput, aHwData[aInput].analogRawValue);             
        }

        // if raw value didn't change, do not go on
        if( aHwData[aInput].analogRawValue == aHwData[aInput].analogRawValuePrev ) continue;        
        
         // Adjust min and max raw values
        if(aHwData[aInput].analogRawValue < minRawValue && aHwData[aInput].analogRawValue >= ADC_MIN_BUS_COUNT){
          minRawValue = aHwData[aInput].analogRawValue;   // add one to the min raw value detected to ensure minValue
          ALPS_RSA0_Taper[0][0] = minRawValue;
          ALPS_RSA0_Taper[0][1] = minRawValue;
          TTE_PS45M_Taper[0][0] = minRawValue;
          TTE_PS45M_Taper[0][1] = minRawValue;
          // SERIALPRINT("New min raw value: "); SERIALPRINTLN(minRawValue);
        } 
        if(aHwData[aInput].analogRawValue > maxRawValue && aHwData[aInput].analogRawValue <= ADC_MAX_BUS_COUNT){
          maxRawValue = aHwData[aInput].analogRawValue;   // take one to the max raw value detected to ensure maxValue
          uint16_t maxRawValueGuard = 5;
          for(int i = 1; i < FADER_ALPS_RSA0_TAPERS_TABLE_SIZE; i++){
            ALPS_RSA0_Taper[i][0] = (uint16_t)(ALPS_RSA0_Taper_Template[i][0]*(maxRawValue-maxRawValueGuard)/100.0);
            ALPS_RSA0_Taper[i][1] = (uint16_t)(ALPS_RSA0_Taper_Template[i][1]*(maxRawValue-maxRawValueGuard)/100.0);
          }
          for(int i = 1; i < FADER_TTE_PS45M_TABLE_SIZE; i++){
            TTE_PS45M_Taper[i][0] = (uint16_t)(TTE_PS45M_Taper_Template[i][0]*maxRawValue/100.0);
            TTE_PS45M_Taper[i][1] = (uint16_t)(TTE_PS45M_Taper_Template[i][1]*maxRawValue/100.0);
          }
          // SERIALPRINT("New max raw value: "); SERIALPRINTLN(maxRawValue);
        } 

        uint16_t linearVal = aHwData[aInput].analogRawValue;

        if(isFaderModule){
          if(isLogFader){
            for(int limitIndex = 0; limitIndex < (FADER_ALPS_RSA0_TAPERS_TABLE_SIZE-1); limitIndex++){   // Check to which interval corresponds the value read
              int nextLimit = ALPS_RSA0_Taper[limitIndex+1][0];

              if(aHwData[aInput].analogRawValue <= nextLimit){
                // Depending on the interval, apply the right math to get the travel % (scaled to the max value)
                
                linearVal = mapl(aHwData[aInput].analogRawValue, ALPS_RSA0_Taper[limitIndex][0], 
                                                                 ALPS_RSA0_Taper[limitIndex+1][0], 
                                                                 ALPS_RSA0_Taper[limitIndex][1],
                                                                 ALPS_RSA0_Taper[limitIndex+1][1]);   
                // Linearize mapping between adjacent table values                    

                linearVal = constrain(linearVal,minRawValue,maxRawValue);   // Constrain to max and min limits
                break;
              }
            }
          }else{
            for(int limitIndex = 0; limitIndex < (FADER_TTE_PS45M_TABLE_SIZE-1); limitIndex++){   // Check to which interval corresponds the value read
              int nextLimit = TTE_PS45M_Taper[limitIndex+1][0];

              if(aHwData[aInput].analogRawValue <= nextLimit){
                // Depending on the interval, apply the right math to get the travel % (scaled to the max value)
                linearVal = mapl(aHwData[aInput].analogRawValue, TTE_PS45M_Taper[limitIndex][0], 
                                                                 TTE_PS45M_Taper[limitIndex+1][0], 
                                                                 TTE_PS45M_Taper[limitIndex][1],
                                                                 TTE_PS45M_Taper[limitIndex+1][1]);   
                
                // Linearize mapping between adjacent table values
                linearVal = constrain(linearVal,minRawValue,maxRawValue); 
                break;
              }
            }                 
          }
        }

        // increase noise th while in auto-select mode, to prevent card jumps
        noiseTh += autoSelectMode*10;

        // Threshold filter
        if(IsNoise( aHwData[aInput].analogRawValue, 
                    aHwData[aInput].analogRawValuePrev, 
                    aInput,
                    noiseTh,
                    true))  continue;                     // if noise is detected in raw value, don't go on
        
        // Get data from config for this input
        uint16_t paramToSend = analog[aInput].parameter[analog_MSB]<<7 | analog[aInput].parameter[analog_LSB];
        byte channelToSend = analog[aInput].channel + 1;
        uint16_t minValue = (is14bit ? analog[aInput].parameter[analog_minMSB]<<7 : 0) | 
                                       analog[aInput].parameter[analog_minLSB];
        uint16_t maxValue = (is14bit ? analog[aInput].parameter[analog_maxMSB]<<7 : 0) | 
                                       analog[aInput].parameter[analog_maxLSB];

        // If min > max, then invert range                               
        bool invert = false;
        if(minValue > maxValue){
          invert = true;
        }

        // constrain raw value (12 bit) to these limits
        uint16_t constrainedValue = constrain(linearVal, 
                                              minRawValue+RAW_THRESHOLD, 
                                              maxRawValue-RAW_THRESHOLD);
        uint16_t hwPositionValue = 0;

        // if(!(aInput%2)) analog[aInput].deadZone = deadZone::dz_on;

        // CENTERED DOUBLE ANALOG
        if(analog[aInput].splitMode == splitModes::splitCenter){   // SPLIT MODE
          uint16_t dead_space = is14bit ? SPLIT_DEAD_ZONE<<6 : SPLIT_DEAD_ZONE;

          // map to min and max values in config
          uint16_t lower = invert ? maxValue : minValue;
          uint16_t higher = maxValue*2+1;   //default
          
          if(analog[aInput].deadZone == deadZone::dz_off){
            higher = invert ? minValue*2+1 :    // double the range
                              maxValue*2+1;
          }else if(analog[aInput].deadZone == deadZone::dz_on){
            higher = invert ? minValue*2+1+dead_space :    // add the extra dead zone range
                              maxValue*2+1+dead_space;
          }
          
          hwPositionValue = mapl(constrainedValue,
                                 minRawValue+RAW_THRESHOLD, 
                                 maxRawValue-RAW_THRESHOLD,
                                 lower,
                                 higher);     // if it is a center duplicate, extend double range
          // Could be configurable percentage of travel for analog control!
          uint16_t centerValue = 0;
          if((lower+higher)%2)   centerValue = (lower+higher+1)/2;
          else                   centerValue = (lower+higher)/2;

          if(analog[aInput].deadZone == deadZone::dz_off){
            if (hwPositionValue < centerValue){
              hwPositionValue = mapl(hwPositionValue, lower, centerValue-1, maxValue, minValue);
              channelToSend = config->midiConfig.splitModeChannel+1;
            }else{
              hwPositionValue = mapl(hwPositionValue, centerValue, higher, minValue, maxValue);
            }
          }else if(analog[aInput].deadZone == deadZone::dz_on){ 
            if (hwPositionValue < centerValue - dead_space/2){
              hwPositionValue = mapl(hwPositionValue, lower, centerValue - dead_space/2 - 1, maxValue, minValue);
              channelToSend = config->midiConfig.splitModeChannel+1;
            }else if(hwPositionValue > centerValue + dead_space/2){
              hwPositionValue = mapl(hwPositionValue, centerValue + dead_space/2 + 1, higher, minValue, maxValue);
            }else{
              if(hwPositionValue < centerValue){
                hwPositionValue = minValue;
                channelToSend = config->midiConfig.splitModeChannel+1;
              }else if(hwPositionValue > centerValue){
                hwPositionValue = minValue;
              }
              else continue; // within the dead zone, skip the rest of the input processing
            }
          }

        }else{  // Normal mode (NOT SPLIT)
          if(analog[aInput].deadZone == deadZone::dz_off){
            // map to min and max values in config
            hwPositionValue = mapl(constrainedValue,
                                   minRawValue+RAW_THRESHOLD, 
                                   maxRawValue-RAW_THRESHOLD,
                                   minValue,
                                   maxValue); 
          }else if(analog[aInput].deadZone == deadZone::dz_on){       // Dead zone ON
            uint16_t dead_space = is14bit ? NORMAL_DEAD_ZONE<<6 : NORMAL_DEAD_ZONE;

            uint16_t lower = invert ? maxValue : minValue;
            uint16_t higher = invert ?  minValue + dead_space+1 :    // Add dead zone to extended range
                                        maxValue + dead_space+1;

            uint16_t extendedCenter = 0;
            if((lower+higher)%2)   extendedCenter = (lower+higher+1)/2;     // Get center of extended range
            else                   extendedCenter = (lower+higher)/2;
            uint16_t outputCenter = 0;
            if((minValue+maxValue)%2)   outputCenter = (minValue+maxValue+1)/2;   // Get center of output range
            else                        outputCenter = (minValue+maxValue)/2;

            hwPositionValue = mapl(constrainedValue,
                                   minRawValue+RAW_THRESHOLD, 
                                   maxRawValue-RAW_THRESHOLD,
                                   lower,
                                   higher); 
            // map to min and max values in config
            if (hwPositionValue < extendedCenter - dead_space/2){  
              hwPositionValue = mapl(hwPositionValue,
                                     lower, 
                                     extendedCenter-dead_space/2-1,
                                     minValue,
                                     outputCenter-1); 
            
            }else if (hwPositionValue > extendedCenter + dead_space/2){  // <<5 cause this is raw value
              hwPositionValue = mapl(hwPositionValue,
                                     extendedCenter+dead_space/2+1, 
                                     higher,
                                     outputCenter+1,
                                     maxValue);   
            
            }else{
              if(hwPositionValue != outputCenter) hwPositionValue = outputCenter;   // if inside dead zone, assign center value
              else continue;
            }              
          }
        }
        
// **********************************************************************************************//
        // Start of takeover mode operation
        // Possible scenarios are No takeover, PickUp and Value Scaling
        // targetValue is the value the scaledValue will progressively move towards
        // scaledValue is the final value to be sent in the message
        uint16_t targetValue = aBankData[currentBank][aInput].analogValue;
        uint16_t scaledValue = 0;
        
        // VALUE SCALING TAKEOVER MODE 
        if( config->board.takeoverMode == takeOverTypes::takeover_valueScaling && 
            aBankData[currentBank][aInput].flags.takeOverOn){ // If takeover mode is ON
          
          // IF THE VALUE CHANGED DIRECTION, RESET PIVOTS TO CURRENT TARGET VALUE AND HARDWARE POSITION
          if (aBankData[currentBank][aInput].flags.lastDirection != aHwData[aInput].analogDirectionRaw){
            aBankData[currentBank][aInput].flags.lastDirection = aHwData[aInput].analogDirectionRaw;
            SetPivotValues(currentBank, aInput, targetValue);
          }

          // Set scaled value to target value, and from there it'll move accordingly
          scaledValue = aBankData[currentBank][aInput].analogValue;
          
          // Difference between the input's physical value and the last pivot the value started scaling from
          uint16_t hwDiff = abs(hwPositionValue - aBankData[currentBank][aInput].hardwarePivot);
          uint16_t valueIncrement = 0, valueDecrement = 0;

          if(hwDiff > 0){  
            scaledValue = aBankData[currentBank][aInput].targetValuePivot;            
            if(aHwData[aInput].analogDirectionRaw == ANALOG_INCREASING){  
              valueIncrement = (hwDiff)*abs(maxValue-aBankData[currentBank][aInput].targetValuePivot)/
                                abs(maxValue-aBankData[currentBank][aInput].hardwarePivot); 
              scaledValue += valueIncrement;                              
              scaledValue = constrain(scaledValue, minValue, maxValue);  
            }else if(aHwData[aInput].analogDirectionRaw == ANALOG_DECREASING){
              valueDecrement = (hwDiff)*abs(aBankData[currentBank][aInput].targetValuePivot-minValue)/
                              abs(aBankData[currentBank][aInput].hardwarePivot-minValue);
              scaledValue -=  valueDecrement;
              scaledValue = constrain(scaledValue, minValue, maxValue);
            } 
            
            if((hwPositionValue >= targetValue && aBankData[currentBank][aInput].hardwarePivot < aBankData[currentBank][aInput].targetValuePivot)||
              (hwPositionValue <= targetValue && aBankData[currentBank][aInput].hardwarePivot > aBankData[currentBank][aInput].targetValuePivot)){
              aBankData[currentBank][aInput].flags.takeOverOn = 0;
            }

            aBankData[currentBank][aInput].flags.lastDirection = aHwData[aInput].analogDirectionRaw;  
            
            aBankData[currentBank][aInput].analogValue = scaledValue;
          }
        // PICK-UP TAKEOVER MODE
        }else if( config->board.takeoverMode == takeOverTypes::takeover_pickup &&  
                  aBankData[currentBank][aInput].flags.takeOverOn){
          uint8_t pickupThreshold = 0;
          if(is14bit){
            pickupThreshold = (maxValue-minValue)>>6; // Pick Up Threshold is relative to the range. Larger range, larger threshold. For 14 bit inputs divide by 64
          }else{
            pickupThreshold = (maxValue-minValue)>>5; // For 7 bit inputs divide by 32
          }
          // If hardware position entered the threshold between (target - threshold) and (target + threshold), turn off take over mode
          if(hwPositionValue >= (targetValue - pickupThreshold) && hwPositionValue <= (targetValue + pickupThreshold)){
            aBankData[currentBank][aInput].flags.takeOverOn = 0;
          }
        // NO TAKEOVER MODE
        }else{
          aBankData[currentBank][aInput].analogValue = hwPositionValue;
        }
// **********************************************************************************************//
        
        aHwData[aInput].analogRawValuePrev = aHwData[aInput].analogRawValue;    // update previous value
        
        if(testAnalog){
          SERIALPRINT(F("Raw value: ")); SERIALPRINT(aHwData[aInput].analogRawValue);                       
          SERIALPRINT(F("\tLinearized value: "));SERIALPRINT(linearVal);
          SERIALPRINT(F("\tMapped value: "));SERIALPRINT(aBankData[currentBank][aInput].analogValue);
          SERIALPRINT(F("\t<- ANA "));SERIALPRINTLN(aInput); 
        }
          
        // if message is configured as NRPN or RPN or PITCH BEND, process again for noise in higher range
        if(is14bit){
          int maxMinDiff = maxValue - minValue;         
          byte noiseTh14 = abs(maxMinDiff) >> 8;    // divide range to get noise threshold. Max th is 127/4 = 64 : Min th is 0.     
          if(IsNoise( aBankData[currentBank][aInput].analogValue, 
                      aBankData[currentBank][aInput].analogValuePrev, 
                      aInput,
                      noiseTh14,
                      false))  continue;
        }
        
        // if after all filtering and noise detecting, we arrived here, check if new processed valued changed from last processed value
        if(aBankData[currentBank][aInput].analogValue != aBankData[currentBank][aInput].analogValuePrev){
          // update as previous value
          aBankData[currentBank][aInput].analogValuePrev = aBankData[currentBank][aInput].analogValue;
          
          uint16_t valueToSend = aBankData[currentBank][aInput].analogValue;

          // Act accordingly to configuration
          switch(analog[aInput].message){
            case analogMessageTypes::analog_msg_note:{
              if(analog[aInput].midiPort & (1<<MIDI_USB))
                MIDI.sendNoteOn( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
              if(analog[aInput].midiPort & (1<<MIDI_HW))
                MIDIHW.sendNoteOn( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
            }break;
            case analogMessageTypes::analog_msg_cc:{
              if(analog[aInput].midiPort & (1<<MIDI_USB))
                MIDI.sendControlChange( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
              if(analog[aInput].midiPort & (1<<MIDI_HW))
                MIDIHW.sendControlChange( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
            }break;
            case analogMessageTypes::analog_msg_pc:{
              if(analog[aInput].midiPort & (1<<MIDI_USB)){
                MIDI.sendProgramChange( valueToSend&0x7f, channelToSend);
              }
              if(analog[aInput].midiPort & (1<<MIDI_HW)){
                MIDIHW.sendProgramChange( valueToSend&0x7f, channelToSend);
              }
            }break;
            case analogMessageTypes::analog_msg_nrpn:{
              updateValue |= ((uint64_t) 1 << (uint64_t) aInput);
            }break;
            case analogMessageTypes::analog_msg_rpn:{
              updateValue |= ((uint64_t) 1 << (uint64_t) aInput);
            }break;
            case analogMessageTypes::analog_msg_pb:{
              int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
                            
              if(analog[aInput].midiPort & (1<<MIDI_USB))
                MIDI.sendPitchBend( valuePb, channelToSend);    
              if(analog[aInput].midiPort & (1<<MIDI_HW))
                MIDIHW.sendPitchBend( valuePb, channelToSend);    
            }break;
            case analogMessageTypes::analog_msg_key:{
              if(analog[aInput].parameter[analog_modifier])
                YTXKeyboard->press(analog[aInput].parameter[analog_modifier]);
              if(analog[aInput].parameter[analog_key])
                YTXKeyboard->press(analog[aInput].parameter[analog_key]);
              
              millisKeyboardPress = millis()+KEYBOARD_MILLIS_ANALOG;
              keyboardReleaseFlag = true; 
            }break;
          }
          // blink status LED
          SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);

          if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Analog, aInput) != lastComponentInfoId)){
            SendComponentInfo(ytxIOBLOCK::Analog, aInput);
          }
        }         
        if(isJoystickX){
          isJoystickX = false; isJoystickY = true;
        }else if(isJoystickY){
          isJoystickY = false;
        }
      }
      // P41 and F41 (4 inputs) take 2 module spaces
      if (hwMapping == AnalogModuleTypes::P41 ||
          hwMapping == AnalogModuleTypes::F41){
        nMod++;     
      }      
    }
  }

  if(priorityMode){
    if(!(++initMuxRead % analogMuxesWithElements)){
      initMuxRead = 0;
    }
    lastMuxRead = initMuxRead+1;
  }else{
    initMuxRead = 0;
    lastMuxRead = analogMuxesWithElements;
  }
}

void AnalogInputs::SendMessage(uint8_t aInput){
  // Get data from config for this input
  uint16_t paramToSend = analog[aInput].parameter[analog_MSB]<<7 | analog[aInput].parameter[analog_LSB];
  byte channelToSend = analog[aInput].channel + 1;
  uint16_t valueToSend = aBankData[currentBank][aInput].analogValue;
  bool is14bit =  analog[aInput].message == analog_msg_nrpn || 
                  analog[aInput].message == analog_msg_rpn || 
                  analog[aInput].message == analog_msg_pb;
  uint16_t minValue = (is14bit ? analog[aInput].parameter[analog_minMSB]<<7 : 0) | 
                                 analog[aInput].parameter[analog_minLSB];
  uint16_t maxValue = (is14bit ? analog[aInput].parameter[analog_maxMSB]<<7 : 0) | 
                                 analog[aInput].parameter[analog_maxLSB];

  // Act accordingly to configuration
  switch(analog[aInput].message){
    case analogMessageTypes::analog_msg_note:{
      if(analog[aInput].midiPort & 0x01)
        MIDI.sendNoteOn( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
      if(analog[aInput].midiPort & 0x02)
        MIDIHW.sendNoteOn( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
    }break;
    case analogMessageTypes::analog_msg_cc:{
      if(analog[aInput].midiPort & 0x01)
        MIDI.sendControlChange( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
      if(analog[aInput].midiPort & 0x02)
        MIDIHW.sendControlChange( paramToSend&0x7f, valueToSend&0x7f, channelToSend);
    }break;
    case analogMessageTypes::analog_msg_pc:{
      if(analog[aInput].midiPort & 0x01){
        MIDI.sendProgramChange( valueToSend&0x7f, channelToSend);
      }
      if(analog[aInput].midiPort & 0x02){
        MIDIHW.sendProgramChange( valueToSend&0x7f, channelToSend);
      }
    }break;
    case analogMessageTypes::analog_msg_nrpn:{
      updateValue |= ((uint64_t) 1 << (uint64_t) aInput);
    }break;
    case analogMessageTypes::analog_msg_rpn:{
      updateValue |= ((uint64_t) 1 << (uint64_t) aInput);
    }break;
    case analogMessageTypes::analog_msg_pb:{
      int16_t valuePb = mapl(valueToSend, minValue, maxValue,((int16_t) minValue)-8192, ((int16_t) maxValue)-8192);
                    
      if(analog[aInput].midiPort & 0x01)
        MIDI.sendPitchBend( valuePb, channelToSend);    
      if(analog[aInput].midiPort & 0x02)
        MIDIHW.sendPitchBend( valuePb, channelToSend);    
    }break;
    case analogMessageTypes::analog_msg_key:{
      if(analog[aInput].parameter[analog_modifier])
        YTXKeyboard->press(analog[aInput].parameter[analog_modifier]);
      if(analog[aInput].parameter[analog_key])
        YTXKeyboard->press(analog[aInput].parameter[analog_key]);
      
      millisKeyboardPress = millis()+KEYBOARD_MILLIS_ANALOG;
      keyboardReleaseFlag = true; 
    }break;
  }
  // blink status LED
  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_OUT);

  if(autoSelectMode && (GetHardwareID(ytxIOBLOCK::Analog, aInput) != lastComponentInfoId)){
    SendComponentInfo(ytxIOBLOCK::Analog, aInput);
  } 
}

void AnalogInputs::SendNRPN(void){
  static unsigned int updateInterval = nrpnIntervalStep;
  static byte inUse = 0;
  
  if(updateValue && (millis() - antMillisAnalogUpdate > updateInterval)){
    antMillisAnalogUpdate = millis();
    
    for(int aInput = 0; aInput < 64; aInput++){
      if((updateValue >> (uint64_t) aInput) & (uint64_t) 0x1){
        updateValue &= ~((uint64_t) 1 << (uint64_t) aInput);
        
        uint16_t paramToSend = analog[aInput].parameter[analog_MSB]<<7 | analog[aInput].parameter[analog_LSB];
        byte channelToSend = analog[aInput].channel + 1; 
        uint16_t valueToSend = aBankData[currentBank][aInput].analogValue;
        
        if(analog[aInput].message == analogMessageTypes::analog_msg_nrpn){
          if(analog[aInput].midiPort & 0x01){
            MIDI.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
            MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
          }
          if(analog[aInput].midiPort & 0x02){
            MIDIHW.sendControlChange( 99, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 98, (paramToSend & 0x7F), channelToSend);
            MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
          }
        }else if(analog[aInput].message == analogMessageTypes::analog_msg_rpn){
          if(analog[aInput].midiPort & 0x01){
            MIDI.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
            MIDI.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDI.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);       
          }
          if(analog[aInput].midiPort & 0x02){
            MIDIHW.sendControlChange( 101, (paramToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 100, (paramToSend & 0x7F), channelToSend);
            MIDIHW.sendControlChange( 6, (valueToSend >> 7) & 0x7F, channelToSend);
            MIDIHW.sendControlChange( 38, (valueToSend & 0x7F), channelToSend);    
          }
        }
        inUse++;
      }      
    }
    updateInterval = inUse*nrpnIntervalStep;
    inUse = 0;
  }else if(!updateValue){
    updateInterval = nrpnIntervalStep;
    inUse = 0;
  }
}


void AnalogInputs::SetBankForAnalog(uint8_t newBank){
  for(uint8_t analogNo = 0; analogNo < nAnalog; analogNo++){
    uint16_t minValue = 0, maxValue = 0;
    uint8_t msgType = 0;
    bool invert = 0;

    // Get config info for this encoder  
    minValue = analog[analogNo].parameter[analog_minMSB]<<7 | analog[analogNo].parameter[analog_minLSB];
    maxValue = analog[analogNo].parameter[analog_maxMSB]<<7 | analog[analogNo].parameter[analog_maxLSB];
    msgType = analog[analogNo].message;
    
    // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
    if(!(msgType == analog_msg_nrpn || msgType == analog_msg_rpn || msgType == analog_msg_pb)){ 
      minValue = minValue & 0x7F;
      maxValue = maxValue & 0x7F;
    }

    // constrain to these limits
    uint16_t constrainedValue = constrain(aHwData[analogNo].analogRawValue,
                                          minRawValue+RAW_THRESHOLD, 
                                          maxRawValue-RAW_THRESHOLD);

    // map to min and max values in config
    uint16_t hwPositionValue = mapl(  constrainedValue,
                                      minRawValue+RAW_THRESHOLD, 
                                      maxRawValue-RAW_THRESHOLD,
                                      minValue,
                                      maxValue); 

    
    if(aBankData[newBank][analogNo].analogValue != hwPositionValue && 
        (!aBankData[newBank][analogNo].flags.takeOverOn || 
          aBankData[newBank][analogNo].targetValuePivot != aBankData[newBank][analogNo].analogValue)){
      // Init takeover mode
      aBankData[newBank][analogNo].flags.takeOverOn = 1;
        
      if(config->board.takeoverMode == takeOverTypes::takeover_valueScaling){
        SetPivotValues(newBank, analogNo, aBankData[newBank][analogNo].analogValue);
      }
    }
  }
}

uint16_t AnalogInputs::GetAnalogValue(uint8_t analogNo){
  if(analogNo < nAnalog){
    return aBankData[currentBank][analogNo].analogValue;
  }
}

void AnalogInputs::SetAnalogValue(uint8_t bank, uint8_t analogNo, uint16_t newValue){
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;
  bool invert = false;
  bool is14bit = false;

  if(aBankData[bank][analogNo].analogValue == newValue) return;

  // Get config info for this encoder  
  minValue = analog[analogNo].parameter[analog_minMSB]<<7 | analog[analogNo].parameter[analog_minLSB];
  maxValue = analog[analogNo].parameter[analog_maxMSB]<<7 | analog[analogNo].parameter[analog_maxLSB];
  msgType = analog[analogNo].message;
  
  // IF NOT 14 BITS, USE LOWER PART FOR MIN AND MAX
  if( msgType == analog_msg_nrpn || msgType == analog_msg_rpn || msgType == analog_msg_pb){
    is14bit = true;      
  }else{
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }
  
  uint8_t pickupThreshold = 0;
  if(is14bit){
    pickupThreshold = (maxValue-minValue)>>6; // Dynamic pickup threshold based on min-max range
  }else{
    pickupThreshold = (maxValue-minValue)>>5;  
  }
  
  if(aBankData[bank][analogNo].analogValue >= newValue - pickupThreshold && 
      aBankData[bank][analogNo].analogValue <= newValue + pickupThreshold) return;
  

  if(config->board.takeoverMode != takeOverTypes::takeover_none){
    
    if(minValue > maxValue){    // If minValue is higher, invert behaviour
      invert = true;
    }

    if(newValue > (invert ? minValue : maxValue)){
      aBankData[bank][analogNo].analogValue = (invert ? minValue : maxValue);
    }
    else if(newValue < (invert ? maxValue : minValue)){
      aBankData[bank][analogNo].analogValue = (invert ? maxValue : minValue);
    }
    else{
      aBankData[bank][analogNo].analogValue = newValue;

    }
    // update prev value
    aBankData[bank][analogNo].analogValuePrev = newValue;
    // Init takeover mode
    aBankData[bank][analogNo].flags.takeOverOn = 1;
  }
  if(config->board.takeoverMode == takeOverTypes::takeover_valueScaling){
    SetPivotValues(bank, analogNo, newValue);
  }
   
  if (bank == currentBank){
    // Update analog feedback if there is any
  }

}

// Set new pivot values for hardware position and target position
// hardwarePivot is the value where the potentiometer is phisically at the time of the call to this function
// targetPivot is the 
void AnalogInputs::SetPivotValues(uint8_t bank, uint8_t analogNo, uint16_t targetValue){
  uint16_t minValue = 0, maxValue = 0;
  uint8_t msgType = 0;

  // Get config info for this encoder  
  minValue = analog[analogNo].parameter[analog_minMSB]<<7 | analog[analogNo].parameter[analog_minLSB];
  maxValue = analog[analogNo].parameter[analog_maxMSB]<<7 | analog[analogNo].parameter[analog_maxLSB];
  msgType = analog[analogNo].message;
  
  // IF NOT 14 BITS, USE LOWER PART BYTE FOR MIN AND MAX
  if(!( msgType == analog_msg_nrpn || 
        msgType == analog_msg_rpn || 
        msgType == analog_msg_pb)){ 
    minValue = minValue & 0x7F;
    maxValue = maxValue & 0x7F;
  }

  // constrain to these limits
  uint16_t constrainedValue = constrain(aHwData[analogNo].analogRawValue, 
                                        minRawValue+RAW_THRESHOLD, 
                                        maxRawValue-RAW_THRESHOLD);

  // map to min and max values in config
  uint16_t hwPositionValue = mapl(  constrainedValue,
                                    minRawValue+RAW_THRESHOLD, 
                                    maxRawValue-RAW_THRESHOLD,
                                    minValue,
                                    maxValue); 

  aBankData[bank][analogNo].hardwarePivot = hwPositionValue;      // Hw pivot is 
  aBankData[bank][analogNo].targetValuePivot = targetValue;
}

// Thanks to Pablo Fullana for the help with this function!
// It's a threshold filter. If the new value stays within the previous value + - the noise threshold set, then it's considered noise
bool AnalogInputs::IsNoise(uint16_t currentValue, uint16_t prevValue, uint16_t input, byte noiseTh, bool raw) {
  bool directionOfChange = (raw  ?  aHwData[input].analogDirectionRaw : 
                                    aHwData[input].analogDirection);
    
  if (directionOfChange == ANALOG_INCREASING){   // CASE 1: If signal is increasing,
    if(currentValue >  prevValue){      // and the new value is greater than the previous,
       return 0;                                        // NOT NOISE!
    }
    else if(currentValue < (prevValue - noiseTh)){  // If, otherwise, it's lower than the previous value and the noise threshold together,
      if(raw)  aHwData[input].analogDirectionRaw = ANALOG_DECREASING;    // means it started to decrease,
      else     aHwData[input].analogDirection    = ANALOG_DECREASING;

      return 0;                                                             // NOT NOISE!
    }
  }
  if (directionOfChange == ANALOG_DECREASING){   // CASE 2: If signal is increasing,
    if(currentValue < prevValue){      // and the new value is lower than the previous,
       return 0;                                        // NOT NOISE!
    }
    else if(currentValue > (prevValue + noiseTh)){  // If, otherwise, it's greater than the previous value and the noise threshold together,
      if(raw)  aHwData[input].analogDirectionRaw = ANALOG_INCREASING; // means it started to increase,
      else     aHwData[input].analogDirection    = ANALOG_INCREASING;

      return 0;                                                              // NOT NOISE!
    }
  }
  return 1;                                           // If everyting above was not true, then IT'S NOISE! Pot is trying to fool us. But we owned you pot ;)
}

uint16_t AnalogInputs::FilterGetNewExponentialAverage(uint8_t input, uint16_t newVal) {
  float alpha = 0.25;

  aHwData[input].exponentialFilter = alpha*newVal + (1-alpha)*aHwData[input].exponentialFilter;

  return aHwData[input].exponentialFilter;
}



/*
  Method:         MuxAnalogRead
  Description:    Read the analog value of a multiplexer channel as an analog input.
  Parameters:
                  mux    - Identifier of the multiplexer desired. Must be MUX_A or MUX_B (0 or 1)
                  chan   - Number of the channel desired. Must be within 1 and 16
  Returns:        uint16_t  - 0..1023: analog value read
                            -1: mux or chan is not valid
*/
int16_t AnalogInputs::MuxAnalogRead(uint8_t mux, uint8_t chan){
    static unsigned int analogADCdata;
    if (chan >= 0 && chan <= 15 && mux < NUM_MUX){     
      chan = MuxMapping[chan];      // Re-map hardware channels to have them read in the header order
      pinMode(muxPin[mux], INPUT);
    }
    else return -1;       // Return ERROR

  // Select channel
  bitRead(chan, 0) ?  PORT->Group[g_APinDescription[_S0].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S0].ulPin) :
            PORT->Group[g_APinDescription[_S0].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S0].ulPin) ;
  bitRead(chan, 1) ?  PORT->Group[g_APinDescription[_S1].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S1].ulPin) :
            PORT->Group[g_APinDescription[_S1].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S1].ulPin) ;
  bitRead(chan, 2) ?  PORT->Group[g_APinDescription[_S2].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S2].ulPin) :
            PORT->Group[g_APinDescription[_S2].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S2].ulPin) ;
  bitRead(chan, 3) ?  PORT->Group[g_APinDescription[_S3].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S3].ulPin) :
            PORT->Group[g_APinDescription[_S3].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S3].ulPin) ;

  analogADCdata = AnalogReadFast(muxPin[mux]);
  return analogADCdata;
}

/*
  Method:         digitalReadKm
  Description:    Read the state of a multiplexer channel as a digital input.
  Parameters:
                  mux    - Identifier of the multiplexer desired. Must be MUX_A or MUX_B (0 or 1)
                  chan   - Number of the channel desired. Must be within 1 and 16
  Returns:        int    - 0: input is LOW
                           1: input is HIGH
                          -1: mux or chan is not valid
*/
int16_t AnalogInputs::MuxDigitalRead(uint8_t mux, uint8_t chan){
    int16_t digitalState;
    if (chan >= 0 && chan <= 15 && mux < NUM_MUX){
      chan = MuxMapping[chan];
    }
    else return -1;     // Return ERROR

  // Select channel
  bitRead(chan, 0) ?  PORT->Group[g_APinDescription[_S0].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S0].ulPin) :
            PORT->Group[g_APinDescription[_S0].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S0].ulPin) ;
  bitRead(chan, 1) ?  PORT->Group[g_APinDescription[_S1].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S1].ulPin) :
            PORT->Group[g_APinDescription[_S1].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S1].ulPin) ;
  bitRead(chan, 2) ?  PORT->Group[g_APinDescription[_S2].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S2].ulPin) :
            PORT->Group[g_APinDescription[_S2].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S2].ulPin) ;
  bitRead(chan, 3) ?  PORT->Group[g_APinDescription[_S3].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S3].ulPin) :
            PORT->Group[g_APinDescription[_S3].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S3].ulPin) ;

    digitalState = digitalRead(muxPin[mux]);
    
    return digitalState;
}

/*
  Method:         digitalReadKm
  Description:    Read the state of a multiplexer channel as a digital input, setting the analog input of the Arduino with a pullup resistor.
  Parameters:
                  mux    - Identifier for the desired multiplexer. Must be MUX_A or MUX_B (0 or 1)
                  chan   - Number for the desired channel. Must be within 1 and 16
  Returns:        int    - 0: input is LOW
                           1: input is HIGH
                          -1: mux or chan is not valid
*/
int16_t AnalogInputs::MuxDigitalRead(uint8_t mux, uint8_t chan, uint8_t pullup){
    int16_t digitalState;
    if (chan >= 0 && chan <= 15 && mux < NUM_MUX){
      chan = MuxMapping[chan];
    }
    else return -1;     // Return ERROR

  // SET PIN HIGH
  // PORT->Group[g_APinDescription[YOUR_PIN].ulPort].OUTSET.reg = (1ul << g_APinDescription[YOUR_PIN].ulPin) ;
  // SET PIN LOW
  // PORT->Group[g_APinDescription[YOUR_PIN].ulPort].OUTCLR.reg = (1ul << g_APinDescription[YOUR_PIN].ulPin) ;

  // Select channel
  bitRead(chan, 0) ?  PORT->Group[g_APinDescription[_S0].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S0].ulPin) :
            PORT->Group[g_APinDescription[_S0].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S0].ulPin) ;
  bitRead(chan, 1) ?  PORT->Group[g_APinDescription[_S1].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S1].ulPin) :
            PORT->Group[g_APinDescription[_S1].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S1].ulPin) ;
  bitRead(chan, 2) ?  PORT->Group[g_APinDescription[_S2].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S2].ulPin) :
            PORT->Group[g_APinDescription[_S2].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S2].ulPin) ;
  bitRead(chan, 3) ?  PORT->Group[g_APinDescription[_S3].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S3].ulPin) :
            PORT->Group[g_APinDescription[_S3].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S3].ulPin) ;


    pinMode(muxPin[mux], INPUT);                // These two lines set the analog input pullup resistor
    digitalWrite(muxPin[mux], HIGH);
    digitalState = digitalRead(muxPin[mux]);     // Read mux pin

    return digitalState;
}

#if defined(__arm__)
//##############################################################################
// Fast analogue read analogReadFast()
// This is a stripped down version of analogRead() where the set-up commands
// are executed during setup()
// ulPin is the analog input pin number to be read.
//  Mk. 2 - has some more bits removed for speed up
//##############################################################################
uint32_t AnalogInputs::AnalogReadFast(byte ADCpin) {
  SelAnalog(ADCpin);
  ADC->INTFLAG.bit.RESRDY = 1;              // Data ready flag cleared
  ADCsync();
  ADC->SWTRIG.bit.START = 1;                // Start ADC conversion
  while ( ADC->INTFLAG.bit.RESRDY == 0 );   // Wait till conversion done
  ADCsync();
  uint32_t valueRead = ADC->RESULT.reg;     // read the result
  ADCsync();
  ADC->SWTRIG.reg = 0x01;                    //  and flush for good measure
  return valueRead;
  
//  REG_ADC_CTRLA = 2;
//  //REG_ADC_INPUTCTRL = 0x0F001800;
//  //REG_ADC_INPUTCTRL = 0x00001900;
////  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ADCpin].ulADCChannelNumber; // Selection for the positive ADC input
//  REG_ADC_SWTRIG = 2;
//  while (!(REG_ADC_INTFLAG & 1));
//  ADCsync();
//  uint32_t valueRead = ADC->RESULT.reg;     // read the result
//  ADCsync();
//  ADC->SWTRIG.reg = 0x01;                    //  and flush for good measure
//  
////  uint32_t valueRead = ADC->RESULT.reg;     // read the result
////  float v = 3.3*((float)valueRead)/4096;
////  SERIALPRINT(valueRead);
////  SERIALPRINT(F(" "));
////  SERIALPRINT(v,3);
////  SERIALPRINTLN();
// 
//  return REG_ADC_RESULT;
}
//##############################################################################



void AnalogInputs::FastADCsetup() {
  const byte gClk = 3; //used to define which generic clock we will use for ADC
  const int cDiv = 1; //divide factor for generic clock
  
  ADC->CTRLA.bit.ENABLE = 0;                     // Disable ADC
  while( ADC->STATUS.bit.SYNCBUSY == 1 );        // Wait for synchronization
  ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 |   // Divide Clock by 64.
                   ADC_CTRLB_RESSEL_12BIT;       // Result on 12 bits
  ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_1 |   // 1 sample
                     ADC_AVGCTRL_ADJRES(0x00ul); // Adjusting result by 0
  ADC->SAMPCTRL.reg = 0x00;                      // Sampling Time Length = 0
  ADC->CTRLA.bit.ENABLE = 1;                     // Enable ADC
  while( ADC->STATUS.bit.SYNCBUSY == 1 );        // Wait for synchronization
  
  //Input control register
//  ADCsync();
//  ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_1X_Val;      // Gain select as 1X
////  ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_DIV2_Val;  // Gain select as 1/2X - When AREF is VCCINT (default)
//  //Set ADC reference source
//  ADCsync();
//  ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_AREFA_Val;
//  // Set sample length and averaging
//  ADCsync();
//  ADC->AVGCTRL.reg = 0x00 ;       //Single conversion no averaging
//  ADCsync();
//  ADC->SAMPCTRL.reg = 0x00;       //Minimal sample length is 1/2 CLK_ADC cycle
//  //Control B register
//  ADCsync();
//  ADC->CTRLB.reg =  PRESCALER_32 | RESOL_12BIT; // Prescale 64, 12 bit resolution, single conversion
//  /* ADC->CTRLB.reg &= 0b1111100011111111;          // mask PRESCALER bits
//  ADC->CTRLB.reg |= ADC_CTRLB_PRESCALER_DIV16;   // divide Clock by 64 */
//  // Enable ADC in control B register
//  ADCsync();
//  ADC->CTRLA.bit.ENABLE = 0x01;

 //Enable interrupts
  // ADC->INTENSET.reg |= ADC_INTENSET_RESRDY; // enable Result Ready ADC interrupts
  // ADCsync();

   // NVIC_EnableIRQ(ADC_IRQn); // enable ADC interrupts
   // NVIC_SetPriority(ADC_IRQn, 1); //set priority of the interrupt - higher number -> lower priority
}


//###################################################################################
// ADC select here
//
//###################################################################################

void AnalogInputs::SelAnalog(uint32_t ulPin){      // Selects the analog input channel in INPCTRL
  ADCsync();
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ulPin].ulADCChannelNumber; // Selection for the positive ADC input
}

static void ADCsync() {
  while (ADC->STATUS.bit.SYNCBUSY == 1); //Just wait till the ADC is free
}
#endif
