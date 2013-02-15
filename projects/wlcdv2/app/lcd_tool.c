/*
 * lcd_tool.c - A simple tool to draw things on the Nokia 6100 LCD.
 * Possible actions are:
 * - draw a rectangle
 * - draw a bmp image
 * - draw a string.
 * Support for drawing on top of a bmp image is supported. 
 * 2011, Vladimir Khusainov, Emcraft Systems
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "lcd_api.h"

/*
 * Globals (local to this module, though)
 */
static char *lcd_tool_name;
static int lcd_tool_debug;
static int lcd_tool_do_box;
static int lcd_tool_do_str;
static int lcd_tool_do_image;
static int lcd_tool_x;
static int lcd_tool_y;
static int lcd_tool_z;
static int lcd_tool_w;
static unsigned short lcd_tool_fg;
static unsigned short lcd_tool_bg;
static int lcd_tool_font;
static char *lcd_tool_str;
static char *lcd_tool_image;
static char *lcd_tool_bmp;

/*
 * Services to print debug and error messages
 */
#define d_print(level, fmt, args...)					\
	if (lcd_tool_debug >= level) fprintf(stdout, "%s:[%s]: " fmt,	\
		  	    	             lcd_tool_name, __func__, ## args)
#define d_error(fmt, args...)						\
	if (lcd_tool_debug >= 1) fprintf(stderr, "%s: " fmt, 		\
				         lcd_tool_name, ## args)

/*
 * Assign appropriate default values to the globals.
 */
static void lcd_tool_defaults(void)
{
	lcd_tool_debug = 1;
	lcd_tool_do_str = 0;
	lcd_tool_do_box = 0;
	lcd_tool_do_image = 0;
	lcd_tool_x = 0;
	lcd_tool_y = 0;
	lcd_tool_z = -1;
	lcd_tool_w = -1;
	lcd_tool_fg = WHITE;
	lcd_tool_bg = BLACK;
	lcd_tool_font = MEDIUM;
	lcd_tool_str = "";
	lcd_tool_image = NULL;
	lcd_tool_bmp = NULL;
}

/*
 * Parse the command string
 * @param argc		Number of arguments
 * @param argv		Arguments array
 * @returns		0->success, <0->error code
 */
static int lcd_tool_param(int argc, char **argv)
{
	int ch;
	int ret = 0;

	while ((ch = getopt(argc, argv, "d:BS:x:y:z:w:f:b:t:i:")) != -1) {
		switch (ch) {
		case 'd':
			lcd_tool_debug = atoi(optarg);
			break;
		case 'B':
			lcd_tool_do_box = 1;
			break;
		case 'S':
			lcd_tool_do_str = 1;
			lcd_tool_str = optarg;
			break;
		case 'x':
			lcd_tool_x = atoi(optarg);
			break;
		case 'y':
			lcd_tool_y = atoi(optarg);
			break;
		case 'z':
			lcd_tool_z = atoi(optarg);
			break;
		case 'w':
			lcd_tool_w = atoi(optarg);
			break;
		case 'b':
			ret = lcd_color_parse(optarg, &lcd_tool_bg);
			if (ret < 0) {
				d_error("incorrect color %s\n", optarg);
				goto Done_release_nothing;
			}
			break;
		case 'f':
			ret = lcd_color_parse(optarg, &lcd_tool_fg);
			if (ret < 0) {
				d_error("incorrect color %s\n", optarg);
				goto Done_release_nothing;
			}
			break;
		case 't':
			ret = lcd_font_parse(optarg, &lcd_tool_font);
			if (ret < 0) {
				d_error("incorrect font %s\n", optarg);
				goto Done_release_nothing;
			}
			break;
		case 'i':
			lcd_tool_image = optarg;
			break;
		default:
			ret = -EIO;
			goto Done_release_nothing;
		}
	}

Done_release_nothing:
	d_print(2, "ret=%d\n", ret);
	return ret;
}
	
/*
 * Entry point
 * @param argc		Number of arguments
 * @param argv		Arguments array
 * @returns		0->success, <0->error code
 */
int main(int argc, char **argv)
{
	int ret = 0;

	/*
	 * Create context for the application
	 */
	lcd_tool_name = argv[0];

	/*
	 * Assign default values to the globals
	 */
	lcd_tool_defaults();

	/*
	 * Parse the parameters
	 */
	ret = lcd_tool_param(argc, argv);
	if (ret < 0) {
		d_error("wrong parameters\n", strerror(errno));
		goto Done_release_nothing;
	}

	/*
 	 * Get access to the LCD device driver
 	 */
	ret = lcd_init("/dev/fb0", "lcd_api", lcd_tool_debug);
	if (ret < 0) {
		d_error("failed to init LCD: %s\n", strerror(errno));
		goto Done_release_nothing;
	}

	/*
 	 * Adjust upper right corner coordinates, if not supplied by user
 	 */
	if (lcd_tool_z < 0) {
		lcd_tool_z = lcd_xres() - 1;
	}	
	if (lcd_tool_w < 0) {
		lcd_tool_w = lcd_yres() - 1;
	}	

	/*
	 * Perform the specified acction (only one action is done)
	 */
	if (lcd_tool_do_box) {
		if (lcd_tool_image != NULL) {
			ret = lcd_img(lcd_tool_image, lcd_tool_x, lcd_tool_y);
		}
		else {
			ret = lcd_box(lcd_tool_x, lcd_tool_y,
					lcd_tool_z, lcd_tool_w, lcd_tool_bg);
		}
		if (ret < 0) {
			d_error("failed to draw a box: %d,%d,%d,%d\n", 
				lcd_tool_x, lcd_tool_y, lcd_tool_z, lcd_tool_w);
			goto Done_release_file;
		}
	}

	else if (lcd_tool_do_str) {
		if (lcd_tool_image != NULL) {
			ret = lcd_str_img(lcd_tool_str, lcd_tool_x, lcd_tool_y,
					lcd_tool_font, lcd_tool_fg,
					lcd_tool_image);
		}
		else {
			ret = lcd_str(lcd_tool_str, lcd_tool_x, lcd_tool_y,
					lcd_tool_font,
					lcd_tool_fg, lcd_tool_bg);
		}	
		if (ret < 0) {
			d_error("failed to draw a string: %s\n", lcd_tool_str);
			goto Done_release_file;
		}
	}

	else if (lcd_tool_do_image) {
		if (lcd_tool_image != NULL) {
			ret = lcd_img_img(lcd_tool_bmp, lcd_tool_x,
					lcd_tool_y, lcd_tool_image);
		}
		else {
			ret = lcd_img(lcd_tool_bmp, lcd_tool_x, lcd_tool_y);
		}	
		if (ret < 0) {
			d_error("failed to draw an image: %s\n",
			        lcd_tool_image);
			goto Done_release_file;
		}
	}

	/*
 	 * Sync up the frame buffer with the LCD hardware
 	 */
	lcd_sync();

Done_release_file:
	lcd_close();

Done_release_nothing:
	d_print(2, "ret=%d\n", ret);
	return ret;
}

