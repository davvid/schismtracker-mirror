/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* vgamem

this simulates a fictional vga-like card that supports three banks of characters, and
a packed display of 4000 32-bit words.

the banks are:
	0x80000000	new overlay

			the layout is relative to the scanline position: it gets pixel
			values from "ovl" which is [640*400]

	0x40000000	half-width font
			the layout of this is based on a special bank of 4bit wide fonts.
			the packing format of the field is:
				fg1 is nybble in bits 22-25
				fg2 is nybble in bits 26-29
				bg1 is nybble in bits 18-21
				bg2 is nybble in bits 14-17
				ch1 is 7 bits; 7-13
				ch2 is 7 bits: 0-6
			lower bits are unused

	0x10000080
			bios font
			this layout looks surprisingly like a real vga card
			(mostly because it was ripped from one ;)
				fg is nybble in bits 8-11
				bg is nybble in bits 12-15
				ch is lower byte
	0x00000000
			regular
			this layout uses the itf font
				fg is nybble in bits 8-11
				bg is nybble in bits 12-15
				ch is lower byte
*/

#include "headers.h"

#include "it.h"
#include "dmoz.h" /* for dmoz_path_concat */
#include "auto/default-font.h"

#include "sdlmain.h"

#include <assert.h>
#include <errno.h>

#include "util.h"
#include "video.h"

/* preprocessor stuff */

#define CHECK_INVERT(tl,br,n) \
do {						\
	if (status.flags & INVERTED_PALETTE) {	\
		n = tl;				\
		tl = br;			\
		br = n;				\
	}					\
} while(0)


/* --------------------------------------------------------------------- */
/* statics */

static byte font_normal[2048];

/* There's no way to change the other fontsets at the moment.
 * (other than recompiling, of course) */
static byte font_alt[2048];
static byte font_half_data[1024];

/* --------------------------------------------------------------------- */
/* globals */

byte *font_data = font_normal; /* this only needs to be global for itf */

/* int font_width = 8, font_height = 8; */

/* --------------------------------------------------------------------- */
/* half-width characters */

/* wth? i don't get this... the half width table isn't linear anymore?
   schism dies with "half width char ba not mapped" when i try inserting a
   note fade, but i have no idea what this code does so i'm not touching it.
	/storlek
   update: fixed note fade bug elsewise. just don't draw it with the whacked-
   looking sinewave chars and it'll be fine, but i still wanna know how this
   code works, so i'm leaving these useless comments in here :) */
static inline int _pack_halfw(int c)
{
	switch (c) {
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'a': case 'A': return 10;
	/* case 'b': */ case 'B': return 11; /* lowercase 'b' used for flat symbol */
	case 'c': case 'C': return 12;
	case 'd': case 'D': return 13;
	case 'e': case 'E': return 14;
	case 'f': case 'F': return 15;
	case 'g': case 'G': return 16;
	case 'h': case 'H': return 17;
	case 'i': case 'I': return 18;
	case 'j': case 'J': return 19;
	case 'k': case 'K': return 20;
	case 'l': case 'L': return 21;
	case 'm': case 'M': return 22;
	case 'n': case 'N': return 23;
	case 'o': case 'O': return 24;
	case 'p': case 'P': return 25;
	case 'q': case 'Q': return 26;
	case 'r': case 'R': return 27;
	case 's': case 'S': return 28;
	case 't': case 'T': return 29;
	case 'u': case 'U': return 30;
	case 'v': case 'V': return 31;
	case 'w': case 'W': return 32;
	case 'x': case 'X': return 33;
	case 'y': case 'Y': return 34;
	case 'z': case 'Z': return 35;

	/* FT2 nonsense */
	case '$': return 36;
	case '<': return 37;
	case '>': return 38;

	case ' ': return 39;
	case 0xad: return 40;
	case 0x5e: return 41;
	case 0xcd: return 42;
	case 0x7e: return 43;
        
        /* Mini-sharps and flats */
        case '-': return 44;
        case '#': return 45;
        case 'b': return 46;
        
	default:
		fprintf(stderr, "FATAL: half-width character %x not mapped\n", c);
		exit(255);
	};
}
static inline int _unpack_halfw(int c)
{
	const unsigned char *zmap = (const unsigned char *)
		"0123456789ABCDEFGHIJKLMNOPQRSTUV"
		"WXYZ$<> \xad\x5e\xcd\x7e-#b.................";
	if (c > 63)
		return 0; /* eh? */
	return (int) zmap[c];
}

