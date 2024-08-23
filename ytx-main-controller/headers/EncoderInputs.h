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

#ifndef ENCODER_INPUTS_H
#define ENCODER_INPUTS_H

#include <SPI.h>
#include <SPIGPIOExpander.h>
#include <SPIEndlessPot.h>

#include "modules.h"
#include "FeedbackClass.h"

//----------------------------------------------------------------------------------------------------
// CLASS DEFINITION
//----------------------------------------------------------------------------------------------------

#define ENCODER_MASK_H			0x3FFE
#define ENCODER_MASK_V			0xE3FF
#define ENCODER_SWITCH_H_ON		0xC001
#define ENCODER_SWITCH_V_ON		0x1C00

#define ENC_MODULE_NUMBER(encNo)	encNo/4

// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

// Use the quarter-step state table (emits a code at every change)
#define R_START_0		0x00     //!< Rotary full step start
#define R_START_1       0x01     //!< Rotary full step start
#define R_START_2       0x02     //!< Rotary full step start
#define R_START_3       0x03     //!< Rotary full step start
#define R_CW_1       	0x04     //!< Rotary full step clock wise final
#define R_CW_2       	0x05     //!< Rotary full step clock begin
#define R_CW_3        	0x06     //!< Rotary full step clock next
#define R_CCW_1      	0x07     //!< Rotary full step counter clockwise begin
#define R_CCW_2      	0x08     //!< Rotary full step counter clockwise final
#define R_CCW_3       	0x09     //!< Rotary full step counter clockwise next

static const PROGMEM uint8_t quarterStepTable[10][4] = {
    // R_START_0
    {R_START_0,      		R_CCW_1 | DIR_CCW,  R_CW_1 | DIR_CW, 		R_START_3},
    // R_START_1
    {R_START_0 | DIR_CW,   	R_START_1,     	  	R_START_2,  			R_CCW_2 | DIR_CCW},
    // R_START_2
    {R_START_0 | DIR_CCW,	R_START_1,  		R_START_2,     			R_CW_2 | DIR_CW},
    // R_START_3
    {R_START_0,    			R_CW_3 | DIR_CW,  	R_CCW_3 | DIR_CW,  		R_START_3},
    // R_CW_1
    {R_START_0 | DIR_CCW,  	R_START_1,     		R_START_2, 				R_CW_2 | DIR_CW},
    // R_CW_2
    {R_START_0,   			R_CW_3 | DIR_CW, 	R_START_2 | DIR_CCW,   	R_START_3},
    // R_CW_3
    {R_START_0 | DIR_CW,   	R_START_1, 			R_START_2, 				R_START_3 | DIR_CCW},
    // R_CCW_1
    {R_START_0 | DIR_CW,   	R_START_1,     		R_START_2, 				R_CCW_2 | DIR_CCW},
    // R_CCW_2
    {R_START_0,   			R_START_1 | DIR_CW, R_CCW_3 | DIR_CCW,     	R_START_3},
    // R_CCW_3
    {R_START_0 | DIR_CCW, 	R_START_1, 			R_START_2, 				R_START_3 | DIR_CW},
};

// Use the half-step state table (emits a code at 00 and 11)
#define RFS_START          0x00     //!< Rotary full step start
#define RFS_CW_FINAL       0x01     //!< Rotary full step clock wise final
#define RFS_CW_BEGIN       0x02     //!< Rotary full step clock begin
#define RFS_CW_NEXT        0x03     //!< Rotary full step clock next
#define RFS_CCW_BEGIN      0x04     //!< Rotary full step counter clockwise begin
#define RFS_CCW_FINAL      0x05     //!< Rotary full step counter clockwise final
#define RFS_CCW_NEXT       0x06     //!< Rotary full step counter clockwise next
#define RFS_CW_TRANS       0x07     //!< Rotary full step counter clockwise next
#define RFS_CCW_TRANS      0x08     //!< Rotary full step counter clockwise next

