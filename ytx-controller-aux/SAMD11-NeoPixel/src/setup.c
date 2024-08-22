/*
 * setup.c
 *
 * Created: 22/8/2024 17:38:45
 *  Author: Ingenieria
 */ 

//! [setup]
#include <asf.h>
#include "defines.h"

extern void MainControllerReception_Handler(void);

void configure_usart(void)
{
	struct usart_module usart_instance;
	
	uint8_t instance_index;
	//! [setup_config]
	struct usart_config config_usart;
	//! [setup_config]
	//! [setup_config_defaults]
	usart_get_config_defaults(&config_usart);
	//! [setup_config_defaults]

	//! [setup_change_config]
	config_usart.baudrate    = BAUD_RATE;
	config_usart.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	config_usart.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	config_usart.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	config_usart.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	config_usart.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;

	//! [setup_change_config]
	//usart_instance.hw->USART.CTRLA.bit.RXPO = 0x3;
	//usart_instance.hw->USART.CTRLA.bit.TXPO = 0x0;
	//usart_instance.hw->USART.CTRLB.bit.RXEN = true;
	//! [setup_set_config]
	while (usart_init(&usart_instance,
		SERCOM2, &config_usart) != STATUS_OK) {
	}
	
	// Interrupt UART CONFIG from https://github.com/avrxml/asf/blob/master/thirdparty/freertos/demo/oled1_xpro_example/cdc.h
	
	//! [setup_set_config]
	/* SERCOM2 handler enabled */
	NVIC_SetPriority(SERCOM2_IRQn, 0);
	//system_interrupt_enable(SERCOM2_IRQn);
	
	// Inject our own interrupt handler
	instance_index = _sercom_get_sercom_inst_index(EDBG_CDC_MODULE);
	_sercom_set_handler(instance_index, MainControllerReception_Handler);
	
	//! [setup_enable]
	usart_enable(&usart_instance);
	// Enable the UART transceiver
	//usart_enable_transceiver(&usart_instance, USART_TRANSCEIVER_TX);
	//usart_enable_transceiver(&usart_instance, USART_TRANSCEIVER_RX);
	//! [setup_enable]
	/* receive complete interrupt set */
	SERCOM2->USART.INTENSET.reg = SERCOM_USART_INTFLAG_RXC;
}


/** Configure LED0, turn it off*/
static void config_led(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LED_YTX_PIN, &pin_conf);
	port_pin_set_output_level(LED_YTX_PIN, LED_0_INACTIVE);
}

void setup(void){
	system_init();
	delay_init();
	config_led();
	configure_usart();

	
	/*Configure system tick to generate periodic interrupts */
	SysTick_Config(ONE_SEC/1000);
	//uint16_t clockRate = system_gclk_gen_get_hz(GCLK_GENERATOR_0);
	/* Enable Interrupts */
	__enable_irq();	
}