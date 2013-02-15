/*
 * (C) Copyright 2011
 *
 * Alexander Potashev, Emcraft Systems, aspotashev@emcraft.com
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

#include <common.h>

#include "wdt.h"

/*
 * Strobe the WDT.
 */
void wdt_strobe(void)
{
	/*
	 * TBD
	 */

	return;
}

/*
 * Disable the WDT.
 */
void
#ifdef CONFIG_LPC18XX_NORFLASH_BOOTSTRAP_WORKAROUND
	__attribute__((section(".lpc18xx_image_top_text")))
#endif
	wdt_disable(void)
{
	/*
	 * TBD
	 */

	return;
}

/*
 * Enable the WDT.
 */
void
#ifdef CONFIG_LPC18XX_NORFLASH_BOOTSTRAP_WORKAROUND
	__attribute__((section(".lpc18xx_image_top_text")))
#endif
	wdt_enable(void)
{
	/*
	 * TBD
	 */

	return;
}
