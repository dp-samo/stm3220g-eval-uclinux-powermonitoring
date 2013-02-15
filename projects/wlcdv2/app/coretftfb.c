/*
 * coretftfb.c - framebuffer driver for the Nokia6100 LCD.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include "coretftfb.h"

/*
 * Driver verbosity level: 0->silent; >0->verbose (the higher,
 * the more verbose, up to 4).
 */
static int coretftfb_debug = 0;

/*
 * User can change verbosity of the driver when installing the module
 */
module_param(coretftfb_debug, int, S_IRUGO);
MODULE_PARM_DESC(coretftfb_debug, "CoreTFP framebeffer verbosity level");

/*
 * ... or when booting the kernel if the driver is linked in statically
 */
#ifndef MODULE
static int __init coretftfb_debug_setup(char *str)
{
	get_option(&str, &coretftfb_debug);
	return 1;
}
__setup("coretftfb_debug=", coretftfb_debug_setup);
#endif

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)					\
	if (coretftfb_debug >= level) printk(KERN_INFO "%s: " fmt,	\
		  			       __func__, ## args)
/*
 * CoreTFT control & status registers
 */
struct coretft {
	unsigned int cntrl;
	unsigned int dflt_rgb;
	unsigned int start_buf;
	unsigned int end_buf;
	unsigned int status;
	unsigned int start_buf_updtd;
	unsigned int end_buf_updtd;
};

/*
 * Base address of CSR for a particular instance of CoreTFT.
 * This is defined by FPGA logic.
 */
#define CORETFT_REGS_BASE		0x40050900
#define CORETFT		((volatile struct coretft *)(CORETFT_REGS_BASE))

/*
 * Define the geometry of the LCD framebuffer.
 * Resolution of the dsiplay in pixels:
 */
#define CORETFTFB_X_RS	320
#define CORETFTFB_Y_RS	240
#define CORETFTFB_RES	16
#define CORETFTFB_SZ	(CORETFTFB_X_RS * CORETFTFB_Y_RS * CORETFTFB_RES / 8)

/*
 * struct coretftfb_par - Device-specific data for CoreTFT
 * @reset	GPIO to control LCD reset (0-31)
 * @lock:	Exclusive access to critical code
 * @buffer:	Main software framebuffer. This is what user-space
 * 		applications get access to via mmap().
 */
struct coretftfb_par {
	int 			reset;
	spinlock_t		lock;
	unsigned short		*buffer;
	int freq;
	struct task_struct	*task;
        struct fb_var_screeninfo* var_tft;
};

/*
 * Busy-loop delay for a specified number of jiffies
 * @param n		Jiffies to delay for
 */
static void coretftfb_delay(int n)
{
	int end = get_jiffies_64() + n;
	int now;

	do
	{
		now = get_jiffies_64();
		cpu_relax();
	} while(now < end);
}

#if 0
/* 
 * SmartFusion IOMUX and IOCFG hardware interfaces
 */
#define MSS_IOMUX_BASE		0xE0042100
struct mss_iomux {
	unsigned int		cr[83];
};
#define MSS_IOMUX		((struct mss_iomux *)(MSS_IOMUX_BASE))

#define MSS_GPIO_BASE		0x40013000
struct mss_gpio {
	unsigned int		cfg[32];
	unsigned int 		irq;
	unsigned int 		in;
	unsigned int 		out;
	
};
#define MSS_GPIO		((struct mss_gpio *)(MSS_GPIO_BASE))
#endif

/*
 * Do the one-time initialization of the CoreTFT controller. 
 * @param p		CoreTFT data structure
 */
static void coretftfb_init(struct coretftfb_par *p)
{
	d_printk(2, "ok\n");
}

/*
 * Reset CoreTFT to a known state
 * @param p		CoreTFT data structure
 */
static void coretftfb_reset(struct coretftfb_par *p)
{
	coretftfb_delay(1);

	d_printk(2, "reset=%d\n", p->reset);
}

/*
 * Framebuffer ops data structure.
 * Everything is actually done by the generic code.
 */
static struct fb_ops coretftfb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fb_sys_read,
	.fb_write	= fb_sys_write,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
};

/*
 * Framebuffer fix data structure
 */
static struct fb_fix_screeninfo coretftfb_fix __devinitdata = {
	.id		= "coretftfb",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.accel		= FB_ACCEL_NONE,
};

/*
 * Framebuffer var data structure
 */
