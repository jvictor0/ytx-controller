/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.
  Copyright (c) 2015 Atmel Corporation/Thibaut VIARD.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include "sam.h"

#include "NeoPixels.h"

#include "sam_ba_monitor.h"
#include "sam_ba_serial.h"
#include "board_definitions.h"

extern uint32_t __sketch_vectors_ptr; // Exported value from linker script
extern void board_init(void);

volatile uint32_t* pulSketch_Start_Address;

static void jump_to_application(void) {

  /* Rebase the Stack Pointer */
  __set_MSP( (uint32_t)(__sketch_vectors_ptr) );

  /* Rebase the vector table base address */
  SCB->VTOR = ((uint32_t)(&__sketch_vectors_ptr) & SCB_VTOR_TBLOFF_Msk);

  /* Jump to application Reset Handler in the application */
  asm("bx %0"::"r"(*pulSketch_Start_Address));
}

static volatile bool main_b_cdc_enable = false;

static volatile bool jump_to_app = false;

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void)
{
  /*
   * Test sketch stack pointer @ &__sketch_vectors_ptr
   * Stay in SAM-BA if value @ (&__sketch_vectors_ptr) == 0xFFFFFFFF (Erased flash cell value)
   */
  if (__sketch_vectors_ptr == 0xFFFFFFFF)
  {
    /* Stay in bootloader */
    return;
  }

  /*
   * Load the sketch Reset Handler address
   * __sketch_vectors_ptr is exported from linker script and point on first 32b word of sketch vector table
   * First 32b word is sketch stack
   * Second 32b word is sketch entry point: Reset_Handler()
   */
  pulSketch_Start_Address = &__sketch_vectors_ptr ;
  pulSketch_Start_Address++ ;

  /*
   * Test vector table address of sketch @ &__sketch_vectors_ptr
   * Stay in SAM-BA if this function is not aligned enough, ie not valid
   */
  if ( ((uint32_t)(&__sketch_vectors_ptr) & ~SCB_VTOR_TBLOFF_Msk) != 0x00)
  {
    /* Stay in bootloader */
    return;
  }

  jump_to_app = true;
}

#define BOOT_MODE_PORT	0
#define BOOT_MODE_PIN	14
/**
 *  \brief SAMD21 SAM-BA Main loop.
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{

	/* configure PA14 (bootloader entry pin) as input pull-up */
	PORT->Group[BOOT_MODE_PORT].PINCFG[BOOT_MODE_PIN].reg = PORT_PINCFG_PULLEN | PORT_PINCFG_INEN;
	PORT->Group[BOOT_MODE_PORT].OUTSET.reg = (1UL << BOOT_MODE_PIN);
  
	/* Wait 1 milisec to stable pin boot mode.
	* The loop value is based on SAMD21 default 1MHz clock @ reset.
	*/
	for (uint32_t i=0; i<250; i++) /* 1ms */
		/* force compiler to not optimize this... */
		__asm__ __volatile__("");
	
	check_start_application();
	
	/* Jump in application if boot mode is disable */
	if (PORT->Group[BOOT_MODE_PORT].IN.reg & (1UL << BOOT_MODE_PIN))
	{
		jump_to_application();
	}
	/* Jump in application if condition is satisfied */
	//

	/* System initialization */
	board_init();
	
	__enable_irq();

	/* UART is enabled in all cases */
	serial_open();

	sam_ba_monitor_init(SAM_BA_INTERFACE_USART);
	
	/* SAM-BA on Serial loop */
	sam_ba_monitor_run();
}


