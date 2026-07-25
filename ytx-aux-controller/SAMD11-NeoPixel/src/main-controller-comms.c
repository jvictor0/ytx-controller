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

#include "main-controller-comms.h"

static bool SendWordToMain(uint16_t word)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	// Main- and ISR-context callers share the single DATA register. Keep the
	// readiness check and write atomic so an RX interrupt cannot consume the
	// DRE slot between them.
	while(!SERCOM2->USART.INTFLAG.bit.DRE){}
	SERCOM2->USART.DATA.reg = word;

	if(!primask){
		__enable_irq();
	}
	return true;
}

bool SendToMain(uint8_t command){
	return SendWordToMain((uint16_t)command | 0x100U);
}

bool SendDataToMain(uint8_t data){
	return SendWordToMain(data);
}
