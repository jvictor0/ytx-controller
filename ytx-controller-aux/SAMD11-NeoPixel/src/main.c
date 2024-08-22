// References
// https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf
// SAM D21 SERCOM SPI Configuration
// http://ww1.microchip.com/downloads/en/AppNotes/00002465A.pdf
// https://github.com/jeelabs/embello/blob/master/explore/1450-dips/leds/main.cpp
// https://github.com/rogerclarkmelbourne/WS2812B_STM32_Libmaple
// https://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/

#include <asf.h>
#include <NeoPixels.h>

#include "utils.h"
#include "types.h"
#include "defines.h"
#include "variables.h"

#include "setup.h"
#include "feedback.h"
#include "main-controller-comms.h"

volatile uint8_t ReceptionBuffer[FeedbackFrame_Size+CHECKSUM_BYTES+1];
volatile FeedbackFrameData FeedbackFramesBuffer[FEEDBACK_BUFFER_LENGTH];

int main (void)
{
	setup();
	
	for(int i = 0; i < 2; i++){
		port_pin_set_output_level(LED_YTX_PIN, LED_0_ACTIVE);
		delay(100);
		port_pin_set_output_level(LED_YTX_PIN, LED_0_INACTIVE);
		delay(100);	
	}
	
	
	port_pin_set_output_level(LED_YTX_PIN, LED_0_ACTIVE);
	
	while(!rcvdInitValues);

	port_pin_set_output_level(LED_YTX_PIN, LED_0_INACTIVE);

	numEncoders = ReceptionBuffer[nEncoders];
	numDigitals1 = ReceptionBuffer[nDigitals1];
	numDigitals2 = ReceptionBuffer[nDigitals2];
	numAnalogFb = ReceptionBuffer[nAnalog];
	currentBrightness = ReceptionBuffer[nBrightness];
	
	bool rainbowOn = ReceptionBuffer[nRainbow];

	feedbackBegin(rainbowOn);	

	delay_ms(50);
	
	SendToMain(END_OF_RAINBOW);

	while (1) {		
		while(readIdx != writeIdx){ // If there is data to update
			// Update LEDs based on 
			feedbackDataUpdate(	FeedbackFramesBuffer[readIdx].updateFrame,
						FeedbackFramesBuffer[readIdx].updateN,
						//FeedbackFramesBuffer[readIdx].updateValue,
						//FeedbackFramesBuffer[readIdx].updateMin, 
						//FeedbackFramesBuffer[readIdx].updateMax,
						FeedbackFramesBuffer[readIdx].updateO, 
						FeedbackFramesBuffer[readIdx].updateState, 
						FeedbackFramesBuffer[readIdx].updateR,
						FeedbackFramesBuffer[readIdx].updateG,
						FeedbackFramesBuffer[readIdx].updateB	);
			
			feedbackPrepareToShow();
			
			if(++readIdx >= FEEDBACK_BUFFER_LENGTH)	
				readIdx = 0;

			showNow = true;
		}
		
		if(timeToShow){		
			if(showNow && (!receivingBank || !receivingFeedbackData)){
				showNow = false;
				SendToMain(SHOW_IN_PROGRESS);
				
				feedbackShow();
				
				SendToMain(SHOW_END);	
			}
			timeToShow = false;
		}
		
		if(changeBrightnessFlag){
			changeBrightnessFlag = false;
			for(int s = 0; s < MAX_STRIPS; s++){
				if(begun[s]){
					setBrightness(s, currentBrightness);
					pixelsShow(s);
				}
			}
		}
		
		if(sendShowEnd){
			SendToMain(SHOW_END);
			sendShowEnd = false;
		}
		
		if(turnAllOffFlag){
			turnAllOffFlag = false;
			setAll(NP_OFF,NP_OFF,NP_OFF);
			showAll();
		}
		if(turnAllOnFlag){
			turnAllOnFlag = false;
			setAll(NP_ON*2, NP_OFF, NP_OFF);
			showAll();
			delay(1500);
			setAll(NP_OFF, NP_ON*2, NP_OFF);
			showAll();
			delay(1500);
			setAll(NP_OFF, NP_OFF, NP_ON*2);
			showAll();
			delay(1500);
			setAll(NP_ON, NP_ON, NP_ON);
			showAll();
		}
		if(rainbowStart){
			rainbowStart = false;
			uint16_t totalLEDs = 8*(numEncoders + (numDigitals1 + numDigitals2)/2);
			
			uint16_t wait = 0;
			if(totalLEDs < 128){
				wait = 512/totalLEDs;
			}else if(totalLEDs >= 128 && totalLEDs < 256){
				wait = 1024/totalLEDs;
			}else{
				wait = 1400/totalLEDs;
			}
			rainbowAll(wait);
		}
	}
}