static struct fb_var_screeninfo coretftfb_var __devinitdata = {
	.xres		= CORETFTFB_X_RS,
	.yres		= CORETFTFB_Y_RS,
	.xres_virtual	= CORETFTFB_X_RS,
	.yres_virtual	= CORETFTFB_Y_RS * 2,
	.height		= -1,
	.width		= -1,
	.vmode		= FB_VMODE_INTERLACED,
	.bits_per_pixel	= 16,
	.red		= { 11, 5, 0 },
	.green		= { 5, 6, 0 },
	.blue		= { 0, 5, 0 },
	.nonstd		= 0,
};

#define LCD_BASE	(0x60000000 +				\
			 ((3 - 1) * 0x4000000))
#define LCD_REG		(*(volatile u16 *)LCD_BASE)
#define LCD_RAM		(*((volatile u16 *)LCD_BASE + 1))

/*
 * LCD driver kernel thread.
 * @param n		LCD-specific data structure
 * @returns		0->success, <0->error code
 */
static int stm32lcd_task(void *n)
{
	struct coretftfb_par *p = (struct coretftfb_par *)n;
	int ret = 0, i;
	unsigned short *b = p->buffer;

	/*
 	 * Increase priority of this thread. It is important
 	 * that it does its job quickly.
 	 */
	set_user_nice(current, 5);

	LCD_REG = 0;
	printk("lcd id %hx, %hx, %hx\n", LCD_RAM, LCD_RAM, LCD_RAM);

	/*
 	 * In an endless loop, until the driver is done
 	 * Re-fresh the LCD, on an as-needed basis.
 	 */
	while (!kthread_should_stop()) {
#if 1
		/* 
		 * Sync up the LCD hardware with the software buffer
		 */
		b = p->buffer + (*p->var_tft).yoffset;
		LCD_REG = 32;
		LCD_RAM = 0;
		LCD_REG = 33;
		LCD_RAM = 319;
		LCD_REG = 34;
		for (i = 0; i < CORETFTFB_X_RS*CORETFTFB_Y_RS; i++) {
			LCD_RAM = *b++;
		}
		/*
 		 * Wait for a while.
 		 */
#endif
		schedule_timeout_interruptible(HZ / p->freq);
	}

	d_printk(2, "ret=%d\n", ret);
	return ret;
}

/*
 * Framebuffer device instance probe & initialization
 * @param d		platfrom device
 * @returns		0->success, <0->error code
 */
