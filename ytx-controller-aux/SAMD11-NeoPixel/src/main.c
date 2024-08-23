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

extern uint32_t millis();

uint32_t antMillisShowBegin;
uint32_t antMillisShowEnd;

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

	feedbackBegin();	

	port_pin_set_output_level(LED_YTX_PIN, LED_0_INACTIVE);

	delay_ms(50);
	
	SendToMain(END_OF_RAINBOW);

	antMillisShowBegin = millis();
	antMillisShowEnd = millis();

	while (1) {		
		if(feedbackDataAvailable()){
			feedbackDataUpdate();
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
			feedbackRainbow();
		}

		if(millis()-antMillisShowBegin > LED_SHOW_TICKS){
			antMillisShowBegin = millis();
			timeToShow = true;
		}
		if(millis()-antMillisShowEnd > SHOW_END_REFRESH_TICKS){
			antMillisShowEnd = millis();

			if(!receivingFeedbackData && !receivingBank && !timeToShow)
				sendShowEnd = true;
		}
	}
}