/* --------------------------------------------------------------------- */
/* ITF loader */

static inline void make_half_width_middot(void)
{
        /* this copies the left half of char 184 in the normal font (two
         * half-width dots) to char 173 of the half-width font (the
         * middot), and the right half to char 184. thus, putting
         * together chars 173 and 184 of the half-width font will
         * produce the equivalent of 184 of the full-width font. */

        font_half_data[173 * 4 + 0] =
                (font_normal[184 * 8 + 0] & 0xf0) |
                (font_normal[184 * 8 + 1] & 0xf0) >> 4;
        font_half_data[173 * 4 + 1] =
                (font_normal[184 * 8 + 2] & 0xf0) |
                (font_normal[184 * 8 + 3] & 0xf0) >> 4;
        font_half_data[173 * 4 + 2] =
                (font_normal[184 * 8 + 4] & 0xf0) |
                (font_normal[184 * 8 + 5] & 0xf0) >> 4;
        font_half_data[173 * 4 + 3] =
                (font_normal[184 * 8 + 6] & 0xf0) |
                (font_normal[184 * 8 + 7] & 0xf0) >> 4;

        font_half_data[184 * 4 + 0] =
                (font_normal[184 * 8 + 0] & 0xf) << 4 |
                (font_normal[184 * 8 + 1] & 0xf);
        font_half_data[184 * 4 + 1] =
                (font_normal[184 * 8 + 2] & 0xf) << 4 |
                (font_normal[184 * 8 + 3] & 0xf);
        font_half_data[184 * 4 + 2] =
                (font_normal[184 * 8 + 4] & 0xf) << 4 |
                (font_normal[184 * 8 + 5] & 0xf);
        font_half_data[184 * 4 + 3] =
                (font_normal[184 * 8 + 6] & 0xf) << 4 |
                (font_normal[184 * 8 + 7] & 0xf);
}

/* just the non-itf chars */
void font_reset_lower(void)
{
        memcpy(font_normal, font_default_lower, 1024);
}

/* just the itf chars */
void font_reset_upper(void)
{
        memcpy(font_normal + 1024, font_default_upper_itf, 1024);
        make_half_width_middot();
}

/* all together now! */
void font_reset(void)
{
        memcpy(font_normal, font_default_lower, 1024);
        memcpy(font_normal + 1024, font_default_upper_itf, 1024);
        make_half_width_middot();
}

/* or kill the upper chars as well */
void font_reset_bios(void)
{
        font_reset_lower();
        memcpy(font_normal + 1024, font_default_upper_alt, 1024);
        make_half_width_middot();
}

/* ... or just one character */
void font_reset_char(int ch)
{
	byte *base;
	int cx;
	
	ch <<= 3;
	cx = ch;
	if (ch >= 1024) {
		base = (byte*)font_default_upper_itf;
		cx -= 1024;
	} else {
		base = (byte*)font_default_lower;
	}
	/* update them both... */
	memcpy(font_normal + ch, base + cx, 8);

	/* update */
        make_half_width_middot();
}

/* --------------------------------------------------------------------- */

static int squeeze_8x16_font(FILE * fp)
{
        byte data_8x16[4096];
        int n;

        if (fread(data_8x16, 4096, 1, fp) != 1)
                return -1;

        for (n = 0; n < 2048; n++)
                font_normal[n] = data_8x16[2 * n] | data_8x16[2 * n + 1];

        return 0;
}

