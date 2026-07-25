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

#ifndef FEEDBACK_CLASS_H
#define FEEDBACK_CLASS_H

#include "modules.h"
#include "Defines.h"
#include <Adafruit_NeoPixel.h>

// Each rotary feedback mode has a horizontal and a vertical progression

const unsigned int PROGMEM walk[2][SPOT_SIZE] = {{0x0000, 0x0002, 0x0006, 0x0004, 0x000C, 0x0008, 0x0018, 
			                                      0x0010, 0x0030, 0x0020, 0x0060, 0x0040, 0x00C0, 0x0080, 
			                                      0x0180, 0x0100, 0x0300, 0x0200, 0x0600, 0x0400, 0x0C00, 
			                                      0x0800, 0x1800, 0x1000, 0x3000, 0x2000},
			                                     {0x0000, 0x2000, 0x6000, 0x4000, 0xC000, 0x8000, 0x8001,
			                                      0x0001, 0x0003, 0x0002, 0x0006, 0x0004, 0x000C, 0x0008,
			                                      0x0018, 0x0010, 0x0030, 0x0020, 0x0060, 0x0040, 0x00C0,
			                                      0x0080, 0x0180, 0x0100, 0x0300, 0x0200}};

const unsigned int PROGMEM simpleSpot[2][S_SPOT_SIZE] = {{0x0000, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 
														  0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000},
					                                     {0x0000, 0x2000, 0x4000, 0x8000, 0x0001, 0x0002, 0x0004,
					                                      0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200}};
const unsigned int PROGMEM simpleSpotInv[2][S_SPOT_SIZE] = {{0x0000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 
															 0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002},
						                                    {0x0000, 0x0200, 0x0100, 0x0080, 0x0040, 0x0020, 0x0010, 
						                                     0x0008, 0x0004, 0x0002, 0x0001, 0x8000, 0x4000, 0x2000}};
	                              
const unsigned int PROGMEM fill[2][FILL_SIZE] = {{0x0000, 0x0002, 0x0006, 0x000E, 0x001E, 0x003E, 0x007E, 
			                                      0x00FE, 0x01FE, 0x03FE, 0x07FE, 0x0FFE, 0x1FFE, 0x3FFE},
			                                     {0x0000, 0x2000, 0x6000, 0xE000, 0xE001, 0xE003, 0xE007, 
			                                      0xE00F, 0xE01F, 0xE03F, 0xE07F, 0xE0FF, 0xE1FF, 0xE3FF}};

const unsigned int PROGMEM fillInv[2][FILL_SIZE] = {{ 0x0000, 0x2000, 0x3000, 0x3800, 0x3C00, 0x3E00, 0x3F00, 
				                                      0x3F80, 0x3FC0, 0x3FE0, 0x3FF0, 0x3FF8, 0x3FFC, 0x3FFE},
				                                     {0x0000, 0x0200, 0x0300, 0x0380, 0x03C0, 0x03E0, 0x03F0, 
				                                      0x03F8, 0x03FC, 0x03FE, 0x03FF, 0x83FF, 0xC3FF, 0xE3FF}};

const unsigned int PROGMEM pivot[2][PIVOT_SIZE] =     {{0x00FE, 0x00FC, 0x00F8, 0x00F0, 0x00E0, 0x00C0, 0x0080, 
			                                      0x0080, 0x0180, 0x0380, 0x0780, 0x0F80, 0x1F80, 0x3F80},
			                                     {0xE00F, 0xC00F, 0x800F, 0x000F, 0x000E, 0x000C, 0x0008, 
			                                      0x0008, 0x0018, 0x0038, 0x0078, 0x00F8, 0x01F8, 0x03F8}};

const unsigned int PROGMEM pivotInv[2][PIVOT_SIZE] =     {{ 0x00FE, 0x00FC, 0x00F8, 0x00F0, 0x00E0, 0x00C0, 0x0080, 
			                    	                  0x0080, 0x0180, 0x0380, 0x0780, 0x0F80, 0x1F80, 0x3F80},
				                                     {0xE00F, 0xC00F, 0x800F, 0x000F, 0x000E, 0x000C, 0x0008, 
				                                      0x0008, 0x0018, 0x0038, 0x0078, 0x00F8, 0x01F8, 0x03F8}};
                              
