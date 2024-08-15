/*
 * CFile1.h
 *
 * Created: 28/03/2019 19:11:06
 *  Author: Franco
 */ 
#ifndef _NEOPIXEL_H_
#define _NEOPIXEL_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sam.h>

#define NEO_GRB  ((1 << 6) | (1 << 4) | (0 << 2) | (2))
#define NEO_KHZ800 0x0000 // 800 KHz datastream

#define MAX_STRIPS	5

 void
	pixelsBegin(uint8_t nStrip, uint16_t nLeds, uint8_t stripPin, uint8_t type),
	pixelsShow(uint8_t nStrip),
	setPin(uint8_t nStrip, uint8_t p),
	setPixelColor(uint8_t nStrip, uint16_t n, uint8_t r, uint8_t g, uint8_t b),
	setPixelColorC(uint8_t nStrip, uint16_t n, uint32_t c),
	setAll(int r, int g, int b),
	setBrightness(uint8_t nStrip, uint8_t b),
	rainbow(uint8_t strip, uint8_t wait),
	rainbowAll(uint8_t wait),
	fadeAllTo(uint32_t lastValue, uint8_t wait),
	meteorRain(uint8_t strip,uint8_t red, uint8_t green, uint8_t blue, uint8_t meteorSize, uint8_t meteorTrailDecay, uint8_t meteorRandomDecay, int SpeedDelay),
	//rainbow(uint8_t wait),
	delay(int delay_time);
 uint16_t 
	numPixels(uint8_t nStrip);
 uint32_t
	getColor32(uint8_t r, uint8_t g, uint8_t b),
	Wheel(uint8_t WheelPos);

#endif