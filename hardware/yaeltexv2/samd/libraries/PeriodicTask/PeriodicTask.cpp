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

//this function gets called by the interrupt at <sampleRate>Hertz


/* 
 *  TIMER SPECIFIC FUNCTIONS FOLLOW
 *  you shouldn't change these unless you know what you're doing
 */
#include "PeriodicTask.h"


void (*mPeriodicTaskCallback)(void);

// void TC5_Handler (void) __attribute__ ((weak, alias("PeriodicTask_Handler")));
#define PTASK_TC         TC5
#define PTASK_TC_IRQn    TC5_IRQn

void TC5_Handler (void) {
  PTASK_TC->COUNT16.INTFLAG.bit.MC0 = 1; //Writing a 1 to INTFLAG.bit.MC0 clears the interrupt so that it will run again

  mPeriodicTaskCallback();
}

PeriodicTask::PeriodicTask(){}

//Configures the TC to generate output events at the sample frequency.
//Configures the TC in Frequency Generation mode, with an event output once
//each time the audio sample frequency period expires.
void PeriodicTask::begin(void (*callback)(void), int sampleRate){
 // select the generic clock generator used as source to the generic clock multiplexer
 GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5)) ;
 while (GCLK->STATUS.bit.SYNCBUSY);

 reset(); //reset TCx

 // Set Timer counter 5 Mode to 16 bits, it will become a 16bit counter ('mode1' in the datasheet)
 PTASK_TC->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
 // Set TCx waveform generation mode to 'match frequency'
 PTASK_TC->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
 //set prescaler
 //the clock normally counts at the GCLK_TC frequency, but we can set it to divide that frequency to slow it down
 //you can use different prescaler divisons here like TC_CTRLA_PRESCALER_DIV1 to get a different range
 PTASK_TC->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_ENABLE; //it will divide GCLK_TC frequency by 1024
 //set the compare-capture register. 
 //The counter will count up to this value (it's a 16bit counter so we use uint16_t)
 //this is how we fine-tune the frequency, make it count to a lower or higher value
 //system clock should be 1MHz (8MHz/8) at Reset by default
 PTASK_TC->COUNT16.CC[0].reg = (uint16_t) (SystemCoreClock / sampleRate);
 while (IsSyncing());

 mPeriodicTaskCallback = callback;
 
 // Configure interrupt request
 NVIC_DisableIRQ(PTASK_TC_IRQn);
 NVIC_ClearPendingIRQ(PTASK_TC_IRQn);
 NVIC_SetPriority(PTASK_TC_IRQn, (1 << __NVIC_PRIO_BITS) - 1); //lowest priority
 NVIC_EnableIRQ(PTASK_TC_IRQn);

 // Enable the TCx interrupt request
 PTASK_TC->COUNT16.INTENSET.bit.MC0 = 1;
 while (IsSyncing()); //wait until TCx is done syncing 
} 

//Function that is used to check if TCx is done syncing
//returns true when it is done syncing
bool PeriodicTask::IsSyncing(){
  return PTASK_TC->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
}

//This function enables TCx and waits for it to be ready
void PeriodicTask::start(){
  PTASK_TC->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; //set the CTRLA register
  while (IsSyncing()); //wait until snyc'd
}

//Reset TCx 
void PeriodicTask::reset(){
  PTASK_TC->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (IsSyncing());
  while (PTASK_TC->COUNT16.CTRLA.bit.SWRST);
}

//disable TCx
void PeriodicTask::stop(){
  PTASK_TC->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
  while (IsSyncing());
}
