/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#ifndef FMT_H
#define FMT_H

#include "song.h"
#include "dmoz.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------------------------------------- */

typedef bool (*fmt_read_info_func) (dmoz_file_t *file, const byte *data, size_t length);
typedef bool (*fmt_load_sample_func) (const byte *data, size_t length, song_sample *smp, char *title);
typedef bool (*fmt_save_sample_func) (FILE *fp, song_sample *smp, char *title);

#define READ_INFO(t) bool fmt_##t##_read_info(dmoz_file_t *file, const byte *data, size_t length)
#define LOAD_SAMPLE(t) bool fmt_##t##_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
#define SAVE_SAMPLE(t) bool fmt_##t##_save_sample(FILE *fp, song_sample *smp, char *title)

READ_INFO(669);
READ_INFO(ams);
READ_INFO(dtm);
READ_INFO(f2r);
READ_INFO(far);
READ_INFO(imf);
READ_INFO(it);
READ_INFO(liq);
READ_INFO(mdl);
READ_INFO(mod);
READ_INFO(mt2);
READ_INFO(mtm);
READ_INFO(ntk);
READ_INFO(s3m);
READ_INFO(stm);
READ_INFO(ult);
READ_INFO(xm);

#ifdef USE_NON_TRACKED_TYPES
READ_INFO(sid);
READ_INFO(mp3);
# ifdef HAVE_VORBIS
READ_INFO(ogg);
# endif
#endif

#ifdef USE_SAMPLE_TYPES
READ_INFO(aiff);        LOAD_SAMPLE(aiff);      SAVE_SAMPLE(aiff);
READ_INFO(au);          LOAD_SAMPLE(au);        SAVE_SAMPLE(au);
READ_INFO(its);         LOAD_SAMPLE(its);       SAVE_SAMPLE(its);
                        LOAD_SAMPLE(raw);       SAVE_SAMPLE(raw);
#endif

#undef READ_INFO
#undef LOAD_SAMPLE
#undef SAVE_SAMPLE

/* --------------------------------------------------------------------------------------------------------- */

/* save the sample's data in little- or big- endian byte order (defined in audio_loadsave.cc)
should probably return something, but... meh :P */
void save_sample_data_LE(FILE *fp, song_sample *smp);
void save_sample_data_BE(FILE *fp, song_sample *smp);

/* shared by the .it, .its, and .iti saving functions */
void save_its_header(FILE *fp, song_sample *smp, char *title);

/* --------------------------------------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! FMT_H */
