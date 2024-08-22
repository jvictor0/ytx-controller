/*
 * utils.h
 *
 * Created: 22/8/2024 17:12:13
 *  Author: Ingenieria
 */ 


#ifndef UTILS_H_
#define UTILS_H_

#include <asf.h>

uint8_t CRC8(const uint8_t *data, uint8_t len);
uint16_t checkSum(const uint8_t *data, uint8_t len);
long mapl(long x, long in_min, long in_max, long out_min, long out_max);

#endif /* UTILS_H_ */