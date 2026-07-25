/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2024 - Yaeltex

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

#ifndef VARIABLES_H_
#define VARIABLES_H_

#include "types.h"
#include "defines.h"

extern volatile uint8_t receivedBytes;
extern volatile bool rcvdInitValues;
extern volatile bool auxReady;
extern volatile bool receivingInit;
extern volatile bool receivingBrightness;

extern volatile uint8_t readIdx;
extern volatile uint8_t writeIdx;
extern volatile uint16_t feedbackFramesPending;
extern volatile uint8_t currentBrightness;
extern volatile bool turnAllOffFlag;
extern volatile bool turnAllOnFlag;
extern volatile bool rainbowStart;
extern volatile bool changeBrightnessFlag;
extern volatile bool receivingFeedbackData;
extern volatile bool receivingBank;
extern volatile bool discardingBurst;
extern volatile uint8_t burstFrameIndex;
extern volatile bool receivingBurstEndCount;
extern volatile uint8_t burstEndCountBytes;
extern volatile uint8_t burstEndCountFirst;
extern volatile bool burstQueueOverflowed;
extern volatile bool feedbackAllocationFailed;
extern volatile bool showNow;
extern volatile bool timeToShow;
extern volatile bool sendShowEnd;
extern volatile uint32_t lastReceiveMillis;
extern volatile uint32_t sercomReceiveErrorCount;
extern volatile uint32_t sercomBufferOverflowCount;

extern volatile uint8_t ReceptionBuffer[FeedbackFrame_Size+CHECKSUM_BYTES+1];
extern volatile FeedbackFrameData FeedbackFramesBuffer[FEEDBACK_BUFFER_LENGTH];


#endif /* VARIABLES_H_ */