/* Hmm. I could've done better with this one. */
int font_load(const char *filename)
{
        FILE *fp;
        long pos;
        byte data[4];
        char *font_dir, *font_file;

        font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
        font_file = dmoz_path_concat(font_dir, filename);
        free(font_dir);

        fp = fopen(font_file, "rb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
		free(font_file);
                return -1;
        }

        fseek(fp, 0, SEEK_END);
        pos = ftell(fp);
        if (pos == 2050) {
                /* Probably an ITF. Check the version. */

                fseek(fp, -2, SEEK_CUR);
                if (fread(data, 2, 1, fp) < 1) {
                        SDL_SetError("%s: %s", font_file,
                                     feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                        fclose(fp);
			free(font_file);
                        return -1;
                }
                if (data[1] != 0x2 || (data[0] != 0x12 && data[0] != 9)) {
                        SDL_SetError("%s: Unsupported ITF file version %02x.%20x", font_file, data[1], data[0]);
                        fclose(fp);
			free(font_file);
                        return -1;
                }
                rewind(fp);
        } else if (pos == 2048) {
                /* It's a raw file -- nothing else to check... */
                rewind(fp);
        } else if (pos == 4096) {
                rewind(fp);
                if (squeeze_8x16_font(fp) == 0) {
                        make_half_width_middot();
                        fclose(fp);
			free(font_file);
                        return 0;
                } else {
                        SDL_SetError("%s: %s", font_file,
                                     feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                        fclose(fp);
			free(font_file);
                        return -1;
                }
        } else {
                SDL_SetError("%s: Invalid font file", font_file);
                fclose(fp);
		free(font_file);
                return -1;
        }

        if (fread(font_normal, 2048, 1, fp) != 1) {
                SDL_SetError("%s: %s", font_file,
                             feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                fclose(fp);
		free(font_file);
                return -1;
        }

        make_half_width_middot();

        fclose(fp);
	free(font_file);
        return 0;
}

int font_save(const char *filename)
{
        FILE *fp;
        byte ver[2] = { 0x12, 0x2 };
        char *font_dir, *font_file;

        font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
        font_file = dmoz_path_concat(font_dir, filename);
	free(font_dir);

        fp = fopen(font_file, "wb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
		free(font_file);
                return -1;
        }

        if (fwrite(font_normal, 2048, 1, fp) < 1 || fwrite(ver, 2, 1, fp) < 1) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
                fclose(fp);
		free(font_file);
                return -1;
        }

        fclose(fp);
	free(font_file);
        return 0;
}

void font_init(void)
{
	memcpy(font_half_data, font_half_width, 1024);
	
        if (font_load(cfg_font) != 0) {
		SDL_ClearError();
                font_reset();
	}
	
        memcpy(font_alt, font_default_lower, 1024);
        memcpy(font_alt + 1024, font_default_upper_alt, 1024);
}

/* --------------------------------------------------------------------- */
static unsigned int vgamem[4000];
static unsigned int vgamem_read[4000];

static unsigned char ovl[640*400]; /* 256K */

void vgamem_flip(void)
{
	memcpy(vgamem_read, vgamem, sizeof(vgamem));
}
void vgamem_lock(void)
{
}
void vgamem_unlock(void)
{
}

void vgamem_clear(void)
{
	memset(vgamem,0,sizeof(vgamem));
}

void vgamem_ovl_alloc(struct vgamem_overlay *n)
{
	n->q = &ovl[ (n->x1*8) + (n->y1 * 5120) ];
	n->width = 8 * ((n->x2 - n->x1) + 1);
	n->height = 8 * ((n->y2 - n->y1) + 1);
	n->skip = (640 - n->width);
}
void vgamem_ovl_apply(struct vgamem_overlay *n)
{
	unsigned int x, y;

	for (y = n->y1; y <= n->y2; y++) {
		for (x = n->x1; x <= n->x2; x++) {
			vgamem[x + (y*80)] = 0x80000000;
		}
	}
}

void vgamem_ovl_clear(struct vgamem_overlay *n, int color)
{
	int i, j;
	unsigned char *q = n->q;
	for (j = 0; j < n->height; j++) {
		for (i = 0; i < n->width; i++) {
			*q = color;
			q++;
		}
		q += n->skip;
	}
}
void vgamem_ovl_drawpixel(struct vgamem_overlay *n, int x, int y, int color)
{
	n->q[ (640*y) + x ] = color;
}
static inline void _draw_line_v(struct vgamem_overlay *n, int x,
		int ys, int ye, int color)
{
	unsigned char *q = n->q + x;
	int y;

	if (ys < ye) {
		q += (ys * 640);
		for (y = ys; y <= ye; y++) {
			*q = color;
			q += 640;
		}
	} else {
		q += (ye * 640);
		for (y = ye; y <= ys; y++) {
			*q = color;
			q += 640;
		}
	}
}
static inline void _draw_line_h(struct vgamem_overlay *n, int xs,
		int xe, int y, int color)
{
	unsigned char *q = n->q + (y * 640);
	int x;
	if (xs < xe) {
		q += xs;
		for (x = xs; x <= xe; x++) {
			*q = color;
			q++;
		}
	} else {
		q += xe;
		for (x = xe; x <= xs; x++) {
			*q = color;
			q++;
		}
	}
}
#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef SGN
# define SGN(x) ((x) < 0 ? -1 : 1)      /* hey, what about zero? */
#endif

void vgamem_ovl_drawline(struct vgamem_overlay *n, int xs,
	int ys, int xe, int ye, int color)
{
        int d, x, y, ax, ay, sx, sy, dx, dy;

        dx = xe - xs;
        if (dx == 0) {
                _draw_line_v(n, xs, ys, ye, color);
                return;
        }

        dy = ye - ys;
        if (dy == 0) {
                _draw_line_h(n, xs, xe, ys, color);
                return;
        }

        ax = ABS(dx) << 1;
        sx = SGN(dx);
        ay = ABS(dy) << 1;
        sy = SGN(dy);

        x = xs;
        y = ys;
        if (ax > ay) {
                /* x dominant */
                d = ay - (ax >> 1);
                for (;;) {
                        vgamem_ovl_drawpixel(n, x, y, color);
                        if (x == xe) break;
                        if (d >= 0) {
                                y += sy;
                                d -= ax;
                        }
                        x += sx;
                        d += ay;
                }
        } else {
                /* y dominant */
                d = ax - (ay >> 1);
                for (;;) {
                        vgamem_ovl_drawpixel(n, x, y, color);
                        if (y == ye) break;
                        if (d >= 0) {
                                x += sx;
                                d -= ay;
                        }
                        y += sy;
                        d += ax;
                }
        }
}


/* write the vgamem routines */
#define BPP 32
#define F1 vgamem_scan32
#define F2 scan32
#define SIZE int
#include "vgamem-scanner.h"
#undef F2
#undef F1
#undef SIZE
#undef BPP

#define BPP 16
#define SIZE short
#define F1 vgamem_scan16
#define F2 scan16
#include "vgamem-scanner.h"
#undef F2
#undef F1
#undef SIZE
#undef BPP

#define BPP 8
#define SIZE char
#define F1 vgamem_scan8
#define F2 scan8
#include "vgamem-scanner.h"
#undef F2
#undef F1
#undef SIZE
#undef BPP

void draw_char(unsigned char c, int x, int y, Uint32 fg, Uint32 bg)
{
        assert(x >= 0 && y >= 0 && x < 80 && y < 50);
	vgamem[x + (y*80)] = c | (fg << 8) | (bg << 12);
}

int draw_text(const char * text, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text) {
                draw_char(*text, x + n, y, fg, bg);
                n++;
                text++;
        }
	
        return n;
}
int draw_text_bios(const char * text, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text) {
                draw_char(0x10000000|*text, x + n, y, fg, bg);
                n++;
                text++;
        }
	
        return n;
}
void draw_fill_chars(int xs, int ys, int xe, int ye, Uint32 color)
{
	unsigned int *mm;
	int x, len;
	mm = &vgamem[(ys * 80) + xs];
	len = (xe - xs)+1;
	ye -= ys;
	do {
		for (x = 0; x < len; x++) {
			mm[x] = (color << 12) | (color << 8);
		}
		mm += 80;
		ye--;
	} while (ye >= 0);
}

