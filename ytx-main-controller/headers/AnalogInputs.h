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

#ifndef ANALOG_INPUTS_H
#define ANALOG_INPUTS_H

#include "Arduino.h"
#include "Defines.h"
#include "FeedbackClass.h"
#include <SPIAnalogExpander.h>

//----------------------------------------------------------------------------------------------------
// CLASS DEFINITION
//----------------------------------------------------------------------------------------------------
#define FADER_ALPS_RSA0_TAPERS_TABLE_SIZE   11
#define FADER_TTE_PS45M_TABLE_SIZE          7
#define ADC_BITS                            12
#define ADC_MAX_COUNT                       4095
#define ADC_MIN_BUS_COUNT                   18    //minimum opamp voltage swing 15mV --> 0.015v*ADC_MAX_COUNT/3.3v
#define ADC_MAX_BUS_COUNT                   4076  //maximum opamp voltage swing 3.3v-15mV --> 3.285v*ADC_MAX_COUNT/3.3v
#define EXTERNAL_MUX_CHANNELS               16

class AnalogInputs{

public:
  typedef struct __attribute__((packed)){
    uint16_t analogValue;         // Variable to store analog values
    uint16_t analogValuePrev;     // Variable to store previous analog values
    uint16_t hardwarePivot;     // Variable to store previous analog values
    uint16_t targetValuePivot;     // Variable to store previous analog values
    struct {
      uint8_t takeOverOn : 1;
      uint8_t lastDirection : 2;
      uint8_t interpolate : 1;
      uint8_t reservedFlags : 4;
    }flags;
  }analogBankData;
  
  void      Init(uint8_t,uint8_t, SPIAdressableBUS*);
  void      Read();
  void      IrqHandler();
  void      SetAnalogValue(uint8_t, uint8_t, uint16_t);
  uint16_t  GetAnalogValue(uint8_t);
  void      SetBankForAnalog(uint8_t);
  uint32_t  AnalogReadFast(byte);
  void      SendMessage(uint8_t);
  void      SendNRPN();
  inline void SetPriority(bool priority) { priorityMode = 0; }
  inline bool IsPriorityModeOn() { return priorityMode; }

  
private:
  void SetPivotValues(uint8_t, uint8_t, uint16_t);
  bool IsNoise(uint16_t, uint16_t, uint16_t , byte, bool);
  void FastADCsetup();
  void StartConversion();
  void SelAnalog(uint32_t);
  
  int16_t MuxAnalogRead(uint8_t , uint8_t);
  int16_t MuxDigitalRead(uint8_t, uint8_t, uint8_t);
  int16_t MuxDigitalRead(uint8_t, uint8_t);
  uint16_t FilterGetNewExponentialAverage(uint8_t, uint16_t);
  
  SPIAnalogExpander *spiAnalogExpander;

  // Variables

  uint8_t nBanks;
  uint8_t nAnalog;
  bool begun;
  uint16_t minRawValue;
  uint16_t maxRawValue;
  bool priorityMode;
  uint8_t analogMuxesWithElements;
  bool spiAnalogExpanderEnable;

  analogBankData **aBankData;

  typedef struct __attribute__((packed)){
    uint16_t analogRawValue : 12;         // Variable to store analog values
    uint16_t analogDirection : 4;            // Variable to store current direction of change
    uint16_t analogRawValuePrev : 12;     // Variable to store previous analog values
    uint16_t analogDirectionRaw : 4;            // Variable to store current direction of change

    // Running average filter variables
    uint16_t exponentialFilter;
  }analogHwData;
  analogHwData *aHwData;

  uint64_t updateValue;
  uint32_t antMillisAnalogUpdate;

  volatile bool computeSampling;
  volatile bool sampleReady;
  
  // Address lines for multiplexer
  const int _S0 = (4u);
  const int _S1 = (3u);
  const int _S2 = (8u);
  const int _S3 = (9u);
  // Input signal of multiplexers
  byte muxPin[NUM_MUX] = {MUX_A_PIN, MUX_B_PIN, MUX_C_PIN, MUX_D_PIN};


  // Do not change - These are used to have the inputs and outputs of the headers in order
  byte MuxMapping[NUM_MUX_CHANNELS] =   {1,        // INPUT 0   - Mux channel 2
                                         0,        // INPUT 1   - Mux channel 0
                                         3,        // INPUT 2   - Mux channel 3
                                         2,        // INPUT 3   - Mux channel 1
                                         12,       // INPUT 4   - Mux channel 12
                                         13,       // INPUT 5   - Mux channel 14
                                         14,       // INPUT 6   - Mux channel 13
                                         15,       // INPUT 7   - Mux channel 15
                                         7,        // INPUT 8   - Mux channel 7
                                         4,        // INPUT 9   - Mux channel 4
                                         5,        // INPUT 10  - Mux channel 6
                                         6,        // INPUT 11  - Mux channel 5
                                         10,        // INPUT 12  - Mux channel 9
                                         9,       // INPUT 13  - Mux channel 10
                                         8,        // INPUT 14  - Mux channel 8
                                         11};      // INPUT 15  - Mux channel 11

// Do not change - These are used to have the inputs and outputs of the headers in order                                      


// TAPER RESPONSE TABLES
  uint8_t TTE_PS45M_Taper_Template[FADER_TTE_PS45M_TABLE_SIZE][2] = 
                                       // {%output,%travel}
                                       {{1,1},          // 0% travel 
                                        {4,10},         // 10% travel
                                        {5,12},         // 20% travel
                                        {10,19},        // 25% travel
                                        {90,81},        // 75% travel
                                        {95,88},        // 80% travel
                                        {100,100}};     // 100% travel
  uint16_t TTE_PS45M_Taper[FADER_TTE_PS45M_TABLE_SIZE][2];

  uint8_t ALPS_RSA0_Taper_Template[FADER_ALPS_RSA0_TAPERS_TABLE_SIZE][2] = 
                                         // {%output,%travel}
                                         {{0,0},       // 0% travel 
                                          {19,10},     // 10% travel
                                          {41,20},     // 20% travel
                                          {62,27},     // 30% travel
                                          {75,40},     // 40% travel
                                          {82,50},     // 50% travel
                                          {89,60},     // 60% travel
                                          {94,70},     // 70% travel
                                          {97,80},     // 80% travel
                                          {99,90},     // 90% travel
                                          {100,100}};  // 100% travel
                                       
  uint16_t ALPS_RSA0_Taper[FADER_ALPS_RSA0_TAPERS_TABLE_SIZE][2];    

};

#endif