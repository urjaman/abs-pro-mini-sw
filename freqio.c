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
#include "freqio.h"

/* Undef to use the analog differential. */
//#define INPUT_DIGITAL

/* Internal IRQ period when no output. */
#define IDLE_FREQ 62500

/* Flags used in GPIOR0 */
#define GPF_SETOUT _BV(0)
#define GPF_SETBASE _BV(1)
#define GPF_CLROUT _BV(2)

static uint24_t time_base = 0;
static uint16_t base_inc = 0;
static uint16_t next_ocr1a;

#define CAPTBUF_CNT 4
static uint8_t captbuf_rp = 0;
static uint8_t captbuf_wp = 0;
static uint24_t captbuf[CAPTBUF_CNT];

#define tcnt1_read(x) do { cli(); (x) = TCNT1; sei(); } while(0)
#define timebase_read(x) do { cli(); (x) = ACCESS_ONCE(time_base); sei(); } while(0)
#define nop() asm volatile("nop")

#ifdef INPUT_DIGITAL
/* T0 rising edge. */
ISR(TIMER0_COMPA_vect)
{
	TIMSK0 = 0;
	sei(); /* Allow other ISRs */
	uint16_t tv;
	uint24_t base,base2;
	/* We do a SW fake input capture here kinda :P, just using the old RTC read technique to retry if fail. */
	do {
		timebase_read(base);
		tcnt1_read(tv);
		nop(); // so much cli+sei above that we specifically allow ISRs to run here to update time_base...
		nop();
		timebase_read(base2);
		/* The point here is that if base!= base2, then we dont know if the TCNT1 is relative to
		 * base or base2, thus re-read. */
	} while (base!=base2); // should very likely run only once, but just race check...
	base += tv;
	uint8_t wp = captbuf_wp;
	captbuf[wp] = base;
	wp++;
	if (wp>=CAPTBUF_CNT) wp = 0;
	captbuf_wp = wp;
	cli();
	TIMSK0 = _BV(OCIE0A);
}
#endif

#ifndef INPUT_DIGITAL
/* Analog comparator rising edge via T1 input capture. */
ISR(TIMER1_CAPT_vect)
{
	uint16_t c = ICR1;
	uint24_t b = time_base;
	b += c;
	uint8_t wp = captbuf_wp;
	captbuf[wp] = b;
	wp++;
	if (wp>=CAPTBUF_CNT) wp = 0;
	captbuf_wp = wp;
}
#endif

/* Timer1 clear / place to put new OCR1A (double-buffered) frequency output value. */
ISR(TIMER1_COMPA_vect)
{
	uint24_t c = time_base;
	c += base_inc;
	time_base = c;


	if (GPIOR0 & GPF_SETBASE) {
		base_inc = OCR1A + 1;
		GPIOR0 &= ~GPF_SETBASE;
	}
	if (GPIOR0 & GPF_SETOUT) {
		if (!(GPIOR0 & GPF_CLROUT)) {
			TCCR1A |= _BV(COM1A0);
		}
		OCR1A = next_ocr1a;
		GPIOR0 &= ~GPF_SETOUT;
		GPIOR0 |= GPF_SETBASE;
	}
	if (GPIOR0 & GPF_CLROUT) {
		if (!(PINB & _BV(1))) {
			TCCR1A &= ~_BV(COM1A0);
			GPIOR0 &= ~GPF_CLROUT;
		}
	}
}

uint24_t freqio_get_now(void)
{
	/* I know, this is a copy of the code inside the ISR, but i want the ISR to inline it. */
	uint16_t tv;
	uint24_t base,base2;
	do {
		timebase_read(base);
		tcnt1_read(tv);
		nop();
		nop();
		timebase_read(base2);
	} while (base!=base2);
	base += tv;
	return base;
}

uint8_t freqio_is_edge(void)
{
	if (captbuf_rp != ACCESS_ONCE(captbuf_wp)) return 1;
	return 0;
}

uint24_t freqio_get_edge(void)
{
	uint24_t r = captbuf[captbuf_rp];
	captbuf_rp++;
	if (captbuf_rp >= CAPTBUF_CNT) captbuf_rp = 0;
	return r;
}

void freqio_out(uint16_t ocr1a)
{
	cli();
	ACCESS_ONCE(next_ocr1a) = ocr1a;
	GPIOR0 |= GPF_SETOUT;
	sei();
}


void freqio_out_inf(void)
{
	cli();
	ACCESS_ONCE(next_ocr1a) = IDLE_FREQ-1;
	GPIOR0 |= GPF_SETOUT;
	GPIOR0 |= GPF_CLROUT;
	sei();
}

void freqio_init(void)
{
	/* Setup Timer1 and the Analog Comparator */
	DIDR1 = 0x03;
	OCR1A = IDLE_FREQ-1;
	base_inc = IDLE_FREQ;
	TCCR1A = _BV(WGM11) | _BV(WGM10);
	TCCR1B = _BV(ICES1) | _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
#ifdef INPUT_DIGITAL
	TIFR1 = _BV(OCF1A);
	TIMSK1 = _BV(OCIE1A);
	ACSR = _BV(ACD);

	/* Set Timer0 to generate an interrupt on every T0 rising edge. */
	/* We could use PCINT, but that'd give unnecessary falling edges, and... */
	/* We already have it attached to T0. */
	OCR0A = 0;
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS02) | _BV(CS01) | _BV(CS00); /* T0 rising edge. */
	TIFR0 = _BV(OCF0A);
	TIMSK0 = _BV(OCIE0A);
#else
	TIFR1 = _BV(ICF1) | _BV(OCF1A);
	TIMSK1 = _BV(ICIE1) | _BV(OCIE1A);
	ACSR = _BV(ACIC);
#endif

}
