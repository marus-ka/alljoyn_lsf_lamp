/*
 * MC_inits.c
 *
 * Created: 06.11.2015 13:51:05
 *  Author: emm
 */ 
#include "asf.h"
#include "main.h"
// UART module for debug.
static struct usart_module cdc_uart_module;

//brief Configure UART console.
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}



void init_MC(void)
{
	// Initialize the UART console.
	configure_console();
	printf(STRING_HEADER);
}