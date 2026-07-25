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

      auxQueueFullOccurred = receivingQueueFullIndex;
      receivingQueueFullIndex = false;
      burstErrorOccurred = true;
      waitingForAck = false;
    }
    return;
  }

  if(isCommand)
  {
    if(rcvByte == SHOW_REQUEST_DIRTY)
    {
      auxShowRequestKind = SHOW_KIND_DIRTY;
    }
    else if(rcvByte == SHOW_REQUEST_ALL)
    {
      auxShowRequestKind = SHOW_KIND_ALL;
    }
    else if(rcvByte == SHOW_END)
    {
      fbShowInProgress = false;
      // Any request observed while the matching grant was in flight was a
      // retry of the show that just ended, not a new physical-show request.
      auxShowRequestKind = SHOW_KIND_NONE;
    }
    else if(rcvByte == INIT_VALUES)
    {
      auxInitAckTagged = true;
    }
    else if(rcvByte == CHANGE_BRIGHTNESS)
    {
      auxCommandAckTagged = true;
    }
    else if(rcvByte == ACK_CMD)
    {
      if(auxInitAckTagged){
        auxInitAckTagged = false;
        auxAckReceived = true;
      }else if(auxCommandAckTagged && !auxBurstTransmissionActive){
        auxCommandAckTagged = false;
        waitingForAck = false;
      }else if(auxBurstTransmissionActive && auxBurstAckExpected){
        waitingForAck = false;
      }
    }
    else if(rcvByte == CHECKSUM_ERROR)
    {
      // Start receiving the error index bytes
      //
      receivingQueueFullIndex = false;
      receivingErrorIndex = true;
      errorIndexBytesReceived = 0;
    }
    else if(rcvByte == RESET_HAPPENED)
    {
      fbShowInProgress = false;
      auxShowRequestKind = SHOW_KIND_NONE;
      waitingForAck = false;
      auxAckReceived = false;
      auxInitAckTagged = false;
      auxCommandAckTagged = false;
      auxBurstTransmissionActive = false;
      auxBurstAckExpected = false;
      auxMemoryErrorReportCount = 0;
      receivingErrorIndex = false;
      errorIndexBytesReceived = 0;
      errorIndexByte1 = 0;
      errorIndexByte2 = 0;
      receivingQueueFullIndex = false;
      auxQueueFullOccurred = false;
      auxResetPending = true;
    }
    else if(rcvByte == AUX_QUEUE_FULL)
    {
      // Queue pressure is flow control, not data corruption. The following
      // duplicated index is the exact first frame the aux did not accept.
      receivingQueueFullIndex = true;
      receivingErrorIndex = true;
      errorIndexBytesReceived = 0;
    }
    else if(rcvByte == AUX_MEMORY_ERROR)
    {
      uint32_t now = micros();
      if(auxMemoryErrorReportCount == 0 ||
         (uint32_t)(now - auxMemoryErrorLastMicros) > AUX_MEMORY_ERROR_CONFIRM_US){
        auxMemoryErrorReportCount = 1;
      }else if(auxMemoryErrorReportCount < 2){
        auxMemoryErrorReportCount++;
      }
      auxMemoryErrorLastMicros = now;
      if(auxMemoryErrorReportCount >= 2){
        auxMemoryError = true;
      }
    }
    else if(rcvByte == END_OF_RAINBOW)
    {
      waitingForRainbow = false;
    }
  }
}
