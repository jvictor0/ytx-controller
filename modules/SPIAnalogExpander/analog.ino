static inline void ADCsync() {
  while (ADC->STATUS.bit.SYNCBUSY == 1); //Just wait till the ADC is free
}

inline void SelAnalog(uint32_t ulPin){      // Selects the analog input channel in INPCTRL
  ADCsync();
  //ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[ulPin].ulADCChannelNumber; // Selection for the positive ADC input
  ADC->INPUTCTRL.bit.MUXPOS = ulPin;
}

void FastADCsetup() {
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
  
}

uint32_t AnalogReadFast(byte ADCpin) {
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
}

int16_t MuxAnalogRead(uint8_t mux, uint8_t chan){
    if (chan >= 0 && chan <= 15 && mux < NUM_MUX){     
      chan = MuxMapping[chan];      // Re-map hardware channels to have them read in the header order
    }
    else return -1;       // Return ERROR

  //Select channel
  bitRead(chan, 0) ?  PORT->Group[g_APinDescription[_S0].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S0].ulPin) :
            PORT->Group[g_APinDescription[_S0].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S0].ulPin) ;
  bitRead(chan, 1) ?  PORT->Group[g_APinDescription[_S1].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S1].ulPin) :
            PORT->Group[g_APinDescription[_S1].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S1].ulPin) ;
  bitRead(chan, 2) ?  PORT->Group[g_APinDescription[_S2].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S2].ulPin) :
            PORT->Group[g_APinDescription[_S2].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S2].ulPin) ;
  bitRead(chan, 3) ?  PORT->Group[g_APinDescription[_S3].ulPort].OUTSET.reg = (1ul << g_APinDescription[_S3].ulPin) :
            PORT->Group[g_APinDescription[_S3].ulPort].OUTCLR.reg = (1ul << g_APinDescription[_S3].ulPin) ;

  return AnalogReadFast(muxPin[mux]);
}

bool isNoise(analogType *input, uint8_t threshold) {
  if (input->direction == ANALOG_INCREASING){   // CASE 1: If signal is increasing,
    if(input->rawValue > input->rawValuePrev){  // and the new value is greater than the previous,
       return 0;                                // NOT NOISE!
    }
    else if(input->rawValue < (input->rawValuePrev - threshold)){  
      // If, otherwise, it's lower than the previous value and the noise threshold together,
      // means it started to decrease,
      input->direction = ANALOG_DECREASING;
      return 0;                                 // NOT NOISE!
    }
  }
  if (input->direction == ANALOG_DECREASING){   // CASE 2: If signal is increasing,
    if(input->rawValue < input->rawValuePrev){  // and the new value is lower than the previous,
       return 0;                                // NOT NOISE!
    }
    else if(input->rawValue > (input->rawValuePrev + threshold)){  
      // If, otherwise, it's greater than the previous value and the noise threshold together,
      // means it started to increase,
      input->direction = ANALOG_INCREASING;

      return 0;                                 // NOT NOISE!
    }
  }
  return 1; // If everyting above was not true, then IT'S NOISE! Pot is trying to fool us. But we owned you pot ;)
}