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

//----------------------------------------------------------------------------------------------------
// SETUP MINIBLOCK
//----------------------------------------------------------------------------------------------------

#include "headers/Defines.h"

void setup() {
  SPI.begin();              // TO ENCODERS AND DIGITAL
  
  Serial.begin(2000000);    // FEEDBACK -> SAMD11

  // LAST RESET CAUSE
  //  SERIALPRINTLN(PM->RCAUSE.reg);

  pinMode(externalVoltagePin, INPUT);
  pinMode(pinResetSAMD11, OUTPUT);
  pinMode(pinBootModeSAMD11, OUTPUT);
  digitalWrite(pinResetSAMD11, HIGH);
  digitalWrite(pinBootModeSAMD11, HIGH);

  // RESET SAMD11
  ResetFBMicro();
  delay(50); // wait for samd11 reset

  analogReference(AR_EXTERNAL);
  
  // Randomize session
  randomSeed(analogRead(A4));

  // EEPROM INITIALIZATION
  uint8_t eepStatus = eep.begin(extEEPROM::twiClock400kHz,extEEPROM::twiClock400kHz); //go fast!
  if (eepStatus) {
    // SERIALPRINT(F("extEEPROM.begin() failed, status = ")); SERIALPRINTLN(eepStatus);
    delay(1000);
    while (1);
  }

  delay(250); // delay to allow correct initialization of the eeprom

  memHost = new memoryHost(&eep, ytxIOBLOCK::BLOCKS_COUNT);
  // General config block
  memHost->ConfigureBlock(ytxIOBLOCK::Configuration, 1, sizeof(ytxConfigurationType), true);
  config = (ytxConfigurationType*) memHost->Block(ytxIOBLOCK::Configuration);    


  if(config->board.fwVersionMaj != FW_VERSION_MAJOR ||
     config->board.fwVersionMin != FW_VERSION_MINOR ||
     config->board.hwVersionMaj != HW_VERSION_MAJOR ||
     config->board.hwVersionMin != HW_VERSION_MINOR){
    
    // WRITE TO EEPROM FW AND HW VERSION
    config->board.fwVersionMin = FW_VERSION_MINOR;
    eep.write(FW_VERSION_ADDR, &config->board.fwVersionMin, sizeof(byte));
    config->board.fwVersionMaj = FW_VERSION_MAJOR;
    eep.write(FW_VERSION_ADDR+1, &config->board.fwVersionMaj, sizeof(byte));
    config->board.hwVersionMin = HW_VERSION_MINOR;
    eep.write(HW_VERSION_ADDR, &config->board.hwVersionMin, sizeof(byte));
    config->board.hwVersionMaj = HW_VERSION_MAJOR;
    eep.write(HW_VERSION_ADDR+1, &config->board.hwVersionMaj, sizeof(byte));
  }


  // Color table block
  memHost->ConfigureBlock(ytxIOBLOCK::ColorTable, 1, sizeof(colorRangeTable), true);
  colorTable = (uint8_t*) memHost->Block(ytxIOBLOCK::ColorTable);
  // memHost->SaveBlockToEEPROM(ytxIOBLOCK::ColorTable); // SAVE COLOR TABLE TO EEPROM FOR NOW

 
#ifdef INIT_CONFIG
   // DUMMY INIT - LATER TO BE REPLACED BY KILOWHAT
  initConfig();
  memHost->SaveBlockToEEPROM(ytxIOBLOCK::Configuration); // SAVE GENERAL CONFIG IN EEPROM
#endif
   

////////////////////////////////////////////////////////////////////////////////////////////////
//// VALID CONFIG  /////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
  #define CONFIG_NOT_VALID    0
  #define FW_CONFIG_MISMATCH  1
  #define CONFIG_VALID        2

  uint8_t configStatus = CONFIG_NOT_VALID;

  if (config->board.signature == SIGNATURE_CHAR){
    if ((config->board.fwVersionMaj == config->board.configVersionMaj &&
        config->board.fwVersionMin == config->board.configVersionMin) ||
        (config->board.fwVersionMaj == config->board.configVersionMaj &&
        config->board.fwVersionMin >= config->board.configVersionMin &&
        config->board.configVersionMin >= VERSION_0_20_MINOR)){
        //exactly same version or firmware version superior to config version 
        configStatus = CONFIG_VALID;
    }else{
      configStatus = FW_CONFIG_MISMATCH;
      memHost->SetFwConfigChangeFlag();
    }
  }

  // Read fw signature from eeprom. If there is, initialize RAM for all controls.
  if (configStatus != CONFIG_NOT_VALID) {      // SIGNATURE CHECK SUCCESS
    
    // MODIFY DESCRIPTORS TO RENAME CONTROLLER
    strcpy((char*)STRING_PRODUCT, config->board.deviceName);
    strcpy((char*)STRING_MANUFACTURER, "Yaeltex");
    USB_DeviceDescriptor.idVendor = MICROCHIP_VID;
    USB_DeviceDescriptorB.idVendor = MICROCHIP_VID;
    USB_DeviceDescriptor.idProduct = config->board.pid;
    USB_DeviceDescriptorB.idProduct = config->board.pid;

    // INIT USB DEVICE (this was taken from Arduino zero's core main.cpp - It was done before setup())
    #if defined(USBCON)
      USBDevice.init();
      USBDevice.attach();
    #endif
    
    // Wait for serial monitor to open
    if(cdcEnabled){
      #if defined(WAIT_FOR_SERIAL)
      while(!SerialUSB);
      #endif
    }

    // Create memory map for eeprom
    memHost->ConfigureBlock(ytxIOBLOCK::Encoder, config->inputs.encoderCount, sizeof(ytxEncoderType), false);
    memHost->ConfigureBlock(ytxIOBLOCK::Analog, config->inputs.analogCount, sizeof(ytxAnalogType), false);
    memHost->ConfigureBlock(ytxIOBLOCK::Digital, config->inputs.digitalCount, sizeof(ytxDigitalType), false);
    memHost->ConfigureBlock(ytxIOBLOCK::Feedback, config->inputs.feedbackCount, sizeof(ytxFeedbackType), false);
    memHost->LayoutBanks();
    memHost->LoadBank(0);

    encoder = (ytxEncoderType*) memHost->Block(ytxIOBLOCK::Encoder);
    analog = (ytxAnalogType*) memHost->Block(ytxIOBLOCK::Analog);
    digital = (ytxDigitalType*) memHost->Block(ytxIOBLOCK::Digital);
    feedback = (ytxFeedbackType*) memHost->Block(ytxIOBLOCK::Feedback);

#ifdef INIT_CONFIG
    for (int b = 0; b < config->banks.count; b++) {
      initInputsConfig(b);
      memHost->SaveBank(b);
    }
    currentBank = memHost->LoadBank(0);
#endif       
    
    initFixedConfig(); // Only will do something if it is implemented
    
    // If there is more than 16 modules adding digitals and encoders, lower SPI speed
    CountModules(); // Count modules in config
    
    uint32_t maxSPEED = SPI_SPEED_2_M;

    if((modulesInConfig.encoders + modulesInConfig.digital[0] + modulesInConfig.digital[1]) >= 16) {  
      maxSPEED = SPI_SPEED_1_5_M;
    }

    if(modulesInConfig.encoders || config->inputs.analogCount>64){
      spiBUS[0] = new SPIAdressableBUS();
      spiBUS[0]->begin(&SPI,SPISettings(SPI_SPEED_1_5_M,MSBFIRST,SPI_MODE0),spiCS[0]);
    }

    if(modulesInConfig.digital[0]){
      spiBUS[1] = new SPIAdressableBUS();
      spiBUS[1]->begin(&SPI,SPISettings(maxSPEED,MSBFIRST,SPI_MODE0),spiCS[1]);
    }

    if(modulesInConfig.digital[1]){
      spiBUS[2] = new SPIAdressableBUS();
      spiBUS[2]->begin(&SPI,SPISettings(maxSPEED,MSBFIRST,SPI_MODE0),spiCS[2]);
    }

    // Initialize classes and elements
    encoderHw.Init(config->banks.count,           // N BANKS
                   config->inputs.encoderCount,   // N INPUTS
                   spiBUS[0]);                    // SPI BUS
    analogHw.Init(config->banks.count,            // N BANKS
                  config->inputs.analogCount,     // N INPUTS
                  spiBUS[0]);                     // SPI BUS
    digitalHw.Init(config->banks.count,           // N BANKS
                   config->inputs.digitalCount,   // N INPUTS
                   spiBUS[1],spiBUS[2]);          // SPI BUS
    feedbackHw.Init(config->banks.count,          // N BANKS
                    config->inputs.encoderCount,  // N ENCODER INPUTS
                    config->inputs.digitalCount,  // N DIGITAL INPUTS
                    0);                           // N INDEPENDENT LEDs

    // SERIALPRINT("CONFIG STATUS: "); SERIALPRINTLN(configStatus);

    if(configStatus == CONFIG_VALID){
      enableProcessing = true; // process inputs on loop
      validConfigInEEPROM = true;
    }
    
////////////////////////////////////////////////////////////////////////////////////////////////
//// INVALID CONFIG  ///////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

  } else {
    SERIALPRINTLN(F("CONFIG NOT VALID"));
    
    // MODIFY DESCRIPTORS TO RENAME CONTROLLER
    strcpy((char*)STRING_PRODUCT, "KilomuxV2");
    strcpy((char*)STRING_MANUFACTURER, "Yaeltex");
    USB_DeviceDescriptor.idVendor = MICROCHIP_VID;
    USB_DeviceDescriptorB.idVendor = MICROCHIP_VID;
    USB_DeviceDescriptor.idProduct = DEFAULT_PID;
    USB_DeviceDescriptorB.idProduct = DEFAULT_PID;
  
    
    // INIT USB DEVICE (this was taken from Arduino zero's core main.cpp - It was done before setup())
    #if defined(USBCON)
      USBDevice.init();
      USBDevice.attach();
    #endif
    
     // Wait for serial monitor to open
    if(cdcEnabled){
      #if defined(WAIT_FOR_SERIAL)
      while(!SerialUSB);
      #endif
    }
    enableProcessing = false;
    validConfigInEEPROM = false;
    //eeErase(128, 0, 65535);
  }

  if(cdcEnabled){
    #ifdef PRINT_CONFIG
      if(validConfigInEEPROM){
        printConfig(ytxIOBLOCK::Configuration, 0);
        for(int b = 0; b < config->banks.count; b++){
          currentBank = memHost->LoadBank(b);
          SERIALPRINTLN("*********************************************");
          SERIALPRINT("************* BANK ");
                              SERIALPRINT(b);
                              SERIALPRINTLN(" ************************");
          SERIALPRINTLN("*********************************************");
          for(int e = 0; e < config->inputs.encoderCount; e++)
            printConfig(ytxIOBLOCK::Encoder, e);
          for(int d = 0; d < config->inputs.digitalCount; d++)
            printConfig(ytxIOBLOCK::Digital, d);
          for(int a = 0; a < config->inputs.analogCount; a++)
            printConfig(ytxIOBLOCK::Analog, a);  
        }
        
      }
    #endif
  }
  
  // Begin MIDI USB port and set handler for Sysex Messages
  MIDI.begin(MIDI_CHANNEL_OMNI); // Se inicializa la comunicación MIDI por USB.
  MIDI.turnThruOff();            // Por default, la librería de Arduino MIDI tiene el THRU en ON, y NO QUEREMOS ESO!
  MIDI.setHandleSystemExclusive(handleSystemExclusiveUSB);
  // Begin MIDI HW (DIN5) port
  MIDIHW.begin(MIDI_CHANNEL_OMNI); // Se inicializa la comunicación MIDI por puerto serie(DIN5).
  MIDIHW.turnThruOff();            // Por default, la librería de Arduino MIDI tiene el THRU en ON, y NO QUEREMOS ESO!
  MIDIHW.setHandleSystemExclusive(handleSystemExclusiveHW);

  // Configure a timer interrupt where we'll call MIDI.read()
  uint32_t sampleRate = 7000; //sample rate, determines how often TC5_Handler is called
  tcConfigure(sampleRate); //configure the timer to run at <sampleRate>Hertz
  tcStartCounter(); //starts the timer

  if(validConfigInEEPROM){ 
    MIDI.setHandleNoteOn(handleNoteOnUSB);
    MIDI.setHandleNoteOff(handleNoteOffUSB);
    MIDI.setHandleControlChange(handleControlChangeUSB);
    MIDI.setHandleProgramChange(handleProgramChangeUSB);
    MIDI.setHandlePitchBend(handlePitchBendUSB);
    MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrameUSB);
    MIDI.setHandleSongPosition(handleSongPositionUSB);
    MIDI.setHandleSongSelect(handleSongSelectUSB);
    MIDI.setHandleTuneRequest(handleTuneRequestUSB);
    MIDI.setHandleClock(handleClockUSB);
    MIDI.setHandleStart(handleStartUSB);
    MIDI.setHandleContinue(handleContinueUSB);
    MIDI.setHandleStop(handleStopUSB);
    MIDIHW.setHandleNoteOn(handleNoteOnHW);
    MIDIHW.setHandleNoteOff(handleNoteOffHW);
    MIDIHW.setHandleControlChange(handleControlChangeHW);
    MIDIHW.setHandleProgramChange(handleProgramChangeHW);
    MIDIHW.setHandlePitchBend(handlePitchBendHW);
    MIDIHW.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrameHW);
    MIDIHW.setHandleSongPosition(handleSongPositionHW);
    MIDIHW.setHandleSongSelect(handleSongSelectHW);
    MIDIHW.setHandleTuneRequest(handleTuneRequestHW);
    MIDIHW.setHandleClock(handleClockHW);
    MIDIHW.setHandleStart(handleStartHW);
    MIDIHW.setHandleContinue(handleContinueHW);
    MIDIHW.setHandleStop(handleStopHW);

    // If this controller is routing messages, make nrpnStepInterval bigger (default is 5ms)
    if( config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB  || 
        config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW   ||
        config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_USB   || 
        config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_HW){
      nrpnIntervalStep = 10;    // milliseconds to send new NRPN message
    }

    // Fill MIDI Buffer with messages in config
    MidiBufferInit();
    
    // If there was a keyboard message found in config, begin keyboard communication
    // SERIALPRINT(F("IS KEYBOARD? ")); SERIALPRINTLN(keyboardEnable ? F("YES") : F("NO"));
    if(keyboardEnable){
      YTXKeyboard = new YTXKeyboard_();
    }

    // Load bank 0 to begin
    currentBank = memHost->LoadBank(0);
    
    #if defined(PRINT_MIDI_BUFFER)
    printMidiBuffer(); 
    #endif

    // SERIALPRINTLN("Waiting for rainbow...");
    // Initialize brigthness and power configuration
    feedbackHw.InitFb();
    
    // Wait for rainbow animation to end 
    while(waitingForRainbow){
      delay(1);
    }
    
    // Set all initial values for feedback to show
    feedbackHw.SetBankChangeFeedback(FB_BANK_CHANGED);

    // Check if firmware or config version changed and prepare for it
    if(memHost->FwConfigVersionChanged()){
      memHost->OnFwConfigVersionChange();
    }

    // Restore last controller state feature
    if(config->board.saveControllerState){
      antMillisSaveControllerState = millis();
      if(memHost->IsCtrlStateMemNew()){     // If first time saving a state or if config or firmware version changed
        uint timeout = 5000; //ms
        memHost->handleSaveControllerState(timeout);       // Saving initial state to clear eeprom memory
      }
      // Load controller state from EEPROM
      memHost->LoadControllerState();   
    }else{
      memHost->SetNewMemFlag();       // if feature is disabled, flag in EEPROM to clear state next time
    }

    //
    // config->board.initialDump = true;
    if(config->board.initialDump){
      DumpControllerState();
    }
    
    // Print valid message
    SERIALPRINTLN(F("YTX VALID CONFIG FOUND"));    
    SetStatusLED(STATUS_BLINK, 2, STATUS_FB_INIT);
  }else if (configStatus == CONFIG_NOT_VALID){
    SERIALPRINTLN(F("YTX VALID CONFIG NOT FOUND"));  
    SetStatusLED(STATUS_BLINK, 3, STATUS_FB_NO_CONFIG);  
  }else if (configStatus == FW_CONFIG_MISMATCH){
    SERIALPRINTLN(F("FIRMWARE AND CONFIG VERSION DON'T MATCH"));  
    SERIALPRINT(F("\nFW_VERSION: ")); SERIALPRINT(config->board.fwVersionMaj); SERIALPRINT(F(".")); SERIALPRINTLN(config->board.fwVersionMin);
    SERIALPRINT(F("CONFIG VERSION: ")); SERIALPRINT(config->board.configVersionMaj); SERIALPRINT(F(".")); SERIALPRINTLN(config->board.configVersionMin);

    SetStatusLED(STATUS_BLINK, 1, STATUS_FB_NO_CONFIG);  
  }

  
  // STATUS LED
  statusLED = new Adafruit_NeoPixel(N_STATUS_PIXEL, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800); 
  statusLED->begin();
  statusLED->setBrightness(STATUS_LED_BRIGHTNESS);
  statusLED->setPixelColor(0, 0, 0, 0); // Set initial color to OFF
  statusLED->clear(); // Set all pixel colors to 'off'
  statusLED->show();
  statusLED->show(); // This sends the updated pixel color to the hardware. Two show() to prevent bug that stays green
  
  SERIALPRINT(F("Free RAM: ")); SERIALPRINTLN(FreeMemory());  

  // Enable watchdog timer to reset if a freeze event happens
  Watchdog.enable(WATCHDOG_RESET_NORMAL);  // 1.5 seconds to reset
  antMillisWD = millis();
  
}

