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

// 14 bit message data types and declarations
typedef struct{
  uint16_t parameter;  
  uint16_t value;
  uint8_t parameterMSB;  
  uint8_t parameterLSB;  
  uint8_t valueMSB;
  uint8_t valueLSB;
} ytx14bitMessage;

ytx14bitMessage nrpnMessage;
ytx14bitMessage rpnMessage;

uint8_t rcvdEncoderMsgType = 0;
uint8_t rcvdEncoderSwitchMsgType = 0;
uint8_t rcvdDigitalMsgType = 0;
uint8_t rcvdAnalogMsgType = 0;  

bool msg14bitComplete = false;


//----------------------------------------------------------------------------------------------------
// COMMS MIDI - SERIAL
//----------------------------------------------------------------------------------------------------

/*
 * Handler for Note On messages received from USB port
 */
void handleNoteOnUSB(byte channel, byte note, byte velocity){
  uint8_t msgType = MIDI.getType();
  rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_note;
  rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_note;
  rcvdDigitalMsgType = digitalMessageTypes::digital_msg_note;
  rcvdAnalogMsgType = analogMessageTypes::analog_msg_note;
  ProcessMidi(msgType, channel, note, velocity, MIDI_USB);

}
/*
 * Handler for Note Off messages received from USB port
 */
void handleNoteOffUSB(byte channel, byte note, byte velocity){
  uint8_t msgType = MIDI.getType();
  rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_note;
  rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_note;
  rcvdDigitalMsgType = digitalMessageTypes::digital_msg_note;
  rcvdAnalogMsgType = analogMessageTypes::analog_msg_note;
  ProcessMidi(msgType, channel, note, velocity, MIDI_USB);
}
/*
 * Handler for CC messages received from USB port
 */
void handleControlChangeUSB(byte channel, byte number, byte value){
  
  uint8_t msgType = MIDI.getType();
  uint16_t fullParam = 0, fullValue = 0;
  msg14bitParser(channel, number, value);
  
  if (msg14bitComplete){
    if( rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_nrpn || 
        rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_nrpn || 
        rcvdDigitalMsgType == digitalMessageTypes::digital_msg_nrpn || 
        rcvdAnalogMsgType == analogMessageTypes::analog_msg_nrpn){
          
      fullParam = nrpnMessage.parameter;
      fullValue = nrpnMessage.value;
//      SERIALPRINTLN();
     // SERIALPRINT(F("NRPN MESSAGE COMPLETE -> "));
     // SERIALPRINT(F("\tPARAM: ")); SERIALPRINT(fullParam);
     // SERIALPRINT(F("\tVALUE: ")); SERIALPRINTLN(fullValue);
    }else if( rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_rpn || 
              rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_rpn || 
              rcvdDigitalMsgType == digitalMessageTypes::digital_msg_rpn || 
              rcvdAnalogMsgType == analogMessageTypes::analog_msg_rpn){
                
      fullParam = rpnMessage.parameter;
      fullValue = rpnMessage.value;
     // SERIALPRINT(F("RPN MESSAGE COMPLETE -> "));
     // SERIALPRINT(F("\tPARAM: ")); SERIALPRINT(fullParam);
     // SERIALPRINT(F("\tVALUE: ")); SERIALPRINTLN(fullValue);
    }   
    ProcessMidi(msgType, channel, fullParam, fullValue, MIDI_USB);
    
    msg14bitComplete = false;
  }else{
    rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_cc;
    rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_cc;
    rcvdDigitalMsgType = digitalMessageTypes::digital_msg_cc;
    rcvdAnalogMsgType = analogMessageTypes::analog_msg_cc;
    
    ProcessMidi(msgType, channel, number, value, MIDI_USB); 
  }  
  
}

/*
 * Handler for Program Change messages received from USB port
 */
void handleProgramChangeUSB(byte channel, byte number){
  uint8_t msgType = MIDI.getType();
  rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_pc_rel;
  rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_pc;
  rcvdDigitalMsgType = digitalMessageTypes::digital_msg_pc;
  rcvdAnalogMsgType = analogMessageTypes::analog_msg_pc;

  ProcessMidi(msgType, channel, number, 0, MIDI_USB);
}

/*
 * Handler for Pitch Bend messages received from USB port
 */
void handlePitchBendUSB(byte channel, int bend){
  uint8_t msgType = MIDI.getType();
  rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_pb;
  rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_pb;
  rcvdDigitalMsgType = digitalMessageTypes::digital_msg_pb;
  rcvdAnalogMsgType = analogMessageTypes::analog_msg_pb;
  msg14bitComplete = true;
  ProcessMidi(msgType, channel, 0, bend, MIDI_USB);
  msg14bitComplete = false;
}
/*
 * Handler for Note On messages received from DIN5 port
 */
void handleNoteOnHW(byte channel, byte note, byte velocity){
  (void)channel;
  (void)note;
  (void)velocity;
}

/*
 * Handler for Note Off messages received from DIN5 port
 */
void handleNoteOffHW(byte channel, byte note, byte velocity){
  (void)channel;
  (void)note;
  (void)velocity;
}

/*
 * Handler for CC messages received from DIN5 port
 */
void handleControlChangeHW(byte channel, byte number, byte value){
  (void)channel;
  (void)number;
  (void)value;
}

/*
 * Handler for Program Change messages received from DIN5 port
 */
void handleProgramChangeHW(byte channel, byte number){
  (void)channel;
  (void)number;
}

/*
 * Handler for Pitch Bend messages received from DIN5 port
 */
void handlePitchBendHW(byte channel, int bend){
  (void)channel;
  (void)bend;
}

/*
 * Handler for Time Code Quarter Frame via USB
 */ 
void handleTimeCodeQuarterFrameUSB(byte data){
  // SERIALPRINTLN("\nTCQF received via USB!");
  // SERIALPRINT("Data: "); SERIALPRINTLN(data); 
  
  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendTimeCodeQuarterFrame(data);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendTimeCodeQuarterFrame(data);
  }
  
  // YOUR CODE HERE
}

/*
 * Handler for Time Code Quarter Frame via HW
 */ 
void handleTimeCodeQuarterFrameHW(byte data){
  (void)data;
}

/*
 * Handler for Song Position via USB
 */ 