const unsigned int PROGMEM spread[2][MIRROR_SIZE] =   {{0x0000, 0x0080, 0x00C0, 0x01C0, 0x01E0, 0x03E0, 0x03F0,
				                                        0x07F0, 0x07F8, 0x0FF8, 0x0FFC, 0x1FFC, 0x1FFE, 0x3FFE},
				                                       {0x0000, 0x0008, 0x000C, 0x001C, 0x001E, 0x003E, 0x003F,
				                                        0x007F, 0x807F, 0x80FF, 0xC0FF, 0xC1FF, 0xE1FF, 0xE3FF}};


const uint8_t PROGMEM gamma8[] = {		// From adafruit NeoPixel library
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		// 0
    0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,		// 16
    1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,		// 32
    3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  7,		// 48
    7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 10, 11, 11, 11, 12, 12,		// 64
   13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,		// 80
   20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,		// 96
   30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,		// 112
   42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,		// 128
   58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,		// 144
   76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,		// 160
   97, 99,100,102,103,105,106,108,109,111,112,114,115,117,119,120,		// 176
  122,124,125,127,129,130,132,134,136,137,139,141,143,145,146,148,		// 192
  150,152,154,156,158,160,162,164,166,168,170,172,174,176,178,180,		// 208
  182,184,186,188,191,193,195,197,199,202,204,206,209,211,213,215,		// 224
  218,220,223,225,227,230,232,235,237,240,242,245,247,250,252,255 };	// 240

typedef enum FeedbackFrame{
	FeedbackFrame_Type, FeedbackFrame_nRing, FeedbackFrame_Orientation, FeedbackFrame_RingStateH, FeedbackFrame_RingStateL, 
	FeedbackFrame_R, FeedbackFrame_G, FeedbackFrame_B, 
	FeedbackFrame_Size,
	FeedbackFrame_nDigital = FeedbackFrame_nRing, FeedbackFrame_DigitalState = FeedbackFrame_RingStateH
};

#define READ_INDEX		0
#define WRITE_INDEX 	1


class FeedbackClass{

public:
	typedef struct __attribute__((packed)){
		uint16_t encRingState;  //The LED output is based on a scaled veryson of the rotary encoder counter
		uint16_t encRingStatePrev;  //The LED output is based on a scaled veryson of the rotary encoder counter
		// uint16_t encRingState2;  //The LED output is based on a scaled veryson of the rotary encoder counter
		// uint16_t encRingStatePrev2;  //The LED output is based on a scaled veryson of the rotary encoder counter
		uint8_t vumeterValue;
		uint8_t colorIndexSwitch;
		uint8_t colorIndexRotary;
		uint8_t rotIntensityFactor;
		uint8_t swIntensityFactor;
	}encFeedbackData;

	typedef struct __attribute__((packed)){
		// uint16_t digitalFbValue; 
		uint8_t colorIndexPrev;
		uint8_t digIntensityFactor;
		// uint8_t unused;
	}digFeedbackData;

	uint16_t fbItemsToSend;	

	void Init(uint8_t, uint8_t, uint16_t, uint16_t);
	void InitFb();
	bool InitAuxController(bool);
	void RecoverAuxControllerReset();
	bool IsBegun();
	bool AuxRecoveryInProgress();
	void Update();
	void SetChangeEncoderFeedback(uint8_t, uint8_t, uint16_t, uint8_t, bool, bool, bool colorSwitchMsg = false, bool valToIntensity = false, bool externalFeedback = false);
	void SetChangeDigitalFeedback(uint16_t, uint16_t, bool, bool, bool, bool externalFeedback = false, bool valToIntensity = false);
	void SetChangeIndependentFeedback(uint8_t, uint16_t, uint16_t, bool, bool externalFeedback = false);
	void SetDigitalLedColorDirect(uint16_t digitalIndex, uint8_t r, uint8_t g, uint8_t b);
	void SetEncoderSwitchLedColorDirect(uint8_t encIndex, uint8_t r, uint8_t g, uint8_t b);
	void SetEncoderRingLedColorDirect(uint8_t encIndex, uint8_t r, uint8_t g, uint8_t b);
	void SetBankChangeFeedback(uint8_t);
	uint8_t GetVumeterValue(uint8_t);
	void SendCommand(uint8_t);
	void SendResetToBootloader();
	void *GetEncoderFBPtr();
	FeedbackClass::encFeedbackData* GetCurrentEncoderFeedbackData(uint8_t bank, uint8_t encNo);
	FeedbackClass::digFeedbackData* GetCurrentDigitalFeedbackData(uint8_t bank, uint8_t digNo);

private:
	typedef struct  __attribute__((packed)){
		uint8_t type;
		uint8_t indexChanged;		// MAX INDEX 255 -> 256 DIGITALS
		uint16_t newValue;
		uint8_t newOrientation : 1;
		uint8_t isShifter : 1;
		uint8_t updatingBank : 1;
		uint8_t rotaryValueToColor : 1;
		uint8_t valueToIntensity : 1;
		uint8_t unused : 3;
	}feedbackUpdateStruct;

