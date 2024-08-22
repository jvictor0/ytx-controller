/*
 * types.h
 *
 * Created: 22/8/2024 16:14:54
 *  Author: Ingenieria
 */ 


#ifndef TYPES_H_
#define TYPES_H_

#include <asf.h>

typedef struct __attribute__((packed)){
	uint8_t updateFrame;	// update strip
	uint8_t updateO;		// update orientation
	uint8_t updateN;		// update ring
	uint16_t updateState;	// each LED on or off
	uint8_t updateR;		// update R intensity
	uint8_t updateG;		// update G intensity
	uint8_t updateB;		// update B intensity
} FeedbackFrameData;


#endif /* TYPES_H_ */