static const PROGMEM uint8_t halfStepTable[7][4] = {
     // RFS_START
     {RFS_START,      RFS_CW_BEGIN,  RFS_CCW_BEGIN, RFS_START},
     // RFS_CW_FINAL
     {RFS_CW_NEXT,    RFS_START,     RFS_CW_FINAL,  RFS_START | DIR_CW},
     // RFS_CW_BEGIN
     {RFS_CW_NEXT,    RFS_CW_BEGIN,  RFS_START,     RFS_START},
     // RFS_CW_NEXT
     {RFS_CW_NEXT,    RFS_CW_BEGIN,  RFS_CW_FINAL | DIR_CW,  RFS_START},
     // RFS_CCW_BEGIN
     {RFS_CCW_NEXT,   RFS_START,     RFS_CCW_BEGIN, RFS_START},
     // RFS_CCW_FINAL
     {RFS_CCW_NEXT,   RFS_CCW_FINAL, RFS_START,     RFS_START | DIR_CCW},
     // RFS_CCW_NEXT
     {RFS_CCW_NEXT,   RFS_CCW_FINAL | DIR_CCW, RFS_CCW_BEGIN, RFS_START},
};

// Use the full-step state table (emits a code at 11)
static const PROGMEM uint8_t fullStepTable[7][4] = {
    // RFS_START
    {RFS_START,      RFS_CW_BEGIN,  RFS_CCW_BEGIN, RFS_START},
    // RFS_CW_FINAL
    {RFS_CW_NEXT,    RFS_START,     RFS_CW_FINAL,  RFS_START | DIR_CW},
    // RFS_CW_BEGIN
    {RFS_CW_NEXT,    RFS_CW_BEGIN,  RFS_START,     RFS_START},
    // RFS_CW_NEXT
    {RFS_CW_NEXT,    RFS_CW_BEGIN,  RFS_CW_FINAL,  RFS_START},
    // RFS_CCW_BEGIN
    {RFS_CCW_NEXT,   RFS_START,     RFS_CCW_BEGIN, RFS_START},
    // RFS_CCW_FINAL
    {RFS_CCW_NEXT,   RFS_CCW_FINAL, RFS_START,     RFS_START | DIR_CCW},
    // RFS_CCW_NEXT
    {RFS_CCW_NEXT,   RFS_CCW_FINAL, RFS_CCW_BEGIN, RFS_START},
};


 /*
 *   Position   Bit1   Bit2
 *   ----------------------
 *     Step1     0      0
 *      1/4      1      0
 *      1/2      1      1
 *      3/4      0      1
 *     Step2     0      0
 */

class EncoderInputs{

public:
	// Data that changes with bank, and encoder
	typedef struct __attribute__((packed)){
		int16_t encoderValue;		// Encoder value 0-127 or 0-16383 (Needs to be int for out of range check against 0)
		int16_t encoderValue2cc;		// Encoder value 0-127 for second CC
		int16_t encoderShiftValue;		// Encoder value 0-127 or 0-16383 (Needs to be int for out of range check against 0)
		uint16_t switchLastValue : 14;		
		uint16_t switchInputState : 1;		// Logic state of the input (could match the HW state, or not)
		uint16_t switchInputStatePrev : 1;
		uint8_t pulseCounter : 4;		// Amount of encoder state changes
		uint8_t shiftRotaryAction : 1;
		uint8_t encFineAdjust : 1;
		uint8_t doubleCC : 1;
		uint8_t buttonSensitivityControlOn : 1;
	}encoderBankData;	// 9 bytes

