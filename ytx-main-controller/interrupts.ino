void ADC_Handler(void) {
  analogHw.IrqHandler();
}

void MIDIpull_Handler(void) {
  // Call MIDI read functions and run callbacks if message arrived
  if(feedbackHw.fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE && !feedbackHw.SendingData()){
    MIDI.read();
    MIDIHW.read();
  }
}


void AuxControllerReception_Handler(){
  uint16_t rcvWord = Serial.read();
  bool isCommand = (rcvWord&0x100) ? true : false;
  uint8_t rcvByte = (uint8_t)(rcvWord&0x00FF);
  // SerialUSB.print("rcvWord: ");SerialUSB.println(rcvWord);
  // SerialUSB.print("rcvByte: ");SerialUSB.println(rcvByte);
  if(isCommand){
    if(rcvByte == SHOW_IN_PROGRESS){
      fbShowInProgress = true;
      antMicrosAuxShow = micros();
      // SerialUSB.println("SHOW_IN_PROGRESS");
    }else if(rcvByte == SHOW_END){
      fbShowInProgress = false;
      // SerialUSB.println("SHOW_END");
    }else if(rcvByte == ACK_CMD){
      waitingForAck = false;
      // SerialUSB.println("ACK_CMD");
    }else if(rcvByte == RESET_HAPPENED){

    }else if(rcvByte == END_OF_RAINBOW){
      waitingForRainbow = false;
    }
  }
}