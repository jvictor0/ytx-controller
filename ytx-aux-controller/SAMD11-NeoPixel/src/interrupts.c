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


#include "utils.h"
#include "types.h"
#include "defines.h"
#include "variables.h"

#include "main-controller-comms.h"


extern FeedbackFrameData FeedbackFramesBuffer[FEEDBACK_BUFFER_LENGTH];


volatile uint32_t millisTicks = 0;

void SysTick_Handler(void)
{
	millisTicks++;	
}

uint32_t millis(){
	return millisTicks;
}

uint32_t failsPerSecond = 0;
uint32_t framesPerSecond = 0;

volatile uint8_t ReceptionBuffer[FeedbackFrame_Size+CHECKSUM_BYTES+1];

void MainControllerReception_Handler(void){
	if(SERCOM2->USART.INTFLAG.bit.RXC){					// if RX interrupt flag is set
		uint16_t rcvWord = SERCOM2->USART.DATA.reg;		// get data from register
		SERCOM2->USART.INTFLAG.bit.RXC = 0;				// clear interrupt flag

	  	bool isCommand = (rcvWord&0x100) ? true : false;
	  	uint8_t rcvByte = (uint8_t)(rcvWord&0x00FF);

		if(isCommand){

			if (rcvByte == INIT_VALUES){
				// INIT VALUES COMMAND
				if(!receivingInit){
					receivingInit = true;
					receivedBytes = 0;
				}
			}else if (rcvByte == CMD_ALL_LEDS_OFF){		// TURN ALL LEDS OFF COMMAND
				turnAllOffFlag = true;
			}else if (rcvByte == CMD_ALL_LEDS_ON){		// TURN ALL LEDS OFF COMMAND
				turnAllOnFlag = true;
			}else if (rcvByte == CMD_RAINBOW_START){		// START RAINBOW
				rainbowStart = true;
			}else if (rcvByte == CHANGE_BRIGHTNESS && !receivingBrightness)	{
				// CHANGE BRIGHTNESS COMMAND
				receivingBrightness = true;
			}else if (rcvByte == BURST_INIT && !receivingBank){
				// SerialUSB.println("BANK INIT");
				// BANK INIT COMMAND
				receivingBank = true;
				receivingFeedbackData = true;
			}else if (rcvByte == BURST_END && receivingBank && receivingFeedbackData){
				// BANK END COMMAND
				// SerialUSB.println("BANK END COMMAND");
				receivingBank = false;
				receivingFeedbackData = false;
				updateBank = true;
			}else if(rcvByte == NEW_FRAME_BYTE){
				// SerialUSB.println("NEW_FRAME_BYTE");
				// FIRST BYTE OF A DATA FRAME
				receivedBytes = 0;
				framesPerSecond++;
				if(!receivingBank) 
					receivingFeedbackData = true;
			}else if(rcvByte == END_OF_FRAME_BYTE){
				// LAST BYTE OF A DATA FRAME
				if(receivingFeedbackData){
					if(receivedBytes != (FeedbackFrame_Size+CHECKSUM_BYTES)){
						failsPerSecond++;
						SendToMain(CHECKSUM_ERROR);
						return;
					}

					// checksum to encoded frame, from 2nd byte, and length is total length without length and checksum bytes (msb and lsb)
					uint16_t checkSumCalc = (2019 + checkSum((const uint8_t *)ReceptionBuffer, FeedbackFrame_Size))&0x00FF;
					uint16_t checkSumRecv = ReceptionBuffer[receivedBytes-CHECKSUM_BYTES];

					if(checkSumCalc==checkSumRecv){
						uint8_t *messageBody = (uint8_t *)&ReceptionBuffer[0];
					
						FeedbackFramesBuffer[writeIdx].updateFrame	= messageBody[FeedbackFrame_Type];
					
						if(	FeedbackFramesBuffer[writeIdx].updateFrame == ENCODER_CHANGE_FRAME ||
							FeedbackFramesBuffer[writeIdx].updateFrame == ENCODER_DOUBLE_FRAME ||
							FeedbackFramesBuffer[writeIdx].updateFrame == ENCODER_VUMETER_FRAME ||
							FeedbackFramesBuffer[writeIdx].updateFrame == ENCODER_SWITCH_CHANGE_FRAME){

							FeedbackFramesBuffer[writeIdx].updateN		=	messageBody[FeedbackFrame_nRing];
							FeedbackFramesBuffer[writeIdx].updateO		=	messageBody[FeedbackFrame_Orientation];
							FeedbackFramesBuffer[writeIdx].updateState	=	messageBody[FeedbackFrame_RingStateH] << 8 | messageBody[FeedbackFrame_RingStateL];

						}else if(	FeedbackFramesBuffer[writeIdx].updateFrame == DIGITAL1_CHANGE_FRAME ||
									FeedbackFramesBuffer[writeIdx].updateFrame == DIGITAL2_CHANGE_FRAME){

							FeedbackFramesBuffer[writeIdx].updateN		=	messageBody[FeedbackFrame_nDigital];
							FeedbackFramesBuffer[writeIdx].updateState	=	messageBody[FeedbackFrame_DigitalState];
						}
						
						FeedbackFramesBuffer[writeIdx].updateR			=	messageBody[FeedbackFrame_R];
						FeedbackFramesBuffer[writeIdx].updateG			=	messageBody[FeedbackFrame_G];
						FeedbackFramesBuffer[writeIdx].updateB			=	messageBody[FeedbackFrame_B];

						if(++writeIdx >= FEEDBACK_BUFFER_LENGTH)	
							writeIdx = 0;
					
						if(!receivingBank) 
							receivingFeedbackData = false;
						
						SendToMain(ACK_CMD);
					}else{
						failsPerSecond++;
						//SerialUSB.println("Checksum error");
						SendToMain(CHECKSUM_ERROR);
					}
				}else if (receivingInit){
					// INIT VALUES BYTES						
					if (receivedBytes == CONFIG_FRAME_SIZE){
						receivedBytes = 0;
						rcvdInitValues = true;
						receivingInit = false;
					}
				}
			}
		//not a command byte -> write to reception buffer
	    }else{

	    	if(receivedBytes >= (sizeof(ReceptionBuffer)-1)){
	    		receivedBytes = 0;
	    	}
			
			ReceptionBuffer[receivedBytes] = rcvByte;	

			receivedBytes++;

			if (receivingBrightness){
				// CHANGE BRIGHTNESS COMMAND - BYTE 2 - NEW BRIGHTNESS
				receivingBrightness = false;
				currentBrightness = rcvByte;
				changeBrightnessFlag = true;
			}
		}	
	}
}