void handleSongPositionUSB(unsigned beats){
  // SERIALPRINTLN("\nSong Position received via USB!");
  // SERIALPRINT("Beats: "); SERIALPRINTLN(beats);

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendSongPosition(beats);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendSongPosition(beats);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Song Position via HW
 */ 
void handleSongPositionHW(unsigned beats){
  (void)beats;
}

/*
 * Handler for Song Select via USB
 */ 
void handleSongSelectUSB(byte songnumber){
  // SERIALPRINTLN("\nSong Select received via USB!");
  // SERIALPRINT("Song number: "); SERIALPRINTLN(songnumber);

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendSongSelect(songnumber);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendSongSelect(songnumber);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Song Select via HW
 */ 
void handleSongSelectHW(byte songnumber){
  (void)songnumber;
}

/*
 * Handler for Tune Request via USB
 */ 
void handleTuneRequestUSB(void){
  // SERIALPRINTLN("\nTune Request received via USB!");

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendTuneRequest();
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendTuneRequest();
  }

  // YOUR CODE HERE
}

/*
 * Handler for Tune Request via HW
 */ 
void handleTuneRequestHW(void){
}

/*
 * Handler for Tune Request via USB
 */ 
void handleClockUSB(void){
  // SERIALPRINTLN("\nClock received via USB!");

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendRealTime(midi::Clock);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendRealTime(midi::Clock);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Tune Request via HW
 */ 
void handleClockHW(void){
  // SERIALPRINTLN("\nClock received via HW!");
  MIDI.sendRealTime(midi::Clock);
}

/*
 * Handler for Start via USB
 */ 
void handleStartUSB(void){
  // SERIALPRINTLN("\nStart received via USB!");

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendRealTime(midi::Start);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendRealTime(midi::Start);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Start via HW
 */ 
void handleStartHW(void){
  // SERIALPRINTLN("\nStart received via HW!");
  MIDI.sendRealTime(midi::Start);
}

// /*
//  * Handler for Tick via USB
//  */ 
// void handleTickUSB(void){
//   SERIALPRINTLN("\nTick received via USB!");
//   // YOUR CODE HERE
// }

// /*
//  * Handler for Tick via HW
//  */ 
// void handleTickHW(void){
//   SERIALPRINTLN("\nTick received via HW!");
//   // YOUR CODE HERE
// }

/*
 * Handler for Continue via USB
 */ 
void handleContinueUSB(void){
  // SERIALPRINTLN("\nContinue received via USB!");

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendRealTime(midi::Continue);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendRealTime(midi::Continue);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Continue via HW
 */ 
void handleContinueHW(void){
  // SERIALPRINTLN("\nContinue received via HW!");
  MIDI.sendRealTime(midi::Continue);
}

/*
 * Handler for Stop via USB
 */ 
void handleStopUSB(void){
  // SERIALPRINTLN("\nStop received via USB!");

  // MIDI REDIRECT
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
    MIDI.sendRealTime(midi::Stop);
  }
  if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
    MIDIHW.sendRealTime(midi::Stop);
  }

  // YOUR CODE HERE
}

/*
 * Handler for Stop via HW
 */ 
void handleStopHW(void){
    // SERIALPRINTLN("\nStop received via HW!");
  MIDI.sendRealTime(midi::Stop);
}


/*
 * NRPN/RPN parser
 * Take current CC message and decide if it is part of a NRPN or RPN message
 * Fills 14 bit message struct properly with corresponding flag if it's been detected
 * Returns if ther is and on going NRPN or RPN message being decoded
 */
void msg14bitParser(byte channel, byte param, byte value){
  static uint8_t prevParam = 0;
  static uint32_t prevMillisNRPN = 0;
  static bool nrpnOnGoing = false;
  static bool rpnOnGoing = false;
  
  // NRPN Message parser
  if (param == midi::NRPNMSB && prevParam != midi::NRPNMSB){    // If new CC arrives 
      prevParam = midi::NRPNMSB;
      nrpnMessage.parameterMSB = value;
      nrpnOnGoing = true;
      decoding14bit = true;
      return;
  }
  else if (param == midi::NRPNLSB && prevParam == midi::NRPNMSB && nrpnOnGoing){
      prevParam = midi::NRPNLSB;
      nrpnMessage.parameterLSB = value;
      nrpnMessage.parameter = nrpnMessage.parameterMSB<<7 | nrpnMessage.parameterLSB;
      return;
  }
  else if (param == midi::DataEntryMSB && prevParam == midi::NRPNLSB && nrpnOnGoing){
      prevParam = midi::DataEntryMSB;
      nrpnMessage.valueMSB = value;
      return;
  }
  else if (param == midi::DataEntryLSB && prevParam == midi::DataEntryMSB && nrpnOnGoing){
      prevParam = 0;
      nrpnMessage.valueLSB = value;
      nrpnMessage.value = nrpnMessage.valueMSB<<7 | nrpnMessage.valueLSB;
      msg14bitComplete = true;
      nrpnOnGoing = false;
      decoding14bit = false;

      rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_nrpn;
      rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_nrpn;
      rcvdDigitalMsgType = digitalMessageTypes::digital_msg_nrpn;
      rcvdAnalogMsgType = analogMessageTypes::analog_msg_nrpn;
         // SERIALPRINT(F("NRPN MESSAGE COMPLETE -> "));
         // SERIALPRINT(F("\tPARAM MSB: ")); SERIALPRINT(nrpnMessage.parameterMSB);
         // SERIALPRINT(F("\tPARAM LSB: ")); SERIALPRINT(nrpnMessage.parameterLSB);
         // SERIALPRINT(F("\tPARAM: ")); SERIALPRINT(nrpnMessage.parameter);
         // SERIALPRINT(F("\tVALUE MSB: ")); SERIALPRINT(nrpnMessage.valueMSB);
         // SERIALPRINT(F("\tVALUE LSB: ")); SERIALPRINT(nrpnMessage.valueLSB);
         // SERIALPRINT(F("\tVALUE: ")); SERIALPRINTLN(nrpnMessage.value);
  }
  ////////////////////////////////////////////////////////////////////////////////////
  // RPN Message parser //////////////////////////////////////////////////////////////
  else if (param == midi::RPNMSB && prevParam != midi::RPNMSB){
      prevParam = midi::RPNMSB;
      rpnMessage.parameterMSB = value;
      rpnOnGoing = true;
      decoding14bit = true;
      return;
  }
  else if (param == midi::RPNLSB && prevParam == midi::RPNMSB && rpnOnGoing){
      prevParam = midi::RPNLSB;
      rpnMessage.parameterLSB = value;
      rpnMessage.parameter = rpnMessage.parameterMSB<<7 | rpnMessage.parameterLSB;
      return;
  }
  else if (param == midi::DataEntryMSB && prevParam == midi::RPNLSB && rpnOnGoing){
      prevParam = midi::DataEntryMSB;
      rpnMessage.valueMSB = value;
      return;
  }
  else if (param == midi::DataEntryLSB && prevParam == midi::DataEntryMSB && rpnOnGoing){
      prevParam = 0;
      rpnMessage.valueLSB = value;
      rpnMessage.value = rpnMessage.valueMSB<<7 | rpnMessage.valueLSB;
      msg14bitComplete = true;
      rpnOnGoing = false;
      decoding14bit = false;
      
      rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_rpn;
      rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_rpn;
      rcvdDigitalMsgType = digitalMessageTypes::digital_msg_rpn;
      rcvdAnalogMsgType = analogMessageTypes::analog_msg_rpn;
         // SERIALPRINT(F("RPN MESSAGE COMPLETE -> "));
         // SERIALPRINT(F("\tPARAM MSB: ")); SERIALPRINT(rpnMessage.parameterMSB);
         // SERIALPRINT(F("\tPARAM LSB: ")); SERIALPRINT(rpnMessage.parameterLSB);
         // SERIALPRINT(F("\tPARAM: ")); SERIALPRINT(rpnMessage.parameter);
         // SERIALPRINT(F("\tVALUE MSB: ")); SERIALPRINT(rpnMessage.valueMSB);
         // SERIALPRINT(F("\tVALUE LSB: ")); SERIALPRINT(rpnMessage.valueLSB);
         // SERIALPRINT(F("\tVALUE: ")); SERIALPRINTLN(rpnMessage.value);
  ////////////////////////////////////////////////////////////////////////////////////
  // Just CC /////////////////////////////////////////////////////////////////////////
  }else{
    rcvdEncoderMsgType = rotaryMessageTypes::rotary_msg_cc;
    rcvdEncoderSwitchMsgType = switchMessageTypes::switch_msg_cc;
    rcvdDigitalMsgType = digitalMessageTypes::digital_msg_cc;
    rcvdAnalogMsgType = analogMessageTypes::analog_msg_cc;
    nrpnOnGoing = false;
    rpnOnGoing = false;
    decoding14bit = false;
    msg14bitComplete = false;
    prevParam = 0;
  }
}


void ProcessMidi(byte msgType, byte channel, uint16_t param, int16_t value, bool midiSrc) {
  
 //uint32_t antMicrosComms = micros(); 
  // MIDI THRU
  if(midiSrc == MIDI_HW){  // IN FROM MIDI HW
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_USB){    // Send to MIDI USB port
      if(msgType == midi::PitchBend){
        uint16_t unsignedValue = (uint16_t) value + 8192; // Correct value
        MIDI.send( (midi::MidiType) msgType, (unsignedValue & 0x7f), (unsignedValue >> 7) & 0x7f, channel);
      }else{
        MIDI.send( (midi::MidiType) msgType, param, value, channel);
      }
    }
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_HW_HW){     // Send to MIDI DIN port
      if(msgType == midi::PitchBend){
        uint16_t unsignedValue = (uint16_t) value + 8192; // Correct value
        MIDIHW.send( (midi::MidiType) msgType, (unsignedValue & 0x7f), (unsignedValue >> 7) & 0x7f, channel);
      }else{
        MIDIHW.send( (midi::MidiType) msgType, param, value, channel);
      }
    }
  }else{        // IN FROM MIDI USB
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_USB){   // Send to MIDI USB port
      if(msgType == midi::PitchBend){
        uint16_t unsignedValue = (uint16_t) value + 8192; // Correct value
        MIDI.send( (midi::MidiType) msgType, (unsignedValue & 0x7f), (unsignedValue >> 7) & 0x7f, channel);
      }else{
        MIDI.send( (midi::MidiType) msgType, param, value, channel);
      }
    }
    if(config->midiConfig.midiMergeFlags & MIDI_MERGE_FLAGS_USB_HW){    // Send to MIDI DIN port
      if(msgType == midi::PitchBend){
        uint16_t unsignedValue = (uint16_t) value + 8192; // Correct value
        MIDIHW.send( (midi::MidiType) msgType, (unsignedValue & 0x7f), (unsignedValue >> 7) & 0x7f, channel);
      }else{
        MIDIHW.send( (midi::MidiType) msgType, param, value, channel);
      }
    }
  }

  if(decoding14bit) return;
  
  channel--; // GO from 1-16 to 0-15
   
  
  // Blink status LED when receiving MIDI message
  SetStatusLED(STATUS_BLINK, 1, statusLEDtypes::STATUS_FB_MSG_IN);

  bool bankUpdated = false;
  // Set NOTES OFF as NOTES ON for the next section to treat them as the same
  if( msgType == midi::NoteOff){
    msgType = midi::NoteOn;      
    value = 0;
  }else if(config->board.remoteBanks && msgType == midi::ProgramChange && channel == config->midiConfig.remoteBankChannel){    //   
    if(value < config->banks.count && config->banks.count > 1){
      ChangeToBank(value);
      bankUpdated = true;
    }
  }

  if(bankUpdated) return;
  
  
  uint16_t unsignedValue = 0;
  bool isNRPN = rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_nrpn       ||
                rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_nrpn ||
                rcvdDigitalMsgType == digitalMessageTypes::digital_msg_nrpn     ||
                rcvdAnalogMsgType == analogMessageTypes::analog_msg_nrpn;

  bool isRPN  = rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_rpn        ||
                rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_rpn  ||
                rcvdDigitalMsgType == digitalMessageTypes::digital_msg_rpn      ||
                rcvdAnalogMsgType == analogMessageTypes::analog_msg_rpn;

  bool isPB   = rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_pb         ||
                rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_pb   ||
                rcvdDigitalMsgType == digitalMessageTypes::digital_msg_pb       ||
                rcvdAnalogMsgType == analogMessageTypes::analog_msg_pb;

  bool isPC   = rcvdEncoderMsgType == rotaryMessageTypes::rotary_msg_pc_rel     ||
                rcvdEncoderSwitchMsgType == switchMessageTypes::switch_msg_pc   ||
                rcvdDigitalMsgType == digitalMessageTypes::digital_msg_pc       ||
                rcvdAnalogMsgType == analogMessageTypes::analog_msg_pc;

  if(msg14bitComplete){
    if(isNRPN){
      msgType = MidiTypeYTX::NRPN;     // USE CUSTOM TYPE FOR NRPN   
      unsignedValue = (uint16_t) value;  
    }else if(isRPN){
      msgType = MidiTypeYTX::RPN;     // USE CUSTOM TYPE FOR RPN 
      unsignedValue = (uint16_t) value;
    }else if(isPB){
      msgType = MidiTypeYTX::PitchBend; 
      // Convert signed PB value (-8192..8191) to unsigned (0..16383)
      unsignedValue = (uint16_t) value + 8192; // Correct value
      param = 0;
    }
  }else{  // 7 bit message
    if(isPC){   // If it is a program change message, set currentProgram for incoming port an message
      encoderHw.SetProgramChange(midiSrc, channel, (uint8_t) value);
      digitalHw.SetProgramChange(midiSrc, channel, (uint8_t) value);  
      //return; // Program change messages exit here
    }

    msgType = (msgType >> 4) & 0x0F;    // Convert to YTX midi type
    unsignedValue = (uint16_t) value;
  }  
  

  if(testMidi){
    SERIALPRINT((midiSrc == MIDI_USB) ? F("MIDI_USB: ") : F("MIDI_HW: "));
    SERIALPRINTF(msgType, HEX); SERIALPRINT(F("\t"));
    SERIALPRINT(channel); SERIALPRINT(F("\t"));
    SERIALPRINT(param); SERIALPRINT(F("\t"));
    SERIALPRINTLN(unsignedValue);
  }
      
  // MIDI MESSAGE COUNTER - IN LOOP IT DISPLAYS QTY OF MESSAGES IN A CERTAIN PERIOD
  // if(!msgCount){
  //   antMicrosFirstMessage = millis();
  // }
  // msgCount++;
  
  // countOn = true;
  

  // v0.15 -> ~150 us 
  for(int fbType = FB_ENCODER; fbType <= FB_ANALOG; fbType++){
    UpdateMidiBuffer(fbType, msgType, channel, param, unsignedValue, midiSrc);  
  }
  
  // RESET VALUES
  rcvdEncoderMsgType = 0;
  rcvdEncoderSwitchMsgType = 0;
  rcvdDigitalMsgType = 0;
  rcvdAnalogMsgType = 0;    
}

