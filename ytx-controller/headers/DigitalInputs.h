/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2020 - Yaeltex

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

#ifndef DIGITAL_INPUTS_H
#define DIGITAL_INPUTS_H

#include <SPI.h>
#include <SPIGPIOExpander.h>

#include "modules.h"
#include "FeedbackClass.h"
//----------------------------------------------------------------------------------------------------
// CLASS DEFINITION
//----------------------------------------------------------------------------------------------------

#define BOUNCE_MILLIS		50 

class DigitalInputs{
public:
	typedef struct __attribute__((packed)){
  		uint16_t digitalInputState : 1;
		uint16_t digitalInputStatePrev : 1;
		uint16_t lastValue : 14;
  	}digitalBankData;
  	
	void Init(uint8_t,uint16_t,SPIAdressableBUS*,SPIAdressableBUS*);
	void Read();
	void SetDigitalValue(uint8_t,uint16_t,uint16_t);
	void SetButtonVelocity(uint8_t);
	void SetProgramChange(uint8_t,uint8_t,uint8_t);
	uint8_t GetButtonVelocity();
	uint8_t GetDigitalButtonBrightness(uint8_t);
	uint16_t GetDigitalValue(uint16_t);
	bool GetDigitalState(uint16_t);
	void DigitalAction(uint16_t,uint16_t, bool initDump = false);
	DigitalInputs::digitalBankData* GetCurrentDigitalStateData(uint8_t bank, uint16_t digNo);
	
private:
	uint8_t nBanks;
	uint16_t nDigitals;
	uint8_t nModules;
	bool begun;
	
	// pointers to modules objects
	SPIGPIOExpander **digitalsModule;

	uint8_t individualScanInterval;
	uint32_t generalMillis;	
	
	uint8_t buttonVelocity;
	uint8_t overrideVelocity;

	uint8_t currentProgram[2][16];  // Program change # for each port (USB and HW) and channel

  	typedef struct __attribute__((packed)){
		uint32_t 	antMillisScan;
		uint16_t 	state;
	  	uint16_t 	statePrev;
	  	uint16_t	digitalIndexStart;
	  	uint8_t 	type;
	  	uint8_t 	unused;
  	}moduleData;
	moduleData *digMData;
	
  	digitalBankData **dBankData;

	typedef struct __attribute__((packed)){
  		uint8_t digitalHWState : 1;
		uint8_t digitalHWStatePrev : 1;
		uint8_t doubleClick : 1;
		uint8_t localStartUpEnabled : 1;
		uint8_t reserved : 4;
  	}digitalHwData;  	
	digitalHwData *dHwData;

	void SetNextAddress(uint8_t, uint8_t);
	void CheckIfChanged(uint8_t);
};


#endif