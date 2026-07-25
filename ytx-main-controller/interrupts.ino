void ADC_Handler(void) {
  analogHw.IrqHandler();
}

void MIDIpull_Handler(void) {
  while(Serial1.available()){
    int incoming = Serial1.read();
    if(incoming < 0) break;

    switch((uint8_t)incoming){
      case 0xF8:
        MIDI.sendRealTime(midi::Clock);
        break;
      case 0xFA:
        MIDI.sendRealTime(midi::Start);
        break;
      case 0xFB:
        MIDI.sendRealTime(midi::Continue);
        break;
      case 0xFC:
        MIDI.sendRealTime(midi::Stop);
        break;
      default:
        break;
    }
  }
}


void AuxControllerReception_Handler(){
  uint16_t rcvWord = Serial.read();
  bool isCommand = (rcvWord&0x100) ? true : false;
  uint8_t rcvByte = (uint8_t)(rcvWord&0x00FF);

  // If we're receiving error index bytes (data bytes after CHECKSUM_ERROR)
  //
  if(receivingErrorIndex && !isCommand)
  {
    if(errorIndexBytesReceived == 0)
    {
      errorIndexByte1 = rcvByte;
      errorIndexBytesReceived = 1;
    }
    else
    {
      errorIndexByte2 = rcvByte;
      errorIndexBytesReceived = 0;
      receivingErrorIndex = false;

      // Verify the two bytes match; if not, treat as error at index 0
      //
      if(errorIndexByte1 == errorIndexByte2)
      {
        burstErrorIndex = errorIndexByte1;
      }
      else
      {
        burstErrorIndex = 0;
      }

      burstErrorOccurred = true;
      waitingForAck = false;
    }
    return;
  }

  if(isCommand)
  {
    if(rcvByte == SHOW_IN_PROGRESS)
    {
      fbShowInProgress = true;
      antMicrosAuxShow = micros();
    }
    else if(rcvByte == SHOW_END)
    {
      fbShowInProgress = false;
    }
    else if(rcvByte == INIT_VALUES)
    {
      auxInitAckTagged = true;
    }
    else if(rcvByte == ACK_CMD)
    {
      if(auxInitAckTagged){
        auxInitAckTagged = false;
        auxAckReceived = true;
      }else if(!auxBurstTransmissionActive || auxBurstAckExpected){
        waitingForAck = false;
      }
    }
    else if(rcvByte == CHECKSUM_ERROR)
    {
      // Start receiving the error index bytes
      //
      receivingErrorIndex = true;
      errorIndexBytesReceived = 0;
    }
    else if(rcvByte == RESET_HAPPENED)
    {
      fbShowInProgress = false;
      waitingForAck = false;
      auxAckReceived = false;
      auxInitAckTagged = false;
      auxBurstTransmissionActive = false;
      auxBurstAckExpected = false;
      receivingErrorIndex = false;
      errorIndexBytesReceived = 0;
      errorIndexByte1 = 0;
      errorIndexByte2 = 0;
      auxResetPending = true;
    }
    else if(rcvByte == AUX_QUEUE_OVERFLOW)
    {
      auxQueueOverflowed = true;
    }
    else if(rcvByte == AUX_MEMORY_ERROR)
    {
      auxMemoryError = true;
    }
    else if(rcvByte == END_OF_RAINBOW)
    {
      waitingForRainbow = false;
    }
  }
}
