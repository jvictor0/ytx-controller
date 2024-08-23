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


#ifndef DEFINES_H_
#define DEFINES_H_

#include <asf.h>

#define NUM_LEDS_ENCODER		16
#define N_ENCODERS_STRIP_1		16
#define N_ENCODERS_STRIP_2		N_ENCODERS_STRIP_1
#define LED_YTX_PIN				PIN_PA02

#define ENC1_STRIP_PIN			PIN_PA17
#define ENC2_STRIP_PIN			PIN_PA16
#define DIG1_STRIP_PIN			PIN_PA08
#define DIG2_STRIP_PIN			PIN_PA09
#define FB_STRIP_PIN			PIN_PA15

// 	system_gclk_gen_get_hz(GCLK_GENERATOR_0) -> 32768 KHz
//	32768*366 = ~12 MHz
//	48 MHz / 12MHz = 4 veces por segundo entra a la interrupcion => 250ms
#define ONE_SEC					system_gclk_gen_get_hz(GCLK_GENERATOR_0)/1000
#define ONE_SEC_TICKS			1000
#define QUARTER_SEC_TICKS		250

#define BAUD_RATE	2000000

#define ACK_CMD					0xAA
#define NEW_FRAME_BYTE          0xF0
#define BURST_INIT              0xF1
#define BURST_END               0xF2
#define CMD_ALL_LEDS_OFF        0xF3
#define CMD_ALL_LEDS_ON         0xF4
#define INIT_VALUES             0xF5
#define CHANGE_BRIGHTNESS       0xF6
#define END_OF_RAINBOW          0xF7
#define CHECKSUM_ERROR          0xF8
#define SHOW_IN_PROGRESS        0xF9
#define SHOW_END                0xFA
#define CMD_RAINBOW_START		0xFB
#define RESET_HAPPENED			0xFC
#define NEW_MIDI_FRAME_BYTE		0xFD
#define END_OF_FRAME_BYTE       0xFF

#define LED_BLINK_TICKS			ONE_SEC_TICKS
#define LED_SHOW_TICKS			15
#define SHOW_END_REFRESH_TICKS	100
#define ACK_TIMEOUT_TICKS		5
#define NP_OFF					0
#define NP_ON					48

#define ENCODER_CHANGE_FRAME			0x00
#define ENCODER_DOUBLE_FRAME      		0x01
#define ENCODER_VUMETER_FRAME			0x02
#define ENCODER_SWITCH_CHANGE_FRAME		0x03
#define DIGITAL1_CHANGE_FRAME			0x04
#define DIGITAL2_CHANGE_FRAME			0x05
#define ANALOG_CHANGE_FRAME				0x06
#define BANK_CHANGE_FRAME				0x07

#define ENCODER_MASK_H			0x3FFE
#define ENCODER_MASK_V			0xE3FF
#define ENCODER_SWITCH_H_ON		0xC001
#define ENCODER_SWITCH_V_ON		0x1C00

enum LedStrips{
	ENCODER1_STRIP, ENCODER2_STRIP, DIGITAL1_STRIP, DIGITAL2_STRIP, FB_STRIP, LAST_STRIP
};

enum configFrame{
	nEncoders, nAnalog, nDigitals1, nDigitals2, nBrightness, nRainbow, CONFIG_FRAME_SIZE
};

enum FeedbackFrame{
	FeedbackFrame_Type, FeedbackFrame_nRing, FeedbackFrame_Orientation, FeedbackFrame_RingStateH, FeedbackFrame_RingStateL,
	FeedbackFrame_R, FeedbackFrame_G, FeedbackFrame_B,
	FeedbackFrame_Size,
	FeedbackFrame_nDigital = FeedbackFrame_nRing, FeedbackFrame_DigitalState = FeedbackFrame_RingStateH
};

#define CHECKSUM_BYTES	1

#define FEEDBACK_BUFFER_LENGTH	128

#endif /* DEFINES_H_ */