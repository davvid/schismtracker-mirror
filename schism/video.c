/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
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
#define NATIVE_SCREEN_WIDTH             640
#define NATIVE_SCREEN_HEIGHT            400

#include "headers.h"
#include "it.h"
#include "osdefs.h"

#if HAVE_SYS_KD_H
# include <sys/kd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/* for memcpy */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "sdlmain.h"

#include <unistd.h>
#include <fcntl.h>

#include "video.h"

#ifndef MACOSX
#include "auto/schismico.h"
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

extern int macosx_did_finderlaunch;

/* leeto drawing skills */
#define MOUSE_HEIGHT    14
static const unsigned int _mouse_pointer[] = {
        /* x....... */  0x80,
        /* xx...... */  0xc0,
        /* xxx..... */  0xe0,
        /* xxxx.... */  0xf0,
        /* xxxxx... */  0xf8,
        /* xxxxxx.. */  0xfc,
        /* xxxxxxx. */  0xfe,
        /* xxxxxxxx */  0xff,
        /* xxxxxxx. */  0xfe,
        /* xxxxx... */  0xf8,
        /* x...xx.. */  0x8c,
        /* ....xx.. */  0x0c,
        /* .....xx. */  0x06,
        /* .....xx. */  0x06,

0,0
};

struct video_cf {
        struct {
                unsigned int width;
                unsigned int height;
        } draw;
        struct {
                unsigned int width;
                unsigned int height;
                unsigned int bpp;
                int fb_hacks;
                int fullscreen;
        } desktop;

        SDL_Rect clip;
        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Texture *texture;
        SDL_Surface *surface;
        /* to convert 32-bit color to 24-bit color */
        unsigned char *cv32backing;
        /* for tv mode */
        unsigned char *cv8backing;
        struct {
                unsigned int x;
                unsigned int y;
                int visible;
        } mouse;

        unsigned int pal[256];
        unsigned int tc_bgr32[256];
};
static struct video_cf video;

const char *video_driver_name(void)
{
        return "sdl";
}

void video_report(void)
{
        log_appendf(5, " Using driver '%s'", SDL_GetCurrentVideoDriver());
        log_appendf(5, "Hardware-accelerated SDL surface");
        log_appendf(5, " Display format: %d bits/pixel",
                    video.surface->format->BitsPerPixel);

        if (video.desktop.fullscreen || video.desktop.fb_hacks) {
                log_appendf(5, " Display dimensions: %dx%d", video.desktop.width, video.desktop.height);
        }
}

