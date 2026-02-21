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

//TODO : add encoders orientation data on initial frame, add checksum and error control(unify with NEW_FRAME_BYTE/END_OF_FRAME_BYTE mechanism)
//TODO : reduce frames data to minimal bitstruct form(implement encoder rings on 13bits?)
//TODO : implement reduce frame to digitals?
//TODO : unify digital1 and digital2 frames?

#include <NeoPixels.h>
#include "variables.h"
#include "feedback.h"

extern uint8_t ReceptionBuffer[FeedbackFrame_Size+CHECKSUM_BYTES+1];

volatile FeedbackFrameData FeedbackFramesBuffer[FEEDBACK_BUFFER_LENGTH];

uint8_t numStripsOn = 0;
uint8_t numEncoders = 0;
uint8_t numDigitals1 = 0;
uint8_t numDigitals2 = 0;

uint8_t whichStripToShow = 0;

uint16_t indexChanged = 0;

void feedbackBegin(){

	while(!rcvdInitValues);

	numEncoders = ReceptionBuffer[nEncoders];
	numDigitals1 = ReceptionBuffer[nDigitals1];
	numDigitals2 = ReceptionBuffer[nDigitals2];

		if(numEncoders){
		if(numEncoders>16){
			pixelsBegin(ENCODER1_STRIP, NUM_LEDS_ENCODER*16, ENC1_STRIP_PIN, NEO_GRB + NEO_KHZ800);
			pixelsBegin(ENCODER2_STRIP, NUM_LEDS_ENCODER*(numEncoders-16), ENC2_STRIP_PIN, NEO_GRB + NEO_KHZ800);
		}else{
			pixelsBegin(ENCODER1_STRIP, NUM_LEDS_ENCODER*numEncoders, ENC1_STRIP_PIN, NEO_GRB + NEO_KHZ800);
		}
	}
	if(numDigitals1){
		pixelsBegin(DIGITAL1_STRIP, numDigitals1, DIG1_STRIP_PIN, NEO_GRB + NEO_KHZ800);
	}
	if(numDigitals2){
		pixelsBegin(DIGITAL2_STRIP, numDigitals2, DIG2_STRIP_PIN, NEO_GRB + NEO_KHZ800);
	}
	
	setAll(NP_OFF,NP_OFF,NP_OFF);
	showAll();
}

void feedbackShow(){
	for (int i = 0; i < LED_STRIP_COUNT; i++){
		if(whichStripToShow&(1<<i)){
			pixelsShow(i);
		}
	}

	whichStripToShow = 0;
}

void feedbackSetBrightness(uint8_t brightness){
	for (int i=0; i < LED_STRIP_COUNT; i++){
		setBrightness(i, brightness);
	}
}

