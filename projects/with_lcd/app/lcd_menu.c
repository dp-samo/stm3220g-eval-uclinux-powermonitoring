/*
 * lcd_menu.c - A demo application for the Nokia 6100 LCD.
 * This application implements a demo user menu, which is a part of
 * a demo application for the Nokia LCD.
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
 * Services to print debug and error messages
 */
#define d_print(level, fmt, args...)					\
	if (lcd_menu_debug >= level) fprintf(stdout, "%s:[%s]: " fmt,	\
		  	     	             lcd_menu_name, __func__, ## args)
#define d_error(fmt, args...)						\
	if (lcd_menu_debug >= 1) fprintf(stderr, "%s: " fmt, 		\
				         lcd_menu_name, ## args)

/*
 * Define geometry of the menu graphics
 */
#define MENU_COL	3
#define MENU_ROW	2
#define MENU_ALL	(MENU_ROW * MENU_COL)
#define MENU_X0		6
#define MENU_Y0		35
#define MENU_W		34
#define MENU_H		30
#define MENU_WD		10
#define	MENU_HD		13

/*
 * Globals (local to this module, though)
 */
static char *lcd_menu_name;
static int lcd_menu_debug = 1;

static unsigned short lcd_menu_bg = BLACK;
static unsigned short lcd_menu_fg = WHITE;

static char *lcd_menu_bg_img = NULL;
static struct lcd_bmp_info lcd_menu_bg_bmp;

static char *lcd_menu_items[MENU_ALL * 2] = {
	"bmp/log.bmp",		"bmp/log_active.bmp",
	"bmp/ether.bmp",	"bmp/ether_active.bmp",
	"bmp/users.bmp",	"bmp/users_active.bmp",
	"bmp/serial.bmp",	"bmp/serial_active.bmp",
	"bmp/settings.bmp",	"bmp/settings_active.bmp",
	"bmp/files.bmp",	"bmp/files_active.bmp",
};
static struct lcd_bmp_info lcd_menu_items_bmp[MENU_ALL * 2];
static char *lcd_menu_items_str[MENU_ALL] = {
	" Log",
	"Ether",
	"Users",
	"Serial",
	"Setup",
	"Files",
};	

/*
 * Create context for the menu 
 * @returns		0->success, <0->error code
 */
static int lcd_menu_init(void)
{
	int i, j;
	char *n;
	int ret = 0;

	/*
 	 * Get access to the menu image bitmaps
 	 */
	for (i = 0; 
	     i < sizeof(lcd_menu_items_bmp)/sizeof(lcd_menu_items_bmp[0]);
	     i++) {
		n = lcd_menu_items[i];
		ret = lcd_bmp_open(n, &lcd_menu_items_bmp[i]);
		if(ret < 0) {
			d_error("failed to open bmp: %s\n", n);
			goto Done_release_bmp;
		}
	}
	
	/*
 	 * If we do not use a background bmp, all is good already
 	 */
	if (!lcd_menu_bg_img) {
		goto Done_release_nothing;
	}

	/*
 	 * Get access to the underlying image bitmap
 	 */
	ret = lcd_bmp_open(lcd_menu_bg_img, &lcd_menu_bg_bmp);
	if(ret < 0) {
		d_error("failed to open bmp: %s\n", lcd_menu_bg_img);
		goto Done_release_nothing;
	}

	/*
	 * Here, means success
	 */
	goto Done_release_nothing;

Done_release_bmp:
	for (j = 0; j < i; j++) {
		lcd_bmp_close(&lcd_menu_items_bmp[j]);
	}

Done_release_nothing:
	d_print(2, "ret=%d\n", ret);
	return ret;
}

/*
 * Get rid of the menu context
 * @returns		0->success, <0->error code
 */
static int lcd_menu_done(void)
{
	int i;
	int ret = 0;

	for (i = 0; 
	     i < sizeof(lcd_menu_items_bmp)/sizeof(lcd_menu_items_bmp[0]);
             i++) {
		lcd_bmp_close(&lcd_menu_items_bmp[i]);
	}

	/*
 	 * If we do not use a backgound bmp, all is good already
 	 */
	if (!lcd_menu_bg_img) {
		goto Done_release_nothing;
	}
	lcd_bmp_close(&lcd_menu_bg_bmp);

Done_release_nothing:
	d_print(2, "ret=%d\n", ret);
	return ret;
}

/*
 * Draw a menu entry
 * @param i		Menu item
 * @param active	Is the active menu item?
 * @returns		0->success, <0->error code
 */
static void lcd_menu_box(int i, int active)
{
	/*
	 * Calculate geometry of the box
	 */
	i--;
	int dx = i / MENU_ROW;
	int dy = i % MENU_ROW;
	int x1 = MENU_X0 + (dx * (MENU_W + MENU_WD));
	int y1 = MENU_Y0 + (dy * (MENU_H + MENU_HD)); 
	int x2 = x1 + MENU_W;
	int y2 = y1 + MENU_H;

	/*
	 * Put the box graphics into framebuffer
	 */
	if (lcd_menu_bg_img) {
		lcd_bmp_bmp(x1, y1, 
			&lcd_menu_items_bmp[i*2 + active], &lcd_menu_bg_bmp);
		lcd_str_bmp(lcd_menu_items_str[i], x1+1, y1-9, 0,
		 	lcd_menu_fg, &lcd_menu_bg_bmp);
	}
	else {
		lcd_bmp(x1, y1, &lcd_menu_items_bmp[i*2 + active],
			lcd_menu_bg);
		lcd_str(lcd_menu_items_str[i], x1+1, y1-9, 0,
		 	lcd_menu_fg, lcd_menu_bg);
	}

	/*
	 * Sync up with the hardware
	 */
	lcd_sync();
}

/*
 * Menu iteractions
 * @param c		Currently active menu entry
 * @returns		Newly active menu entry
 */
static int lcd_menu_do(int c)
{
	char str[128];
	int new, cur;
	int i;

	/*
 	 * Draw the menu anew
 	 * TO-DO: there must be a call to lcd_box to clear the menu area
 	 */
	for (i = 1, cur = c; i <= MENU_ALL; i++) {
		lcd_menu_box(i, i == cur);
		
	}	

	/*
	 * Menu iteration
	 */
	new = cur = c;
	while (1) {
		/*
		 * Get a user command
		 */
		printf("> "); 
		fflush(stdout);
		fgets(str, sizeof(str), stdin);

		/*
		 * Parse the command and perform appripriate action
		 */
		if (str[0] == 'p') {
			/*
 			 * Action: get out
 			 */
			goto Done;
		}
		else if (str[0] == 'u' || str[0] == '\n') {
			/*
 			 * Action: go to next
 			 */
			new = cur == MENU_ALL ? 1 : cur + 1;
		}
		else if (str[0] == 'd') {
			/*
 			 * Action: go to previous
 			 */
			new = cur == 1 ? MENU_ALL : cur - 1;
		}

		/*
		 * Active has changed -> redraw affected entries
		 */
		lcd_menu_box(cur, 0);
		lcd_menu_box(new, 1);
		cur = new;
	}

Done:
	d_print(2, "ret=%d\n", cur);
	return cur;
}

/*
 * Menu entry chosen
 * @param c		Currently active menu entry
 */
static void lcd_menu_choice(int c)
{
	char str[64];
	char *s1, *s2;
	int x1, x2, x3, x4, y1, y2, y3, y4;
	unsigned short f1, f2;

	x1 = 4; x2 = lcd_xres()-4;
	y1 = 25; y2 = 110;
	x3 = 11; y3 = 70;
	x4 = x3+35; y4 = y3-20;
	f1 = WHITE; f2 = PINK;

	/*
	 * Clear the menu box
	 */
	if (lcd_menu_bg_img) {
		lcd_box_bmp(x1, y1, x2, y2, &lcd_menu_bg_bmp);
	}
	else {
		lcd_box(x1, y1, x2, y2, lcd_menu_bg);
	}
	lcd_sync();

	/*
	 * Draw the entry specific stuff
	 */
	s1 = "You've chosen: ";
	s2 = lcd_menu_items_str[c-1];
	if (lcd_menu_bg_img) {
		lcd_str_bmp(s1, x3, y3, 2, f1, &lcd_menu_bg_bmp);
		lcd_str_bmp(s2, x4, y4, 2, f2, &lcd_menu_bg_bmp);
	}
	else {
		lcd_str(s1, x3, y3, 2, f1, lcd_menu_bg);
		lcd_str(s2, x4, y4, 2, f2, lcd_menu_bg);
	}
	lcd_sync();

	/*
	 * Get a user command
	 */
	printf(">> "); 
	fflush(stdout);
	fgets(str, sizeof(str), stdin);

	/*
	 * Clear the menu again
	 */
	if (lcd_menu_bg_img) {
		lcd_box_bmp(x1, y1, x2, y2, &lcd_menu_bg_bmp);
	}
	else {
		lcd_box(x1, y1, x2, y2, lcd_menu_bg);
	}
	lcd_sync();

	d_print(2, "c=%d\n", c);
}
	 
/*
 * Entry point
 * @param argc		Number of arguments
 * @param argv		Arguments array
 * @returns		0->success, <0->error code
 */
int main(int argc, char **argv)
{
	int c;
	int ch;
	int ret = 0;

	/*
	 * Create context for the application
	 */
	lcd_menu_name = argv[0];

	/*
	 * Parse the parameters
	 */
	while ((ch = getopt(argc, argv, "i:b:f:")) != -1) {
		switch (ch) {
		case 'i':
			lcd_menu_bg_img = optarg;
			break;
		case 'b':
			ret = lcd_color_parse(optarg, &lcd_menu_bg);
			if (ret < 0) {
				d_error("incorrect color %s\n", optarg);
				goto Done_release_nothing;
			}
			break;
		case 'f':
			ret = lcd_color_parse(optarg, &lcd_menu_fg);
			if (ret < 0) {
				d_error("incorrect color %s\n", optarg);
				goto Done_release_nothing;
			}
			break;
		}
	}

	/*
 	 * Get access to the LCD device driver
 	 */
	ret = lcd_init("/dev/fb0", "lcd_api", lcd_menu_debug);
	if (ret < 0) {
		d_error("failed to init LCD: %s\n", strerror(errno));
		goto Done_release_nothing;
	}

	/*
 	 * Prepare to run the menu
 	 */
	ret = lcd_menu_init();
	if (ret < 0) {
		d_error("failed to setup init menu\n");
		goto Done_release_lcd;
	}

	/*
 	 * Run the menu
 	 */
	c = 1;
	while (1) {
		c = lcd_menu_do(c);
		lcd_menu_choice(c);
	}

Done_release_menu:
	lcd_menu_done();
Done_release_lcd:
	lcd_close();
Done_release_nothing:
	d_print(2, "ret=%d\n", ret);
	return ret;
}
