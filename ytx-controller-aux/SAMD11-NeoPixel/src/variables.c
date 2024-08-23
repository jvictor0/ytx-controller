/*
 * CFile1.c
 *
 * Created: 22/8/2024 16:20:06
 *  Author: Ingenieria
 */ 
#include "types.h"
#include "defines.h"





volatile uint8_t receivedBytes = 0;
volatile bool rcvdInitValues = false;
volatile bool receivingInit = false;
volatile bool receivingBrightness = false;
volatile bool updateBank = false;

volatile uint8_t readIdx = 0;
volatile uint8_t writeIdx = 0;
volatile bool turnAllOffFlag = false;
volatile bool turnAllOnFlag = false;
volatile bool rainbowStart = false;
volatile bool changeBrightnessFlag = false;
volatile bool receivingFeedbackData = false;
volatile bool receivingBank = false;
volatile bool showNow = false;
volatile bool timeToShow = false;
volatile bool sendShowEnd = false;