void video_init()
{
        memset(&video, 0, sizeof(video));

        video.window = SDL_CreateWindow("Schism Tracker",
                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                        NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT, 0);
        if (!video.window) {
                perror("SDL_CreateWindow");
                exit(EXIT_FAILURE);
        }

        video.renderer = SDL_CreateRenderer(video.window, -1, 0);
        video.texture = SDL_CreateTexture(video.renderer,
                                          SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          NATIVE_SCREEN_WIDTH,
                                          NATIVE_SCREEN_HEIGHT);
        SDL_RenderSetLogicalSize(video.renderer,
                                 NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

        video.surface = SDL_CreateRGBSurface(0,
                                             NATIVE_SCREEN_WIDTH,
                                             NATIVE_SCREEN_HEIGHT, 32,
                                             0x00ff0000, 0x0000ff00,
                                             0x000000ff, 0xff000000);
        video.cv32backing = mem_alloc(NATIVE_SCREEN_WIDTH * 8);
        video.cv8backing = mem_alloc(NATIVE_SCREEN_WIDTH);

        video.clip.x = 0;
        video.clip.y = 0;
        video.clip.w = NATIVE_SCREEN_WIDTH;
        video.clip.h = NATIVE_SCREEN_HEIGHT;

        video.desktop.fb_hacks = 0;
        video.desktop.width = NATIVE_SCREEN_WIDTH;
        video.desktop.height = NATIVE_SCREEN_HEIGHT;
        video.desktop.bpp = video.surface->format->BitsPerPixel;

        video.draw.width = NATIVE_SCREEN_WIDTH;
        video.draw.height = NATIVE_SCREEN_HEIGHT;

        video.mouse.visible = MOUSE_EMULATED;
}

int video_is_fullscreen(void)
{
        return video.desktop.fullscreen;
}

void enter_fullscreen(void)
{
        Uint32 flags = SDL_GetWindowFlags(video.window);
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

        video.desktop.fullscreen = 1;
        SDL_SetWindowFullscreen(video.window, flags);
}

void exit_fullscreen(void)
{
        Uint32 flags = SDL_GetWindowFlags(video.window);
        flags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;

        video.desktop.fullscreen = 0;
        SDL_SetWindowFullscreen(video.window, flags);
}

void video_shutdown(void)
{
        if (video.desktop.fullscreen) {
                exit_fullscreen();
        }
}

void video_fullscreen(int tri)
{
        if (tri == 0 || video.desktop.fb_hacks) {
                video.desktop.fullscreen = 0;

        } else if (tri == 1) {
                video.desktop.fullscreen = 1;

        } else if (tri < 0) {
                video.desktop.fullscreen = video.desktop.fullscreen ? 0 : 1;
        }

        if (video.desktop.fullscreen) {
                enter_fullscreen();
        } else {
                exit_fullscreen();
        }
}

void video_startup(void)
{
        char *q;
        SDL_Rect **modes;
        int i, j, x, y;

        /* okay, i think we're ready */
        SDL_ShowCursor(SDL_DISABLE);

        /* because first mode is 0 */
        vgamem_clear();
        vgamem_flip();

#ifndef MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
        SDL_Surface *icon = xpmdata(_schism_icon_xpm);
        SDL_WM_SetIcon(icon, NULL);
        SDL_FreeSurface(icon);
#endif
        if ((q = getenv("SCHISM_VIDEO_DEPTH"))) {
                i=atoi(q);
                if (i == 32) video.desktop.bpp=32;
                else if (i == 24) video.desktop.bpp=24;
                else if (i == 16) video.desktop.bpp=16;
                else if (i == 8) video.desktop.bpp=8;
        }
        video_fullscreen(video.desktop.fullscreen);
}

static void _sdl_pal(int i, int rgb[3])
{
        video.pal[i] = SDL_MapRGB(video.surface->format,
                        rgb[0], rgb[1], rgb[2]);
}

static void _bgr32_pal(int i, int rgb[3])
{
        video.tc_bgr32[i] = rgb[2] |
                        (rgb[1] << 8) |
                        (rgb[0] << 16) | (255 << 24);
}

void video_colors(unsigned char palette[16][3])
{
        static SDL_Color imap[16];
        void (*fun)(int i,int rgb[3]);
        const int lastmap[] = { 0,1,2,3,5 };
        int rgb[3], i, j, p;

        if (video.surface->format->BytesPerPixel == 1) {
                const int depthmap[] = { 0, 15,14,7,
                                        8, 8, 9, 12,
                                        6, 1, 2, 2,
                                        10, 3, 11, 11 };
                /* okay, indexed color */
                for (i = 0; i < 16; i++) {
                        video.pal[i] = i;
                        imap[i].r = palette[i][0];
                        imap[i].g = palette[i][1];
                        imap[i].b = palette[i][2];
                        imap[i].a = 255;

                        rgb[0]=palette[i][0];
                        rgb[1]=palette[i][1];
                        rgb[2]=palette[i][2];
                        _bgr32_pal(i, rgb);

                }
                for (i = 128; i < 256; i++) {
                        video.pal[i] = depthmap[(i>>4)];
                }
                for (i = 128; i < 256; i++) {
                        j = i - 128;
                        p = lastmap[(j>>5)];
                        rgb[0] = (int)palette[p][0] +
                                (((int)(palette[p+1][0]
                                - palette[p][0]) * (j&31)) /32);
                        rgb[1] = (int)palette[p][1] +
                                (((int)(palette[p+1][1]
                                - palette[p][1]) * (j&31)) /32);
                        rgb[2] = (int)palette[p][2] +
                                (((int)(palette[p+1][2]
                                - palette[p][2]) * (j&31)) /32);
                        _bgr32_pal(i, rgb);
                }
                SDL_SetPaletteColors(video.surface->format->palette,
                                     imap, 0, 16);
                return;
        }

        /* make our "base" space */
        for (i = 0; i < 16; i++) {
                rgb[0]=palette[i][0];
                rgb[1]=palette[i][1];
                rgb[2]=palette[i][2];
                _sdl_pal(i, rgb);
                _bgr32_pal(i, rgb);
        }
        /* make our "gradient" space */
        for (i = 128; i < 256; i++) {
                j = i - 128;
                p = lastmap[(j>>5)];
                rgb[0] = (int)palette[p][0] +
                        (((int)(palette[p+1][0] - palette[p][0]) * (j&31)) /32);
                rgb[1] = (int)palette[p][1] +
                        (((int)(palette[p+1][1] - palette[p][1]) * (j&31)) /32);
                rgb[2] = (int)palette[p][2] +
                        (((int)(palette[p+1][2] - palette[p][2]) * (j&31)) /32);
                _sdl_pal(i, rgb);
                _bgr32_pal(i, rgb);
        }
}

void video_refresh(void)
{
        vgamem_flip();
        vgamem_clear();
}

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, unsigned int mouseline[80])
{
        unsigned int z;

        memset(mouseline, 0, 80*sizeof(unsigned int));
        if (video.mouse.visible != MOUSE_EMULATED
            || !(status.flags & IS_FOCUSED)
            || y < video.mouse.y
            || y >= video.mouse.y+MOUSE_HEIGHT) {
                return;
        }

        z = _mouse_pointer[ y - video.mouse.y ];
        mouseline[x] = z >> v;
        if (x < 79) mouseline[x+1] = (z << (8-v)) & 0xff;
}