#ifdef INIT_CONFIG
void initConfig() {
  // SET NUMBER OF INPUTS OF EACH TYPE
  config->banks.count = 1;
  config->inputs.encoderCount = 0;
  config->inputs.analogCount = 68;
  config->inputs.digitalCount = 0;
  config->inputs.feedbackCount = 0;

  config->board.rainbowOn = false;
  config->board.takeoverMode = takeOverTypes::takeover_none;
  config->board.saveControllerState = false;
  config->board.initialDump = false;

  config->midiConfig.midiMergeFlags = 0x00;

  config->board.signature = SIGNATURE_CHAR;
  strcpy(config->board.deviceName, "ANALOG SPI");
  config->board.pid = 0xEBCA;
  strcpy(config->board.serialNumber, "Y20SXD001");

//  config->banks.shifterId[0] = 0;
//  config->banks.shifterId[1] = 1;
//  config->banks.shifterId[2] = 2;
//  config->banks.shifterId[3] = 3;
  config->banks.shifterId[0] = 0;
  config->banks.shifterId[1] = 0;
  config->banks.shifterId[2] = 0;
  config->banks.shifterId[3] = 0;
  config->banks.shifterId[4] = 0;
  config->banks.shifterId[5] = 0;
  config->banks.shifterId[6] = 0;
  config->banks.shifterId[7] = 0;
  
  config->banks.momToggFlags = 0b11111111;

  //  for(int i = 15; i>=0; i--){
  //    SERIALPRINT(((config->banks.momToggFlags)>>i)&1,BIN);
  //  }
  //  SERIALPRINTLN();

  config->hwMapping.encoder[0] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[1] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[2] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[3] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[4] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[5] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[6] = EncoderModuleTypes::ENCODER_NONE;
  config->hwMapping.encoder[7] = EncoderModuleTypes::ENCODER_NONE;

  config->hwMapping.digital[0][0] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][1] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][2] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][3] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][4] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][5] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][6] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[0][7] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][0] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][1] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][2] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][3] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][4] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][6] = DigitalModuleTypes::DIGITAL_NONE;
  config->hwMapping.digital[1][7] = DigitalModuleTypes::DIGITAL_NONE;


  config->hwMapping.analog[0][0] = AnalogModuleTypes::P41 | ((AnalogModuleTypes::P41)<<4);
  config->hwMapping.analog[0][1] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[0][2] = AnalogModuleTypes::P41;
  config->hwMapping.analog[0][3] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[0][4] = AnalogModuleTypes::P41;
  config->hwMapping.analog[0][5] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[0][6] = AnalogModuleTypes::P41;
  config->hwMapping.analog[0][7] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[1][0] = AnalogModuleTypes::P41;
  config->hwMapping.analog[1][1] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[1][2] = AnalogModuleTypes::P41;
  config->hwMapping.analog[1][3] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[1][4] = AnalogModuleTypes::P41;
  config->hwMapping.analog[1][5] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[1][6] = AnalogModuleTypes::P41;
  config->hwMapping.analog[1][7] = AnalogModuleTypes::ANALOG_NONE;

  config->hwMapping.analog[2][0] = AnalogModuleTypes::P41;
  config->hwMapping.analog[2][1] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[2][2] = AnalogModuleTypes::P41;
  config->hwMapping.analog[2][3] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[2][4] = AnalogModuleTypes::P41;
  config->hwMapping.analog[2][5] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[2][6] = AnalogModuleTypes::P41;
  config->hwMapping.analog[2][7] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[3][0] = AnalogModuleTypes::P41;
  config->hwMapping.analog[3][1] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[3][2] = AnalogModuleTypes::P41;
  config->hwMapping.analog[3][3] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[3][4] = AnalogModuleTypes::P41;
  config->hwMapping.analog[3][5] = AnalogModuleTypes::ANALOG_NONE;
  config->hwMapping.analog[3][6] = AnalogModuleTypes::P41;
  config->hwMapping.analog[3][7] = AnalogModuleTypes::ANALOG_NONE;
}


