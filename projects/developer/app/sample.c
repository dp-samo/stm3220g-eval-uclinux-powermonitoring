/*
 * sample.c - The simplest loadable kernel module.
 * Intended as a template for development of more
 * meaningful kernel modules.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

/*
 * Driver verbosity level: 0->silent; >0->verbose
 */
static int sample_debug = 0;

/*
 * User can change verbosity of the driver
 */
module_param(sample_debug, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(sample_debug, "Sample driver verbosity level");

/*
 * Service to print debug messages
 */
#define d_printk(level, fmt, args...)				\
	if (sample_debug >= level) printk(KERN_INFO "%s: " fmt,	\
					__func__, ## args)

/*
 * Device major number
 */
static uint sample_major = 166;

/*
 * User can change the major number
 */
module_param(sample_major, uint, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(sample_major, "Sample driver major number");

/*
 * Device name
 */
static char * sample_name = "sample";

/*
 * Device access lock. Only one process can access the driver at a time
 */
static int sample_lock = 0;

/*
 * Device "data"
 */
static char sample_str[] = "This is the simplest loadable kernel module\n";
static char *sample_end;

/*
 * Device open
 */
static int sample_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	/*
	 * One process at a time
	 */
	if (sample_lock ++ > 0) {
		ret = -EBUSY;
		goto Done;
	}

	/*
 	 * Increment the module use counter
 	 */
	try_module_get(THIS_MODULE);

	/*
 	 * Do open-time calculations
 	 */
	sample_end = sample_str + strlen(sample_str);

Done:
	d_printk(2, "lock=%d\n", sample_lock);
	return ret;
}

/*
 * Device close
 */
static int sample_release(struct inode *inode, struct file *file)
{
	/*
 	 * Release device
 	 */
	sample_lock = 0;

	/*
 	 * Decrement module use counter
 	 */
	module_put(THIS_MODULE);

	d_printk(2, "lock=%d\n", sample_lock);
	return 0;
}

/* 
 * Device read
 */
static ssize_t sample_read(struct file *filp, char *buffer,
			 size_t length, loff_t * offset)
{
	char * addr;
	unsigned int len = 0;
	int ret = 0;

	/*
 	 * Check that the user has supplied a valid buffer
 	 */
	if (! access_ok(0, buffer, length)) {
		ret = -EINVAL;
		goto Done;
	}

	/*
 	 * Get access to the device "data"
 	 */
	addr = sample_str + *offset;

	/*
	 * Check for an EOF condition.
	 */
	if (addr >= sample_end) {
		ret = 0;
		goto Done;
	}

	/*
 	 * Read in the required or remaining number of bytes into
 	 * the user buffer
 	 */
	len = addr + length < sample_end ? length : sample_end - addr;
	strncpy(buffer, addr, len);
	*offset += len;
	ret = len;

Done:
	d_printk(3, "length=%d,len=%d,ret=%d\n", length, len, ret);
	return ret;
}

/* 
 * Device write
 */
static ssize_t sample_write(struct file *filp, const char *buffer,
			  size_t length, loff_t * offset)
{
	d_printk(3, "length=%d\n", length);
	return -EIO;
}

/*
 * Device operations
 */
static struct file_operations sample_fops = {
	.read = sample_read,
	.write = sample_write,
	.open = sample_open,
	.release = sample_release
};

static int __init sample_init_module(void)
{
	int ret = 0;

	/*
 	 * check that the user has supplied a correct major number
 	 */
	if (sample_major == 0) {
		printk(KERN_ALERT "%s: sample_major can't be 0\n", __func__);
		ret = -EINVAL;
		goto Done;
	}

	/*
 	 * Register device
 	 */
	ret = register_chrdev(sample_major, sample_name, &sample_fops);
	if (ret < 0) {
		printk(KERN_ALERT "%s: registering device %s with major %d "
				  "failed with %d\n",
		       __func__, sample_name, sample_major, ret);
		goto Done;
	}
	
Done:
	d_printk(1, "name=%s,major=%d\n", sample_name, sample_major);

	return ret;
}
static void __exit sample_cleanup_module(void)
{
	/*
	 * Unregister device
	 */
	unregister_chrdev(sample_major, sample_name);

	d_printk(1, "%s\n", "clean-up successful");
}

module_init(sample_init_module);
module_exit(sample_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Khusainov, vlad@emcraft.com");
MODULE_DESCRIPTION("Sample device driver");