#define FIXED_BITS 8
#define FIXED_MASK ((1 << FIXED_BITS) - 1)
#define ONE_HALF_FIXED (1 << (FIXED_BITS - 1))
#define INT2FIXED(x) ((x) << FIXED_BITS)
#define FIXED2INT(x) ((x) >> FIXED_BITS)
#define FRAC(x) ((x) & FIXED_MASK)

static void _blit1n(int bpp, unsigned char *pixels, unsigned int pitch)
{
        unsigned int *csp, *esp, *dp;
        unsigned int c00, c01, c10, c11;
        unsigned int outr, outg, outb;
        unsigned int pad;
        int fixedx, fixedy, scalex, scaley;
        unsigned int y, x,ey,ex,t1,t2;
        unsigned int mouseline[80];
        unsigned int mouseline_x, mouseline_v;
        int iny, lasty;

        mouseline_x = (video.mouse.x / 8);
        mouseline_v = (video.mouse.x % 8);

        csp = (unsigned int *)video.cv32backing;
        esp = csp + NATIVE_SCREEN_WIDTH;
        lasty = -2;
        iny = 0;
        pad = pitch - (video.clip.w * bpp);
        scalex = INT2FIXED(NATIVE_SCREEN_WIDTH-1) / video.clip.w;
        scaley = INT2FIXED(NATIVE_SCREEN_HEIGHT-1) / video.clip.h;
        for (y = 0, fixedy = 0; (y < video.clip.h); y++, fixedy += scaley) {
                iny = FIXED2INT(fixedy);
                if (iny != lasty) {
                        make_mouseline(mouseline_x, mouseline_v, iny, mouseline);

                        /* we'll downblit the colors later */
                        if (iny == lasty + 1) {
                                /* move up one line */
                                vgamem_scan32(iny+1, csp, video.tc_bgr32, mouseline);
                                dp = esp; esp = csp; csp=dp;
                        } else {
                                vgamem_scan32(iny, (csp = (unsigned int *)video.cv32backing),
                                        video.tc_bgr32, mouseline);
                                vgamem_scan32(iny+1, (esp = (csp + NATIVE_SCREEN_WIDTH)),
                                        video.tc_bgr32, mouseline);
                        }
                        lasty = iny;
                }
                for (x = 0, fixedx = 0; x < video.clip.w; x++, fixedx += scalex) {
                        ex = FRAC(fixedx);
                        ey = FRAC(fixedy);

                        c00 = csp[FIXED2INT(fixedx)];
                        c01 = csp[FIXED2INT(fixedx) + 1];
                        c10 = esp[FIXED2INT(fixedx)];
                        c11 = esp[FIXED2INT(fixedx) + 1];

#if FIXED_BITS <= 8
                        /* When there are enough bits between blue and
                         * red, do the RB channels together
                         * See http://www.virtualdub.org/blog/pivot/entry.php?id=117
                         * for a quick explanation */
#define REDBLUE(Q) ((Q) & 0x00FF00FF)
#define GREEN(Q) ((Q) & 0x0000FF00)
                        t1 = REDBLUE((((REDBLUE(c01)-REDBLUE(c00))*ex) >> FIXED_BITS)+REDBLUE(c00));
                        t2 = REDBLUE((((REDBLUE(c11)-REDBLUE(c10))*ex) >> FIXED_BITS)+REDBLUE(c10));
                        outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

                        t1 = GREEN((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00));
                        t2 = GREEN((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10));
                        outg = (((((t2-t1)*ey) >> FIXED_BITS) + t1) >> 8) & 0xFF;

                        outr = (outb >> 16) & 0xFF;
                        outb &= 0xFF;
#undef REDBLUE
#undef GREEN
#else
#define BLUE(Q) (Q & 255)
#define GREEN(Q) ((Q >> 8) & 255)
#define RED(Q) ((Q >> 16) & 255)
                        t1 = ((((BLUE(c01)-BLUE(c00))*ex) >> FIXED_BITS)+BLUE(c00)) & 0xFF;
                        t2 = ((((BLUE(c11)-BLUE(c10))*ex) >> FIXED_BITS)+BLUE(c10)) & 0xFF;
                        outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

                        t1 = ((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00)) & 0xFF;
                        t2 = ((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10)) & 0xFF;
                        outg = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

                        t1 = ((((RED(c01)-RED(c00))*ex) >> FIXED_BITS)+RED(c00)) & 0xFF;
                        t2 = ((((RED(c11)-RED(c10))*ex) >> FIXED_BITS)+RED(c10)) & 0xFF;
                        outr = ((((t2-t1)*ey) >> FIXED_BITS) + t1);
#undef RED
#undef GREEN
#undef BLUE
#endif
                        /* write output "pixel" */
                        switch (bpp) {
                        case 4:
                                /* inline MapRGB */
                                (*(unsigned int *)pixels) = 0xFF000000 | (outr << 16) | (outg << 8) | outb;
                                break;
                        case 3:
                                /* inline MapRGB */
                                (*(unsigned int *)pixels) = (outr << 16) | (outg << 8) | outb;
                                break;
                        case 2:
                                /* inline MapRGB if possible */
                                if (video.surface->format->palette) {
                                        /* err... */
                                        (*(unsigned short *)pixels) = SDL_MapRGB(
                                                video.surface->format, outr, outg, outb);
                                } else if (video.surface->format->Gloss == 2) {
                                        /* RGB565 */
                                        (*(unsigned short *)pixels) = ((outr << 8) & 0xF800) |
                                                ((outg << 3) & 0x07E0) |
                                                (outb >> 3);
                                } else {
                                        /* RGB555 */
                                        (*(unsigned short *)pixels) = 0x8000 |
                                                ((outr << 7) & 0x7C00) |
                                                ((outg << 2) & 0x03E0) |
                                                (outb >> 3);
                                }
                                break;
                        case 1:
                                /* er... */
                                (*pixels) = SDL_MapRGB(
                                        video.surface->format, outr, outg, outb);
                                break;
                        };
                        pixels += bpp;
                }
                pixels += pad;
        }
}