#define INTENSIDAD_NP 255
void initInputsConfig(uint8_t b) {
  int i = 0;
  // uint8_t idx = (b==0 ? 5 : b==1 ? 1 : b==2 ? 10 : b==3 ? 12 : b == 4 ? 9 : b==5 ? 2 : b == 6 ? 6 : 14);
  uint8_t idx = 64;

  for (i = 0; i < config->inputs.encoderCount; i++) {
    encoder[i].rotBehaviour.hwMode = rotaryModes::rot_absolute;
    encoder[i].rotBehaviour.speed = encoderRotarySpeed::rot_variable_speed_2;
   
//    encoder[i].rotaryConfig.message = (i) % (rotary_msg_rpn + 1) + 1;
    encoder[i].rotaryConfig.message = rotary_msg_cc;
    encoder[i].rotaryConfig.channel = 0;
//    encoder[i].rotaryConfig.channel = b;
    encoder[i].rotaryConfig.midiPort = midiPortsType::midi_hw_usb;
    //    SERIALPRINTLN(encoder[i].rotaryConfig.midiPort);
    encoder[i].rotaryConfig.parameter[rotary_LSB] = i;
    encoder[i].rotaryConfig.parameter[rotary_MSB] = 0;
    encoder[i].rotaryConfig.parameter[rotary_minLSB] = 0;
    encoder[i].rotaryConfig.parameter[rotary_minMSB] = 0;
    encoder[i].rotaryConfig.parameter[rotary_maxLSB] = 127;
    encoder[i].rotaryConfig.parameter[rotary_maxMSB] = 0;
    strcpy(encoder[i].rotaryConfig.comment, "");
    
    encoder[i].rotaryFeedback.mode = encoderRotaryFeedbackMode::fb_fill;
   // encoder[i].rotaryFeedback.mode = i % 4;
    encoder[i].rotaryFeedback.source = feedbackSource::fb_src_local_usb_hw;
    encoder[i].rotaryFeedback.channel = 0;
//    encoder[i].rotaryFeedback.channel = b;
//    encoder[i].rotaryFeedback.message = (i) % (rotary_msg_rpn + 1) + 1;
    encoder[i].rotaryFeedback.message = rotary_msg_cc;
    encoder[i].rotaryFeedback.parameterLSB = i;
    encoder[i].rotaryFeedback.parameterMSB = 0;
    encoder[i].rotaryFeedback.rotaryValueToColor = true;
    encoder[i].rotaryFeedback.valueToIntensity = true;
    //    encoder[i].rotaryFeedback.color[R-R] = (i%10)*7+20;
    //    encoder[i].rotaryFeedback.color[R-R] = (i*8)+20*(b+1);
    //    encoder[i].rotaryFeedback.color[G-R] = (i*4)+40*b;
    //    encoder[i].rotaryFeedback.color[B-R] = (i*2)+20;
    
    encoder[i].rotaryFeedback.color[R_INDEX] = pgm_read_byte(&colorRangeTable[idx][R_INDEX]);
    encoder[i].rotaryFeedback.color[G_INDEX] = pgm_read_byte(&colorRangeTable[idx][G_INDEX]);
    encoder[i].rotaryFeedback.color[B_INDEX] = pgm_read_byte(&colorRangeTable[idx][B_INDEX]);
    
    // encoder[i].rotaryFeedback.color[R_INDEX] =  (b == 0) ? 0x9A : (b == 1) ? INTENSIDAD_NP : (b == 2) ? INTENSIDAD_NP : (b == 3) ? 0             : (b == 4) ? 0             : (b == 5) ? 0xFF : (b == 6) ? 0xFF : 0x00;
    // encoder[i].rotaryFeedback.color[G_INDEX] =  (b == 0) ? 0xCD : (b == 1) ? INTENSIDAD_NP : (b == 2) ? 0x00          : (b == 3) ? INTENSIDAD_NP : (b == 4) ? 0             : (b == 5) ? 0x8C : (b == 6) ? 0x14 : INTENSIDAD_NP;
    // encoder[i].rotaryFeedback.color[B_INDEX] =  (b == 0) ? 0x32 : (b == 1) ? 0x00          : (b == 2) ? INTENSIDAD_NP : (b == 3) ? INTENSIDAD_NP : (b == 4) ? INTENSIDAD_NP : (b == 5) ? 0x00 : (b == 6) ? 0x93 : 0x00;

//    encoder[i].rotaryFeedback.color[R_INDEX] = (b == 0) ? 0x9A : (b == 1) ? INTENSIDAD_NP : (b == 2) ? INTENSIDAD_NP  : 0;
//    encoder[i].rotaryFeedback.color[G_INDEX] = (b == 0) ? 0xCD : (b == 1) ? INTENSIDAD_NP : (b == 2) ? 0x00           : INTENSIDAD_NP;
//    encoder[i].rotaryFeedback.color[B_INDEX] = (b == 0) ? 0x32 : (b == 1) ? 0x00 :          (b == 2) ? INTENSIDAD_NP  : INTENSIDAD_NP;
    

    encoder[i].switchConfig.mode = switchModes::switch_mode_message;
    encoder[i].switchConfig.doubleClick = switchDoubleClickModes::switch_doubleClick_none;
    encoder[i].switchConfig.action = switchActions::switch_momentary;
    encoder[i].switchConfig.channel = 0;
    encoder[i].switchConfig.message = switchMessageTypes::switch_msg_note;
    encoder[i].switchConfig.midiPort = midiPortsType::midi_hw_usb;
    encoder[i].switchConfig.parameter[switch_parameter_LSB] = i;
    encoder[i].switchConfig.parameter[switch_parameter_MSB] = 0;
    encoder[i].switchConfig.parameter[switch_minValue_LSB] = 0;
    encoder[i].switchConfig.parameter[switch_minValue_MSB] = 0;
    encoder[i].switchConfig.parameter[switch_maxValue_LSB] = 127;
    encoder[i].switchConfig.parameter[switch_maxValue_MSB] = 0;

    
    encoder[i].switchFeedback.source = feedbackSource::fb_src_local_usb_hw;
    encoder[i].switchFeedback.localBehaviour = fb_lb_on_with_press;
    encoder[i].switchFeedback.channel = 0;
//    encoder[i].switchFeedback.message = (i) % (switch_msg_rpn + 1) + 1;
    encoder[i].switchFeedback.message = switch_msg_note;
    encoder[i].switchFeedback.parameterLSB = i;
    encoder[i].switchFeedback.parameterMSB = 0;
    encoder[i].switchFeedback.valueToColor = false;
    encoder[i].switchFeedback.lowIntensityOff = false;
    encoder[i].switchFeedback.valueToIntensity = true;
    encoder[i].switchFeedback.color[R_INDEX] = pgm_read_byte(&colorRangeTable[idx][R_INDEX]);
    encoder[i].switchFeedback.color[G_INDEX] = pgm_read_byte(&colorRangeTable[idx][G_INDEX]);
    encoder[i].switchFeedback.color[B_INDEX] = pgm_read_byte(&colorRangeTable[idx][B_INDEX]);
    // encoder[i].switchFeedback.color[R_INDEX] = 0xDD;
    // encoder[i].switchFeedback.color[G_INDEX] = 0x00;
    // encoder[i].switchFeedback.color[B_INDEX] = 0x00;
  }

  // encoder[0].switchConfig.mode = switchModes::switch_mode_velocity;
  
  // encoder[0].switchConfig.parameter[switch_parameter_MSB] = 0;
  // encoder[0].switchConfig.parameter[switch_minValue_LSB] = 0;
  // encoder[0].switchConfig.parameter[switch_minValue_MSB] = 0;
  // encoder[0].switchConfig.parameter[switch_maxValue_LSB] = 127;
  // encoder[0].switchConfig.parameter[switch_maxValue_MSB] = 0;

//  // BANK 0 shifter
//  encoder[0].switchFeedback.color[R_INDEX] = 0x9A;
//  encoder[0].switchFeedback.color[G_INDEX] = 0xCD;
//  encoder[0].switchFeedback.color[B_INDEX] = 0x32;
//  // BANK 1 shifter
//  encoder[1].switchFeedback.color[R_INDEX] = INTENSIDAD_NP;
//  encoder[1].switchFeedback.color[G_INDEX] = INTENSIDAD_NP;
//  encoder[1].switchFeedback.color[B_INDEX] = 0x00;
//  // BANK 2 shifter
//  encoder[2].switchFeedback.color[R_INDEX] = INTENSIDAD_NP;
//  encoder[2].switchFeedback.color[G_INDEX] = 0x00;
//  encoder[2].switchFeedback.color[B_INDEX] = INTENSIDAD_NP;
//  // BANK 3 shifter
//  encoder[3].switchFeedback.color[R_INDEX] = 0;
//  encoder[3].switchFeedback.color[G_INDEX] = INTENSIDAD_NP;
//  encoder[3].switchFeedback.color[B_INDEX] = INTENSIDAD_NP;

  for (i = 0; i < config->inputs.digitalCount; i++) {
    //    digital[i].actionConfig.action = (i % 2) * switchActions::switch_toggle;
    digital[i].actionConfig.action = switchActions::switch_momentary;
    //    digital[i].actionConfig.message = (i) % (digital_rpn + 1) + 1;
    digital[i].actionConfig.message = digitalMessageTypes::digital_msg_note;

    digital[i].actionConfig.channel = (8+i <= 127) ? 0 : 1;
//    digital[i].actionConfig.channel = b;
    digital[i].actionConfig.midiPort = midiPortsType::midi_hw_usb;
    //    SERIALPRINTLN(digital[i].actionConfig.midiPort);
    //    if(!i%digital_ks){
    //      digital[i].actionConfig.parameter[digital_key] = '+';
    //      digital[i].actionConfig.parameter[digital_modifier] = KEY_LEFT_CTRL;
    //    }else{
    digital[i].actionConfig.parameter[digital_LSB] = (8+i <= 127) ? 8+i : i;
    digital[i].actionConfig.parameter[digital_MSB] = 0;
    uint16_t minVal = 0;
    uint16_t maxVal = 127;
    digital[i].actionConfig.parameter[digital_minLSB] = minVal & 0xFF;
    digital[i].actionConfig.parameter[digital_minMSB] = (minVal >> 7) & 0xFF;
    digital[i].actionConfig.parameter[digital_maxLSB] = maxVal & 0xFF;
    digital[i].actionConfig.parameter[digital_maxMSB] = (maxVal >> 7) & 0xFF;
    strcpy(digital[i].actionConfig.comment, "");

    //    digital[6].actionConfig.message = digital_msg_key;
    //    digital[6].actionConfig.parameter[digital_LSB] = KEY_UP_ARROW;
    //    digital[13].actionConfig.message = digital_msg_key;
    //    digital[13].actionConfig.parameter[digital_LSB] = KEY_LEFT_ARROW;
    //    digital[14].actionConfig.message = digital_msg_key;
    //    digital[14].actionConfig.parameter[digital_LSB] = KEY_DOWN_ARROW;
    //    digital[15].actionConfig.message = digital_msg_key;
    //    digital[15].actionConfig.parameter[digital_LSB] = KEY_RIGHT_ARROW<<<  ;

    digital[i].feedback.source = feedbackSource::fb_src_local_usb_hw;
    digital[i].feedback.localBehaviour = feedbackLocalBehaviour::fb_lb_on_with_press;
    digital[i].feedback.channel = (8+i <= 127) ? 0 : 1;
//    digital[i].feedback.channel = b;
    digital[i].feedback.message = digitalMessageTypes::digital_msg_note;
    digital[i].feedback.parameterLSB = (8+i <= 127) ? 8+i : i;
    digital[i].feedback.parameterMSB = 0;
    digital[i].feedback.valueToColor = true;
    digital[i].feedback.lowIntensityOff = false;
    digital[i].feedback.valueToIntensity = true;
    digital[i].feedback.color[R_INDEX] = pgm_read_byte(&colorRangeTable[idx][R_INDEX]);
    digital[i].feedback.color[G_INDEX] = pgm_read_byte(&colorRangeTable[idx][G_INDEX]);
    digital[i].feedback.color[B_INDEX] = pgm_read_byte(&colorRangeTable[idx][B_INDEX]);
    // digital[i].feedback.color[R_INDEX] = (b == 0) ? 0x9A : (b == 1) ? INTENSIDAD_NP : (b == 2) ? INTENSIDAD_NP : (b == 3) ? 0             : (b == 4) ? 0              : (b == 5) ? 0xFF : (b == 6) ? 0xFF : 0x00;
    // digital[i].feedback.color[G_INDEX] = (b == 0) ? 0xCD : (b == 1) ? INTENSIDAD_NP : (b == 2) ? 0x00          : (b == 3) ? INTENSIDAD_NP : (b == 4) ? 0              : (b == 5) ? 0x8C : (b == 6) ? 0x14 : INTENSIDAD_NP;
    // digital[i].feedback.color[B_INDEX] = (b == 0) ? 0x32 : (b == 1) ? 0x00          : (b == 2) ? INTENSIDAD_NP : (b == 3) ? INTENSIDAD_NP : (b == 4) ? INTENSIDAD_NP  : (b == 5) ? 0x00 : (b == 6) ? 0x93 : 0x00;
  }
  
   // BANK 0 shifter
  // digital[0].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[0].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[0].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 1 shifter
  // digital[1].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[1].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[1].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 2 shifter
  // digital[2].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[2].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[2].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 3 shifter
  // digital[3].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[3].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[3].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;

  // // BANK 4 shifter
  // digital[8].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[8].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[8].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 5 shifter
  // digital[9].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[9].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[9].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 6 shifter
  // digital[10].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[10].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[10].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  // // BANK 7 shifter
  // digital[11].feedback.color[R_INDEX] = BANK_INDICATOR_COLOR_R;
  // digital[11].feedback.color[G_INDEX] = BANK_INDICATOR_COLOR_G;
  // digital[11].feedback.color[B_INDEX] = BANK_INDICATOR_COLOR_B;
  
    for (int i = 0; i < config->inputs.analogCount; i++) {
//    analog[i].message = analog_msg_nrpn;
//    analog[i].message = i%2 ? analogMessageTypes::analog_msg_cc : analogMessageTypes::analog_msg_nrpn;
    analog[i].message = analogMessageTypes::analog_msg_cc;
    if(i<32 || i>35)
      analog[i].message = analogMessageTypes::analog_msg_none;
    else
      analog[i].message = analogMessageTypes::analog_msg_cc;

    analog[i].channel = 1;
    analog[i].midiPort = midiPortsType::midi_hw_usb;
    analog[i].parameter[rotary_LSB] = 32+i;
//                                          (analog[i].message == analogMessageTypes::analog_msg_pb)) ? 127 : 0;

    strcpy(analog[i].comment, "");

    analog[i].splitMode = splitModes::normal;
  }

  for (i = 0; i < config->inputs.analogCount; i++) {
//    analog[i].message = analog_msg_nrpn;
//    analog[i].message = i%2 ? analogMessageTypes::analog_msg_cc : analogMessageTypes::analog_msg_nrpn;
    analog[i].message = analogMessageTypes::analog_msg_cc;
    analog[i].channel = b;
    analog[i].midiPort = midiPortsType::midi_hw_usb;
    analog[i].parameter[rotary_LSB] = 32+i;
    analog[i].parameter[rotary_MSB] = 0;
    analog[i].parameter[rotary_minLSB] = 0;
//    analog[i].parameter[rotary_minMSB] = (i%4 == 0) ? 16 : ((i+1)%4 == 0) ? 32 : ((i+2)%4 == 0) ? 64 : 127;
    analog[i].parameter[rotary_minMSB] = 0;
    analog[i].parameter[rotary_maxLSB] = 127;
//    analog[i].parameter[rotary_maxMSB] = (i%4 == 0) ? 127 : ((i+1)%4 == 0) ? 64 : ((i+2)%4 == 0) ? 32 : 16;
    analog[i].parameter[rotary_maxMSB] = 127;
    
//    analog[i].parameter[rotary_maxMSB] = ((analog[i].message == analogMessageTypes::analog_msg_nrpn) ||
//                                          (analog[i].message == analogMessageTypes::analog_msg_rpn) ||
//                                          (analog[i].message == analogMessageTypes::analog_msg_pb)) ? 127 : 0;

    strcpy(analog[i].comment, "ytx Ana.");
    
//    analog[i].feedback.message = i%2 ? analogMessageTypes::analog_msg_cc : analogMessageTypes::analog_msg_nrpn;
    analog[i].feedback.message = analog_msg_cc;
    analog[i].feedback.channel = b;
    analog[i].feedback.source = midiPortsType::midi_hw_usb;
    analog[i].feedback.parameterLSB = 32+i;
    analog[i].feedback.parameterMSB = 0;
    analog[i].feedback.valueToColor = false;
    analog[i].feedback.color[R_INDEX] = 0xFF;
    analog[i].feedback.color[G_INDEX] = 0x00;
    analog[i].feedback.color[B_INDEX] = 0x00;
  }
}
#endif

