/*
 * (C) Copyright 2011
 * Emcraft Systems, <www.emcraft.com>
 * Yuri Tikhonov <yur@emcraft.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/types.h>

#include <mach/hardware.h>
#include <mach/uart.h>

static inline void putc(char c)
{
	volatile u32	*uart_sr = (u32 *)(STM32_DBG_USART_APBX +
					   STM32_DBG_USART_OFFS +
					   STM32_UART_SR);
	volatile u32	*uart_dr = (u32 *)(STM32_DBG_USART_APBX +
					   STM32_DBG_USART_OFFS +
					   STM32_UART_DR);

	while (!(*uart_sr & STM32_USART_SR_TXE))
		barrier();
	*uart_dr = c;
}

static inline void flush(void)
{
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
