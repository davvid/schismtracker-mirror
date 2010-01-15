/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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
#include "headers.h"

#include "sdlmain.h"
#include "it.h"
#include "osdefs.h"

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#ifdef USE_XKB
# include <X11/XKBlib.h>
#endif

static int virgin = 1;
static unsigned int delay, rate;

#ifdef USE_XKB
static XkbDescPtr us_kb_map;
#endif

static void _key_info_setup(void)
{
        Display *dpy;
        SDL_SysWMinfo info;

        if (!virgin) return;
        virgin = 0;

        memset(&info, 0, sizeof(info));
        SDL_VERSION(&info.version);
        if (SDL_GetWMInfo(&info)) {
                if (info.info.x11.lock_func)
                        info.info.x11.lock_func();
                dpy = info.info.x11.display;
        } else {
                dpy = NULL;
        }
        if (!dpy) {
                dpy = XOpenDisplay(NULL);
                if (!dpy) return;
                memset(&info, 0, sizeof(info));
        }

#ifdef USE_XKB
        /* Dear X11,
                You suck.
        Sincerely, Storlek */
        char blank[] = "";
        char symbols[] = "+us(basic)";
        XkbComponentNamesRec rec = {
                .symbols = symbols,
                .keymap = blank,
                .keycodes = blank,
                .types = blank,
                .compat = blank,
                .geometry = blank,
        };
        us_kb_map = XkbGetKeyboardByName(dpy, XkbUseCoreKbd, &rec,
                        XkbGBN_AllComponentsMask, XkbGBN_AllComponentsMask, False);
        if (!us_kb_map)
                log_appendf(3, "Warning: XKB support missing or broken; keyjamming might not work right");

        if (XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &delay, &rate)) {
                if (info.info.x11.unlock_func)
                        info.info.x11.unlock_func();
                return;
        }
#else
        log_appendf(3, "Warning: XKB support not compiled in; keyjamming might not work right");
#endif

        /* eh... */
        delay = 125;
        rate = 30;

        if (info.info.x11.unlock_func)
                info.info.x11.unlock_func();
}

unsigned int key_repeat_rate(void)
{
        _key_info_setup();
        return rate;
}

unsigned int key_repeat_delay(void)
{
        _key_info_setup();
        return delay;
}

#ifdef USE_XKB
int key_scancode_lookup(int k, int def)
{
        static unsigned int d;
        KeySym sym;

        if (us_kb_map != NULL &&
                        XkbTranslateKeyCode(us_kb_map, k, 0, &d, &sym)) {
                return sym;
        }
        return def;
}
#endif