	void Init(uint8_t,uint8_t, SPIAdressableBUS *);
	void Read();
	void SwitchAction(uint8_t, uint8_t, int8_t, bool initDump = false);
	void SendRotaryMessage(uint8_t, uint8_t, bool initDump = false);
	void SendRotaryAltMessage(uint8_t, uint8_t, bool initDump = false);
	void SetBankForEncoders(uint8_t);
	void SetEncoderValue(uint8_t bank, uint8_t encNo, uint16_t value);
	void SetEncoderShiftValue(uint8_t, uint8_t, uint16_t);
	void SetEncoder2cc(uint8_t, uint8_t, uint16_t);
	void SetEncoderSwitchValue(uint8_t, uint8_t, uint16_t);
	void SetProgramChange(uint8_t,uint8_t,uint8_t);
	void RefreshData(uint8_t, uint8_t);
	uint8_t GetModuleOrientation(uint8_t);
	uint8_t GetModuleType(uint8_t);
	uint8_t GetThisEncoderBank(uint8_t);
	uint8_t GetEncoderBrightness(uint8_t);
	uint16_t GetEncoderValue(uint8_t);
	uint16_t GetEncoderValue2(uint8_t);
	uint16_t GetEncoderShiftValue(uint8_t);
	uint16_t GetEncoderSwitchValue(uint8_t);
	EncoderInputs::encoderBankData* GetCurrentEncoderStateData(uint8_t bank, uint8_t encNo);
	bool EncoderShiftedBufferMatch(uint16_t);
	bool GetEncoderSwitchState(uint8_t);
	bool IsShiftActionOn(uint8_t);
	bool IsDoubleCC(uint8_t);
	bool IsFineAdj(uint8_t);
	bool IsBankShifted(uint8_t);
	bool EncodersInMotion(void);

private:
	uint8_t nBanks;
	uint8_t nEncoders;
	uint8_t nModules;
	bool begun;

	// pointers to modules objects
	void **encodersModule;

	uint8_t priorityCount = 0;				// Amount of modules in priority list
  	uint8_t priorityList[2] = {0};			// Priority list: 1 or 2 modules to be read at a time, when changing
  	unsigned long priorityTime = 0;		// Timer for priority
	
	uint8_t currentProgram[2][16];  // Program change # for each port (USB and HW) and channel

	typedef struct __attribute__((packed)){
		uint16_t state;				// 16 bits - each is the state of one of the MCP digital pins
  		uint16_t statePrev;			// 16 bits - each is the previous state of one of the MCP digital pins	
  		uint8_t orientation:1;
  		uint8_t detent:1;
  		uint8_t unused1:6;
	}moduleData;				//5 bytes
	moduleData* encMData;	

	encoderBankData** eBankData;

	// HW Data and per encoder data
	typedef struct __attribute__((packed)){
		uint32_t millisUpdatePrev;		// Millis of last encoder change (accel calc)
		int16_t encoderValuePrev;		// Previous encoder value
		uint8_t currentSpeed : 4;     	// Speed the encoder moves at
		uint8_t nextJump : 4;      		// Speed the encoder moves at
		uint8_t thisEncoderBank;		// Bank for this encoder. Might be different to the rest.
		uint8_t encoderValuePrev2cc;	// Previous encoder value

		uint8_t a : 1;
		uint8_t b : 1;
		uint8_t switchHWState : 1;			// Logic state of the button
		uint8_t switchHWStatePrev : 1;		// Previous logic state of the button
		int8_t encoderDirection : 2;		// +1 or -1
		int8_t encoderDirectionPrev : 2;		// +1 or -1

		uint32_t lastSwitchBounce : 28;
		uint32_t clickCount : 2;
		uint32_t changed : 1;
		uint32_t debounceSwitchPressed : 1;

		uint8_t encoderState : 6;			// Logic state of encoder inputs
		uint8_t bankShifted : 1;
	    uint8_t encoderChange : 1;        	// Goes true when a change in the encoder state is detected
		uint8_t statesAcc;
	}encoderData;			//16 bytes
	encoderData* eHwData;

	// CLASS METHODS
	void InitRotaryModule(SPIAdressableBUS*,uint8_t);
	void SwitchCheck(uint8_t, uint8_t);
	void EncoderAction(uint8_t, uint8_t);
	void AddToPriority(uint8_t);
	void SetFeedback(uint8_t, uint8_t, uint8_t, uint8_t);
	void FilterClear(uint8_t);
	void ReadModule(uint8_t);
	bool ModuleHasActivity(uint8_t);
	bool RotaryHasConfiguration(uint8_t);
	int DecodeRotaryDirection(uint8_t, uint8_t);
	int16_t FilterGetNewAverage(uint8_t, uint16_t);
};


#endif