int draw_text_len(const char * text, int len, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text && n < len) {
                draw_char(*text, x + n, y, fg, bg);
                n++;
                text++;
        }
        draw_fill_chars(x + n, y, x + len - 1, y, bg);
        return n;
}
int draw_text_bios_len(const char * text, int len, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text && n < len) {
                draw_char(0x10000000|*text, x + n, y, fg, bg);
                n++;
                text++;
        }
        draw_fill_chars(x + n, y, x + len - 1, y, bg);
        return n;
}

/* --------------------------------------------------------------------- */

void draw_half_width_chars(byte c1, byte c2, int x, int y,
			   Uint32 fg1, Uint32 bg1, Uint32 fg2, Uint32 bg2)
{
        assert(x >= 0 && y >= 0 && x < 80 && y < 50);
	vgamem[x + (y*80)] =
		0x40000000
		| (fg1 << 22) | (fg2 << 26)
		| (bg1 << 18) | (bg2 << 14)
		| (_pack_halfw(c1) << 7) 
		| (_pack_halfw(c2));
}
/* --------------------------------------------------------------------- */
/* boxes */

enum box_type {
        BOX_THIN_INNER = 0, BOX_THIN_OUTER, BOX_THICK_OUTER
};

static const byte boxes[4][8] = {
        {139, 138, 137, 136, 134, 129, 132, 131},       /* thin inner */
        {128, 130, 133, 135, 129, 134, 131, 132},       /* thin outer */
        {142, 144, 147, 149, 143, 148, 145, 146},       /* thick outer */
};

