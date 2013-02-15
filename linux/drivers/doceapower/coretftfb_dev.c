/*
 * coretftfb_dev.c - Registration of platform devices for CoreTFP
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/clkdev.h>
#include <mach/platform.h>
#include "coretftfb.h"

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)					\
	if (coretftfb_dev_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					       __func__, ## args)

/*
 * Service to print error messages
 */
#define d_error(fmt, args...)						\
	printk(KERN_ERR "%s:%d,%s: " fmt, __FILE__, __LINE__, __func__,	\
	       ## args)

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int coretftfb_dev_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(coretftfb_dev_debug, int, S_IRUGO);
MODULE_PARM_DESC(coretftfb_dev_debug,
	"CoreTFT device registration verbosity level");

/*
 * Platform info for the CoreTFT instance
 * TO-DO: this structure needs to be extended to 
 * contain all info specific to the platform device such
 * as GPIO user to power-on/off the LCD, memory width, etc
 */
static struct coretftfb_plat coretftfb_plat = {
	.reset			= 22,
};

/*
 * Platform resources for the CoreTFP instance
 * TO-DO: this needs to be updated to define a memory region
 * allocated to the CoreTFT instance.
 * The framebuffer driver needs to be updated to
 * retrieve this data from the platform device and
 * use it as a pointer to the CoreTFP registers.
 */
static struct resource coretftfb_resources[] = {
	{
		.start		= 0, // # 0x16DE8237FE,
		.end		= 0, // # 0x16DE848FFE,
		.flags		= IORESOURCE_MEM,
	},
};

/*
 * Unless this callback is defined, the driver core code
 * complains about no release being defined for the device
 */
static void coretftfb_dev_release(struct device *dev)
{
	d_printk(1, "%s\n", "ok");
}

/*
 * Platform device for CoreTFP
 */
static struct platform_device coretftfb_dev = {
	.name			= "coretftfb",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(coretftfb_resources),
	.resource		= coretftfb_resources,
	.dev 			= {
		.platform_data	= &coretftfb_plat,
		.release	= coretftfb_dev_release,
	},
};

/*
 * Driver init
 */
static int __init coretftfb_dev_init_module(void)
{
	int ret = 0;

	/*
	 * Register CoreTFT platform device
	 */
	ret = platform_device_register(&coretftfb_dev);
	if (ret < 0) {
		d_error("can't register CoreTFT\n");
		goto Done;
	}

Done:
	d_printk(1, "ret=%d\n", ret);
	return ret;
}

/*
 * Driver clean-up
 */
static void __exit coretftfb_dev_cleanup_module(void)
{
	/*
	 * Unregister the CoreTFT platform device
	 */
	platform_device_unregister(&coretftfb_dev);

	d_printk(1, "%s\n", "ok");
}

module_init(coretftfb_dev_init_module);
module_exit(coretftfb_dev_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Khusainov, vlad@emcraft.com");
MODULE_DESCRIPTION("CoreTFT device registration");

