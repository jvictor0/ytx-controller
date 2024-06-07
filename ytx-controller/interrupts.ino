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