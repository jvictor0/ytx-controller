void ADC_Handler(void) {
  if (ADC->INTFLAG.bit.RESRDY) {
    ADC->INTFLAG.bit.RESRDY = 1; // Limpia la bandera de interrupción
    analogHw.IrqHandler();
  }
}

void TC5_Handler (void) {
  TC5->COUNT16.INTFLAG.bit.MC0 = 1; //Writing a 1 to INTFLAG.bit.MC0 clears the interrupt so that it will run again

  // Call MIDI read function and if message arrived, the callbacks get called
  if(feedbackHw.fbItemsToSend < FEEDBACK_UPDATE_BUFFER_SIZE && !feedbackHw.SendingData()){
    MIDI.read();
    MIDIHW.read();
  }
    
  countTimer++;
}

void SERCOM5_Handler()
{
  Serial.IrqHandler();  // Call irq handler

  if(Serial.available()){
    byte cmd = Serial.read();
    // SERIALPRINT("IRQ:"); SERIALPRINTLNF(cmd, HEX);
    if(cmd == SHOW_IN_PROGRESS){
      fbShowInProgress = true;
      antMicrosAuxShow = micros();
      // SERIALPRINTLN("SHOW IN PROGRESS");
      // Serial.read();
    }else if(cmd == SHOW_END){
      fbShowInProgress = false;
      // SERIALPRINTLN("SHOW ENDED");
      // Serial.read();
    }else if(cmd == ACK_CMD){
      waitingForAck = false;
      // SERIALPRINTLN("SHOW ENDED");
      // Serial.read();
    }else if(cmd == RESET_HAPPENED){
      feedbackHw.InitAuxController(true); // Flag reset so it doesn't do a rainbow
      // Serial.read();
    }else if(cmd == END_OF_RAINBOW){
      waitingForRainbow = false;
      // SERIALPRINTLN("END OF RAINBOW RECEIVED!");
      // Serial.read();
    }
  }
}