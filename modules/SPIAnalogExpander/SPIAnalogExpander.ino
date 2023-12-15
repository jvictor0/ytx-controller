#include "defines.h"
#include <SPIAnalogExpander.h>
#include <SPIAddressableSlave.h>

#define SLAVE_BASE_ADDRESS       ANALOG_EXPANDER_BASE_ADDRESS

//Control Register mapping (view SPIAnalogExpander)
#define CTRL_REG_COUNT sizeof(SPIAnalogExpanderParameters)

//UserData Register mapping
#define USR_REG_COUNT  (MAX_ANALOG_INPUTS/8 + MAX_ANALOG_INPUTS*sizeof(uint16_t))

SPIAnalogExpanderParameters parameters;

analogType analog[MAX_ANALOG_INPUTS];

uint16_t *analogRegister;
uint8_t  *activeChannels;

void cleanup(void){

}

void configure(void){
  memcpy(&parameters,SPIAddressableSlaveModule.getControlRegistersPointer(),sizeof(SPIAnalogExpanderParameters));
  SERIALPRINT("Conf recibida,  inputs: ");SERIALPRINT(parameters.inputs);SERIALPRINT(" filter: ");SERIALPRINT(parameters.expFilter);
  SERIALPRINT(" noise: ");SERIALPRINTLN(parameters.noiseThreshold);
}

void setup (void)
{
  #if defined(SERIAL_DEBUG)
    Serial.begin (115200);   // debugging
  #endif

  SPIAddressableSlaveModule.begin(SLAVE_BASE_ADDRESS,CTRL_REG_COUNT,USR_REG_COUNT);
  // SPIAddressableSlaveModule.setTransmissionCompleteCallback(cleanup);
  SPIAddressableSlaveModule.setConfigurationCompleteCallback(configure);
  SPIAddressableSlaveModule.setAddress(0);

  analogRegister = (uint16_t *)SPIAddressableSlaveModule.getUserRegistersPointer();
  activeChannels = (uint8_t *)((uint32_t)SPIAddressableSlaveModule.getUserRegistersPointer()+MAX_ANALOG_INPUTS*sizeof(uint16_t));

  // Set output pins for multiplexers
  pinMode(_S0, OUTPUT);
  pinMode(_S1, OUTPUT);
  pinMode(_S2, OUTPUT);
  pinMode(_S3, OUTPUT);

  // init ADC peripheral
  analogReference(AR_EXTERNAL);
  FastADCsetup();

  memset((void*)analog,0,sizeof(analog));
  
  parameters.inputs = 0;
  parameters.expFilter = 0.25;
  parameters.noiseThreshold = 10;
  
}// end of setup

uint32_t antMillisPrintLoop = 0;

void loop(){
  //process spi slave events
  SPIAddressableSlaveModule.hook();

  // Update ADC readings
  // Scan analog inputs
  for(int aInput = 0; aInput < parameters.inputs; aInput++){
    byte mux = aInput < 16 ? MUX_A : (aInput < 32 ? MUX_B : ( aInput < 48 ? MUX_C : ( aInput < 64 ? MUX_D : ( aInput < 80 ? MUX_E : MUX_F))));    // Select correct multiplexer for this input
    
    byte muxChannel = aInput % NUM_MUX_CHANNELS;        
    
    uint16_t newread = MuxAnalogRead(mux, muxChannel)&0x0FFF;
    analog[aInput].rawValue = parameters.expFilter*newread + (1-parameters.expFilter)*analog[aInput].rawValue;

    if(analog[aInput].rawValue != analog[aInput].rawValuePrev &&
      !isNoise(&analog[aInput],parameters.noiseThreshold)){

        #if defined(SERIAL_DEBUG)
          Serial.print("Channel ");Serial.print(aInput);
          Serial.print(" -> ");Serial.println(analog[aInput].rawValue);
        #endif
        analogRegister[aInput] = analog[aInput].rawValue;

        analog[aInput].rawValuePrev = analog[aInput].rawValue;

        activeChannels[aInput/8] |= (1<<aInput%8);
    }else{
      activeChannels[aInput/8] &= ~(1<<aInput%8);
    }
  }

  if(millis()-antMillisPrintLoop>1000){
    SERIALPRINTLN("Loop");
    antMillisPrintLoop = millis();
  }
}
