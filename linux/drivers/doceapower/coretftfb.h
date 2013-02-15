/*
 * coretftfb.h - interface to the framebuffer driver for the CoreTFT IP
 *
 * Author: Vladimir Khusainov, vlad@emcraft.com
 * Copyright 2011 Emcraft Systems
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _LINUX_CORETFTFB_H
#define _LINUX_CORETFTFB_H 1

#include <linux/ioctl.h>

#ifdef __KERNEL__

/*
 * struct coretftfb_plat - Nokia6100 LCD platform data
 * @reset:           		GPIO to control LCD reset (0-31, corresponding
 * 				to MSS_GPIO[0-31]).
 */
struct coretftfb_plat {
	int reset;
};

#define LCD_BASE    (0x60000000 +               \
                     ((3 - 1) * 0x4000000))
#define LCD_REG     (*(volatile u16 *)LCD_BASE)
#define LCD_RAM     (*((volatile u16 *)LCD_BASE + 1))


#ifndef FBIO_WAITFORVSYNC
    #define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif

static int coretftfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);
static void set_standby_mode(unsigned long arg);

#endif /* __KERNEL__ */

#endif /* _LINUX_CORETFTFB_H */
