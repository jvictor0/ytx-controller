// Constant value definitions
#define POT_SENSITIVITY 50        //Higher number = more turns needed to reach max value
#define ADC_MAX_VALUE   4095

uint8_t arduinoWiring[POT_COUNT][2] = {{0,1},{9,4},{17,18},{19,25}};
uint8_t aInputs[POT_COUNT][2] = {{18,19},{7,16},{4,5},{10,11}};

static inline void ADCsync() {
  while (ADC->STATUS.bit.SYNCBUSY == 1); //Just wait till the ADC is free
}

inline void SelAnalog(uint32_t ulPin){      // Selects the analog input channel in INPCTRL
  ADCsync();
  ADC->INPUTCTRL.bit.MUXPOS = ulPin;
}

void FastADCsetup() {
  // const byte gClk = 3; //used to define which generic clock we will use for ADC
  // const int cDiv = 1; //divide factor for generic clock
  
  ADC->CTRLA.bit.ENABLE = 0;                     // Disable ADC
  while( ADC->STATUS.bit.SYNCBUSY == 1 );        // Wait for synchronization
  ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC1_Val; //set internal reference
  ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV32 |   // Divide Clock by 32
                   ADC_CTRLB_RESSEL_12BIT;       // Result on 12 bits
  ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_1 |   // 1 sample
                     ADC_AVGCTRL_ADJRES(0x00ul); // Adjusting result by 0
  ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(1);  // Sampling Time Length = 0
  ADC->CTRLA.bit.ENABLE = 1;                     // Enable ADC
  while( ADC->STATUS.bit.SYNCBUSY == 1 );        // Wait for synchronization
  
  //configure inputs pins
  for(uint8_t i=0;i<POT_COUNT;i++){
    pinPeripheral(arduinoWiring[i][0],PIO_ANALOG);
    pinPeripheral(arduinoWiring[i][1],PIO_ANALOG);
  }
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

int decodeInfinitePot(uint8_t i)
{
  int direction = 0;
  int hysteresis = parameters.hysteresis;
  float expAvgConstant = parameters.expFilter;

  rotary[i].valueA = expAvgConstant*AnalogReadFast(aInputs[i][0]) + (1-expAvgConstant)*rotary[i].valueA;
  rotary[i].valueB = expAvgConstant*AnalogReadFast(aInputs[i][1]) + (1-expAvgConstant)*rotary[i].valueB;


  /****************************************************************************
  * Step 1 decode each  individual pot tap's direction
  ****************************************************************************/
  // First check direction for Tap A
  if (rotary[i].valueA > (rotary[i].previousValueA + hysteresis))       // check if new reading is higher (by <debounce value>), if so...
  {
    rotary[i].dirA = 1;                                              // ...direction of tap A is up
  }
  else if (rotary[i].valueA < (rotary[i].previousValueA - hysteresis))  // check if new reading is lower (by <debounce value>), if so...
  {
    rotary[i].dirA = -1;                                             // ...direction of tap A is down
  }
  else
  {
    rotary[i].dirA = 0;                                              // No change
  }
  // then check direction for tap B
  if (rotary[i].valueB > (rotary[i].previousValueB + hysteresis))       // check if new reading is higher (by <debounce value>), if so...
  {
    rotary[i].dirB = 1;                                              // ...direction of tap B is up
  }
  else if (rotary[i].valueB < (rotary[i].previousValueB - hysteresis))  // check if new reading is lower (by <debounce value>), if so...
  {
    rotary[i].dirB = -1;                                             // ...direction of tap B is down
  }
  else
  {
    rotary[i].dirB = 0;                                              // No change
  }

  /****************************************************************************
  * Step 2: Determine actual direction of ENCODER based on each individual
  * potentiometer tap´s direction and the phase
  ****************************************************************************/
  if (rotary[i].dirA == -1 and rotary[i].dirB == -1)       //If direction of both taps is down
  {
    if (rotary[i].valueA > rotary[i].valueB)               // If value A above value B...
    {
      direction = 1;                         // ...direction is up
    }
    else
    {
      direction = -1;                        // otherwise direction is down
    }
  }
  else if (rotary[i].dirA == 1 and rotary[i].dirB == 1)    //If direction of both taps is up
  {
    if (rotary[i].valueA < rotary[i].valueB)               // If value A below value B...
    {
      direction = 1;                         // ...direction is up
    }
    else
    {
      direction = -1;                        // otherwise direction is down
    }
  }
  else if (rotary[i].dirA == 1 and rotary[i].dirB == -1)   // If A is up and B is down
  {
    if ( (rotary[i].valueA > (ADC_MAX_VALUE/2)) || (rotary[i].valueB > (ADC_MAX_VALUE/2)) )  //If either pot at upper range A/B = up/down means direction is up
    {
      direction = 1;
    }
    else                                     //otherwise if both pots at lower range A/B = up/down means direction is down
    {
      direction = -1;
    }
  }
  else if (rotary[i].dirA == -1 and rotary[i].dirB == 1)
  {
    if ( (rotary[i].valueA < (ADC_MAX_VALUE/2)) || (rotary[i].valueB < (ADC_MAX_VALUE/2)))   //If either pot  at lower range, A/B = down/up means direction is down
    {
      direction = 1;
    }
    else                                     //otherwise if bnoth pots at higher range A/B = up/down means direction is down
    {
      direction = -1;
    }
  }
  else
  {
    direction = 0;                           // if any of tap A or B has status unchanged (0), indicate unchanged
  }

  /****************************************************************************
  * Step 3: Calculate value based on direction, how big change in ADC value,
  * and sensitivity. Avoid values around zero and max  as value has flat region
  ****************************************************************************/
  if (rotary[i].dirA != 0 && rotary[i].dirB != 0)         //If both taps indicate movement
  {
    #if defined(COMPUTE_POT_VALUE)
      if ((rotary[i].valueA < ADC_MAX_VALUE*0.8) && (rotary[i].valueA > ADC_MAX_VALUE*0.2))         // if tap A is not at endpoints
      {
        rotary[i].Value = rotary[i].Value + direction*abs(rotary[i].valueA - rotary[i].previousValueA)/POT_SENSITIVITY; //increment value
      }
      else                                    // If tap A is close to end points, use tap B to calculate value
      {
        rotary[i].Value = rotary[i].Value + direction*abs(rotary[i].valueB - rotary[i].previousValueB)/POT_SENSITIVITY;  //Make sure to add/subtract at least 1, and then additionally the jump in voltage
      }

      // Finally apply output value limit control
      if (rotary[i].Value <= 0)
      {
        rotary[i].Value = 0;
      }
      else if (rotary[i].Value >= MAX_POT_VALUE)
      {
        rotary[i].Value = MAX_POT_VALUE;
      } 
    #endif
                                              // Update prev value storage
    rotary[i].previousValueA = rotary[i].valueA;          // Update previous value variable
    rotary[i].previousValueB = rotary[i].valueB;          // Update previous value variable
    
    SERIALPRINT("Pot ");
    SERIALPRINT(i);
    SERIALPRINT(": ");
    SERIALPRINTLN(direction);
  }
  
  return direction;
}