static void _blit11(int bpp, unsigned char *pixels, unsigned int pitch,
                        unsigned int *tpal)
{
        unsigned int mouseline_x = (video.mouse.x / 8);
        unsigned int mouseline_v = (video.mouse.x % 8);
        unsigned int mouseline[80];
        unsigned char *pdata;
        unsigned int x, y;
        int pitch24;

        switch (bpp) {
        case 4:
                for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
                        make_mouseline(mouseline_x, mouseline_v, y, mouseline);
                        vgamem_scan32(y, (unsigned int *)pixels, tpal, mouseline);
                        pixels += pitch;
                }
                break;
        case 3:
                /* ... */
                pitch24 = pitch - (NATIVE_SCREEN_WIDTH * 3);
                if (pitch24 < 0) {
                        return; /* eh? */
                }
                for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
                        make_mouseline(mouseline_x, mouseline_v, y, mouseline);
                        vgamem_scan32(y,(unsigned int*)video.cv32backing,tpal, mouseline);
                        /* okay... */
                        pdata = video.cv32backing;
                        for (x = 0; x < NATIVE_SCREEN_WIDTH; x++) {
#if WORDS_BIGENDIAN
                                memcpy(pixels, pdata+1, 3);
#else
                                memcpy(pixels, pdata, 3);
#endif
                                pdata += 4;
                                pixels += 3;
                        }
                        pixels += pitch24;
                }
                break;
        case 2:
                for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
                        make_mouseline(mouseline_x, mouseline_v, y, mouseline);
                        vgamem_scan16(y, (unsigned short *)pixels, tpal, mouseline);
                        pixels += pitch;
                }
                break;
        case 1:
                for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
                        make_mouseline(mouseline_x, mouseline_v, y, mouseline);
                        vgamem_scan8(y, (unsigned char *)pixels, tpal, mouseline);
                        pixels += pitch;
                }
                break;
        };
}