void feedbackRainbow(){
	uint16_t totalLEDs = 8*(numEncoders + (numDigitals1 + numDigitals2)/2);

	if(totalLEDs>0){	
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




bool feedbackDataAvailable(){
	return (readIdx != writeIdx);
}

void feedbackDataUpdate()
{
	while(readIdx != writeIdx){ // If there is data to update

			uint8_t frame = FeedbackFramesBuffer[readIdx].updateFrame;
			uint8_t elementToChange = FeedbackFramesBuffer[readIdx].updateN;
			uint8_t orientationMeta = FeedbackFramesBuffer[readIdx].updateO;
			bool isBlendFrame = (frame == ENCODER_BLEND_FRAME);
			bool vertical = isBlendFrame ? (orientationMeta & 0x01) : orientationMeta;
			uint16_t newState = FeedbackFramesBuffer[readIdx].updateState;
			uint8_t intR = FeedbackFramesBuffer[readIdx].updateR;
			uint8_t intG = FeedbackFramesBuffer[readIdx].updateG;
			uint8_t intB = FeedbackFramesBuffer[readIdx].updateB;
							
		//uint8_t brightnessMult = 1;
		//uint8_t minMaxDif = abs(max-min);
		int8_t lastLedOn = 0;
		
			if(frame == ENCODER_CHANGE_FRAME || frame == ENCODER_BLEND_FRAME){		// ROTARY CHANGE
				bool ledOnOrOff = false;
				bool ledForSwitch = false;
				int8_t lowerRingBit = -1;
				int8_t upperRingBit = -1;
				uint8_t secondaryWeight = 0;
				bool secondaryIsLowerBit = false;

				if(isBlendFrame){
					secondaryWeight = (orientationMeta >> 2) & 0x3F;
					secondaryIsLowerBit = (orientationMeta & 0x02) ? true : false;

					// Find the two active ring LEDs (if present) to apply weighted blend.
					for(int i = 0; i < 16; i++){
						ledOnOrOff = newState&(1<<i);
						if(vertical){
							ledOnOrOff &= ((ENCODER_MASK_V>>i)&1);
							ledForSwitch = ((ENCODER_SWITCH_V_ON>>i)&1);
						}else{
							ledOnOrOff &= ((ENCODER_MASK_H>>i)&1);
							ledForSwitch = ((ENCODER_SWITCH_H_ON>>i)&1);
						}
						if(ledOnOrOff && !ledForSwitch){
							if(lowerRingBit < 0){
								lowerRingBit = i;
							}else{
								upperRingBit = i;
								break;
							}
						}
					}
				}
				
				for (int i = 0; i < 16; i++) {
					ledOnOrOff = newState&(1<<i);		// Get LED state
				
				if(vertical){									// Encoder is vertical
					ledOnOrOff &= ((ENCODER_MASK_V>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_V_ON>>i)&1);	// is it a switch LED or a ring LED
				}
				else{											// Encoder is horizontal
					ledOnOrOff &= ((ENCODER_MASK_H>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_H_ON>>i)&1);	// is it a switch LED or a ring LED
				}
					
					if (ledOnOrOff && !ledForSwitch) {				// If LED is for ring, and its state is ON
						uint8_t outR = intR;
						uint8_t outG = intG;
						uint8_t outB = intB;

						if(isBlendFrame && lowerRingBit >= 0 && upperRingBit >= 0){
							bool thisIsSecondary = false;
							if(i == lowerRingBit){
								thisIsSecondary = secondaryIsLowerBit;
							}else if(i == upperRingBit){
								thisIsSecondary = !secondaryIsLowerBit;
							}

							uint8_t ledWeight = thisIsSecondary ? secondaryWeight : (63 - secondaryWeight);
							outR = (uint8_t)(((uint16_t)intR * ledWeight) / 63U);
							outG = (uint8_t)(((uint16_t)intG * ledWeight) / 63U);
							outB = (uint8_t)(((uint16_t)intB * ledWeight) / 63U);
						}

						if(elementToChange < N_ENCODERS_STRIP_1){					// Is it an encoder on the first strip or second?
							setPixelColor(	ENCODER1_STRIP,					// N strip
							NUM_LEDS_ENCODER*elementToChange + i,	// N led
							outR, outG, outB);				// R, G, B
							
							// BASED ON FEEDBACK METHOD, calculate brightness multiplier for adjacent LEDs based on value
						// ONLY FOR FILL FEEDBACK METHOD, calculate brightness multiplier based on value
						//if(!vertical && i != 13){
						//if(minMaxDif > 48){
						//brightnessMult = (minMaxDif/13) + 1 - abs(newValue - min)%(minMaxDif/13);
						//}
						//setPixelColor(	ENCODER1_STRIP,						// N strip
						//NUM_LEDS_ENCODER*elementToChange + i + 1,	// N led +1
						//intR/brightnessMult,				// R
						//intG/brightnessMult,				// G
						//intB/brightnessMult);				// B
							//}
							}else{		// ENCODER STRIP 2
							setPixelColor(	ENCODER2_STRIP,										// N strip
							NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,// N led
							outR, outG, outB);									// R, G, B
						}
						lastLedOn = i;
						} else if(ledForSwitch){
					// IF IT IS A LED FOR THE SWITCH, DO NOTHING
					} else {											// Ring LED, state OFF
					if(elementToChange < N_ENCODERS_STRIP_1){					// ENCODER STRIP 1
						setPixelColor(	ENCODER1_STRIP,						// N strip
						NUM_LEDS_ENCODER*elementToChange + i,		// N led
						NP_OFF,	NP_OFF, NP_OFF);			// R, G, B
						//if(!vertical && i != 13 && lastLedOn >= 0){
						//if(minMaxDif > 48){
						//brightnessMult = (minMaxDif/13) + 1 - abs(newValue - min)%(minMaxDif/13);
						//}
						//setPixelColor(ENCODER1_STRIP, 16*elementToChange + lastLedOn + 1 , intR/brightnessMult, intG/brightnessMult, intB/brightnessMult); // Draw new pixel
						//}
						}else{															// ENCODER STRIP 2
						setPixelColor(	ENCODER2_STRIP,										// N strip
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,// N led
						NP_OFF, NP_OFF, NP_OFF);							// R, G, B
					}
				}
			}
			}else if(frame == ENCODER_DOUBLE_FRAME){		// ROTARY 2CC CHANGE
			bool ledOnOrOff = false;
			bool ledForSwitch = false;
			
			for (int i = 0; i < 16; i++) {
				ledOnOrOff = newState&(1<<i);		// Get LED state
				
				if(vertical){									// Encoder is vertical
					ledOnOrOff &= ((ENCODER_MASK_V>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_V_ON>>i)&1);	// is it a switch LED or a ring LED
				}
				else{											// Encoder is horizontal
					ledOnOrOff &= ((ENCODER_MASK_H>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_H_ON>>i)&1);	// is it a switch LED or a ring LED
				}
				
				if (ledOnOrOff && !ledForSwitch) {				// If LED is for ring, and its state is ON
					if(elementToChange < N_ENCODERS_STRIP_1){					// Is it an encoder on the first strip or second?
						setPixelColor(	ENCODER1_STRIP,							// N strip
						NUM_LEDS_ENCODER*elementToChange + i,			// N led
						intR, intG, intB);						// R, G, B
						}else{		// ENCODER STRIP 2
						setPixelColor(	ENCODER2_STRIP,													// N strip
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,			// N led
						intR, intG, intB);												// R, G, B
					}
					lastLedOn = i;
				}
			}
			}else if(frame == ENCODER_VUMETER_FRAME){		// ROTARY CHANGE
			bool ledOnOrOff = false;
			bool ledForSwitch = false;
			
			for (int i = 0; i < 16; i++) {
				ledOnOrOff = newState&(1<<i);		// Get LED state
				
				if(vertical){									// Encoder is vertical
					ledOnOrOff &= ((ENCODER_MASK_V>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_V_ON>>i)&1);	// is it a switch LED or a ring LED
					if((i >= 13 && i <= 15) || i >= 0 && i <= 4){
						intR = 9; intG = 88; intB = 103;
						}else if(i >= 5 && i <= 7){
						intR = 100; intG = 100; intB = 0;
						}else if(i >= 8 && i <= 9){
						intR = 200; intG = 0; intB = 0;
					}
				}
				else{											// Encoder is horizontal
					ledOnOrOff &= ((ENCODER_MASK_H>>i)&1);			// get LED state
					ledForSwitch = ((ENCODER_SWITCH_H_ON>>i)&1);	// is it a switch LED or a ring LED
					if(i >= 1 && i <= 8){
						intR = 9; intG = 88; intB = 103;
						}else if(i >= 9 && i <= 11){
						intR = 100; intG = 100; intB = 0;
						}else if(i >= 12 && i <= 13){
						intR = 200; intG = 0; intB = 0;
					}
				}
				
				if (ledOnOrOff && !ledForSwitch) {				// If LED is for ring, and its state is ON
					if(elementToChange < N_ENCODERS_STRIP_1){					// Is it an encoder on the first strip or second?
						setPixelColor(	ENCODER1_STRIP,					// N strip
						NUM_LEDS_ENCODER*elementToChange + i,	// N led
						intR, intG, intB);				// R, G, B
						}else{		// ENCODER STRIP 2
						setPixelColor(	ENCODER2_STRIP,										// N strip
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,// N led
						intR, intG, intB);									// R, G, B
					}
					lastLedOn = i;
					} else if(ledForSwitch){
					// IF IT IS A LED FOR THE SWITCH, DO NOTHING
					} else {											// Ring LED, state OFF
					if(elementToChange < N_ENCODERS_STRIP_1){					// ENCODER STRIP 1
						setPixelColor(	ENCODER1_STRIP,						// N strip
						NUM_LEDS_ENCODER*elementToChange + i,		// N led
						NP_OFF,	NP_OFF, NP_OFF);			// R, G, B
						//if(!vertical && i != 13 && lastLedOn >= 0){
						//if(minMaxDif > 48){
						//brightnessMult = (minMaxDif/13) + 1 - abs(newValue - min)%(minMaxDif/13);
						//}
						//setPixelColor(ENCODER1_STRIP, 16*elementToChange + lastLedOn + 1 , intR/brightnessMult, intG/brightnessMult, intB/brightnessMult); // Draw new pixel
						//}
						}else{															// ENCODER STRIP 2
						setPixelColor(	ENCODER2_STRIP,										// N strip
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,// N led
						NP_OFF, NP_OFF, NP_OFF);							// R, G, B
					}
				}
			}
		}
		else if(frame == ENCODER_SWITCH_CHANGE_FRAME){		// SWITCH CHANGE
			bool ledOnOrOff = 0;
			bool ledForRing = false;
			for (int i = 0; i < NUM_LEDS_ENCODER; i++) {
				ledOnOrOff = newState&(1<<i);
				if(vertical){									// Encoder is vertical
					ledOnOrOff &= ((ENCODER_SWITCH_V_ON>>i)&1);		// Get LED state masked
					ledForRing = ((ENCODER_MASK_V>>i)&1);			// Check if we are at a switch LED or a ring LED
				}
				else{											// Encoder is horizontal
					ledOnOrOff &= ((ENCODER_SWITCH_H_ON>>i)&1);		// Get LED state masked
					ledForRing = ((ENCODER_MASK_H>>i)&1);			// Check if we are at a switch LED or a ring LED
				}
				
				if (ledOnOrOff && !ledForRing) {			// If it is a SWITCH LED and it's supposed to be ON
					if(elementToChange < N_ENCODERS_STRIP_1){				// Encoder STRIP 1
						setPixelColor(	ENCODER1_STRIP,					// N strip
						NUM_LEDS_ENCODER*elementToChange + i, // N led
						intR, intG, intB);				// R, G, B
						}else{																	// Encoder STRIP 2
						setPixelColor(	ENCODER2_STRIP,											// N strip
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i,	// N LED
						intR, intG, intB);										// R, G, B
					}
					} else if(ledForRing){
					// IF IT IS A LED FOR THE RING, DO NOTHING
					} else {											// Switch LED, state OFF
					if(elementToChange < N_ENCODERS_STRIP_1){					// Encoder STRIP 1
						setPixelColor(	ENCODER1_STRIP,						// N strip
						NUM_LEDS_ENCODER*elementToChange + i,		// N led
						NP_OFF, NP_OFF, NP_OFF);			// Draw new pixel
						}else{																	// Encoder STRIP 2
						setPixelColor(	ENCODER2_STRIP,											// N STRIP
						NUM_LEDS_ENCODER*(elementToChange-N_ENCODERS_STRIP_1) + i ,	// N LED
						NP_OFF, NP_OFF, NP_OFF);								// Draw new pixel
					}
				}
			}
			}else if(frame == DIGITAL1_CHANGE_FRAME){
			if (newState){
				setPixelColor(DIGITAL1_STRIP, elementToChange, intR, intG, intB); // Draw new pixel
				}else{
				setPixelColor(DIGITAL1_STRIP, elementToChange, NP_OFF, NP_OFF, NP_OFF); // Draw new pixel
			}
			}else if(frame == DIGITAL2_CHANGE_FRAME){
			if (newState){
				setPixelColor(DIGITAL2_STRIP, (elementToChange-numDigitals1), intR, intG, intB); // Draw new pixel
				}else{
				setPixelColor(DIGITAL2_STRIP, (elementToChange-numDigitals1), NP_OFF, NP_OFF, NP_OFF); // Draw new pixel
			}
		}

		indexChanged = FeedbackFramesBuffer[readIdx].updateN;

			if(FeedbackFramesBuffer[readIdx].updateFrame == ENCODER_CHANGE_FRAME	||
			   FeedbackFramesBuffer[readIdx].updateFrame == ENCODER_BLEND_FRAME	||
			   FeedbackFramesBuffer[readIdx].updateFrame == ENCODER_VUMETER_FRAME ||
			   FeedbackFramesBuffer[readIdx].updateFrame == ENCODER_DOUBLE_FRAME	||
			   FeedbackFramesBuffer[readIdx].updateFrame == ENCODER_SWITCH_CHANGE_FRAME){
			if(indexChanged < N_ENCODERS_STRIP_1){
				whichStripToShow |= (1<<ENCODER1_STRIP);
			}else{
				whichStripToShow |= (1<<ENCODER2_STRIP);
			}
		}else if (FeedbackFramesBuffer[readIdx].updateFrame == DIGITAL1_CHANGE_FRAME){
			whichStripToShow |= (1<<DIGITAL1_STRIP);
		}else if (FeedbackFramesBuffer[readIdx].updateFrame == DIGITAL2_CHANGE_FRAME){
			whichStripToShow |= (1<<DIGITAL2_STRIP);
		}

		if(++readIdx >= FEEDBACK_BUFFER_LENGTH)	
			readIdx = 0;
	}
}
