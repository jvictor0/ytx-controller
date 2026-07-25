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

#include "types.h"
#include "defines.h"

volatile uint8_t receivedBytes = 0;
volatile bool rcvdInitValues = false;
volatile bool auxReady = false;
volatile bool receivingInit = false;
volatile bool receivingBrightness = false;

volatile uint8_t readIdx = 0;
volatile uint8_t writeIdx = 0;
volatile uint16_t feedbackFramesPending = 0;
volatile uint8_t currentBrightness = 255;
volatile bool turnAllOffFlag = false;
volatile bool turnAllOnFlag = false;
volatile bool rainbowStart = false;
volatile bool changeBrightnessFlag = false;
volatile bool receivingFeedbackData = false;
volatile bool receivingBank = false;
volatile bool discardingBurst = false;
volatile uint8_t burstFrameIndex = 0;
volatile bool receivingBurstEndCount = false;
volatile uint8_t burstEndCountBytes = 0;
volatile uint8_t burstEndCountFirst = 0;
volatile bool feedbackAllocationFailed = false;
volatile bool showNow = false;
volatile bool timeToShow = false;
volatile bool sendShowEnd = false;
volatile uint32_t lastReceiveMillis = 0;
volatile uint32_t sercomReceiveErrorCount = 0;
volatile uint32_t sercomBufferOverflowCount = 0;
volatile uint8_t ReceptionBuffer[FeedbackFrame_Size+CHECKSUM_BYTES+1];
volatile FeedbackFrameData FeedbackFramesBuffer[FEEDBACK_BUFFER_LENGTH];