void UpdateMidiBuffer(byte fbType, byte msgType, byte channel, uint16_t param, uint16_t value, bool midiSrc){
  bool thereIsAMatch = false;
  
  if(!msg14bitComplete){  // IF IT'S A 7 BIT MESSAGE
    // BOUNDARY BINARY SEARCH - Search lower bound and upper bound for a single parameter - Buffer is sorted based on parameter numbers
    int lowerB = lower_bound_search7(midiMsgBuf7, param, 0, midiRxSettings.lastMidiBufferIndex7);
    int upperB = upper_bound_search7(midiMsgBuf7, param, 0, midiRxSettings.lastMidiBufferIndex7);

    if((lowerB < 0) || (upperB < 0)) return;  // if any of the boundaries are negative, return, since parameter wasn't found
    
    // Search among the matches for the parameter if the rest of the message is a match in the buffer
    for(uint32_t idx = lowerB; idx < upperB; idx++){
      if(midiMsgBuf7[idx].channel == channel){
        if(midiMsgBuf7[idx].message == msgType){
          if(midiMsgBuf7[idx].type == fbType){
            if(midiMsgBuf7[idx].port & (1 << midiSrc)){
              midiMsgBuf7[idx].value = value;
              midiMsgBuf7[idx].banksToUpdate = midiMsgBuf7[idx].banksPresent;

              bool thereAreShiftedEncoders = false;
              // If encoder is shifted to a different bank, config won't match, with this, we keep it in the buffer
             if(fbType == FB_ENCODER) thereAreShiftedEncoders = encoderHw.EncoderShiftedBufferMatch(idx);

              if((midiMsgBuf7[idx].banksToUpdate >> currentBank) & 0x1){
                // Reset bank flag
                if(thereAreShiftedEncoders == false){   // if there aren't shifted encoders, currentBank is updated
                  midiMsgBuf7[idx].banksToUpdate &= ~(1 << currentBank);
                }
                // SERIALPRINTLN(F("Message in 7 bit buffer"));
                SearchMsgInConfigAndUpdate( midiMsgBuf7[idx].type,
                                            midiMsgBuf7[idx].message,
                                            channel,                      // Send channel, and not channel in midi rx list to allow color switcher
                                            midiMsgBuf7[idx].parameter,
                                            midiMsgBuf7[idx].value,
                                            midiMsgBuf7[idx].port);
              }
              
            }
          }
        }
      }
    }
  }else{    // 14 bit message received
    int lowerB = lower_bound_search14(midiMsgBuf14, param, 0, midiRxSettings.lastMidiBufferIndex14);
    int upperB = upper_bound_search14(midiMsgBuf14, param, 0, midiRxSettings.lastMidiBufferIndex14);
    
    if((lowerB < 0) || (upperB < 0)) return;  // if any of the boundaries are negative, return, since parameter wasn't found
    
    for(uint32_t idx = lowerB; idx < upperB; idx++){
      if(midiMsgBuf14[idx].channel == channel){    
        if(midiMsgBuf14[idx].message == msgType){
          if(midiMsgBuf14[idx].type == fbType){
            if(midiMsgBuf14[idx].port & (1 << midiSrc)){
              midiMsgBuf14[idx].value = value;      // Update value in MIDI RX buffer
              midiMsgBuf14[idx].banksToUpdate = midiMsgBuf14[idx].banksPresent;
              if((midiMsgBuf14[idx].banksToUpdate >> currentBank) & 0x1){
                // Reset bank flag
                midiMsgBuf14[idx].banksToUpdate &= ~(1 << currentBank);

                SearchMsgInConfigAndUpdate( midiMsgBuf14[idx].type,
                                            midiMsgBuf14[idx].message,
                                            midiMsgBuf14[idx].channel,
                                            midiMsgBuf14[idx].parameter,
                                            midiMsgBuf14[idx].value,
                                            midiMsgBuf14[idx].port);
              }
              thereIsAMatch = true;
            }
          }
        }
      }
    }
  }   
}

