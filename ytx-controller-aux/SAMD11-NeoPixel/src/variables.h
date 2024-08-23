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