	void AddCheckSum();
	bool SendFeedbackData();
	int8_t SendDataIfReady();
	void FillFrameWithEncoderData(byte);
	void FillFrameWithDigitalData(byte);
	int8_t ProcessQueuedFeedbackEntry(uint8_t);
	bool IsWireFeedbackType(uint8_t);
	void SendAuxInitializationFrame();
	bool WaitForAuxAck(uint32_t, bool);
	void PrepareAuxAckWait(bool);
	bool SendAuxInitializationWithRetry(uint8_t, uint32_t);
	bool SendAuxCommandWithRetry(uint8_t, int16_t);
	void SetShifterFeedback();
	void WaitForMIDI(bool);
	uint8_t NextFeedbackIndex(uint8_t);
	void IncreaseBufferIndex(bool);
	void QueueFeedbackUpdate(uint8_t, uint8_t, uint16_t, uint8_t, bool, bool, bool, bool, bool);
	bool IsCoalescableType(uint8_t);
	int16_t FindPendingUpdate(uint8_t, uint8_t, bool);
	void ClearCoalesceIndex();
	void RebuildCoalesceIndex();
	void RegisterCoalesceSlot(uint8_t);
	void UnregisterCoalesceSlot(uint8_t);
	int16_t FindOldestReplaceableUnsentSlot();
	int16_t FindNewestReplaceableUnsentSlot();
	int16_t EncoderCoalesceTypeIndex(uint8_t);
	int16_t DigitalCoalesceTypeIndex(uint8_t);
	int16_t BankCoalesceTypeIndex(uint8_t);
	uint16_t* CoalesceSlotPtr(uint8_t, uint8_t, bool);
	

	uint8_t nBanks;
	uint8_t nEncoders;
	uint16_t nDigitals;
	uint16_t nIndependent;

	bool begun;
	
	volatile bool feedbackDataToSend;
	uint8_t fbMessagesSent;
	uint16_t burstEntriesProcessed;
	uint16_t burstEntriesBeforeFirstFrame;
	uint16_t burstFrameEntryCounts[MSG_BUFFER_AUX];
	uint8_t burstRetryCount;
	uint16_t burstItemsRemaining;
	uint8_t burstSendIdx;
	bool burstInProgress;
	bool burstAwaitingAck;
	bool auxInitRecoveryInProgress;
	bool auxInitWaitingForBoot;
	uint8_t auxInitRetryCount;
	uint8_t auxInitResetCount;
	uint32_t antMicrosAuxInit;
	bool updatingBankFeedback;
	bool bankControlSlotReserved;
	
	feedbackUpdateStruct feedbackUpdateBuffer[FEEDBACK_UPDATE_BUFFER_SIZE];
	uint8_t feedbackUpdateReadIdx;
	uint8_t feedbackUpdateWriteIdx;
	uint16_t *encoderCoalesceSlots;
	uint16_t *digitalCoalesceSlots;
	uint16_t bankCoalesceSlots[3];
	

	uint8_t feedbackFrameBuffer[FeedbackFrame_Size];
 	
 	bool waitingMoreData;
    uint32_t antMillisWaitMoreData;

	encFeedbackData** encFbData;
	digFeedbackData** digFbData;
		                            

	/*This is a 2 dimensional array with 3 LED sequences.  The outer array is the sequence;
	  the inner arrays are the values to output at each step of each sequence.  The output values
	  are 16 bit hex values (hex math is actually easier here!).  An LED will be on if its
	  corresponding binary bit is a one, for example: 0x7 = 0000000000000111 and the first 3 LEDs
	  will be on.

	  The data type must be 'unsigned int' if the sequence uses the bottom LED since it's value is 0x8000 (out of range for signed int).
	*/



};

#endif
