/*
 * CFile1.c
 *
 * Created: 22/8/2024 16:20:06
 *  Author: Ingenieria
 */ 
#include "types.h"
#include "defines.h"

uint8_t numStripsOn = 0;
uint8_t numEncoders = 0;
uint8_t numDigitals1 = 0;
uint8_t numDigitals2 = 0;
uint8_t numAnalogFb = 0;
uint8_t currentBrightness = 0;

bool receivingMIDI = false;
uint32_t failsPerSecond = 0;
uint32_t framesPerSecond = 0;

volatile uint8_t tickShow = LED_SHOW_TICKS;
volatile uint8_t tickShowEnd = SHOW_END_REFRESH_TICKS;

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

uint16_t indexChanged = 0;
uint8_t whichStripToShow = 0;

