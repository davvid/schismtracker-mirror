/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifndef MPLINK_H
#define MPLINK_H

#include "it.h"
#include "song.h"

#include "stdafx.h"
#include "sndfile.h"

extern CSoundFile *mp;

extern char song_filename[]; /* the full path (as given to song_load) */
extern char song_basename[]; /* everything after the last slash */

/* milliseconds = (samples * 1000) / frequency */
extern unsigned long samples_played;

extern unsigned long max_channels_used;

#endif /* ! MPLINK_H */