static int __devinit coretftfb_probe(struct platform_device *d)
{
	struct fb_info *i;
	struct coretftfb_par *p;
	struct coretftfb_plat *f = d->dev.platform_data;
	char *n = coretftfb_fix.id;
	int ret = 0;
	unsigned int v;

	/*
 	 * Allocate a framebuffer info structure for this CoreTFT
 	 */
	i = framebuffer_alloc(sizeof(struct coretftfb_par), &d->dev);
	if (!i) {
		ret = -ENOMEM;
		dev_err(&d->dev, "allocation of framebuffer info failed"
			" for %s\n", n);
		goto Done_release_nothing;
	}

	/*
 	 * Store the framebuffer handle in the platform device
 	 */
	dev_set_drvdata(&d->dev, i);

	/*
 	 * Get access to the device-specific data structure (which
 	 * is a part of the framebuffer data structure).
 	 */
	p = i->par;

	/*
 	 * Allocate the framebuffer for the CoreTFT.
 	 * This has to be page-aligned. 
 	 */
//	p->buffer = 0x64000000UL+32*1024*1024-320*240*2*2;
        p->buffer = alloc_pages_exact(CORETFTFB_SZ * 2, GFP_DMA | __GFP_ZERO);
	if (!p->buffer) {
		ret = -ENOMEM;
		dev_err(&d->dev, "allocation of buffer failed for %s\n", n);
		goto Done_release_framebuffer;
	}
#if 0
	v = (long)p->buffer | 0x80000000UL;
	//printk("start_buf=%x\n", v);
	CORETFT->start_buf = v;
	udelay(1000);
	while (CORETFT->start_buf_updtd != v) { /*printk("start_buf=%x\n", CORETFT->start_buf_updtd);*/ break; }

	v = CORETFTFB_SZ;
	//printk("end_buf=%x\n", v);
	CORETFT->end_buf = v;
	while (CORETFT->end_buf_updtd != ((long)p->buffer | 0x80000000UL) + v);

	//printk("status=%08x endbuf %08x\n", CORETFT->status, CORETFT->end_buf_updtd);

	memset((void *)p->buffer, 255, CORETFTFB_SZ);

	CORETFT->cntrl = CORETFT->cntrl | (1 << 0); // swreset off
	CORETFT->cntrl = CORETFT->cntrl | (1 << 6); // ctrlen on
	CORETFT->cntrl = CORETFT->cntrl | (1 << 8); // dmaen on
	CORETFT->cntrl = CORETFT->cntrl | (1 << 1); // pwr on

	//printk("ctrl=%08x\n", CORETFT->cntrl);
	udelay(1000);
	//printk("status=%08x\n", CORETFT->status);

	while (!(CORETFT->status & (1 << 1)));
	//printk("power done\n");
	while (!(CORETFT->status & (1 << 6)));
	//printk("ctrl enabled\n");
	while (!(CORETFT->status & (1 << 5)));
	//printk("dma enabled\n");

	CORETFT->cntrl = CORETFT->cntrl | (1 << 5); // backlight on
	CORETFT->cntrl = CORETFT->cntrl | (1 << 7); // memen on
#endif
	/*
 	 * Fill in the framebuffer data structure
 	 */
	i->fbops = &coretftfb_ops;
	i->var = coretftfb_var;
	i->fix = coretftfb_fix;
	i->screen_base = (char *) p->buffer;
	i->screen_size = CORETFTFB_SZ;
	i->flags = FBINFO_DEFAULT;
	i->fix.smem_start = virt_to_phys(p->buffer);
	i->fix.smem_len	= CORETFTFB_SZ * 2;
	i->fix.line_length = CORETFTFB_SZ / CORETFTFB_Y_RS;

	/*
 	 * Remember the platform configuration
 	 */
	p->reset = f->reset;
	
	/*
	 * Do the hardware initialization and reset of CoreTFT
	 */	
	coretftfb_init(p);
	coretftfb_reset(p);

	/*
 	 * Register the framebuffer driver
 	 */
	ret = register_framebuffer(i);
	if (ret < 0) {
		dev_err(&d->dev, "failed to register framebuffer for %s\n", n);
		goto Done_release_buffer;
	}

	p->task = kthread_run(stm32lcd_task, p, "stm32lcd");
	p->freq = 30;

        p->var_tft = &coretftfb_var;
	/*
 	 * Tell the world we have arrived
 	 */
	dev_info(&d->dev, "fb%d: %s (CoreTFT) framebuffer\n", i->node, n);

	/*
 	 * If here, we have been successful
 	 */
	goto Done_release_nothing;

Done_release_buffer:
	free_pages_exact(p->buffer, CORETFTFB_SZ);
Done_release_framebuffer:
	framebuffer_release(i);
Done_release_nothing:
	d_printk(1, "name=%s,d=%p,ret=%d\n", n, d, ret);
	return ret;
}

/*
 * Device instance shutdown
 * @param d		platfrom device
 * @returns		0->success, <0->error code
 */
#if 0
static int __devexit coretftfb_remove(struct platform_device *d)
#else
static int coretftfb_remove(struct platform_device *d)
#endif
{
	struct coretftfb_par *p;
	struct fb_info *i = dev_get_drvdata(&d->dev);
	int ret = 0;

	if (i) {
		p = i->par;
		unregister_framebuffer(i);
		free_pages_exact(p->buffer, CORETFTFB_SZ);
		framebuffer_release(i);
	}

	d_printk(1, "d=%p,i=%p,ret=%d\n", d, i, ret);
	return ret;
}

/* 
 * Platform device driver data structure for this driver
 */
static struct platform_driver coretftfb_driver = {
	.driver 	= {
		.name		= "coretftfb",
		.owner		= THIS_MODULE,
	},
	.probe		= coretftfb_probe,
#if 0
	.remove		= __devexit_p(coretftfb_remove),
#else
	.remove		= coretftfb_remove,
#endif
};

/*
 * Module start-up
 * @returns		0->success, <0->error code
 */
static int __init coretftfb_init_module(void)
{
	int ret;

	/*
	 * Register the device driver
	 */
	ret = platform_driver_register(&coretftfb_driver);

	d_printk(1, "ret=%d\n", ret);
	return ret;
}

/*
 * Module shut-down
 */
static void __exit coretftfb_cleanup_module(void)
{

	/*
	 * Unregister the device driver
	 */
	platform_driver_unregister(&coretftfb_driver);

	d_printk(1, "%s\n", "clean-up successful");
}

/*
 * Module registration data
 */
module_init(coretftfb_init_module);
module_exit(coretftfb_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Khusainov, vlad@emcraft.com");
MODULE_DESCRIPTION("CoreTFT framebuffer");