void video_blit(void)
{
        unsigned char *pixels = NULL;
        unsigned int bpp = 0;
        unsigned int pitch = 0;

        bpp = video.surface->format->BytesPerPixel;
        pixels = (unsigned char *)video.surface->pixels;
        pixels += video.clip.y * video.surface->pitch;
        pixels += video.clip.x * bpp;
        pitch = video.surface->pitch;

        vgamem_lock();

        /* hardware scaling provided by SDL */
        _blit11(bpp, pixels, pitch, video.pal);

        SDL_UpdateTexture(video.texture, NULL, pixels, pitch);
        SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);
        SDL_RenderPresent(video.renderer);

        vgamem_unlock();
}

int video_mousecursor_visible(void)
{
        return video.mouse.visible;
}

void video_mousecursor(int vis)
{
        const char *state[] = {
                "Mouse disabled",
                "Software mouse cursor enabled",
                "Hardware mouse cursor enabled",
        };

        if (status.flags & NO_MOUSE) {
                // disable it no matter what
                video.mouse.visible = MOUSE_DISABLED;
                //SDL_ShowCursor(0);
                return;
        }

        switch (vis) {
        case MOUSE_CYCLE_STATE:
                vis = (video.mouse.visible + 1) % MOUSE_CYCLE_STATE;
                /* fall through */
        case MOUSE_DISABLED:
        case MOUSE_SYSTEM:
        case MOUSE_EMULATED:
                video.mouse.visible = vis;
                status_text_flash("%s", state[video.mouse.visible]);
        case MOUSE_RESET_STATE:
                break;
        default:
                video.mouse.visible = MOUSE_EMULATED;
        }

        SDL_ShowCursor(video.mouse.visible == MOUSE_SYSTEM);

        // Totally turn off mouse event sending when the mouse is disabled
        int evstate = video.mouse.visible == MOUSE_DISABLED ? SDL_DISABLE : SDL_ENABLE;
        if (evstate != SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
                SDL_EventState(SDL_MOUSEMOTION, evstate);
                SDL_EventState(SDL_MOUSEBUTTONDOWN, evstate);
                SDL_EventState(SDL_MOUSEBUTTONUP, evstate);
        }
}

void video_translate(unsigned int vx, unsigned int vy,
                unsigned int *x, unsigned int *y)
{
        if ((signed) vx < video.clip.x) vx = video.clip.x;
        vx -= video.clip.x;

        if ((signed) vy < video.clip.y) vy = video.clip.y;
        vy -= video.clip.y;

        if ((signed) vx > video.clip.w) vx = video.clip.w;
        if ((signed) vy > video.clip.h) vy = video.clip.h;

        vx *= NATIVE_SCREEN_WIDTH;
        vy *= NATIVE_SCREEN_HEIGHT;
        vx /= (video.draw.width - (video.draw.width - video.clip.w));
        vy /= (video.draw.height - (video.draw.height - video.clip.h));

        if (video.mouse.visible && (video.mouse.x != vx || video.mouse.y != vy)) {
                status.flags |= SOFTWARE_MOUSE_MOVED;
        }
        video.mouse.x = vx;
        video.mouse.y = vy;
        if (x) *x = vx;
        if (y) *y = vy;
}