void SearchMsgInConfigAndUpdate(byte fbType, byte msgType, byte channel, uint16_t param, uint16_t value, uint8_t midiSrc){
  // If it is a regular message, check if it matches the feedback configuration for all the inputs (only the current bank)
  byte messageToCompare = 0;
  uint16_t paramToCompare = 0;

  switch(fbType){
    case FB_ENCODER:{
      // SWEEP ALL ENCODERS - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){
        switch(msgType){
          case MidiTypeYTX::NoteOn:         { messageToCompare = rotaryMessageTypes::rotary_msg_note;   
                                              paramToCompare = encoder[encNo].rotaryFeedback.parameterLSB;  } break;
          case MidiTypeYTX::ControlChange:  { messageToCompare = 
                                              (encoder[encNo].rotaryFeedback.message == rotaryMessageTypes::rotary_msg_cc) ?
                                                      rotaryMessageTypes::rotary_msg_cc :
                                                      rotaryMessageTypes::rotary_msg_vu_cc;             
                                              paramToCompare = encoder[encNo].rotaryFeedback.parameterLSB;  } break;
          case MidiTypeYTX::ProgramChange:  { messageToCompare = rotaryMessageTypes::rotary_msg_pc_rel;     } break;    
          case MidiTypeYTX::NRPN:           { messageToCompare = rotaryMessageTypes::rotary_msg_nrpn;      
                                              paramToCompare =  encoder[encNo].rotaryFeedback.parameterMSB<<7 | 
                                                                encoder[encNo].rotaryFeedback.parameterLSB; } break;
          case MidiTypeYTX::RPN:            { messageToCompare = rotaryMessageTypes::rotary_msg_rpn;    
                                              paramToCompare =  encoder[encNo].rotaryFeedback.parameterMSB<<7 | 
                                                                encoder[encNo].rotaryFeedback.parameterLSB; } break;
          case MidiTypeYTX::PitchBend:      { messageToCompare = rotaryMessageTypes::rotary_msg_pb;         } break;
        }

        if( paramToCompare == param  || 
            messageToCompare == rotaryMessageTypes::rotary_msg_pb ||
            messageToCompare == rotaryMessageTypes::rotary_msg_pc_rel){
          if(encoder[encNo].rotaryFeedback.channel == channel){
            if(encoder[encNo].rotaryFeedback.message == messageToCompare){
              if(encoder[encNo].rotaryFeedback.source & midiSrc){    
                // If there's a match, set encoder value and feedback
                if(encoderHw.GetEncoderValue(encNo) != value || 
                    encoder[encNo].rotBehaviour.hwMode != rotaryModes::rot_absolute ||
                    !(encoder[encNo].rotaryFeedback.source & feedbackSource::fb_src_local)){   // If it isn't local, update feedback to any value
                  
                  encoderHw.SetEncoderValue(currentBank, encNo, value);
                                    
                  // SERIALPRINTLN(F("Encoder match!"));
                }
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_2CC:{
      // SWEEP ALL ENCODERS - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){   
        // SWEEP ALL ENCODERS SWITCHES
        if(encoder[encNo].switchFeedback.parameterLSB == param){ 
          if(encoder[encNo].switchFeedback.channel == channel){
            if(encoder[encNo].switchFeedback.source & midiSrc){    
              // If there's a match, set encoder value and feedback
              // SERIALPRINTLN(F("2cc MATCH"));
              if((encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc) ){
                encoderHw.SetEncoder2cc(currentBank, encNo, value);                
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_VUMETER:{
      // SWEEP ALL ENCODERS - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){
        if( encoder[encNo].rotaryFeedback.parameterLSB == param){
          if(channel == config->midiConfig.vumeterChannel){
            if(encoder[encNo].rotaryFeedback.message == rotary_msg_vu_cc){
              if(encoder[encNo].rotaryFeedback.source & midiSrc){      
                // If there's a match, set encoder value and feedback
                feedbackHw.SetChangeEncoderFeedback(FB_ENC_VUMETER, encNo, value, 
                                                    encoderHw.GetModuleOrientation(encNo/4), NO_SHIFTER, NO_BANK_UPDATE, EXTERNAL_FEEDBACK);  // HARDCODE: N° of encoders in module / is
                feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, encNo, encoderHw.GetEncoderValue(encNo), 
                                                    encoderHw.GetModuleOrientation(encNo/4), NO_SHIFTER, NO_BANK_UPDATE, EXTERNAL_FEEDBACK);   // HARDCODE: N° of encoders in module / is 
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_VAL_TO_COLOR:{
      // SWEEP ALL ENCODERS - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){
        if( encoder[encNo].rotaryFeedback.parameterLSB == param){
          if(channel == config->midiConfig.valueToColorChannel){
            if(encoder[encNo].rotaryFeedback.source & midiSrc){      
              feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                      encNo, 
                                      value, 
                                      encoderHw.GetModuleOrientation(encNo/4), 
                                      NO_SHIFTER, 
                                      NO_BANK_UPDATE, 
                                      COLOR_CHANGE,                   // Color change message
                                      NO_VAL_TO_INT,
                                      EXTERNAL_FEEDBACK);
              if(encoder[encNo].switchConfig.mode == switchModes::switch_mode_2cc){   // 
                feedbackHw.SetChangeEncoderFeedback(FB_ENC_2CC, 
                                                    encNo, 
                                                    encoderHw.GetEncoderValue2(encNo), 
                                                    encoderHw.GetModuleOrientation(encNo/4), 
                                                    NO_SHIFTER, 
                                                    NO_BANK_UPDATE,
                                                    NO_COLOR_CHANGE,                // it's not color change message
                                                    NO_VAL_TO_INT,
                                                    EXTERNAL_FEEDBACK); 
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_VAL_TO_INT:{
      // SWEEP ALL ENCODERS
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){
        if( encoder[encNo].rotaryFeedback.parameterLSB == param){
          if(channel == config->midiConfig.valueToIntensityChannel){
            if(encoder[encNo].rotaryFeedback.source & midiSrc){  
              feedbackHw.SetChangeEncoderFeedback(FB_ENCODER, 
                                                  encNo, 
                                                  value, 
                                                  encoderHw.GetModuleOrientation(encNo/4), 
                                                  NO_SHIFTER, 
                                                  NO_BANK_UPDATE, 
                                                  NO_COLOR_CHANGE,                   
                                                  VAL_TO_INT,           //value to intensity
                                                  EXTERNAL_FEEDBACK);
            }
          }
        }
      }
    }break;
    
    case FB_ENC_SHIFT:{
      // SWEEP ALL ENCODERS SWITCHES - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){   
          switch(msgType){
            case MidiTypeYTX::NoteOn:         { messageToCompare = rotaryMessageTypes::rotary_msg_note;   
                                                paramToCompare = encoder[encNo].switchFeedback.parameterLSB;      } break;

            case MidiTypeYTX::ControlChange:  { messageToCompare = rotaryMessageTypes::rotary_msg_cc;    
                                                paramToCompare = encoder[encNo].switchFeedback.parameterLSB;      } break;

            case MidiTypeYTX::ProgramChange:  { messageToCompare = rotaryMessageTypes::rotary_msg_pc_rel;         } break;

            case MidiTypeYTX::NRPN:           { messageToCompare = rotaryMessageTypes::rotary_msg_nrpn;      
                                                paramToCompare =  encoder[encNo].switchFeedback.parameterMSB<<7 | 
                                                                  encoder[encNo].switchFeedback.parameterLSB;     } break;

            case MidiTypeYTX::RPN:            { messageToCompare = rotaryMessageTypes::rotary_msg_rpn;      
                                                paramToCompare =  encoder[encNo].switchFeedback.parameterMSB<<7 | 
                                                                  encoder[encNo].switchFeedback.parameterLSB;     } break;

            case MidiTypeYTX::PitchBend:      { messageToCompare = rotaryMessageTypes::rotary_msg_pb;             } break;
          }

        // SWEEP ALL ENCODERS SWITCHES
        if(paramToCompare == param || 
            messageToCompare == rotaryMessageTypes::rotary_msg_pb ||
            messageToCompare == rotaryMessageTypes::rotary_msg_pc_rel){ 
          if(encoder[encNo].switchFeedback.channel == channel){
            if(encoder[encNo].switchFeedback.message == messageToCompare){
              if(encoder[encNo].switchFeedback.source & midiSrc){    
              // If there's a match, set encoder value and feedback
                if(encoderHw.GetEncoderShiftValue(encNo) != value ||
                  !(encoder[encNo].switchFeedback.source & feedbackSource::fb_src_local)) // If it isn't local, update feedback to any value
                  encoderHw.SetEncoderShiftValue(currentBank, encNo, value);  
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_SWITCH:{
      // SERIALPRINTLN(F("Encoder switch match in buffer, checking config"));
      // SWEEP ALL ENCODERS SWITCHES - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){   
          switch(msgType){
            case MidiTypeYTX::NoteOn:         { messageToCompare = switchMessageTypes::switch_msg_note;   
                                                paramToCompare = encoder[encNo].switchFeedback.parameterLSB;      } break;

            case MidiTypeYTX::ControlChange:  { messageToCompare = switchMessageTypes::switch_msg_cc;   
                                                paramToCompare = encoder[encNo].switchFeedback.parameterLSB;      } break;

            case MidiTypeYTX::ProgramChange:  { return;                                                           } break;    // Program change don't show feedback on encoder switches

            case MidiTypeYTX::NRPN:           { messageToCompare = switchMessageTypes::switch_msg_nrpn;      
                                                paramToCompare =  encoder[encNo].switchFeedback.parameterMSB<<7 | 
                                                                  encoder[encNo].switchFeedback.parameterLSB;     } break;

            case MidiTypeYTX::RPN:            { messageToCompare = switchMessageTypes::switch_msg_rpn;      
                                                paramToCompare  =  encoder[encNo].switchFeedback.parameterMSB<<7 | 
                                                                  encoder[encNo].switchFeedback.parameterLSB;     } break;

            case MidiTypeYTX::PitchBend:      { messageToCompare = switchMessageTypes::switch_msg_pb;             } break;
          }
        
        // SWEEP ALL ENCODERS SWITCHES
        if(paramToCompare == param || 
            messageToCompare == switchMessageTypes::switch_msg_pb){ 
          if(encoder[encNo].switchFeedback.channel == channel){
            // SERIALPRINTLN(F("CHN MATCH"));
            if(encoder[encNo].switchFeedback.message == messageToCompare){
              // SERIALPRINTLN(F("MSG MATCH"));
              if(encoder[encNo].switchFeedback.source & midiSrc){  
                // If there's a match, set encoder value and feedback
                if(!IsShifter(encNo)) { // If it is a shifter bank, don't update
                  // SERIALPRINTLN(F("FULL MATCH"));
                  if(messageToCompare == switchMessageTypes::switch_msg_pb){
                    if(value == 8192)     value = 0;
                    else if(value == 0)   value = 1;    // hack to make it turn off with center value, and not with lower value
                  }
                  uint16_t minVal = 0, maxVal = 0;
                  if(IS_ENCODER_SW_7_BIT(encNo)){
                    minVal = encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
                    maxVal = encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
                  }else{
                    minVal = encoder[encNo].switchConfig.parameter[switch_minValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_minValue_LSB];
                    maxVal = encoder[encNo].switchConfig.parameter[switch_maxValue_MSB]<<7 | encoder[encNo].switchConfig.parameter[switch_maxValue_LSB];
                  }
                  // Only update value if value to color is enabled, or, for fixed mode, if value to update is either MIN or MAX
                  // (prevents random values sent by softwares changing the state of the LEDs when switching banks)
                  if(encoder[encNo].switchFeedback.valueToColor || value == minVal || value == maxVal){
                    encoderHw.SetEncoderSwitchValue(currentBank, encNo, value);
                  }
                }
              }
            }
          }
        }
      }
    }break;
    case FB_ENC_SW_VAL_TO_INT:{
      // SWEEP ALL ENCODERS - // FIX FOR SHIFT ROTARY ACTION AND CHANGE ROTARY CONFIG FOR ROTARY FEEDBACK IN ALL CASES
      for(uint8_t encNo = 0; encNo < config->inputs.encoderCount; encNo++){
        if( encoder[encNo].switchFeedback.parameterLSB == param){
          if(channel == config->midiConfig.valueToIntensityChannel){
            if(encoder[encNo].switchFeedback.source & midiSrc){    
              feedbackHw.SetChangeEncoderFeedback(FB_ENC_SWITCH, 
                                                  encNo, 
                                                  value, 
                                                  encoderHw.GetModuleOrientation(encNo/4), 
                                                  NO_SHIFTER, 
                                                  NO_BANK_UPDATE, 
                                                  NO_COLOR_CHANGE,                   
                                                  VAL_TO_INT,           //value to intensity
                                                  EXTERNAL_FEEDBACK);
            }
          }
        }
      }
    }break;
    
    case FB_DIGITAL:
    case FB_DIG_VAL_TO_INT:{
      // SWEEP ALL DIGITAL
      for(uint16_t digNo = 0; digNo < config->inputs.digitalCount; digNo++){
        switch(msgType){
          case MidiTypeYTX::NoteOn:         { messageToCompare = digitalMessageTypes::digital_msg_note;   
                                              paramToCompare = digital[digNo].feedback.parameterLSB;      } break;

          case MidiTypeYTX::ControlChange:  { messageToCompare = digitalMessageTypes::digital_msg_cc;   
                                              paramToCompare = digital[digNo].feedback.parameterLSB;      } break;

          case MidiTypeYTX::ProgramChange:  { return;                                                     } break;    // Program change don't show feedback on digitals

          case MidiTypeYTX::NRPN:           { messageToCompare = digitalMessageTypes::digital_msg_nrpn;   
                                              paramToCompare =  digital[digNo].feedback.parameterMSB<<7 |
                                                                digital[digNo].feedback.parameterLSB;     } break;

          case MidiTypeYTX::RPN:            { messageToCompare = digitalMessageTypes::digital_msg_rpn;    
                                              paramToCompare =  digital[digNo].feedback.parameterMSB<<7 |
                                                                digital[digNo].feedback.parameterLSB;     } break;

          case MidiTypeYTX::PitchBend:      { messageToCompare = digitalMessageTypes::digital_msg_pb;     } break;
        } 
        
        if(paramToCompare == param || 
            messageToCompare == digitalMessageTypes::digital_msg_pb  ||
            messageToCompare == digitalMessageTypes::digital_msg_pc){
          if((fbType == FB_DIGITAL && digital[digNo].feedback.channel == channel) ||
             (fbType == FB_DIG_VAL_TO_INT && channel == config->midiConfig.valueToIntensityChannel)){
            if(digital[digNo].feedback.message == messageToCompare){
              if(digital[digNo].feedback.source & midiSrc){
                if(!IsShifter(digNo+config->inputs.encoderCount)) {  // If it is a bank shifter, don't update
                  // If there's a match, set encoder value and feedback
                  if(messageToCompare == digitalMessageTypes::digital_msg_pb){
                    if(value == 8192)     value = 0;    // make center value of pitch bend  (PITCH 0) turn off the LED and set digital value on 0
                    else if(value == 0)   value = 1;    // don't turn off LED for value (PITCH -8192)
                  }
                  
                  if (fbType == FB_DIGITAL){
                    uint16_t minVal = 0, maxVal = 0;
                    if(IS_DIGITAL_7_BIT(digNo)){
                      minVal = digital[digNo].actionConfig.parameter[digital_minLSB];
                      maxVal = digital[digNo].actionConfig.parameter[digital_maxLSB];
                    }else{
                      minVal = digital[digNo].actionConfig.parameter[digital_minMSB]<<7 | digital[digNo].actionConfig.parameter[digital_minLSB];
                      maxVal = digital[digNo].actionConfig.parameter[digital_maxMSB]<<7 | digital[digNo].actionConfig.parameter[digital_maxLSB];
                    }

                    // Only update value if value to color is enabled, or, for fixed mode, if value to update is either MIN or MAX
                    // (prevents random values sent by softwares changing the state of the LEDs when switching banks)
                    if(digital[digNo].feedback.valueToColor || value == minVal || value == maxVal){
                      digitalHw.SetDigitalValue(currentBank, digNo, value);
                    }
                    // SERIALPRINTLN(F("DIGITAL MATCH"));
                  }else if(fbType == FB_DIG_VAL_TO_INT){
                    feedbackHw.SetChangeDigitalFeedback(digNo, 
                                                        value, 
                                                        true, 
                                                        NO_SHIFTER, NO_BANK_UPDATE, EXTERNAL_FEEDBACK, VAL_TO_INT);
                    // SERIALPRINTLN(F("DIGITAL VAL TO INT"));
                  }
                }
              }
            }
          }
        }
      }
    }break;
    case FB_ANALOG:{
      // SWEEP ALL ANALOG
      for(uint8_t analogNo = 0; analogNo < config->inputs.analogCount; analogNo++){
        // analog has no feedback yet
        switch(msgType){
          case MidiTypeYTX::NoteOn:         { messageToCompare = analogMessageTypes::analog_msg_note;   
                                              paramToCompare = analog[analogNo].feedback.parameterLSB;      } break;

          case MidiTypeYTX::ControlChange:  { messageToCompare = analogMessageTypes::analog_msg_cc;   
                                              paramToCompare = analog[analogNo].feedback.parameterLSB;      } break;

          case MidiTypeYTX::ProgramChange:  { messageToCompare = analogMessageTypes::analog_msg_pc;         } break;

          case MidiTypeYTX::NRPN:           { messageToCompare = analogMessageTypes::analog_msg_nrpn;   
                                              paramToCompare =  analog[analogNo].feedback.parameterMSB<<7 |
                                                                analog[analogNo].feedback.parameterLSB;     } break;

          case MidiTypeYTX::RPN:            { messageToCompare = analogMessageTypes::analog_msg_rpn;    
                                              paramToCompare =  analog[analogNo].feedback.parameterMSB<<7 |
                                                                analog[analogNo].feedback.parameterLSB;     } break;

          case MidiTypeYTX::PitchBend:      { messageToCompare = analogMessageTypes::analog_msg_pb;         } break;
        }
      
        if(paramToCompare == param || 
            messageToCompare == analogMessageTypes::analog_msg_pb  ||
            messageToCompare == analogMessageTypes::analog_msg_pc){
          if(analog[analogNo].feedback.channel == channel){
            if(analog[analogNo].feedback.message == messageToCompare){
              if(analog[analogNo].feedback.source & midiSrc){
                // If there's a match, set encoder value and feedback
                // SERIALPRINTLN(F("ANALOG MATCH"));
                analogHw.SetAnalogValue(currentBank, analogNo, value);
              }
            }
          }
        }
      }
    }break;
    default: break;
  }
}

void CheckSerialSAMD11(){
  if(Serial.available()){
    byte cmd = Serial.read();
    if(cmd == SHOW_IN_PROGRESS){
      fbShowInProgress = true;
      feedbackHw.SendCommand(ACK_CMD);
    }else if(cmd == SHOW_END){
      fbShowInProgress = false;
      feedbackHw.SendCommand(ACK_CMD);
    }else if(cmd == RESET_HAPPENED){
      feedbackHw.InitAuxController(true); // Flag reset so it doesn't do a rainbow
    }
  }
}

void CheckSerialUSB(){
  if(SerialUSB.available()){
    char cmd = SerialUSB.read();

  if(cmd == 't'){
    testMode = true;
    SERIALPRINTLN(F("\n--------- WELCOME TO TEST MODE ---------\n"));
    SERIALPRINT(F("\nSend a command to begin each test:\n"));
    SERIALPRINT(F("\"e\": Test encoders state\n"));
    SERIALPRINT(F("\"s\": Test encoders switches\n"));
    SERIALPRINT(F("\"d\": Test digitals\n"));
    SERIALPRINT(F("\"a\": Test analog\n"));
    SERIALPRINT(F("\"h\": Test hardware: encoders+digitals+analog\n"));
    SERIALPRINT(F("\"l\": All LEDs ON\n"));
    SERIALPRINT(F("\"o\": All LEDs OFF\n"));
    SERIALPRINT(F("\"i\": Monitor incoming MIDI\n"));
    SERIALPRINT(F("\"y\": Monitor incoming SysEx\n"));
    SERIALPRINT(F("\"r\": Rainbow\n"));
    SERIALPRINT(F("\"b\": Restore bank LEDs\n"));
    SERIALPRINT(F("\"m\": Print loop micros\n"));
    SERIALPRINT(F("\"p\": Power connection?\n"));
    SERIALPRINT(F("\"c\": Print config\n"));
    SERIALPRINT(F("\"q\": Print elements mapping\n"));
    SERIALPRINT(F("\"u\": Print midi buffer\n"));
    SERIALPRINT(F("\"f\": Free RAM\n"));
    SERIALPRINT(F("\"v\": Erase controller state from EEPROM\n"));
    SERIALPRINT(F("\"w\": Erase whole EEPROM\n"));
    SERIALPRINT(F("\"x\": Reset to bootloader\n"));
    SERIALPRINT(F("\"z\": Exit test mode\n"));
  }else if(testMode && cmd == 'a'){
    testAnalog = !testAnalog;
    SERIALPRINT(F("\nTEST MODE FOR ANALOG ")); SERIALPRINT(testAnalog ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'd'){
    testDigital = !testDigital;
    SERIALPRINT(F("\nTEST MODE FOR DIGITAL ")); SERIALPRINT(testDigital ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'e'){
    testEncoders = !testEncoders;
    SERIALPRINT(F("\nTEST MODE FOR ENCODERS ")); SERIALPRINT(testEncoders ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 's'){
    testEncoderSwitch = !testEncoderSwitch;
    SERIALPRINT(F("\nTEST MODE FOR ENCODER SWITCHES ")); SERIALPRINT(testEncoderSwitch ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'm'){
    testMicrosLoop = !testMicrosLoop;
    SERIALPRINT(F("\nTEST MODE FOR LOOP MICROS ")); SERIALPRINT(testMicrosLoop ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'l'){
    feedbackHw.SendCommand(CMD_ALL_LEDS_ON);
  }else if(testMode && cmd == 'o'){
    feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
  }else if(testMode && cmd == 'b'){
    feedbackHw.SetBankChangeFeedback(FB_BANK_CHANGED); 
  }else if(testMode && cmd == 'r'){
    feedbackHw.SendCommand(CMD_RAINBOW_START);
  }else if(testMode && cmd == 'f'){
    SERIALPRINT(F("Free RAM: ")); SERIALPRINTLN(FreeMemory());
  }else if(testMode && cmd == 'p'){
    uint8_t powerAdapterConnected = !digitalRead(externalVoltagePin);
    SERIALPRINT(F("\nPOWER SUPPLY CONNECTED? ")); SERIALPRINT(powerAdapterConnected ? F("YES\n") : F("NO\n"));
  }else if(testMode && cmd == 'i'){
    testMidi = !testMidi;
    SERIALPRINT(F("\nMONITOR INCOMING MIDI ")); SERIALPRINT(testMidi ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'y'){
    testSysex = !testSysex;
    SERIALPRINT(F("\nMONITOR INCOMING SYSEX ")); SERIALPRINT(testSysex ? F("ENABLED\n") : F("DISABLED\n"));
  }else if(testMode && cmd == 'c'){
    if(validConfigInEEPROM){
      printConfig(ytxIOBLOCK::Configuration, 0);
    }else{
      SERIALPRINTLN(F("\nEEPROM Configuration not valid\n"));  
    }
  }else if(testMode && cmd == 'q'){
    if(validConfigInEEPROM){
      for(int b = 0; b < config->banks.count; b++){
        currentBank = memHost->LoadBank(b);
        SERIALPRINTLN("\n\n*********************************************");
        SERIALPRINT("************* BANK ");
                            SERIALPRINT(b);
                            SERIALPRINTLN(" ************************");
        SERIALPRINTLN("*********************************************\n\n");
        for(int e = 0; e < config->inputs.encoderCount; e++)
          printConfig(ytxIOBLOCK::Encoder, e);
        for(int d = 0; d < config->inputs.digitalCount; d++)
          printConfig(ytxIOBLOCK::Digital, d);
        for(int a = 0; a < config->inputs.analogCount; a++)
          printConfig(ytxIOBLOCK::Analog, a);  
      }
    }else{
      SERIALPRINTLN(F("\nEEPROM Configuration not valid\n"));  
    }
  }else if(testMode && cmd == 'u'){
    printMidiBuffer();  
  }else if(testMode && cmd == 'x'){
    SERIALPRINTLN("Rebooting to bootloader mode...");

    feedbackHw.SendCommand(CMD_ALL_LEDS_OFF);
    SelfReset(RESET_TO_BOOTLOADER);  

  }else if(testMode && cmd == 'v'){
    SERIALPRINTLN("Erasing controller state...");  
    eeErase(128, CTRLR_STATE_GENERAL_SETT_ADDRESS, 65535);
    SERIALPRINTLN("Controller state erased. Rebooting..."); 
    SelfReset(RESET_TO_CONTROLLER);
  }else if(testMode && cmd == 'w'){
    SERIALPRINTLN("Erasing eeprom...");
    eeErase(128, 0, 65535);
    SERIALPRINTLN("Done! Rebooting...");
    SelfReset(RESET_TO_CONTROLLER);
  }else if(testMode && cmd == 'z'){
    SERIALPRINTLN(F("\nALL TEST MODES DISABLED\n"));
    testMode = false;
    testEncoders = false;
    testAnalog = false;
    testEncoderSwitch = false;
    testDigital = false;
    testMicrosLoop = false;
    testMidi = false;
    feedbackHw.SetBankChangeFeedback(FB_BANK_CHANGED); 
  }else if(testMode && cmd == 'h'){
    static bool testHardware = false;
    testHardware = !testHardware;
    SERIALPRINT(F("\nTEST MODE FOR ALL HARDWARE ")); SERIALPRINT(testHardware ? F("ENABLED\n") : F("DISABLED\n"));
    testEncoders = testHardware;
    testAnalog = testHardware;
    testEncoderSwitch = testHardware;
    testDigital = testHardware;
  }else if(testMode && cmd == 'j'){

  }
  } 
}
