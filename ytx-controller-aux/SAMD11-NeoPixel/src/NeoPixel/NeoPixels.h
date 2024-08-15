/*
 * CFile1.h
 *
 * Created: 28/03/2019 19:11:06
 *  Author: Franco
 */ 
#ifndef _NEOPIXEL_H_
#define _NEOPIXEL_H_

#include <asf.h>

#define NEO_GRB  ((1 << 6) | (1 << 4) | (0 << 2) | (2))
#define NEO_KHZ800 0x0000 // 800 KHz datastream

#define MAX_STRIPS	5

uint16_t
	numLEDs[MAX_STRIPS],       // Number of RGB LEDs in strip
	numBytes[MAX_STRIPS];      // Size of 'pixels' buffer below (3 or 4 bytes/pixel)
uint8_t
	brightness[MAX_STRIPS],
	*pixels[MAX_STRIPS],        // Holds LED color values (3 or 4 bytes each)
	nStrips,
	rOffset[MAX_STRIPS],       // Index of red byte within each 3- or 4-byte pixel
	gOffset[MAX_STRIPS],       // Index of green byte
	bOffset[MAX_STRIPS];       // Index of blue 
uint32_t
	endTime;       // Latch timing reference
bool
	begun[MAX_STRIPS];
int8_t
	pin[MAX_STRIPS];           // Output pin number (-1 if not yet set)

 void
	pixelsBegin(uint8_t nStrip, uint16_t n, uint8_t stripPin, uint8_t t),
	pixelsShow(uint8_t nStrip),
	showAll(void),
	setPin(uint8_t nStrip, uint8_t p),
	setPixelColor(uint8_t nStrip, uint16_t n, uint8_t r, uint8_t g, uint8_t b),
	setPixelColor2(uint8_t nStrip, uint16_t n, uint8_t r, uint8_t g, uint8_t b),
	setPixelColorC(uint8_t nStrip, uint16_t n, uint32_t c),
	setAll(int r, int g, int b),
	setBrightness(uint8_t nStrip, uint8_t b),
	rainbow(uint8_t strip, uint8_t wait),
	rainbowAll(uint16_t wait),
	fadeAllTo(uint32_t lastValue, uint8_t wait),
	fadeToBlack(uint8_t strip,int ledNo, uint8_t fadeValue),
	meteorRain(uint8_t strip,uint8_t red, uint8_t green, uint8_t blue, uint8_t meteorSize, uint8_t meteorTrailDecay, bool meteorRandomDecay, int SpeedDelay),
	//rainbow(uint8_t wait),
	delay(int delay_time);
 uint16_t 
	numPixels(uint8_t nStrip);
 uint32_t
	getPixelColor(uint8_t strip, uint16_t n),
	getColor32(uint8_t r, uint8_t g, uint8_t b),
	Wheel(uint8_t WheelPos);

#endif