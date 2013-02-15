/*
 * lcd_api.h - Interface to the Nokia 6100 LCD
 * 2011, Vladimir Khusainov, Emcraft Systems
 */

#ifndef __LCD_API_H__	
#define __LCD_API_H__

/*
 * Get access to the lcd
 * @param d		name of the framebuffer device
 * @param n		name to use for debug messages (arbitrary string)
 * @param dbg		verbosity (0->silent; up->more verbose)
 * @returns		0->success, <0->error code
 */
extern int lcd_init(char *d, char *n, int dbg);

/*
 * Release the lcd
 * @returns		0->success, <0->error code
 */
extern int lcd_close(void);

/*
 * Sync up the software framebuffer with the hardware LCD
 * @returns		0->success, <0->error code
 */
extern int lcd_sync(void);

/*
 * Get X resolution
 * @returns		X resolution
 */
extern int lcd_xres(void);

/*
 * Get Y resolution
 * @returns		Y resolution
 */
extern int lcd_yres(void);

/*
 * Data structure representing a bitmap file
 * @fd			file descriptor
 * @sz			file size in bytes
 * @wd			image width
 * @hg			image height
 * @pix			bits per pixel
 * @file		address of the memory mapping
 * @data		starting address of bitmap data
 */
struct lcd_bmp_info {
	int		fd;
	int		sz;
	int		wd;
	int		hg;
	int		pix;
	unsigned char 	*file;
	unsigned char 	*data;
};

/*
 * Open a bmp file
 * @param img		bmp file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param i		bmp handle (filled in by the function)
 * @returns		0->success, <0->error code
 */
extern int lcd_bmp_open(char * img, struct lcd_bmp_info *i);

/*
 * Release a bmp file
 * @param i		bmp handle
 * @returns		0->success, <0->error code
 */
extern int lcd_bmp_close(struct lcd_bmp_info *i);

/*
 * Copy a bmp image to the frame buffer
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param i		bmp handle
 * @param c		Background color. This is used to fill in
 * 			"transparent" pixels represented in the bitmap
 * 			file by a special value (0x1).
 * @returns		0->success, <0->error code
 */
extern int lcd_bmp(int x, int y, struct lcd_bmp_info *i, unsigned short);

/*
 * Copy an image in the bmp format to the frame buffer,
 * over another bmp image.
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param info		Image handle
 * @param over		Underlying bmp handle. Pixels of that image
 * 			are used to fill in for "transparent pixels"
 * 			represeted by a special value (0x1) in
 * 			in the bitmap image being drawn. When a trasparent
 * 			pixel is found, the function shows a corresponding
 * 			pixel of the underlying background image.
 * 			Geometry of the underlying image is full resolution
 * 			of the LCD, starting at 0x0.
 * @returns		0->success, <0->error code
 */
extern int lcd_bmp_bmp(
	int x, int y, struct lcd_bmp_info *info, struct lcd_bmp_info *over);

/*
 * Fill a rectangle with a specified color
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param c		Color
 * @returns		0->success, <0->error code
 */
extern int lcd_box(int x1, int y1, int x2, int y2, unsigned short c);
	
/*
 * Fill a rectangle with a picture from a bmp image.
 * BMP image has the geometry of a full-screen LCD.
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param i		bmp handle
 * @returns		0->success, <0->error code
 */
extern int lcd_box_bmp(int x1, int y1, int x2, int y2, struct lcd_bmp_info *i);

/*
 * Fill a rectangle with a picture from a bmp file.
 * BMP image has the geometry of a full-screen LCD.
 * @param x1		Start coordinate (x)
 * @param y1		Start coordinate (y)
 * @param x2		End coordinate (x)
 * @param y2		End coordinate (y)
 * @param img		bmp file
 * @returns		0->success, <0->error code
 */
extern int lcd_box_img(int x1, int y1, int x2, int y2, char *img);

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
extern int lcd_str(
	char *str, int x, int y, int sz, unsigned short fg, unsigned short bg);

/*
 * Draw a string, over a specified bmp image.
 * It is assumed that the bmp image has the geometry of
 * a full-screen LCD.
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param bg		Foreground color
 * @param info		bmp handle
 * @returns		0->success, <0->error code
 */
extern int lcd_str_bmp(
	char *str, int x, int y, int sz,
	unsigned short fg, struct lcd_bmp_info *info);

/*
 * Draw a string, over a specified bmp file.
 * It is assumed that the bmp image has the geometry of
 * a full-screen LCD.
 * @param str		String to draw
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param sz		Font size (0->smallest, 2->largest)
 * @param bg		Foreground color
 * @param img		bmp file
 * @returns		0->success, <0->error code
 */
extern int lcd_str_img(
	char *str, int x, int y, int sz, unsigned short fg, char *img);

/*
 * Draw an image
 * @param img		Image file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @returns		0->success, <0->error code
 */
extern int lcd_img(char *img, int x, int y);

/*
 * Draw an image over a bmp file.
 * @param img		Image file in the BMP 16-bit format (4-X,4-R,4-G,4-B)
 * @param x		Start coordinate (x)
 * @param y		Start coordinate (y)
 * @param bmp		bmp file for the underlying picture
 * @returns		0->success, <0->error code
 */
extern int lcd_img_img(char *img, int x, int y, char *bmp);

/*
 * Some frequently-used colors
 */
#define WHITE 0xFFF
#define BLACK 0x000
#define RED 0xF00
#define GREEN 0x0F0
#define BLUE 0x00F
#define CYAN 0x0FF
#define MAGENTA 0xF0F
#define YELLOW 0xFF0
#define BROWN 0xB22
#define ORANGE 0xFA0
#define PINK 0xF6A

/*
 * Parse a color string parameter
 * @param s		Color specification string
 * @param c		Pointer to the color value 
 * @returns		0->success, <0->error code
 */
extern int lcd_color_parse(char *s, unsigned short *c);

/*
 * Font sizes
 */
#define SMALL 0
#define MEDIUM 1
#define LARGE 2

/*
 * Parse a font string parameter
 * @param s		Font specification string
 * @param f		Pointer to the font value 
 * @returns		0->success, <0->error code
 */
extern int lcd_font_parse(char *s, int *f);

#endif
