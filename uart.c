/*
 * This file is part of the abs-pro-mini-sw project.
 *
 * Copyright (C) 2015 Urja Rannikko <urjaman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "main.h"
#include "uart.h"

// UART MODULE START
typedef uint8_t urxbufoff_t;
typedef uint8_t utxbufoff_t;
static uint8_t volatile uart_rcvbuf[UART_BUFLEN];
static urxbufoff_t volatile uart_rcvwptr;
static urxbufoff_t uart_rcvrptr;

#ifndef UART_POLLED_TX
static uint8_t volatile uart_sndbuf[UARTTX_BUFLEN];
static utxbufoff_t volatile uart_sndwptr;
static utxbufoff_t volatile uart_sndrptr;
#endif

#define nop() asm volatile("nop")

ISR(USART_RX_vect)
{
	urxbufoff_t reg = uart_rcvwptr;
	uart_rcvbuf[reg++] = UDR0;
	if(reg==UART_BUFLEN) reg = 0;
	uart_rcvwptr = reg;
}

#ifndef UART_POLLED_TX
ISR(USART_UDRE_vect)
{
	utxbufoff_t reg = uart_sndrptr;
	if (uart_sndwptr != reg) {
		UDR0 = uart_sndbuf[reg++];
		if (reg==UARTTX_BUFLEN) reg = 0;
		uart_sndrptr = reg;
		return;
	} else {
		UCSR0B &= ~_BV(5);
		return;
	}
}
#endif

uint8_t uart_isdata(void)
{
	if (uart_rcvwptr != uart_rcvrptr) { return 1; }
	else { return 0; }
}


static void uart_waiting(void)
{
	cli();
	if (uart_rcvwptr == uart_rcvrptr) { /* Race condition check */
		sleep_enable();
		sei();
		sleep_cpu();
		sleep_disable();
	}
	sei();
}

uint8_t uart_recv(void)
{
	urxbufoff_t reg;
	uint8_t val;
	while (!uart_isdata()) uart_waiting();
	reg = uart_rcvrptr;
	val = uart_rcvbuf[reg++];
	if(reg==UART_BUFLEN) reg = 0;
	uart_rcvrptr = reg;
	return val;
}

void uart_send(uint8_t val)
{
#ifndef UART_POLLED_TX
	utxbufoff_t reg;
	cli();
	while (uart_sndwptr+1==uart_sndrptr || (uart_sndwptr+1==UARTTX_BUFLEN
	                                        && !uart_sndrptr)) {
		sei();
		nop();
		nop();
		cli();
	}
	reg = uart_sndwptr;
	UCSR0B |= _BV(5); // make sure the transmit int is on
	uart_sndbuf[reg++] = val; // add byte to the transmit queue
	if(reg==UARTTX_BUFLEN) reg = 0;
	uart_sndwptr = reg;
	sei();
#else
	while (!(UCSR0A & _BV(UDRE0))); // wait for space in reg
	UDR0 = val;
#endif
}

void uart_init(void)
{
#include <util/setbaud.h>
// Assuming uart.h defines BAUD
	uart_rcvwptr = 0;
	uart_rcvrptr = 0;
#ifndef UART_POLLED_TX
	uart_sndwptr = 0;
	uart_sndrptr = 0;
#endif
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	UCSR0C = 0x06; // Async USART,No Parity,1 stop bit, 8 bit chars
	UCSR0A &= 0xFC;
#if USE_2X
	UCSR0A |= (1 << U2X0);
#endif
#ifndef UART_POLLED_TX
	UCSR0B = 0xB8; // RX complete interrupt enable, UDRE int en, Receiver & Transmitter enable
#else
	UCSR0B = 0x98; // RXC int, receiver adn transmitter
#endif
}

void uart_wait_txdone(void)
{
#ifndef UART_POLLED_TX
	while (uart_sndwptr != uart_sndrptr);
#endif
}

