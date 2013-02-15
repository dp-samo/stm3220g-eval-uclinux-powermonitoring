/*
 * lcd_font.h - Some simple bitmap fonts
 *
 * FONT6x8 - SMALL font (mostly 5x7)
 * FONT8x8 - MEDIUM font (8x8 characters, a bit thicker)
 * FONT8x16 - LARGE font (8x16 characters, thicker)
 *
 * Note: ASCII characters 0x00 through 0x1F are not included in these fonts.
 * First row of each font contains the number of columns, the
 * number of rows and the number of bytes per character.
 *
 * Author: Jim Parise, James P Lynch July 7, 2007
 * With some minor modifincations by Vladimir K, emcraft.com, 2011
 */

#ifndef __LCD_FONT_H__	
#define __LCD_FONT_H__

/*
 * Pixels taken by a character on the horizontal scale
 */
#define lcd_font_char_sz(sz)	(sz == 0 ? 6 : 8)

/*
 * Actual fonts
 */
extern const unsigned char lcd_font_6x8[97][8];
extern const unsigned char lcd_font_8x8[97][8];
extern const unsigned char lcd_font_8x16[97][16];

#endif

