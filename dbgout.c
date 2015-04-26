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
#include "dbgout.h"

void sendstr_P(PGM_P str)
{
	unsigned char val;
	for(;;) {
		val = pgm_read_byte(str);
		if (val) SEND(val);
		else break;
		str++;
	}
}

static void sendstr(const unsigned char * str)
{
	unsigned char val;
	for(;;) {
		val = *str;
		if (val) SEND(val);
		else break;
		str++;
	}
}

void sendcrlf(void)
{
	sendstr_P(PSTR("\r\n"));
}

void sendint(uint32_t val)
{
	unsigned char buf[11];
	ultoa(val,(char*)buf,10);
	sendstr(buf);
}
