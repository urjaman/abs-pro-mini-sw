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
#include "freqio.h"
#include "dbgout.h"

void setup(void);
void loop(void);


static uint24_t tick_time_base = 0;
static uint24_t tick_time_last = 0;
static int16_t tick_count = -1;
// 0.5ms
#define MIN_PERIOD 125
// 0.5s
#define MAX_PERIOD 125000

/* What is the desired amount of time between changes of output frequency? */
/* Bigger time => better input frequency accuracy (at higher Hz). */
/* At slow Hz the time between 2 rising edges dominates this. */
#define OUT_PERIOD 25000

static void print_fphz(uint16_t hz)
{
	DBGINT(hz/16);
	DBGSTR(".");
	DBGINT(((hz&0xF)*10)/16);
	DBGSTR("Hz ");
}

static uint8_t debug_noise = 0;
static void noise_output(void)
{
	if (debug_noise) {
		DBGSTR(" NOISY:");
		DBGINT(debug_noise);
		debug_noise = 0;
	}
}


#define MULT 12
#define DIV 29
#define CALIB 65536


static void calc_set_out_freq(uint32_t fphz)
{
	/* This is the relatively flexible rescaling stage. */
	DBGSTR("I: "); print_fphz(fphz);
	fphz *= MULT;
	fphz = (fphz+(DIV/2)) / DIV;
	fphz *= CALIB;
	fphz += 32768;
	fphz = fphz>>16;

	DBGSTR("O: "); print_fphz(fphz);
	/* Then we figure out the OCR1A to output the new fphz. */
	// fOut = F_CPU / (2*64*(1+OCR1A))
	// with all of the crap simplified (the fixedp format,etc)
	// i think it comes out as...
	uint32_t ocr = ((uint32_t)F_CPU / ((uint32_t)fphz*8))-1;
	DBGSTR("OCR1A: "); DBGINT(ocr);
	if (ocr >= 65535) {
		// Too slow, clip to 0 Hz / inf period...
		freqio_out_inf();
	} else {
		freqio_out(ocr);
	}
	noise_output();
	CRLF();
}

static void process_freqio(void)
{
	if (freqio_is_edge()) {
		uint24_t r = freqio_get_edge();
		uint24_t diff = r - tick_time_base;
		if (tick_count < 0) {
			tick_time_base = r;
			diff = 0;
		} else {
			uint24_t difl = r - tick_time_last;
			if (difl < MIN_PERIOD) {
				debug_noise++;
				return; /* Assume noise. */
			}
			/* This is mostly here to protect the logic in all cases...
			   IRL we should trip the below case without this mostly. */
			if (diff > MAX_PERIOD) goto set_zero;
		}
		tick_time_last = r;
		tick_count++;
		if (diff >= OUT_PERIOD) {
			// Fixed points Hz, X.4
			uint32_t fphz = (((uint32_t)4000000 * tick_count)+(diff/2)) / diff;
			calc_set_out_freq(fphz);
			tick_count = 0;
			tick_time_base = r;
		}
		return;
	}
set_zero: {
		uint24_t r = freqio_get_now();
		uint24_t diff = r - tick_time_base;
		if (diff >= MAX_PERIOD) {
			if (tick_count >= 0) {
				freqio_out_inf();
			}
			tick_time_base = r;
			tick_count = -1;
			DBGSTR("I&O: 0Hz"); 
			noise_output();
			CRLF();
		}
	}
}

void setup(void)
{
	/* Ports */
	GPIOR0 = 0;
	DDRC = 0;
	PORTC = 0x3F;
	DDRD = _BV(1);
	PORTD = _BV(5) | _BV(2) | _BV(3) | _BV(1) | _BV(0);
	DDRB = _BV(1);
	PORTB = _BV(5) | _BV(4) | _BV(3) | _BV(2) | _BV(0);

	freqio_init();
	uart_init();
        freqio_out_inf();
	sei();

}

void loop(void)
{
	process_freqio();
}

#ifndef ARDUINO
void main(void) __attribute__((noreturn));
void main(void)
{
	setup();
	do {
		loop();
	} while(1);
}
#endif
