/*
 * CFile1.c
 *
 * Created: 28/03/2019 19:03:16
 *  Author: Franco
 */ 

#include <NeoPixels.h>
#include "defines.h"

extern volatile uint32_t millisTicks;

static uint32_t sampleSysTickStart(void)
{
	for(;;){
		(void)SysTick->CTRL;
		uint32_t value = SysTick->VAL;
		if(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)){
			return value;
		}
	}
}

static bool sampleSysTickEnd(uint32_t *value)
{
	bool tickWrapped = false;

	for(;;){
		tickWrapped |= (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0;
		uint32_t sampledValue = SysTick->VAL;
		uint32_t afterSample = SysTick->CTRL;
		tickWrapped |= (afterSample & SysTick_CTRL_COUNTFLAG_Msk) != 0;
		if(!(afterSample & SysTick_CTRL_COUNTFLAG_Msk)){
			*value = sampledValue;
			return tickWrapped;
		}
	}
}

static void accountForMaskedShowTime(uint16_t bytes, uint32_t startValue,
	                                  uint32_t endValue, bool tickWrapped)
{
	uint32_t period = SysTick->LOAD + 1U;

	if(!tickWrapped || period == 0){
		return;
	}

	// An 800 kHz strip consumes 10 us per RGB byte. The start/end counter
	// values provide the sub-millisecond phase; the byte count disambiguates
	// how many full SysTick periods elapsed while interrupts were masked.
	uint32_t expectedCycles = ((uint32_t)bytes * period) /
	                          WS2812_BYTES_PER_AUX_TICK;
	int32_t wrapNumerator = (int32_t)expectedCycles -
	                        (int32_t)startValue +
	                        (int32_t)endValue;
	uint32_t elapsedTicks = 1;
	if(wrapNumerator > 0){
		elapsedTicks = ((uint32_t)wrapNumerator + (period / 2U)) / period;
		if(elapsedTicks == 0){
			elapsedTicks = 1;
		}
	}

	millisTicks += elapsedTicks;
	SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
}

bool pixelsBegin(uint8_t nStrip, uint16_t n, uint8_t stripPin, uint8_t t) {
	if(nStrip >= MAX_STRIPS) return false;
	
	pin[nStrip] = stripPin;

	if(pin[nStrip] >= 0) {
		struct port_config pin_conf;
		port_get_config_defaults(&pin_conf);

		pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
		port_pin_set_config(pin[nStrip], &pin_conf);
		port_pin_set_output_level(pin[nStrip], false);
	}

	// Allocate new data -- note: ALL PIXELS ARE CLEARED
	numBytes[nStrip] = n * 3;
	if((pixels[nStrip] = (uint8_t *)malloc(numBytes[nStrip]))) {
		memset(pixels[nStrip], 0, numBytes[nStrip]);
		numLEDs[nStrip] = n;
	} else {
		numLEDs[nStrip] = numBytes[nStrip] = 0;
		return false;
	}

	rOffset[nStrip] = (t >> 4) & 0b11; // regarding R/G/B/W offsets
	gOffset[nStrip] = (t >> 2) & 0b11;
	bOffset[nStrip] =  t       & 0b11;

	begun[nStrip] = true;
	nStrips++;
	return true;
}

void pixelsEndAll(void){
	for(uint8_t strip = 0; strip < MAX_STRIPS; strip++){
		if(pixels[strip]){
			free(pixels[strip]);
			pixels[strip] = NULL;
		}
		numLEDs[strip] = 0;
		numBytes[strip] = 0;
		begun[strip] = false;
	}
	nStrips = 0;
}

void pixelsShow(uint8_t nStrip){
	if(nStrip >= MAX_STRIPS) return;
	if(!pixels[nStrip]) return;
	if(!begun[nStrip]) return;
	
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	uint32_t systickStart = sampleSysTickStart();
	// Data latch = 300+ microsecond pause in the output stream.  Rather than
	// put a delay at the end of the function, the ending time is noted and
	// the function will simply hold off (if needed) on issuing the
	// subsequent round of data until the latch time has elapsed.  This
	// allows the mainline code to start generating the next frame of data
	// rather than stalling for the latch.
	// while(!canShow());
	// endTime is a private member (rather than global var) so that multiple
	// instances on different pins can be quickly issued in succession (each
	// instance doesn't delay the next).

	// In order to make this code runtime-configurable to work with any pin,
	// SBI/CBI instructions are eschewed in favor of full PORT writes via the
	// OUT or ST instructions.  It relies on two facts: that peripheral
	// functions (such as PWM) take precedence on output pins, so our PORT-
	// wide writes won't interfere, and that interrupts are globally disabled
	// while data is being issued to the LEDs, so no other code will be
	// accessing the PORT.  The code takes an initial 'snapshot' of the PORT
	// state, computes 'pin high' and 'pin low' values, and writes these back
	// to the PORT register as needed.
	uint8_t  *ptr, *end, p, bitMask, portNum;
	uint32_t  pinMask;

	portNum =  0;
	pinMask =  (1ul <<  pin[nStrip]);
	ptr     =  pixels[nStrip];
	end     =  ptr + numBytes[nStrip];
	p       = *ptr++;
	bitMask =  0x80;

	volatile uint32_t *set = &(PORT->Group[portNum].OUTSET.reg), 
		*clr = &(PORT->Group[portNum].OUTCLR.reg);

	for(;;) {
		*set = pinMask;
		asm("nop; nop; nop; nop; nop; nop; nop; nop;");
		if(p & bitMask) {
			asm("nop; nop; nop; nop; nop; nop; nop; nop;"
			"nop; nop; nop; nop; nop; nop; nop; nop;"
			"nop; nop; nop; nop;");
			*clr = pinMask;
			} else {
			*clr = pinMask;
			asm("nop; nop; nop; nop; nop; nop; nop; nop;"
			"nop; nop; nop; nop; nop; nop; nop; nop;"
			"nop; nop; nop; nop;");
		}
		if(bitMask >>= 1) {
			asm("nop; nop; nop; nop; nop; nop; nop; nop; nop;");
			} else {
			if(ptr >= end) break;
			p       = *ptr++;
			bitMask = 0x80;
		}
	}
	uint32_t systickEnd;
	bool tickWrapped = sampleSysTickEnd(&systickEnd);
	accountForMaskedShowTime(numBytes[nStrip], systickStart, systickEnd, tickWrapped);
	if(!primask){
		__enable_irq();
	}
	//endTime = micros(); // Save EOD time for latch on next call
}

// Set pixel color from separate R,G,B components:
void setPixelColor(uint8_t nStrip,
	uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
	
	if(nStrip >= MAX_STRIPS) return;
	if(!begun[nStrip]) return;
	
	if(n < numLEDs[nStrip]) {
		if(brightness[nStrip]) { // See notes in setBrightness()
			r = (r * brightness[nStrip]) >> 8;
			g = (g * brightness[nStrip]) >> 8;
			b = (b * brightness[nStrip]) >> 8;
		}
		uint8_t *p;
		p = &pixels[nStrip][n * 3];    // 3 bytes per pixel
		
		p[rOffset[nStrip]] = r;          // R,G,B always stored
		p[gOffset[nStrip]] = g;
		p[bOffset[nStrip]] = b;
	}
}

// Set pixel color from separate R,G,B components:
//void setPixelColor2(uint8_t nStrip,
//uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
	//
	//if(nStrip >= MAX_STRIPS) return;
	//if(!begun[nStrip]) return;
	//
	//if(n < numLEDs[nStrip]) {
		//if(brightness[nStrip]) { // See notes in setBrightness()
			//r = ((255-r) * brightness[nStrip]) >> 8;
			//g = ((255-g) * brightness[nStrip]) >> 8;
			//b = ((255-b) * brightness[nStrip]) >> 8;
		//}
		//uint8_t *p;
		//p = &pixels[nStrip][n * 3];    // 3 bytes per pixel
		//
		//p[rOffset[nStrip]] = r;          // R,G,B always stored
		//p[gOffset[nStrip]] = g;
		//p[bOffset[nStrip]] = b;
	//}
//}

// Set pixel color from 'packed' 32-bit RGB color:
void setPixelColorC(uint8_t nStrip,uint16_t n, uint32_t c) {
	if(nStrip >= MAX_STRIPS) return;
	if(!begun[nStrip]) return;
	
	if(n < numLEDs[nStrip]) {
		uint8_t *p,
		r = (uint8_t)(c >> 16),
		g = (uint8_t)(c >>  8),
		b = (uint8_t)c;
		if(brightness[nStrip]) { // See notes in setBrightness()
			r = (r * brightness[nStrip]) >> 8;
			g = (g * brightness[nStrip]) >> 8;
			b = (b * brightness[nStrip]) >> 8;
		}
		
		p = &pixels[nStrip][n * 3];
		
		p[rOffset[nStrip]] = r;
		p[gOffset[nStrip]] = g;
		p[bOffset[nStrip]] = b;
	}
}

uint32_t getPixelColor(uint8_t strip, uint16_t n) {
	if(n >= numPixels(strip)) return 0; // Out of bounds, return no color.

	uint8_t *p;

	p = &pixels[n * 3];
	if(brightness[strip]) {
		// Stored color was decimated by setBrightness(). Returned value
		// attempts to scale back to an approximation of the original 24-bit
		// value used when setting the pixel color, but there will always be
		// some error -- those bits are simply gone. Issue is most
		// pronounced at low brightness levels.
		return	(((uint32_t)(p[rOffset[strip]] << 8) / brightness[strip]) << 16) |
				(((uint32_t)(p[gOffset[strip]] << 8) / brightness[strip]) <<  8) |
				( (uint32_t)(p[bOffset[strip]] << 8) / brightness[strip]		 );
	} else {
		// No brightness adjustment has been made -- return 'raw' color
		return ((uint32_t)p[rOffset[strip]] << 16) |
		((uint32_t)p[gOffset[strip]] <<  8) |
		(uint32_t)p[bOffset[strip]];
	}
}

// Adjust output brightness; 0=darkest (off), 255=brightest.  This does
// NOT immediately affect what's currently displayed on the LEDs.  The
// next call to show() will refresh the LEDs at this level.  However,
// this process is potentially "lossy," especially when increasing
// brightness.  The tight timing in the WS2811/WS2812 code means there
// aren't enough free cycles to perform this scaling on the fly as data
// is issued.  So we make a pass through the existing color data in RAM
// and scale it (subsequent graphics commands also work at this
// brightness level).  If there's a significant step up in brightness,
// the limited number of steps (quantization) in the old data will be
// quite visible in the re-scaled version.  For a non-destructive
// change, you'll need to re-render the full strip data.  C'est la vie.
void setBrightness(uint8_t nStrip,uint8_t b) {
	if(nStrip >= MAX_STRIPS) return;
	if(!begun[nStrip]) return;
	
	// Stored brightness value is different than what's passed.
	// This simplifies the actual scaling math later, allowing a fast
	// 8x8-bit multiply and taking the MSB.  'brightness' is a uint8_t,
	// adding 1 here may (intentionally) roll over...so 0 = max brightness
	// (color values are interpreted literally; no scaling), 1 = min
	// brightness (off), 255 = just below max brightness.
	uint8_t newBrightness = b + 1;
	if(newBrightness != brightness[nStrip]) { // Compare against prior value
		// Brightness has changed -- re-scale existing data in RAM
		uint8_t  c,
		*ptr           = pixels[nStrip],
		oldBrightness = brightness[nStrip] - 1; // De-wrap old brightness value
		uint16_t scale;
		if(oldBrightness == 0) scale = 0; // Avoid /0
		else if(b == 255) scale = 65535 / oldBrightness;
		else scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;
		for(uint16_t i=0; i<numBytes[nStrip]; i++) {
			c      = *ptr;
			*ptr++ = (c * scale) >> 8;
		}
		brightness[nStrip] = newBrightness;
	}
}

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t getColor32(uint8_t r, uint8_t g, uint8_t b) {
	return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

void setAll(int r, int g, int b){
	for (int s = 0; s < MAX_STRIPS; s++){
		if(begun[s]){
			for (int i = 0;i<numLEDs[s];i++){
				setPixelColor(s,i,r,g,b);
			}
		}
	}
}

uint16_t numPixels(uint8_t nStrip){
	return numLEDs[nStrip];
}

void setPin(uint8_t nStrip,uint8_t p) {
	//if(begun && (pin >= 0)) pinMode(pin, INPUT);
	pin[nStrip] = p;
	if(begun[nStrip]) {
		struct port_config pin_conf;
		port_get_config_defaults(&pin_conf);

		pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
		port_pin_set_config(pin[nStrip], &pin_conf);
		port_pin_set_output_level(pin[nStrip], false);
	}
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(uint8_t WheelPos) {
	if(WheelPos < 85) {
		return getColor32(255 - WheelPos * 3, 0, WheelPos * 3);
	}
	else if(WheelPos < 170) {
		WheelPos -= 85;
		return getColor32(0, WheelPos * 3, 255 - WheelPos * 3);
		
	}
	else {
		WheelPos -= 170;
		return getColor32(WheelPos * 3, 255 - WheelPos * 3, 0);
	}
}

void rainbow(uint8_t strip, uint8_t wait) {
	uint16_t i, j, s;
	
	for(j=0; j<256; j++) {
		for(i=0; i<numPixels(strip); i++) {
			uint32_t color32 = Wheel((i*1+j) & 255);
			setPixelColorC(strip, i, color32);
		}
		pixelsShow(strip);
		delay(wait);
	}
}

void showAll(){
	for(int s = 0; s < MAX_STRIPS; s++){
		if(begun[s]){
			pixelsShow(s);
		}
	}
}

void rainbowAll(uint16_t wait) {
	uint16_t i, j, s;
	
	for(j=0; j<256; j++) {
		for (int s = 0; s < MAX_STRIPS; s++){
			if(begun[s]){
				for(i=0; i<numPixels(s); i++) {
					uint32_t color32 = Wheel((i*1+j) & 255);
					setPixelColorC(s, i, color32);	
				}	
			}
		}
		for (int s = 0; s < MAX_STRIPS;s++){
			if(begun[s])	pixelsShow(s);
		}
				
		delay(wait);
	}
}

void delay(int delay_time){
	delay_ms(delay_time);
}

//void fadeAllTo(uint32_t lastValue, uint8_t wait){	 // NOT WORKING
	//uint8_t targetR = (lastValue & 0x00ff0000UL) >> 16;
	//uint8_t targetG = (lastValue & 0x0000ff00UL) >> 8;
	//uint8_t targetB = (lastValue & 0x000000ffUL);
	//uint8_t fadeComplete = 0;
	//uint8_t fadeCompleteMask = 0;
	//uint16_t nLedsOnTarget[3];
	//uint8_t currentR, currentG, currentB;
	//uint8_t *p;
		//
	//for (int s = 0; s < nStrips; s++){
		//fadeCompleteMask |= 1<<s;
		//nLedsOnTarget[s] = 0;
	//}
	//do{
		//for (int s = 0; s < nStrips; s++){
			//for(uint8_t i=0; i < numPixels(s); i++) {
				//// NeoPixel
					//
				//p = &pixels[s][i * 3];    // 3 bytes per pixel
					//
				//currentR = (p[rOffset[s]] & 0x00ff0000UL) >> 16;
				//currentG = (p[gOffset[s]] & 0x0000ff00UL) >> 8;
				//currentB = (p[bOffset[s]] & 0x000000ffUL);
//
				//if (targetR > currentR) {
					//currentR++;
				//} else if (targetR < currentR) {
					//currentR--;
				//}
				//// green
				//if (targetG > currentG) {
					//currentG++;
				//} else if (targetG < currentG) {
					//currentG--;
				//}
				//// blue
				//if (targetB > currentB) {
					//currentB++;
				//} else if (targetB < currentB) {
					//currentB--;
				//}	
				//setPixelColor(s, i, currentR,currentG,currentB);
				//if (targetR == currentR && targetG == currentG && targetB == currentB) {
					//nLedsOnTarget[s]++;
				//}
			//}
		//}
		//for (int s = 0; s < nStrips; s++){
			//pixelsShow(s);
			//if(nLedsOnTarget[s] == numPixels(s))
				//fadeCompleteMask |= (1<<s);
		//}
		//
		//delay(wait);
	//}while(fadeComplete != fadeCompleteMask);
//}
//

//
//void fadeToBlack(uint8_t strip,int ledNo, uint8_t fadeValue) {
	//// NeoPixel
	//
	//uint8_t r, g, b;
	//int value;
	//uint8_t *p;
	//p = &pixels[strip][ledNo * 3];    // 3 bytes per pixel
	//
	//r = (p[rOffset[strip]] & 0x00ff0000UL) >> 16;
	//g = (p[rOffset[strip]] & 0x0000ff00UL) >> 8;
	//b = (p[rOffset[strip]] & 0x000000ffUL);
//
	//r=(r<=10)? 0 : (int) r-(r*fadeValue/256);
	//g=(g<=10)? 0 : (int) g-(g*fadeValue/256);
	//b=(b<=10)? 0 : (int) b-(b*fadeValue/256);
	//
	//setPixelColor(strip, ledNo, r,g,b);
//}
//
//void meteorRain(uint8_t strip,uint8_t red, uint8_t green, uint8_t blue, uint8_t meteorSize, uint8_t meteorTrailDecay, bool meteorRandomDecay, int SpeedDelay) {
	//setAll(0,0,0);
	//
	//for(int i = 0; i < numPixels(strip)+numPixels(strip); i++) {
		//
		//// fade brightness all LEDs one step
		//for(int j=0; j<numPixels(strip); j++) {
			//if( (!meteorRandomDecay) || (random()*10>5) ) {
				//fadeToBlack(strip,j, meteorTrailDecay );
			//}
		//}
		//
		//// draw meteor
		//for(int j = 0; j < meteorSize; j++) {
			//if( ( i-j <numPixels(strip)) && (i-j>=0) ) {
				//setPixelColor(strip,i-j, red, green, blue);
			//}
		//}
		//
		//pixelsShow(strip);
		//delay(SpeedDelay);
	//}
//}