static void _draw_box_internal(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br, const byte ch[8])
{
        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(ch[0], xs, ys, tl, 2);       /* TL corner */
        draw_char(ch[1], xe, ys, br, 2);       /* TR corner */
        draw_char(ch[2], xs, ye, br, 2);       /* BL corner */
        draw_char(ch[3], xe, ye, br, 2);       /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(ch[4], n, ys, tl, 2);        /* top */
                draw_char(ch[5], n, ye, br, 2);        /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char(ch[6], xs, n, tl, 2);        /* left */
                draw_char(ch[7], xe, n, br, 2);        /* right */
        }
}

void draw_thin_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br)
{
        _draw_box_internal(xs, ys, xe, ye, tl, br, boxes[BOX_THIN_INNER]);
}

void draw_thick_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br)
{
        /* this one can't use _draw_box_internal because the corner
         * colors are different */

        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(153, xs, ys, tl, 2); /* TL corner */
        draw_char(152, xe, ys, tl, 2); /* TR corner */
        draw_char(151, xs, ye, tl, 2); /* BL corner */
        draw_char(150, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(148, n, ys, tl, 2);  /* top */
                draw_char(143, n, ye, br, 2);  /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char(146, xs, n, tl, 2);  /* left */
                draw_char(145, xe, n, br, 2);  /* right */
        }
}

void draw_thin_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THIN_OUTER]);
}

void draw_thin_outer_cornered_box(int xs, int ys, int xe, int ye, int flags)
{
        const int colors[4][2] = { {3, 1}, {1, 3}, {3, 3}, {1, 1} };
        int tl = colors[flags & BOX_SHADE_MASK][0];
        int br = colors[flags & BOX_SHADE_MASK][1];
        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(128, xs, ys, tl, 2); /* TL corner */
        draw_char(141, xe, ys, 1, 2);  /* TR corner */
        draw_char(140, xs, ye, 1, 2);  /* BL corner */
        draw_char(135, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(129, n, ys, tl, 2);  /* top */
                draw_char(134, n, ye, br, 2);  /* bottom */
        }

        for (n = ys + 1; n < ye; n++) {
                draw_char(131, xs, n, tl, 2);  /* left */
                draw_char(132, xe, n, br, 2);  /* right */
        }
}

void draw_thick_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THICK_OUTER]);
}
