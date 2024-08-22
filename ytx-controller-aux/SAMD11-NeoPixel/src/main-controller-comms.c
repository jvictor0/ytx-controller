/*
 * main_controller_comms.c
 *
 * Created: 22/8/2024 18:09:23
 *  Author: Ingenieria
 */ 
#include "main-controller-comms.h"

bool SendToMain(uint8_t command){
	if(SERCOM2->USART.INTFLAG.bit.DRE){
		SERCOM2->USART.DATA.reg = (((uint16_t)command) + 0x100);
		return 1;
	}else{
		return 0;
	}
}