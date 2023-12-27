#ifndef _SPIAnalogExpander_H
#define _SPIAnalogExpander_H

#if (ARDUINO >= 100) 
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <SPI.h>

#include "SPIAddressable.h"

#define MAX_ANALOG_CHANNELS 96

typedef struct __attribute__((packed)){
  uint8_t inputs;
  float	expFilter;
  uint8_t noiseThreshold;
}SPIAnalogExpanderParameters;

class SPIAnalogExpander : public SPIAddressableElement {     
    public:
    	void begin(SPIAdressableBUS *);
        void configure(SPIAnalogExpanderParameters *);
        void getActiveChannels(void);
        bool isActiveChannel(uint32_t n);
        int16_t analogRead(uint32_t n);

    private:
        uint8_t activeChannels[MAX_ANALOG_CHANNELS/8];
};

#endif //_SPIAnalogExpander_H