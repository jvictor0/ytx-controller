/*
 * variables.h
 *
 * Created: 22/8/2024 16:20:47
 *  Author: Ingenieria
 */ 


#ifndef VARIABLES_H_
#define VARIABLES_H_

#include "types.h"
#include "defines.h"

extern uint8_t numStripsOn;
extern uint8_t numEncoders;
extern uint8_t numDigitals1;
extern uint8_t numDigitals2;
extern uint8_t numAnalogFb;
extern uint8_t currentBrightness;

extern uint32_t failsPerSecond;
extern uint32_t framesPerSecond;

extern uint16_t indexChanged;
extern uint8_t whichStripToShow;

extern volatile uint8_t tickShow;
extern volatile uint8_t tickShowEnd;

extern volatile uint8_t receivedBytes;
extern volatile bool rcvdInitValues;
extern volatile bool receivingInit;
extern volatile bool receivingBrightness;
extern volatile bool updateBank;

extern volatile uint8_t readIdx;
extern volatile uint8_t writeIdx;
extern volatile bool turnAllOffFlag;
extern volatile bool turnAllOnFlag;
extern volatile bool rainbowStart;
extern volatile bool changeBrightnessFlag;
extern volatile bool receivingFeedbackData;
extern volatile bool receivingBank;
extern volatile bool showNow;
extern volatile bool timeToShow;
extern volatile bool sendShowEnd;


#endif /* VARIABLES_H_ */