void initFixedConfig() {
  // MOD HERE IF NEEDED
  
}


void printConfig(uint8_t block, uint8_t i){
  Watchdog.disable();
  Watchdog.enable(WATCHDOG_RESET_PRINT);
  if(block == ytxIOBLOCK::Configuration){
    SERIALPRINTLN(F("--------------------------------------------------------"));
    SERIALPRINTLN(F("GENERAL CONFIGURATION"));

    SERIALPRINT(F("Controller name: ")); SERIALPRINTLN(config->board.deviceName);

    SERIALPRINT(F("Config signature: ")); SERIALPRINTLNF(config->board.signature, HEX);
    
    SERIALPRINT(F("\nFW_VERSION: ")); SERIALPRINT(config->board.fwVersionMaj); SERIALPRINT(F(".")); SERIALPRINTLN(config->board.fwVersionMin);
    SERIALPRINT(F("HW_VERSION: ")); SERIALPRINT(config->board.hwVersionMaj); SERIALPRINT(F(".")); SERIALPRINTLN(config->board.hwVersionMin);
    SERIALPRINT(F("CONFIG VERSION: ")); SERIALPRINT(config->board.configVersionMaj); SERIALPRINT(F(".")); SERIALPRINTLN(config->board.configVersionMin);

    SERIALPRINT(F("Encoder count: ")); SERIALPRINTLN(config->inputs.encoderCount);
    SERIALPRINT(F("Analog count: ")); SERIALPRINTLN(config->inputs.analogCount);
    SERIALPRINT(F("Digital count: ")); SERIALPRINTLN(config->inputs.digitalCount);
    SERIALPRINT(F("Feedback count: ")); SERIALPRINTLN(config->inputs.feedbackCount);
    
    SERIALPRINT(F("Device name: ")); SERIALPRINTLN((char*)config->board.deviceName);
    SERIALPRINT(F("USB-PID: ")); SERIALPRINTLNF(config->board.pid,HEX);
    SERIALPRINT(F("Serial number: ")); SERIALPRINTLN((char*)config->board.serialNumber);

    SERIALPRINT(F("Boot FLAG: ")); SERIALPRINTLN(config->board.bootFlag ? F("YES") : F("NO"));
    SERIALPRINT(F("Takeover Moder: ")); SERIALPRINTLN(config->board.takeoverMode == 0 ? F("NONE") :
                                                              config->board.takeoverMode == 1 ? F("PICKUP") :
                                                              config->board.takeoverMode == 2 ? F("VALUE SCALING") : F("NOT DEFINED"));
    SERIALPRINT(F("Rainbow ON: ")); SERIALPRINTLN(config->board.rainbowOn ? F("YES") : F("NO"));
    SERIALPRINT(F("Remote banks: ")); SERIALPRINTLN(config->board.remoteBanks ? F("YES") : F("NO"));
    SERIALPRINT(F("Remember Controller State: ")); SERIALPRINTLN(config->board.saveControllerState ? F("YES") : F("NO"));
    SERIALPRINT(F("Dump state on startup: ")); SERIALPRINTLN(config->board.initialDump ? F("YES") : F("NO"));
    
    SERIALPRINT(F("Qty 7 bit msgs: ")); SERIALPRINTLN(config->board.qtyMessages7bit);
    SERIALPRINT(F("Qty 14 bit msgs: ")); SERIALPRINTLN(config->board.qtyMessages14bit);
    
    for(int mE = 0; mE < 8; mE++){
      SERIALPRINT(F("Encoder module ")); SERIALPRINT(mE); SERIALPRINT(F(": ")); 
      uint8_t moduleType = config->hwMapping.encoder[mE]&0x0F;
      SERIALPRINTLN(moduleType == 0 ? F("NONE") :
                        moduleType == 1 ? F("E41H") :
                        moduleType == 2 ? F("E41V") :
                        moduleType == 3 ? F("E41H_D") : 
                        moduleType == 4 ? F("E41V_D") : F("NOT DEFINED"));
    }
    for(int mE = 0; mE < 8; mE++){
      SERIALPRINT(F("Encoder module ")); SERIALPRINT(mE+8); SERIALPRINT(F(": ")); 
      uint8_t moduleType = (config->hwMapping.encoder[mE]>>4)&0x0F;
      SERIALPRINTLN(moduleType == 0 ? F("NONE") :
                    moduleType == 1 ? F("E41H") :
                    moduleType == 2 ? F("E41V") :
                    moduleType == 3 ? F("E41H_D") : 
                    moduleType == 4 ? F("E41V_D") : F("NOT DEFINED"));
    }
    for(int aPort = 0; aPort < 8; aPort++){
      for(int mA = 0; mA < 8; mA++){
        int moduleType = (aPort < 4) ? config->hwMapping.analog[aPort][mA]&0x0F : 
                          (config->hwMapping.analog[aPort-4][mA]>>4)&0x0F;

        SERIALPRINT(F("Analog port/module ")); SERIALPRINT(aPort); SERIALPRINT(F("/")); SERIALPRINT(mA); SERIALPRINT(F(": ")); 
        SERIALPRINTLN(    moduleType == AnalogModuleTypes::ANALOG_NONE ? F("NONE") :
                          moduleType == AnalogModuleTypes::P41         ? F("P41") :
                          moduleType == AnalogModuleTypes::F41         ? F("F41") :
                          moduleType == AnalogModuleTypes::JAF         ? F("JAF") : 
                          moduleType == AnalogModuleTypes::JAL         ? F("JAL") : 
                          moduleType == AnalogModuleTypes::F21100      ? F("F21.100") : F("NOT DEFINED"));

      }
    }
    for(int dPort = 0; dPort < 2; dPort++){
      for(int mD = 0; mD < 8; mD++){
        SERIALPRINT(F("Digital port/module ")); SERIALPRINT(dPort); SERIALPRINT(F("/")); SERIALPRINT(mD); SERIALPRINT(F(": ")); 
        SERIALPRINTLN(config->hwMapping.digital[dPort][mD] == 0 ? F("NONE") :
                          config->hwMapping.digital[dPort][mD] == 1 ? F("RB41") :
                          config->hwMapping.digital[dPort][mD] == 2 ? F("RB42") :
                          config->hwMapping.digital[dPort][mD] == 3 ? F("RB82") : 
                          config->hwMapping.digital[dPort][mD] == 4 ? F("ARC41") : F("NOT DEFINED"));
      }
    }
    
    SERIALPRINT(F("Midi merge routing:")); 
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB)  SERIALPRINT(F(" USB -> USB /"));
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW)   SERIALPRINT(F(" USB -> HW /"));
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_USB)   SERIALPRINT(F(" HW -> USB /"));
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_HW)    SERIALPRINT(F(" HW -> HW"));
    SERIALPRINTLN();

    SERIALPRINT(F("\nSpecial Features Channels\n"));
    SERIALPRINT(F("\tValue to Color: ")); SERIALPRINTLN(config->midiConfig.valueToColorChannel+1);
    SERIALPRINT(F("\tValue to Intensity: ")); SERIALPRINTLN(config->midiConfig.valueToIntensityChannel+1);
    SERIALPRINT(F("\tVumeter: ")); SERIALPRINTLN(config->midiConfig.vumeterChannel+1);
    SERIALPRINT(F("\tSplit Mode: ")); SERIALPRINTLN(config->midiConfig.splitModeChannel+1);
    SERIALPRINT(F("\tRemote Banks: ")); SERIALPRINTLN(config->midiConfig.remoteBankChannel+1);

    SERIALPRINT(F("\nNumber of banks: ")); SERIALPRINTLN(config->banks.count);
    for(int b = 0; b < MAX_BANKS; b++){
      SERIALPRINT(F("Bank ")); SERIALPRINT(b); SERIALPRINT(F(" shifter ID: ")); SERIALPRINT(config->banks.shifterId[b]); 
      SERIALPRINT(F(" Action for bank ")); SERIALPRINT(b); SERIALPRINT(F(" is ")); SERIALPRINTLN((config->banks.momToggFlags>>b) & 0x1 ? F("TOGGLE") : F("MOMENTARY")); 
    }
    
  }else if(block == ytxIOBLOCK::Encoder){
    SERIALPRINTLN(F("--------------------------------------------------------"));
    SERIALPRINT(F("Encoder ")); SERIALPRINT(i); SERIALPRINTLN(F(":"));
    SERIALPRINT(F("HW mode: ")); SERIALPRINTLN( encoder[i].rotBehaviour.hwMode == rotaryModes::rot_absolute         ? F("Absolute") : 
                                                        encoder[i].rotBehaviour.hwMode == rotaryModes::rot_rel_binaryOffset ? F("Binary Offset") : 
                                                        encoder[i].rotBehaviour.hwMode == rotaryModes::rot_rel_complement2  ? F("2's Complement") : 
                                                        encoder[i].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit    ? F("Signed Bit") : 
                                                        encoder[i].rotBehaviour.hwMode == rotaryModes::rot_rel_signedBit2   ? F("Signed Bit 2") : 
                                                        encoder[i].rotBehaviour.hwMode == rotaryModes::rot_rel_singleValue  ? F("Single Value") : F("NOT DEFINED"));
    SERIALPRINT(F("Speed: ")); SERIALPRINTLN( encoder[i].rotBehaviour.speed == rot_variable_speed_1 ? F("ACCEL 1") : 
                                                      encoder[i].rotBehaviour.speed == rot_variable_speed_2 ? F("ACCEL 2") : 
                                                      encoder[i].rotBehaviour.speed == rot_variable_speed_3 ? F("ACCEL 3") : 
                                                      encoder[i].rotBehaviour.speed == rot_fixed_speed_1 ? F("FIXED 1") : 
                                                      encoder[i].rotBehaviour.speed == rot_fixed_speed_2 ? F("FIXED 2") : 
                                                      encoder[i].rotBehaviour.speed == rot_fixed_speed_3 ? F("FIXED 3") : F("NOT DEFINED"));
    
    SERIALPRINTLN(); 
    SERIALPRINT(F("Rotary Message: ")); 
    switch(encoder[i].rotaryConfig.message){
      case rotary_msg_none:   { SERIALPRINTLN(F("NONE"));           } break;
      case rotary_msg_note:   { SERIALPRINTLN(F("NOTE"));           } break;
      case rotary_msg_cc:     { SERIALPRINTLN(F("CC"));             } break;
      case rotary_msg_vu_cc:  { SERIALPRINTLN(F("VUMETER CC"));     } break;
      case rotary_msg_pc_rel: { SERIALPRINTLN(F("PROGRAM CHANGE")); } break;
      case rotary_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));           } break;
      case rotary_msg_rpn:    { SERIALPRINTLN(F("RPN"));            } break;
      case rotary_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));     } break;
      case rotary_msg_key:    { SERIALPRINTLN(F("KEYSTROKE"));      } break;
      default:                { SERIALPRINTLN(F("NOT DEFINED"));    } break;
    }
    if(encoder[i].rotaryConfig.message != rotary_msg_key){
      SERIALPRINT(F("Rotary MIDI Port: ")); SERIALPRINTLN( encoder[i].rotaryConfig.midiPort == 0 ? F("NONE") : 
                                                                encoder[i].rotaryConfig.midiPort == 1 ? F("USB") :
                                                                encoder[i].rotaryConfig.midiPort == 2 ? F("MIDI HW") : 
                                                                encoder[i].rotaryConfig.midiPort == 3 ? F("USB + MIDI HW") : F("NOT DEFINED"));
      SERIALPRINT(F("Rotary MIDI Channel: ")); SERIALPRINTLN(encoder[i].rotaryConfig.channel+1);
      SERIALPRINT(F("Rotary Parameter: ")); SERIALPRINTLN(IS_ENCODER_ROT_14_BIT (i) ? 
                                                                  encoder[i].rotaryConfig.parameter[rotary_MSB] << 7 | encoder[i].rotaryConfig.parameter[rotary_LSB] : 
                                                                  encoder[i].rotaryConfig.parameter[rotary_LSB]);
      SERIALPRINT(F("Rotary MIN value: ")); SERIALPRINTLN(IS_ENCODER_ROT_14_BIT (i) ?
                                                                  encoder[i].rotaryConfig.parameter[rotary_minMSB] << 7 | encoder[i].rotaryConfig.parameter[rotary_minLSB] :
                                                                  encoder[i].rotaryConfig.parameter[rotary_minLSB]);
      SERIALPRINT(F("Rotary MAX value: ")); SERIALPRINTLN(IS_ENCODER_ROT_14_BIT (i) ?
                                                                  encoder[i].rotaryConfig.parameter[rotary_maxMSB] << 7 | encoder[i].rotaryConfig.parameter[rotary_maxLSB] :
                                                                  encoder[i].rotaryConfig.parameter[rotary_maxLSB]);
    }else{
      SERIALPRINT(F("Rotary Modifier Left: ")); SERIALPRINTLN(encoder[i].rotaryConfig.parameter[rotary_modifierLeft] == 0 ? F("NONE")      :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierLeft] == 1   ? F("ALT")       :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierLeft] == 2   ? F("CTRL/CMD")  :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierLeft] == 3   ? F("SHIFT")     : F("NOT DEFINED"));
      SERIALPRINT(F("Rotary Key Left: ")); SERIALPRINTLN((char) encoder[i].rotaryConfig.parameter[rotary_keyLeft]);
      SERIALPRINT(F("Rotary Modifier Right: ")); SERIALPRINTLN(encoder[i].rotaryConfig.parameter[rotary_modifierRight]  == 0  ? F("NONE")      :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierRight]     == 1  ? F("ALT")       :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierRight]     == 2  ? F("CTRL/CMD")  :
                                                                    encoder[i].rotaryConfig.parameter[rotary_modifierRight]     == 3  ? F("SHIFT")     : F("NOT DEFINED"));
      SERIALPRINT(F("Rotary Key Right: ")); SERIALPRINTLN((char) encoder[i].rotaryConfig.parameter[rotary_keyRight]);
      
    }
    SERIALPRINT(F("Rotary Comment: ")); SERIALPRINTLN((char*)encoder[i].rotaryConfig.comment);

    SERIALPRINTLN(); 
    SERIALPRINT(F("Rotary Feedback mode: ")); SERIALPRINTLN(encoder[i].rotaryFeedback.mode == 0 ? F("SPOT") : 
                                                                    encoder[i].rotaryFeedback.mode == 1 ? F("MIRROR") :
                                                                    encoder[i].rotaryFeedback.mode == 2 ? F("FILL") : 
                                                                    encoder[i].rotaryFeedback.mode == 3 ? F("PIVOT") : F("NOT DEFINED"));
    SERIALPRINT(F("Rotary Feedback source: ")); SERIALPRINTLN(encoder[i].rotaryFeedback.source == fb_src_usb ? F("USB") : 
                                                                      encoder[i].rotaryFeedback.source == fb_src_hw ? F("MIDI HW") :
                                                                      encoder[i].rotaryFeedback.source == fb_src_hw_usb ? F("USB + MIDI HW") : 
                                                                      encoder[i].rotaryFeedback.source == fb_src_local ? F("LOCAL") :
                                                                      encoder[i].rotaryFeedback.source == fb_src_local_usb ? F("LOCAL + USB") :
                                                                      encoder[i].rotaryFeedback.source == fb_src_local_hw ? F("LOCAL + MIDI HW") :
                                                                      encoder[i].rotaryFeedback.source == fb_src_local_usb_hw ? F("LOCAL + USB + MIDI HW") :F("NOT DEFINED"));             
    SERIALPRINT(F("Rotary Feedback Message: "));
    switch(encoder[i].rotaryFeedback.message){
      case rotary_msg_none:   { SERIALPRINTLN(F("NONE"));           } break;
      case rotary_msg_note:   { SERIALPRINTLN(F("NOTE"));           } break;
      case rotary_msg_cc:     { SERIALPRINTLN(F("CC"));             } break;
      case rotary_msg_vu_cc:  { SERIALPRINTLN(F("VUMETER CC"));     } break;
      case rotary_msg_pc_rel: { SERIALPRINTLN(F("PROGRAM CHANGE")); } break;
      case rotary_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));           } break;
      case rotary_msg_rpn:    { SERIALPRINTLN(F("RPN"));            } break;
      case rotary_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));     } break;
      default:                { SERIALPRINTLN(F("NOT DEFINED"));    } break;
    }                                                                                                                         
    SERIALPRINT(F("Rotary Feedback MIDI Channel: ")); SERIALPRINTLN(encoder[i].rotaryFeedback.channel+1);
    SERIALPRINT(F("Rotary Feedback Parameter: ")); SERIALPRINTLN(IS_ENCODER_ROT_FB_14_BIT (i) ?
                                                                          encoder[i].rotaryFeedback.parameterMSB << 7 | encoder[i].rotaryFeedback.parameterLSB : 
                                                                          encoder[i].rotaryFeedback.parameterLSB);
    SERIALPRINT(F("Rotary feedback Value to color: ")); SERIALPRINTLN(encoder[i].rotaryFeedback.rotaryValueToColor ? F("YES") : F("NO"));
    SERIALPRINT(F("Rotary feedback Value to Intensity: ")); SERIALPRINTLN(encoder[i].rotaryFeedback.valueToIntensity ? F("YES") : F("NO"));
    SERIALPRINT(F("Rotary Feedback Color: "));  SERIALPRINTF(encoder[i].rotaryFeedback.color[0],HEX); 
                                                    SERIALPRINTF(encoder[i].rotaryFeedback.color[1],HEX);
                                                    SERIALPRINTLNF(encoder[i].rotaryFeedback.color[2],HEX);
        
                                                              
    SERIALPRINTLN(); 
    SERIALPRINT(F("Switch action: ")); SERIALPRINTLN(encoder[i].switchConfig.action == 0 ? F("MOMENTARY")   : F("TOGGLE"));
    SERIALPRINT(F("Switch double click config: ")); SERIALPRINTLN(encoder[i].switchConfig.doubleClick == 0  ? F("NONE") :
                                                                          encoder[i].switchConfig.doubleClick == 1  ? F("JUMP TO MIN") :
                                                                          encoder[i].switchConfig.doubleClick == 2  ? F("JUMP TO CENTER") : 
                                                                          encoder[i].switchConfig.doubleClick == 3  ? F("JUMP TO MAX") : F("NOT DEFINED"));
    SERIALPRINT(F("Switch Mode: ")); 
    switch(encoder[i].switchConfig.mode){
      case switch_mode_none:            { SERIALPRINTLN(F("NONE"));                       } break;
      case switch_mode_message:         { SERIALPRINTLN(F("MIDI MSG"));                   } break;
      case switch_mode_shift_rot:       { SERIALPRINTLN(F("SHIFT ROTARY ACTION"));        } break;
      case switch_mode_fine:            { SERIALPRINTLN(F("FINE ADJUST"));                } break;
      case switch_mode_2cc:             { SERIALPRINTLN(F("DOUBLE CC"));                  } break;
      case switch_mode_quick_shift:     { SERIALPRINTLN(F("QUICK SHIFT TO BANK"));        } break;
      case switch_mode_quick_shift_note:{ SERIALPRINTLN(F("QUICK SHIFT TO BANK + NOTE")); } break;
      default:                          { SERIALPRINTLN(F("NOT DEFINED"));                } break;
    }                                                                         

    SERIALPRINT(F("Switch Message: ")); 
    if(encoder[i].switchConfig.mode != switch_mode_shift_rot){
      SERIALPRINT(encoder[i].switchConfig.message); SERIALPRINT(F(" ")); 
      switch(encoder[i].switchConfig.message){
        case switch_msg_note:   { SERIALPRINTLN(F("NOTE"));             } break;
        case switch_msg_cc:     { SERIALPRINTLN(F("CC"));               } break;
        case switch_msg_pc:     { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
        case switch_msg_pc_m:   { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
        case switch_msg_pc_p:   { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
        case switch_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));             } break;
        case switch_msg_rpn:    { SERIALPRINTLN(F("RPN"));              } break;
        case switch_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));       } break;
        case switch_msg_key:    { SERIALPRINTLN(F("KEYSTROKE"));        } break;
        default:                { SERIALPRINTLN(F("NOT DEFINED"));      } break;
      }
    }else if(encoder[i].switchConfig.mode == switch_mode_shift_rot){
      SERIALPRINT(encoder[i].switchConfig.message); SERIALPRINT(F(" - ")); 
      switch(encoder[i].rotaryFeedback.message){
        case rotary_msg_none:   { SERIALPRINTLN(F("NONE"));           } break;
        case rotary_msg_note:   { SERIALPRINTLN(F("NOTE"));           } break;
        case rotary_msg_cc:     { SERIALPRINTLN(F("CC"));             } break;
        case rotary_msg_vu_cc:  { SERIALPRINTLN(F("VUMETER CC"));     } break;
        case rotary_msg_pc_rel: { SERIALPRINTLN(F("PROGRAM CHANGE")); } break;
        case rotary_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));           } break;
        case rotary_msg_rpn:    { SERIALPRINTLN(F("RPN"));            } break;
        case rotary_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));     } break;
        default:                { SERIALPRINTLN(F("NOT DEFINED"));    } break;
      }
    }
    if(encoder[i].switchConfig.message != switch_msg_key){
      SERIALPRINT(F("Switch MIDI Channel: ")); SERIALPRINTLN(encoder[i].switchConfig.channel+1);
      SERIALPRINT(F("Switch MIDI Port: ")); SERIALPRINTLN(encoder[i].switchConfig.midiPort == 0 ? F("NONE") : 
                                                                  encoder[i].switchConfig.midiPort == 1 ? F("USB") :
                                                                  encoder[i].switchConfig.midiPort == 2 ? F("MIDI HW") : 
                                                                  encoder[i].switchConfig.midiPort == 3 ? F("USB + MIDI HW") : F("NOT DEFINED"));
      SERIALPRINT(F("Switch Parameter: ")); SERIALPRINTLN(IS_ENCODER_SW_14_BIT (i) ? 
                                                                  encoder[i].switchConfig.parameter[switch_parameter_MSB] << 7 | encoder[i].switchConfig.parameter[switch_parameter_LSB] :
                                                                  encoder[i].switchConfig.parameter[switch_parameter_LSB]);
      SERIALPRINT(F("Switch MIN value: ")); SERIALPRINTLN(IS_ENCODER_SW_14_BIT (i) ? 
                                                                  encoder[i].switchConfig.parameter[switch_minValue_MSB] << 7 | encoder[i].switchConfig.parameter[switch_minValue_LSB] :
                                                                  encoder[i].switchConfig.parameter[switch_minValue_LSB]);
      SERIALPRINT(F("Switch MAX value: ")); SERIALPRINTLN(IS_ENCODER_SW_14_BIT (i) ? 
                                                                  encoder[i].switchConfig.parameter[switch_maxValue_MSB] << 7 | encoder[i].switchConfig.parameter[switch_maxValue_LSB] :
                                                                  encoder[i].switchConfig.parameter[switch_maxValue_LSB]);
    }else{
      SERIALPRINT(F("Switch Key: ")); SERIALPRINTLN((char) encoder[i].switchConfig.parameter[switch_key]);
      SERIALPRINT(F("Switch Modifier: ")); SERIALPRINTLN( encoder[i].switchConfig.parameter[switch_modifier] == 0 ? F("NONE")      :
                                                                  encoder[i].switchConfig.parameter[switch_modifier] == 1 ? F("ALT")       :
                                                                  encoder[i].switchConfig.parameter[switch_modifier] == 2 ? F("CTRL/CMD")  :
                                                                  encoder[i].switchConfig.parameter[switch_modifier] == 3 ? F("SHIFT")     : F("NOT DEFINED"));
      
    }
      
    

    SERIALPRINTLN(); 
    SERIALPRINT(F("Switch Feedback source: ")); SERIALPRINTLN(encoder[i].switchFeedback.source == fb_src_usb ? F("USB") : 
                                                                      encoder[i].switchFeedback.source == fb_src_hw ? F("MIDI HW") :
                                                                      encoder[i].switchFeedback.source == fb_src_hw_usb ? F("USB + MIDI HW") : 
                                                                      encoder[i].switchFeedback.source == fb_src_local ? F("LOCAL") :
                                                                      encoder[i].switchFeedback.source == fb_src_local_usb ? F("LOCAL + USB") :
                                                                      encoder[i].switchFeedback.source == fb_src_local_hw ? F("LOCAL + MIDI HW") :
                                                                      encoder[i].switchFeedback.source == fb_src_local_usb_hw ? F("LOCAL + USB + MIDI HW") :F("NOT DEFINED"));   
    SERIALPRINT(F("Switch Feedback Local behaviour: ")); SERIALPRINTLN(encoder[i].switchFeedback.localBehaviour == 0 ? F("ON WITH PRESS") :
                                                                               encoder[i].switchFeedback.localBehaviour == 1 ? F("ALWAYS ON") : F("NOT DEFINED"));
    SERIALPRINT(F("Switch Feedback Message: "));                                                                      
    if(encoder[i].switchConfig.mode != switch_mode_shift_rot){
      SERIALPRINT(F("Switch: ")); 
      switch(encoder[i].switchFeedback.message){
        case switch_msg_note:   { SERIALPRINTLN(F("NOTE"));             } break;
        case switch_msg_cc:     { SERIALPRINTLN(F("CC"));               } break;
        case switch_msg_pc:     { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
        case switch_msg_pc_m:   { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
        case switch_msg_pc_p:   { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
        case switch_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));             } break;
        case switch_msg_rpn:    { SERIALPRINTLN(F("RPN"));              } break;
        case switch_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));       } break;
        case switch_msg_key:    { SERIALPRINTLN(F("KEYSTROKE"));        } break;
        default:                { SERIALPRINTLN(F("NOT DEFINED"));      } break;
      }
    }else if(encoder[i].switchConfig.mode == switch_mode_shift_rot){
      SERIALPRINT(F("Rotary: ")); 
      switch(encoder[i].switchFeedback.message){
        case rotary_msg_none:   { SERIALPRINTLN(F("NONE"));           } break;
        case rotary_msg_note:   { SERIALPRINTLN(F("NOTE"));           } break;
        case rotary_msg_cc:     { SERIALPRINTLN(F("CC"));             } break;
        case rotary_msg_vu_cc:  { SERIALPRINTLN(F("VUMETER CC"));     } break;
        case rotary_msg_pc_rel: { SERIALPRINTLN(F("PROGRAM CHANGE")); } break;
        case rotary_msg_nrpn:   { SERIALPRINTLN(F("NRPN"));           } break;
        case rotary_msg_rpn:    { SERIALPRINTLN(F("RPN"));            } break;
        case rotary_msg_pb:     { SERIALPRINTLN(F("PITCH BEND"));     } break;
        case rotary_msg_key:     { SERIALPRINTLN(F("KEYSTROKE"));     } break;
        default:                { SERIALPRINTLN(F("NOT DEFINED"));    } break;
      }
    }

    SERIALPRINT(F("Switch Feedback MIDI Channel: ")); SERIALPRINTLN(encoder[i].switchFeedback.channel+1);
    SERIALPRINT(F("Switch Feedback Parameter: ")); SERIALPRINTLN(IS_ENCODER_SW_FB_14_BIT (i) ?
                                                                          encoder[i].switchFeedback.parameterMSB << 7 | encoder[i].switchFeedback.parameterLSB : 
                                                                          encoder[i].switchFeedback.parameterLSB);
    SERIALPRINT(F("Switch Feedback Value To Color: ")); SERIALPRINTLN(encoder[i].switchFeedback.valueToColor ? F("YES") : F("NO")); 
    SERIALPRINT(F("Switch Feedback Value to Intensity: ")); SERIALPRINTLN(encoder[i].switchFeedback.valueToIntensity ? F("YES") : F("NO"));
    
    if(!encoder[i].switchFeedback.valueToColor){
      SERIALPRINT(F("Switch Feedback Low Intensity OFF: ")); SERIALPRINTLN(encoder[i].switchFeedback.lowIntensityOff ? F("YES") : F("NO")); 
      SERIALPRINT(F("Switch Feedback Color: "));  SERIALPRINTF(encoder[i].switchFeedback.color[0],HEX); 
                                                      SERIALPRINTF(encoder[i].switchFeedback.color[1],HEX);
                                                      SERIALPRINTLNF(encoder[i].switchFeedback.color[2],HEX);  
    }
    


  }else if(block == ytxIOBLOCK::Digital){
    SERIALPRINTLN(F("--------------------------------------------------------"));
    SERIALPRINT(F("Digital ")); SERIALPRINT(i); SERIALPRINTLN(F(":"));
    SERIALPRINT(F("Digital action: ")); SERIALPRINTLN(digital[i].actionConfig.action == 0 ? F("MOMENTARY") : F("TOGGLE"));
    
    SERIALPRINT(F("Digital Message: ")); 
    switch(digital[i].actionConfig.message){
      case digital_msg_none:  { SERIALPRINTLN(F("NONE"));             } break;
      case digital_msg_note:  { SERIALPRINTLN(F("NOTE"));             } break;
      case digital_msg_cc:    { SERIALPRINTLN(F("CC"));               } break;
      case digital_msg_pc:    { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
      case digital_msg_pc_m:  { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
      case digital_msg_pc_p:  { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
      case digital_msg_nrpn:  { SERIALPRINTLN(F("NRPN"));             } break;
      case digital_msg_rpn:   { SERIALPRINTLN(F("RPN"));              } break;
      case digital_msg_pb:    { SERIALPRINTLN(F("PITCH BEND"));       } break;
      case digital_msg_key:   { SERIALPRINTLN(F("KEYSTROKE"));        } break;
      default:                { SERIALPRINTLN(F("NOT DEFINED"));      } break;
    }
    if(digital[i].actionConfig.message != digital_msg_key){
      SERIALPRINT(F("Digital MIDI Channel: ")); SERIALPRINTLN(digital[i].actionConfig.channel+1);
      SERIALPRINT(F("Digital MIDI Port: ")); SERIALPRINTLN(digital[i].actionConfig.midiPort == 0 ? F("NONE") : 
                                                                  digital[i].actionConfig.midiPort == 1 ? F("USB") :
                                                                  digital[i].actionConfig.midiPort == 2 ? F("MIDI HW") : 
                                                                  digital[i].actionConfig.midiPort == 3 ? F("USB + MIDI HW") : F("NOT DEFINED"));
      SERIALPRINT(F("Digital Parameter: ")); SERIALPRINTLN(IS_DIGITAL_14_BIT(i) ? 
                                                                    digital[i].actionConfig.parameter[digital_MSB] << 7 | digital[i].actionConfig.parameter[digital_LSB] :
                                                                    digital[i].actionConfig.parameter[digital_LSB]);
      SERIALPRINT(F("Digital MIN value: ")); SERIALPRINTLN(IS_DIGITAL_14_BIT(i) ? 
                                                                    digital[i].actionConfig.parameter[digital_minMSB] << 7 | digital[i].actionConfig.parameter[digital_minLSB] - (digital[i].actionConfig.message == digital_msg_pb ? 8192 : 0):
                                                                    digital[i].actionConfig.parameter[digital_minLSB]);
      SERIALPRINT(F("Digital MAX value: ")); SERIALPRINTLN(IS_DIGITAL_14_BIT(i) ? 
                                                                    digital[i].actionConfig.parameter[digital_maxMSB] << 7 | digital[i].actionConfig.parameter[digital_maxLSB] - (digital[i].actionConfig.message == digital_msg_pb ? 8192 : 0): 
                                                                    digital[i].actionConfig.parameter[digital_maxLSB]);
    }else{
      SERIALPRINT(F("Digital Key: ")); SERIALPRINT(digital[i].actionConfig.parameter[digital_key]);  SERIALPRINT(" = "); SERIALPRINTLN((char) digital[i].actionConfig.parameter[digital_key]);      SERIALPRINT(F("Digital Modifier: ")); SERIALPRINTLN(digital[i].actionConfig.parameter[digital_modifier] == 0 ? F("NONE")      :
                                                                  digital[i].actionConfig.parameter[digital_modifier] == 1 ? F("ALT")       :
                                                                  digital[i].actionConfig.parameter[digital_modifier] == 2 ? F("CTRL/CMD")  :
                                                                  digital[i].actionConfig.parameter[digital_modifier] == 3 ? F("SHIFT")     : F("NOT DEFINED"));
      
    }

    
    SERIALPRINT(F("Digital Comment: ")); SERIALPRINTLN((char*)digital[i].actionConfig.comment);

    SERIALPRINTLN(); 
    SERIALPRINT(F("Digital Feedback source: ")); SERIALPRINTLN( digital[i].feedback.source == fb_src_usb ? F("USB") : 
                                                                        digital[i].feedback.source == fb_src_hw ? F("MIDI HW") :
                                                                        digital[i].feedback.source == fb_src_hw_usb ? F("USB + MIDI HW") : 
                                                                        digital[i].feedback.source == fb_src_local ? F("LOCAL") :
                                                                        digital[i].feedback.source == fb_src_local_usb ? F("LOCAL + USB") :
                                                                        digital[i].feedback.source == fb_src_local_hw ? F("LOCAL + MIDI HW") :
                                                                        digital[i].feedback.source == fb_src_local_usb_hw ? F("LOCAL + USB + MIDI HW") :F("NOT DEFINED")); 

    SERIALPRINT(F("Digital Feedback Local behaviour: ")); SERIALPRINTLN(digital[i].feedback.localBehaviour == 0 ? F("ON WITH PRESS") :
                                                                                digital[i].feedback.localBehaviour == 1 ? F("ALWAYS ON") : F("NOT DEFINED"));
    SERIALPRINT(F("Digital Feedback Message: ")); 
    switch(digital[i].feedback.message){
      case digital_msg_none:    { SERIALPRINTLN(F("NONE"));             } break;
      case digital_msg_note:    { SERIALPRINTLN(F("NOTE"));             } break;
      case digital_msg_cc:      { SERIALPRINTLN(F("CC"));               } break;
      case digital_msg_pc:      { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
      case digital_msg_pc_m:    { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
      case digital_msg_pc_p:    { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
      case digital_msg_nrpn:    { SERIALPRINTLN(F("NRPN"));             } break;
      case digital_msg_rpn:     { SERIALPRINTLN(F("RPN"));              } break;
      case digital_msg_pb:      { SERIALPRINTLN(F("PITCH BEND"));       } break;
      case digital_msg_key:     { SERIALPRINTLN(F("KEYSTROKE"));        } break;
      default:                  { SERIALPRINTLN(F("NOT DEFINED"));      } break;
    }
    SERIALPRINT(F("Digital Feedback MIDI Channel: ")); SERIALPRINTLN(digital[i].feedback.channel+1);
    SERIALPRINT(F("Digital Feedback Parameter: ")); SERIALPRINTLN(IS_DIGITAL_FB_14_BIT(i) ? 
                                                                          digital[i].feedback.parameterMSB << 7 | digital[i].feedback.parameterLSB :
                                                                          digital[i].feedback.parameterLSB);
    SERIALPRINT(F("Digital Feedback Value To Color: ")); SERIALPRINTLN(digital[i].feedback.valueToColor ? F("YES") : F("NO")); 
    SERIALPRINT(F("Digital Feedback Value to Intensity: ")); SERIALPRINTLN(digital[i].feedback.valueToIntensity ? F("YES") : F("NO"));
    SERIALPRINT(F("Digital Feedback Low Intenstity OFF: ")); SERIALPRINTLN(digital[i].feedback.lowIntensityOff ? F("YES") : F("NO")); 
    SERIALPRINT(F("Digital Feedback Color: ")); SERIALPRINTF(digital[i].feedback.color[0],HEX); 
                                                      SERIALPRINTF(digital[i].feedback.color[1],HEX);
                                                      SERIALPRINTLNF(digital[i].feedback.color[2],HEX);  
  }else if(block == ytxIOBLOCK::Analog){
    SERIALPRINTLN(F("--------------------------------------------------------"));
    SERIALPRINT(F("Analog ")); SERIALPRINT(i); SERIALPRINTLN(F(":"));
    
    SERIALPRINT(F("Analog Message: ")); 
    switch(analog[i].message){
      case analog_msg_none:  { SERIALPRINTLN(F("NONE"));             } break;
      case analog_msg_note:  { SERIALPRINTLN(F("NOTE"));             } break;
      case analog_msg_cc:    { SERIALPRINTLN(F("CC"));               } break;
      case analog_msg_pc:    { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
      case analog_msg_pc_m:  { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
      case analog_msg_pc_p:  { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
      case analog_msg_nrpn:  { SERIALPRINTLN(F("NRPN"));             } break;
      case analog_msg_rpn:   { SERIALPRINTLN(F("RPN"));              } break;
      case analog_msg_pb:    { SERIALPRINTLN(F("PITCH BEND"));       } break;
      case analog_msg_key:   { SERIALPRINTLN(F("KEYSTROKE"));        } break;
      default:               { SERIALPRINTLN(F("NOT DEFINED"));      } break;
    }

    if(analog[i].message != analog_msg_key){
      SERIALPRINT(F("Analog MIDI Channel: ")); SERIALPRINTLN(analog[i].channel+1);
      SERIALPRINT(F("Analog MIDI Port: ")); SERIALPRINTLN(analog[i].midiPort == 0 ? F("NONE") : 
                                                                  analog[i].midiPort == 1 ? F("USB") :
                                                                  analog[i].midiPort == 2 ? F("MIDI HW") : 
                                                                  analog[i].midiPort == 3 ? F("USB + MIDI HW") : F("NOT DEFINED"));
      SERIALPRINT(F("Analog Parameter: ")); SERIALPRINTLN(IS_ANALOG_14_BIT(i) ? 
                                                                    analog[i].parameter[analog_MSB] << 7 | analog[i].parameter[analog_LSB] :
                                                                    analog[i].parameter[analog_LSB]);
      SERIALPRINT(F("Analog MIN value: ")); SERIALPRINTLN(IS_ANALOG_14_BIT(i) ? 
                                                                    analog[i].parameter[analog_minMSB] << 7 | analog[i].parameter[analog_minLSB] - (analog[i].message == analog_msg_pb ? 8192 : 0):
                                                                    analog[i].parameter[analog_minLSB]);
      SERIALPRINT(F("Analog MAX value: ")); SERIALPRINTLN(IS_ANALOG_14_BIT(i) ? 
                                                                    analog[i].parameter[analog_maxMSB] << 7 | analog[i].parameter[analog_maxLSB] - (analog[i].message == analog_msg_pb ? 8192 : 0): 
                                                                    analog[i].parameter[analog_maxLSB]);
    }else{
      SERIALPRINT(F("Analog Key: ")); SERIALPRINTLN((char) analog[i].parameter[analog_key]);
      SERIALPRINT(F("Analog Modifier: ")); SERIALPRINTLN( analog[i].parameter[analog_modifier] == 0 ? F("NONE")      :
                                                                  analog[i].parameter[analog_modifier] == 1 ? F("ALT")       :
                                                                  analog[i].parameter[analog_modifier] == 2 ? F("CTRL/CMD")  :
                                                                  analog[i].parameter[analog_modifier] == 3 ? F("SHIFT")     : F("NOT DEFINED"));
      
    }

      
    SERIALPRINT(F("Analog Comment: ")); SERIALPRINTLN((char*)analog[i].comment);

    SERIALPRINTLN(); 
    SERIALPRINT(F("Analog Feedback source: ")); SERIALPRINTLN(  analog[i].feedback.source == fb_src_usb ? F("USB") : 
                                                                        analog[i].feedback.source == fb_src_hw ? F("MIDI HW") :
                                                                        analog[i].feedback.source == fb_src_hw_usb ? F("USB + MIDI HW") : 
                                                                        analog[i].feedback.source == fb_src_local ? F("LOCAL") :
                                                                        analog[i].feedback.source == fb_src_local_usb ? F("LOCAL + USB") :
                                                                        analog[i].feedback.source == fb_src_local_hw ? F("LOCAL + MIDI HW") :
                                                                        analog[i].feedback.source == fb_src_local_usb_hw ? F("LOCAL + USB + MIDI HW") :F("NOT DEFINED"));   

    SERIALPRINT(F("Analog Feedback Local behaviour: ")); SERIALPRINTLN( analog[i].feedback.localBehaviour == 0 ? F("ON WITH PRESS") :
                                                                                analog[i].feedback.localBehaviour == 1 ? F("ALWAYS ON") : F("NOT DEFINED"));
    SERIALPRINT(F("Analog Feedback Message: ")); 
    switch(analog[i].feedback.message){
      case analog_msg_none:    { SERIALPRINTLN(F("NONE"));             } break;
      case analog_msg_note:    { SERIALPRINTLN(F("NOTE"));             } break;
      case analog_msg_cc:      { SERIALPRINTLN(F("CC"));               } break;
      case analog_msg_pc:      { SERIALPRINTLN(F("PROGRAM CHANGE #")); } break;
      case analog_msg_pc_m:    { SERIALPRINTLN(F("PROGRAM CHANGE -")); } break;
      case analog_msg_pc_p:    { SERIALPRINTLN(F("PROGRAM CHANGE +")); } break;
      case analog_msg_nrpn:    { SERIALPRINTLN(F("NRPN"));             } break;
      case analog_msg_rpn:     { SERIALPRINTLN(F("RPN"));              } break;
      case analog_msg_pb:      { SERIALPRINTLN(F("PITCH BEND"));       } break;
      case analog_msg_key:     { SERIALPRINTLN(F("KEYSTROKE"));        } break;
      default:                 { SERIALPRINTLN(F("NOT DEFINED"));      } break;
    }
    SERIALPRINT(F("Analog Feedback MIDI Channel: ")); SERIALPRINTLN(analog[i].feedback.channel+1);
    SERIALPRINT(F("Analog Feedback Parameter: ")); SERIALPRINTLN(IS_ANALOG_FB_14_BIT(i) ? 
                                                                          analog[i].feedback.parameterMSB << 7 | analog[i].feedback.parameterLSB :
                                                                          analog[i].feedback.parameterLSB);
    SERIALPRINT(F("Analog Feedback Value to color: ")); SERIALPRINTLN(analog[i].feedback.valueToColor ? F("YES") : F("NO")); 
    SERIALPRINT(F("Analog Feedback Value to intensity: ")); SERIALPRINTLN(analog[i].feedback.valueToIntensity ? F("YES") : F("NO")); 
    SERIALPRINT(F("Analog Feedback Color: ")); SERIALPRINTF(analog[i].feedback.color[0],HEX); 
                                                      SERIALPRINTF(analog[i].feedback.color[1],HEX);
                                                      SERIALPRINTLNF(analog[i].feedback.color[2],HEX);  
  }

  Watchdog.disable();
  Watchdog.enable(WATCHDOG_RESET_NORMAL);
}
