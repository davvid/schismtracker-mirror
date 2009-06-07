/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#ifndef SLURP_H
#define SLURP_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>
#include <unistd.h>

/* --------------------------------------------------------------------- */

typedef struct _slurp_struct slurp_t;
struct _slurp_struct {
        size_t length;
        uint8_t *data;
	int extra;
	void *bextra;
	void (*closure)(slurp_t *);
};

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* slurp returns NULL and sets errno on error. 'buf' is only meaningful if you've already stat()'d
the file; in most cases it can simply be NULL. If size is nonzero, it overrides the file's size as
returned by stat -- this can be used to read only part of a file, or if the file size is known but
a stat structure is not available. */
slurp_t *slurp(const char *filename, struct stat *buf, size_t size);

void unslurp(slurp_t * t);

#ifdef WIN32
int slurp_win32(slurp_t *useme, const char *filename, size_t st);
#endif

#if HAVE_MMAP
int slurp_mmap(slurp_t *useme, const char *filename, size_t st);
#endif


#ifdef __cplusplus
}
#endif

#endif /* ! SLURP_H */
