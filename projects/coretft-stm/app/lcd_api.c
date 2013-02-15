/*
 * lcd_api.c - Access functions for the Nokia 6100 LCD
 * 2011, Vladimir Khusainov, Emcraft Systems
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/nokia6100fb.h>
#include "lcd_font.h"
#include "lcd_api.h"

/*
 * Services to print debug and error messages
 */
#define d_print(level, fmt, args...)					\
	if (lcd_debug >= level) fprintf(stdout, "%s:[%s]: " fmt,	\
		  	    	        lcd_app_name, __func__, ## args)
#define d_error(fmt, args...)						\
	if (lcd_debug >= 1) fprintf(stderr, "%s: " fmt,			\
				    lcd_app_name, ## args)

/*
 * Some auxilary globals (local to this module though)
 */
static char *lcd_app_name;
static int lcd_debug;
static char *lcd_dev_name;
static int lcd_fd;
static int lcd_x_rs;
static int lcd_y_rs;
static int lcd_pixels;
static unsigned short *lcd_buf;

/*
 * LCD geometry
 */
#define LCD_X_RS		lcd_x_rs
#define LCD_Y_RS		lcd_y_rs
#define LCD_BUF_SZ		(LCD_X_RS * LCD_Y_RS * (lcd_pixels/8))

/*
 * Get access to the lcd
 * @param d		name of the framebuffer device
 * @param n		name to use for debug messages
 * @param dbg		verbosity (0->silent; up->more verbose)
 * @returns		0->success, <0->error code
 */
int lcd_init(char *d, char *n, int dbg)
{
	struct fb_var_screeninfo vinfo;
	int ret = 0;

	/*
	 * Provide the minimal execution context
	 */
	lcd_dev_name = d;
	lcd_app_name = n;
	lcd_debug = dbg;

	/*
 	 * Get access to the LCD device driver
 	 */
	lcd_fd = open(lcd_dev_name, O_RDWR);
	if (lcd_fd < 0) {
		d_error("failed to open %s: %s\n",
			lcd_dev_name, strerror(errno));
		ret = lcd_fd;
		goto Done_release_nothing;
	}

	/*
 	 * Obtain geometry of the framebuffer
 	 */
	ret = ioctl(lcd_fd, FBIOGET_VSCREENINFO, &vinfo);
	if (ret < 0) {
		d_error("failed to get resolution for %s: %s\n",
			lcd_dev_name, strerror(errno));
		goto Done_release_file;
        }
	lcd_x_rs = vinfo.xres;
	lcd_y_rs = vinfo.yres;
	lcd_pixels = vinfo.bits_per_pixel;

	/*
 	 * Create a mapping for the file
 	 */
	lcd_buf = mmap(NULL, LCD_BUF_SZ,
		       PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
	if (lcd_buf == MAP_FAILED) {
		d_error("failed to mmap %s: %s\n",
			lcd_dev_name, strerror(errno));
		ret = errno;
		goto Done_release_file;
	}
	d_print(2, "%s (%d bytes) mapped at 0x%p\n",
		lcd_dev_name, LCD_BUF_SZ, lcd_buf);

	/*
 	 * If here, succcess
 	 */
	goto Done_release_nothing;

Done_release_mmap:
	munmap(lcd_buf, LCD_BUF_SZ);
Done_release_file:
	close(lcd_fd);

Done_release_nothing:
	d_print(2, "nm=%s,dbg=%d,ret=%d\n", n, dbg, ret);
	return ret;
}

/*
 * Release the lcd
 * @returns		0->success, <0->error code
 */
int lcd_close(void)
{
	int ret;

	munmap(lcd_buf, LCD_BUF_SZ);
	ret = close(lcd_fd);

	d_print(2, "ret=%d\n", ret);
	return ret;
}

/*
 * Sync up the software framebuffer with the hardware lcd
 * @returns		0->success, <0->error code
 */
int lcd_sync(void)
{
	int ret = 0;

	/*
	 * Issue an explicit command to sync up the framebuffer with
	 * the LCD hardware. From the standards perspective,
	 * a more appopriate interface would be fsync(). The framebuffer
	 * framework does allow a framebuffer driver to define a fsync
	 * callback, however it is targetted towards running deferred
	 * I/O jobs and MMU-based implementations of trackable lists
	 * of affected framebuffer pages. This could be made work with
	 * the low-end LCDs too, however the overhead is a bit too much
	 * for low-end no-MMU microprocessors these low-end LCDs are
	 * typically used with. 
	 *
	 * Hence, an ioctl().
	 *
	 * With ioctl, we could use the existing FBIOPAN_DISPLAY,
	 * which makes little sense for the low-end LCDs, 
	 * but eventually calls a framebuffer-specific pan_display hook,
	 * if defined. However, this ioctl has the overhead of 
	 * copying a rather large data structure from the user space
	 * to the kernel and back, on each call. 
	 *
	 * So, we just use the rude-force approach below. Define no new 
	 * ioctl command and just pass a 0 as the command to 
	 * the framebuffer driver. At least with 2.6.30+ kernels,
	 * this successfully reaches a low-level framebuffer driver
	 * (via an ioctl callback).
	 */
	ret = ioctl(lcd_fd, FBIO_SYNC, 0);

	d_print(3, "ret=%d\n", ret);
	return ret;
}

/*
 * Get X resolution
 * @returns		X resolution
 */
int lcd_xres(void)
{
	return LCD_X_RS;
}

/*
 * Get Y resolution
 * @returns		Y resolution
 */
int lcd_yres(void)
{
	return LCD_Y_RS;
}

/*
 * Check a coordinate for validity
 * @param x		X coordinate
 * @param y		Y coordinate
 * @returns		0->success, <0->error code
 */
static int inline lcd_check_coordinate(int x, int y)
{
	int ret = 0;

	if (x < 0 || x >= LCD_X_RS ||
	    y < 0 || y >= LCD_Y_RS) ret = -EIO;

	return ret;
}

/*
 * Fill a pixel
 * @param p		Pointer to the frame buffer
 * @param x		Coordinate (x)
 * @param y		Coordinate (y)
 * @param c		Color
 * @returns		0->success, <0->error code
 */
static void inline lcd_pixel_set(void *p, int x, int y, unsigned short c)
{
	unsigned short *buf = (unsigned short *)p;

	buf[y * LCD_X_RS + x] = c;
}

/*
 * Get a pixel
 * @param p		Pointer to the frame buffer
 * @param x		Coordinate (x)
 * @param y		Coordinate (y)
 * @param c		Color
 * @returns		0->success, <0->error code
 */
static unsigned short inline lcd_pixel_get(void *p, int x, int y)
{
	unsigned short *buf = (unsigned short *)p;

	return buf[y * LCD_X_RS + x];
}

/*
 * Get access to an image
 * @param img		Image file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param i		Image handle (gets filled in by the function)
 * @returns		0->success, <0->error code
 */
int lcd_bmp_open(char * img, struct lcd_bmp_info *i)
{
	unsigned char *p;
	int ret = 0;

	/*
 	 * Get access to the image file
 	 */
	i->fd = open(img, O_RDONLY);
	if (i->fd < 0) {
		d_error("failed to open %s: %s\n", img, strerror(errno));
		ret = i->fd;
		goto Done_release_nothing;
	}

	/*
	 * Calculate the size of file. Not quite clean, but 
	 * this shouldn't fail, given the open has been successful.
	 */
	i->sz = lseek(i->fd, 0, SEEK_END);

	/*
 	 * Create a mapping for the file
 	 */
	i->file = mmap(NULL, i->sz, PROT_READ, MAP_PRIVATE, i->fd, 0);
	if (i->file == MAP_FAILED) {
		d_error("failed to mmap %s: %s\n", img, strerror(errno));
		ret  = errno;
		goto Done_release_file;
	}

	/*
	 * Pointer to the bmp file data
	 */
	p = i->file;

	/*
	 * Check the bmp file magic.
	 * ...
	 * Magic value as well as formwat of the bmp header
	 * are as defined by wikipedia.
	 */
	if (*((unsigned short *)p) != 0x4d42) {
		d_error("not a bmp file: %s\n", img);
		ret = -EIO;
		goto Done_release_mmap;
	}

	/*
 	 * Copy the header data to the bmp info structure
 	 */
	i->wd = p[0x12];
	i->hg = p[0x16];
	i->pix = p[0x1c];
	i->data = p + *((unsigned int *)(p + 0xa));
	i->sz =  *((unsigned int *)(p + 0x2));

	/*
	 * If here -> success
	 */
	goto Done_release_nothing;

Done_release_mmap:
	munmap(i->file, i->sz);
Done_release_file:
	close(i->fd);
Done_release_nothing:
	d_print(3, "img=%s,w=%d,h=%d,p=%d,o=%d,sz=%d,ret=%d\n",
		img, i->wd, i->hg, i->pix, i->data - i->file, i->sz, ret);
	return ret;
}

/*
 * Release an image
 * @param i		Image handle
 * @returns		0->success, <0->error code
 */
int lcd_bmp_close(struct lcd_bmp_info *i)
{
	int ret = 0;

	munmap(i->file, i->sz);
	close(i->fd);
}

/*
 * Copy an image in the bmp format to the frame buffer,
 * possibly over another bmp image.
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param info		Image handle
 * @param over		Underlying bmp handle
 * @param c		Background color
 * @returns		0->success, <0->error code
 */
static int lcd_bmp_raw(
	int x1, int y1, struct lcd_bmp_info *info,
	struct lcd_bmp_info *over, unsigned short c)
{
	unsigned short r, b;
	int i, j, l;
	int w = info->wd;
	int h = info->hg;
	int x2 = x1 + w-1;
	int y2 = y1 + h-1;
	unsigned short *p = (unsigned short *)info->data;
	unsigned short *f = over ? (unsigned short *)over->data : NULL;
	int ret = 0;

	/*
	 * Check validity of request
	 */
	ret = lcd_check_coordinate(x1, y1);
	if (ret < 0) {
		d_error("incorrect coordinate: %d,%d\n", x1, y1);
		goto Done_release_nothing;
	}
	ret = lcd_check_coordinate(x2, y2);
	if (ret < 0) {
		d_error("incorrect coordinate: %d,%d\n", x2, y2);
		goto Done_release_nothing;
	}

	/*
 	 * Fill in the pixel memory
 	 * A "transparent pixel" is filled either by a corresponding pixel
 	 * from the underlying bitmap image (if the image is provided) or
 	 * by the specified color (no underlying image is provided).
 	 */
	for (l = 0, i = y1; i <= y2; i++) {
		for (j = x1; j <= x2; j++) {
			b = p[l++];
			r = b==1 ? (over ? lcd_pixel_get(f, j, i) : c) : b;
			lcd_pixel_set(lcd_buf, j, i, r);
		}
	}

Done_release_nothing:
	d_print(3, "w,h=%d,%d,x1,y1=%d,%d,x2,y2=%d,%d,i=%p,%d,%d,ret=%d\n",
		w, h, x1, y1, x2, y2, 
		info, over ? over->wd : 0, over ? over->hg : 0, ret);
	return ret;
}

/*
 * Copy an image in the bmp format to the frame buffer.
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param info		Image handle
 * @param c		Background color
 * @returns		0->success, <0->error code
 */
int lcd_bmp(int x, int y, struct lcd_bmp_info *info, unsigned short c)
{
	int ret = lcd_bmp_raw(x, y, info, NULL, c);

	return ret;
}

/*
 * Copy an image in the bmp format to the frame buffer,
 * over another bmp image.
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param info		Image handle
 * @param over		Underlying bmp handle
 * @returns		0->success, <0->error code
 */
int lcd_bmp_bmp(
	int x, int y, struct lcd_bmp_info *info, struct lcd_bmp_info *over)
{
	int ret = lcd_bmp_raw(x, y, info, over, 0);

	return ret;
}

/*
 * Fill a rectangle, with either a piece of a specified bmp image
 * or with a specified color. bmp image is used if info is not NULL;
 * color is used otherwise. 
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param c		Color
 * @param info		bmp handle
 * @returns		0->success, <0->error code
 */
static int lcd_box_raw(
	int x1, int y1, int x2, int y2,
	unsigned short c, struct lcd_bmp_info *info)
{
	int i, j;
	unsigned short *f = info ? (unsigned short *)info->data : NULL;
	int ret = 0;

	/*
	 * Check validity of the request
	 */
	ret = lcd_check_coordinate(x1, y1);
	if (ret < 0) {
		d_error("incorrect coordinate: %d,%d\n", x1, y1);
		goto Done_release_nothing;
	}
	ret = lcd_check_coordinate(x2, y2);
	if (ret < 0) {
		d_error("incorrect coordinate: %d,%d\n", x2, y2);
		goto Done_release_nothing;
	}
	if (x1 > x2 || y1 > y2) {
		d_error("incorrect rectangle geometry: %d,%d,%d,%d\n",
			x1, y1, x2, y2);
		ret = -EIO;
		goto Done_release_nothing;
	}

	/*
 	 * Fill in the rectangle area in the frame buffer
 	 */
	for (i = x1; i <= x2; i++) {
		for (j = y1; j <= y2; j++) {
			c = info ? lcd_pixel_get(f, i, j) : c;
			lcd_pixel_set(lcd_buf, i, j, c);
		}
	}

	/*
 	 * If we are here, we have been successful
 	 */
	ret = 0;

Done_release_nothing:
	return ret;
}

/*
 * Fill a rectangle with a specified color
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param c		Color
 * @returns		0->success, <0->error code
 */
int lcd_box(int x1, int y1, int x2, int y2, unsigned short c)
{
	int ret = lcd_box_raw(x1, y1, x2, y2, c, NULL);

	d_print(3, "x1,y1=%d,%d,x2,y2=%d,%d,c=%x,ret=%d\n",
		x1, y1, x2, y2, c, ret);
	return ret;
}

/*
 * Fill a rectangle with a picture from a bmp image.
 * It is assumed that the bmp image has the geometry of
 * the full-screen LCD; picture is copied from the same
 * position of the image where it gets filled in the LCD.
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param i		bmp handle
 * @returns		0->success, <0->error code
 */
int lcd_box_bmp(int x1, int y1, int x2, int y2, struct lcd_bmp_info *i)
{
	int ret = lcd_box_raw(x1, y1, x2, y2, 0, i);

	d_print(3, "x1,y1=%d,%d,x2,y2=%d,%d,i=%d,%d,ret=%d\n",
		x1, y1, x2, y2, i->wd, i->hg, ret);
	return ret;
}

/*
 * Fill a rectangle with a picture from a bmp file.
 * It is assumed that the bmp image has the geometry of
 * the full-screen LCD; picture is copied from the same
 * position of the image where it gets filled in the LCD.
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param img		bmp file
 * @returns		0->success, <0->error code
 */
int lcd_box_img(int x1, int y1, int x2, int y2, char *img)
{
	struct lcd_bmp_info info;
	int ret = 0;

	/*
 	 * Get access to the image
 	 */
	ret = lcd_bmp_open(img, &info);
	if (ret < 0) {
		d_error("failed to open bmp %s\n", img);
		goto Done_release_nothing;
	}

	/*
	 * Copy the image to the frame buffer
	 */
	ret = lcd_box_bmp(x1, y1, x2, y2, &info);
	if (ret < 0) {
		d_error("failed to draw bmp %s\n", img);
	}

	/*
 	 * Get rid of the image
 	 */
	lcd_bmp_close(&info);

Done_release_nothing:
	d_print(3, "x1,y1=%d,%d,x2,y2=%d,%d,i=%s,ret=%d\n",
		x1, y1, x2, y2, img, ret);
 	return ret;
}

/*
 * Draw a character, either over a specified bmp image
 * or over a specified color. bmp image is used if info is not NULL;
 * color is used otherwise. 
 * @param ch		Character to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param fg		Foreground color
 * @param bg		background color
 * @param info		bmp handle
 * @returns		0->success, <0->error code
 */
static int inline lcd_char_raw(
	char ch, int x, int y, int sz, 
	unsigned short fg, unsigned short bg, struct lcd_bmp_info *info)
{
	int i, j, m, c, x1, x2;
	unsigned int col, row, byt;
	unsigned char *p;
	char b;
	unsigned char *tbl[] = 
		{(unsigned char *)lcd_font_6x8,
		 (unsigned char *)lcd_font_8x8,
		 (unsigned char *)lcd_font_8x16
	};
	unsigned short *f = info ? (unsigned short *)info->data: NULL;
	int ret = 0;

	/*
 	 * Check coordinate validity
 	 */
	ret = lcd_check_coordinate(x, y);
	if (ret < 0) {
		d_error("incorrect coordinate: %d,%d\n", x, y);
		goto Done_release_nothing;
	}

	/*
	 * Get a pointer to the table representing selected font
	 */
	p = (unsigned char *)tbl[sz];

	/*
 	 * Get the number of columns, rows and bytes in each character
 	 */
	col = p[0];
	row = p[1];
	byt = p[2];

	/*
 	 * Copy the character, pixel by pixel, to the framebuffer
 	 */
	p += byt * (ch - 0x1F);
	y += row-1;
	x1 = x;
	x2 = x + col-1;
	for (i = 0; i < byt; i++) {
		b = p[i];
		for (m = 0x80; x <= x2; m >>= 1, x++) {
			c = m & b ? fg : (info ? lcd_pixel_get(f, x, y) : bg);
			lcd_pixel_set(lcd_buf, x, y, c);
		}
		y--;
		x = x1;
	}

Done_release_nothing:
	d_print(4, "x,y=%d,%d,c,r,b=%d,%d,%d,ch=%c,%d,clr=%x,%x,ret=%d\n",
		x, y, col, row, byt, ch, sz, fg, bg, ret);
	return ret;
}

/*
 * Draw a string, either over a specified bmp image
 * or over a specified color. bmp image is used if info is not NULL;
 * color is used otherwise. 
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param fg		Foreground color
 * @param bg		Background color
 * @param info		bmp handle
 * @returns		0->success, <0->error code
 */
static int inline lcd_str_raw(
	char *str, int x, int y, int sz,
	unsigned short fg, unsigned short bg, struct lcd_bmp_info *info)
{
	int d = lcd_font_char_sz(sz);
	char *s = str;
	int ret = 0;

	/*
 	 * Draw the string, character by character
 	 */
	while (*s)
	{
		ret = lcd_char_raw(*s++, x, y, sz, fg, bg, info);
		if (ret < 0) {
			goto Done_release_nothing;
		}

		/* 
		 * Advance to the position of the next character
		 */
		x += d;
	}

Done_release_nothing:
 	return ret;
}

/*
 * Draw a string, over a specified color. 
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param fg		Foreground color
 * @param bg		Background color
 * @returns		0->success, <0->error code
 */
int lcd_str(
	char *str, int x, int y, int sz, unsigned short fg, unsigned short bg)
{
	int ret = lcd_str_raw(str, x, y, sz, fg, bg, NULL);

	d_print(3, "x,y=%d,%d,str=%s,%d,clr=%x,%x,ret=%d\n",
		x, y, str, sz, fg, bg, ret);
	return ret;
}
	
/*
 * Draw a string, over a specified bmp image.
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param bg		Foreground color
 * @param info		bmp handle
 * @returns		0->success, <0->error code
 */
int lcd_str_bmp(
	char *str, int x, int y, int sz,
	unsigned short fg, struct lcd_bmp_info *info)
{
	int ret = lcd_str_raw(str, x, y, sz, fg, 0, info);

	d_print(3, "x,y=%d,%d,str=%s,%d,clr=%x,%d,%d,ret=%d\n",
		x, y, str, sz, fg, info->wd, info->hg, ret);
	return ret;
}

/*
 * Draw a string, over a specified bmp file.
 * It is assumed that the bmp image has the geometry of
 * the full-screen LCD; picture is copied from the same
 * position of the image where it gets filled in the LCD.
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param bg		Foreground color
 * @param img		bmp file
 * @returns		0->success, <0->error code
 */
int lcd_str_img(
	char *str, int x, int y, int sz, unsigned short fg, char *img)
{
	struct lcd_bmp_info info;
	int ret = 0;

	/*
 	 * Get access to the image
 	 */
	ret = lcd_bmp_open(img, &info);
	if (ret < 0) {
		d_error("failed to open bmp %s\n", img);
		goto Done_release_nothing;
	}

	/*
	 * Copy the image to the frame buffer
	 */
	ret = lcd_str_bmp(str, x, y, sz, fg, &info);
	if (ret < 0) {
		d_error("failed to draw str %s over %s\n", str, img);
	}

	/*
 	 * Get rid of the image
 	 */
	lcd_bmp_close(&info);

Done_release_nothing:
	d_print(3, "x,y=%d,%d,str=%s,%d,clr=%x,%s,ret=%d\n",
		x, y, str, sz, fg, img, ret);
 	return ret;
}

/*
 * Draw an image
 * @param image		Image file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @returns		0->success, <0->error code
 */
int lcd_img(char *img, int x, int y)
{
	struct lcd_bmp_info info;
	int ret = 0;

	/*
 	 * Get access to the image
 	 */
	ret = lcd_bmp_open(img, &info);
	if (ret < 0) {
		d_error("failed to open bmp %s\n", img);
		goto Done_release_nothing;
	}

	/*
	 * Copy the image to the frame buffer
	 */
	ret = lcd_bmp(x, y, &info, 0);
	if (ret < 0) {
		d_error("failed to draw bmp %s\n", img);
	}

	/*
 	 * Get rid of the image
 	 */
	lcd_bmp_close(&info);

Done_release_nothing:
	d_print(3, "x,y=%d,%d,image=%s,ret=%d\n", x, y, img, ret);
 	return ret;
}

/*
 * Draw an image over a bmp file.
 * @param img		Image file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param bmp		bmp file
 * @returns		0->success, <0->error code
 */
int lcd_img_img(char *img, int x, int y, char *bmp)
{
	struct lcd_bmp_info info, snap;
	int ret = 0;

	/*
 	 * Get access to the image
 	 */
	ret = lcd_bmp_open(img, &info);
	if (ret < 0) {
		d_error("failed to open bmp %s\n", img);
		goto Done_release_nothing;
	}

	/*
 	 * Get access to the bmp file
 	 */
	ret = lcd_bmp_open(bmp, &snap);
	if (ret < 0) {
		d_error("failed to open bmp %s\n", bmp);
		goto Done_release_image;
	}

	/*
	 * Copy the image to the frame buffer
	 */
	ret = lcd_bmp_bmp(x, y, &info, &snap);
	if (ret < 0) {
		d_error("failed to draw bmp %s over %s\n", img, bmp);
	}

	/*
 	 * Get rid of the bmp file
 	 */
	lcd_bmp_close(&snap);

Done_release_image:
	/*
 	 * Get rid of the image
 	 */
	lcd_bmp_close(&info);

Done_release_nothing:
	d_print(3, "x,y=%d,%d,image=%s,%s,ret=%d\n", 
		x, y, img, bmp, ret);
 	return ret;
}

/*
 * Parse a color string parameter
 * @param s		Color specification string
 * @param c		Pointer to the color value 
 * @returns		0->success, <0->error code
 */
int lcd_color_parse(char *s, unsigned short *c)
{
	int i;
	static struct {
		char *s;
		unsigned short v;
	} tbl[] = {
		"white",	WHITE,
		"black",	BLACK,
		"red",		RED,
		"green",	GREEN,
		"blue",		BLUE,
		"cyan",		CYAN,
		"magenta",	MAGENTA,
		"yellow",	YELLOW,
		"brown",	BROWN,
		"orange",	ORANGE,
		"pink",		PINK,	
		NULL, 		0,
	};
	int ret = 0;

	for (i = 0; tbl[i].s != NULL; i++) {
		if (!strcmp(s, tbl[i].s)) {
			*c = tbl[i].v;
			goto Done_release_nothing;
		}
	}
	ret = -EIO;

Done_release_nothing:
	return ret;
}

/*
 * Parse a font string parameter
 * @param s		Font specification string
 * @param f		Pointer to the font value 
 * @returns		0->success, <0->error code
 */
int lcd_font_parse(char *s, int *f)
{
	int i;
	static struct {
		char *s;
		int v;
	} tbl[] = {
		"small",	SMALL,
		"medium",	MEDIUM,
		"large",	LARGE,
		NULL, 		0,
	};
	int ret = 0;

	for (i = 0; tbl[i].s != NULL; i++) {
		if (!strcmp(s, tbl[i].s)) {
			*f = tbl[i].v;
			goto Done_release_nothing;
		}
	}
	ret = -EIO;

Done_release_nothing:
	return ret;
}

