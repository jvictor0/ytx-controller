#pragma once

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

bool activeRainbow = false;
bool ledsUpdateOk = true;
volatile uint8_t tickShow = LED_SHOW_TICKS;
volatile uint8_t tickShowEnd = SHOW_END_REFRESH_TICKS;

//enum MsgFrameEnc{
////msgLength = 0, frameType, nRing, orientation,ringStateH, ringStateL, currentValue,
//e_msgLength = 0, e_fill1, e_frameType, e_nRing, e_orientation, e_ringStateH, e_ringStateL, e_currentValue, e_minVal,
//e_fill2, e_maxVal, e_R, e_G, e_B, e_checkSum_MSB, e_checkSum_LSB,
//e_ENDOFFRAME,
//e_nDigital = e_nRing, e_digitalState = e_ringStateH
//};
//enum MsgFrameDec{
////msgLength = 0, frameType, nRing, orientation,ringStateH, ringStateL, currentValue,
//d_frameType, d_nRing, d_orientation, d_ringStateH, d_ringStateL, d_currentValue, d_minVal,
//d_maxVal, d_R, d_G, d_B,
//d_nDigital = d_nRing, d_digitalState = d_ringStateH
//};
enum MsgFrameEnc{
	//msgLength = 0, frameType, nRing, orientation,ringStateH, ringStateL, currentValue,
	e_msgLength = 0, e_fill1, e_frameType, e_nRing, e_orientation, e_ringStateH, e_ringStateL, e_R, e_G,
	e_fill2, e_B, e_checkSum_MSB, e_checkSum_LSB,
	e_ENDOFFRAME,
	e_nDigital = e_nRing, e_digitalState = e_ringStateH
};
enum MsgFrameDec{
	//msgLength = 0, frameType, nRing, orientation,ringStateH, ringStateL, currentValue,
	d_frameType, d_nRing, d_orientation, d_ringStateH, d_ringStateL, d_R, d_G, d_B,
	d_nDigital = d_nRing, d_digitalState = d_ringStateH
};

enum InitFrame{
	nEncoders, nAnalog, nDigitals1, nDigitals2, nBrightness, nRainbow, INIT_ENDOFFRAME
};
enum LedStrips{
	ENCODER1_STRIP, ENCODER2_STRIP, DIGITAL1_STRIP, DIGITAL2_STRIP, FB_STRIP, LAST_STRIP
};
//! [rx_buffer_var]
#define MAX_RX_BUFFER_LENGTH_ENC   e_ENDOFFRAME+1
#define MAX_RX_BUFFER_LENGTH_DEC   d_B+1
volatile uint8_t rxArrayIndex = 0;
volatile bool rxComplete = false;
volatile bool rcvdInitValues = false;
volatile bool receivingInit = false;
volatile bool receivingBrightness = false;
volatile bool updateBank = false;
volatile uint8_t rx_bufferEnc[MAX_RX_BUFFER_LENGTH_ENC];
volatile uint8_t rx_bufferDec[MAX_RX_BUFFER_LENGTH_DEC];
volatile uint16_t checkSumCalc = 0;
volatile uint16_t checkSumRecv = 0;

#define RING_BUFFER_LENGTH	128

//uint8_t a[1872];

typedef struct __attribute__((packed)){
	uint8_t updateFrame;	// update strip
	uint8_t updateO;		// update orientation
	uint8_t updateN;		// update ring
	//uint8_t updateValue;	// update value
	//uint8_t updateMin;	// update min value
	//uint8_t updateMax;	// update min value
	uint16_t updateState;	// each LED on or off
	uint8_t updateR;		// update R intensity
	uint8_t updateG;		// update G intensity
	uint8_t updateB;		// update B intensity
} RingBufferData;

volatile RingBufferData ringBuffer[RING_BUFFER_LENGTH];
volatile uint8_t readIdx = 0;
volatile uint8_t writeIdx = 0;
volatile uint8_t bufferCurrentSize = 0;
volatile bool changeBrightnessFlag = false;
volatile uint8_t turnAllOffFlag = false;
volatile uint8_t turnAllOnFlag = false;
volatile uint8_t rainbowStart = false;
volatile uint8_t onGoingFrame = false;
volatile bool receivingLEDdata = false;
volatile bool receivingBank = false;
volatile bool ledShow = false;
volatile bool timeToShow = false;
volatile bool sendShowEnd = false;
volatile bool frameComplete = false;
volatile bool readingBuffer = false;

volatile uint16_t tickCount = ONE_SEC_TICKS;
volatile uint16_t ticksRainbow = 0;

volatile uint8_t msgCount = 0;

//! [module_inst]
struct usart_module usart_instance;
//! [module_inst]

uint16_t indexChanged = 0;

uint8_t whichStripToShow = 0;

bool blinkLED = false;
uint8_t blinkLEDcount = 0;

uint8_t numStripsOn = 0;
uint8_t numEncoders = 0;
uint8_t numDigitals1 = 0;
uint8_t numDigitals2 = 0;
uint8_t numAnalogFb = 0;
uint8_t currentBrightness = 0;
//! [rx_buffer_var]

uint8_t decodeSysEx(volatile uint8_t* inSysEx, volatile uint8_t* outData, uint8_t inLength);
uint8_t encodeSysEx(uint8_t* inData, uint8_t* outSysEx, uint8_t inLength);

void usart_read_callback(struct usart_module *const usart_module);
void usart_write_callback(struct usart_module *const usart_module);

void RX_Handler  ( void );
bool SendToMain(uint8_t command);

void configure_usart(void);
void configure_usart_callbacks(void);

uint16_t checkSum(volatile uint8_t *data, uint8_t len);
uint8_t CRC8(const uint8_t *data, uint8_t len);

void UpdateLEDs(uint8_t nStrip, uint8_t nToChange, bool vertical, uint16_t newState, 
				uint8_t intR, uint8_t intG, uint8_t intB);

long mapl(long x, long in_min, long in_max, long out_min, long out_max);

