/*
 * feedback.h
 *
 * Created: 22/8/2024 18:15:17
 *  Author: Ingenieria
 */ 


#ifndef FEEDBACK_H_
#define FEEDBACK_H_

void feedbackBegin(bool rainbowOn);
void feedbackShow();
void feedbackPrepareToShow();
void feedbackDataUpdate(uint8_t nStrip, uint8_t nToChange, bool vertical, uint16_t newState, uint8_t R, uint8_t G, uint8_t B);

#endif /* FEEDBACK_H_ */