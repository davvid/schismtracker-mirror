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

#define NEED_BYTESWAP
#include "headers.h"
#include "slurp.h"
#include "fmt.h"

#include "sndfile.h"

/* --------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

int fmt_stm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        /* data[29] is the type: 1 = song, 2 = module (with samples) */
        if (!(length > 28 && data[28] == 0x1a && (data[29] == 1 || data[29] == 2)
              && (memcmp(data + 14, "!Scream!", 8) || memcmp(data + 14, "BMOD2STM", 8))))
                return 0;

        /* I used to check whether it was a 'song' or 'module' and set the description
        accordingly, but it's fairly pointless information :) */
        file->description = "Scream Tracker 2";
        /*file->extension = str_dup("stm");*/
        file->type = TYPE_MODULE_MOD;
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data, 20);
        file->title[20] = 0;
        return 1;
}

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
struct stm_sample {
        char name[12];
        uint8_t zero;
        uint8_t inst_disk; // lol disks
        uint16_t reserved;
        uint16_t length, loop_start, loop_end;
        uint8_t volume;
        uint8_t reserved2;
        uint16_t c5speed;
        uint32_t morejunk;
        uint16_t paragraphs; // what?
};
#pragma pack(pop)

static uint8_t stm_effects[16] = {
        FX_NONE,               // .
        FX_SPEED,              // A
        FX_POSITIONJUMP,       // B
        FX_PATTERNBREAK,       // C
        FX_VOLUMESLIDE,        // D
        FX_PORTAMENTODOWN,     // E
        FX_PORTAMENTOUP,       // F
        FX_TONEPORTAMENTO,     // G
        FX_VIBRATO,            // H
        FX_TREMOR,             // I
        FX_ARPEGGIO,           // J
        // KLMNO can be entered in the editor but don't do anything
};

/* ST2 says at startup:
"Remark: the user ID is encoded in over ten places around the file!"
I wonder if this is interesting at all. */


static void load_stm_pattern(song_note_t *note, slurp_t *fp)
{
        int row, chan;
        uint8_t v[4];

        for (row = 0; row < 64; row++, note += 64 - 4) {
                for (chan = 0; chan < 4; chan++, note++) {
                        slurp_read(fp, v, 4);
                        
                        // mostly copied from modplug...
                        if (v[0] < 251)
                                note->note = (v[0] >> 4) * 12 + (v[0] & 0xf) + 37;
                        note->instrument = v[1] >> 3;
                        if (note->instrument > 31)
                                note->instrument = 0; // oops never mind, that was crap
                        note->volparam = (v[1] & 0x7) + (v[2] >> 1); // I don't understand this line
                        if (note->volparam <= 64)
                                note->voleffect = VOLFX_VOLUME;
                        else
                                note->volparam = 0;
                        note->param = v[3]; // easy!
                        
                        note->effect = stm_effects[v[2] & 0xf];
                        // patch a couple effects up
                        switch (note->effect) {
                        case FX_SPEED:
                                // I don't know how Axx really works, but I do know that this
                                // isn't it. It does all sorts of mindbogglingly screwy things:
                                //      01 - very fast,
                                //      0F - very slow.
                                //      10 - fast again!
                                // I don't get it.
                                note->param >>= 4;
                                break;
                        case FX_PATTERNBREAK:
                                note->param = (note->param & 0xf0) * 10 + (note->param & 0xf);
                                break;
                        case FX_POSITIONJUMP:
                                // This effect is also very weird.
                                // Bxx doesn't appear to cause an immediate break -- it merely
                                // sets the next order for when the pattern ends (either by
                                // playing it all the way through, or via Cxx effect)
                                // I guess I'll "fix" it later...
                                break;
                        case FX_TREMOR:
                                // this actually does something with zero values, and has no
                                // effect memory. which makes SENSE for old-effects tremor,
                                // but ST3 went and screwed it all up by adding an effect
                                // memory and IT followed that, and those are much more popular
                                // than STM so we kind of have to live with this effect being
                                // broken... oh well. not a big loss.
                                break;
                        default:
                                // Anything not listed above is a no-op if there's no value.
                                // (ST2 doesn't have effect memory)
                                if (!note->param)
                                        note->effect = FX_NONE;
                                break;
                        }
                }
        }
}

int fmt_stm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
        char id[8];
        uint8_t tmp[4];
        int npat, n;

        slurp_seek(fp, 20, SEEK_SET);
        slurp_read(fp, id, 8);
        slurp_read(fp, tmp, 4);

        if (!(
                // this byte is guaranteed to be 0x1a, always...
                tmp[0] == 0x1a
                // from the doc:
                //      1 - song (contains no samples)
                //      2 - module (contains samples)
                // I'm not going to care about "songs".
                && tmp[1] == 2
                // and check the file tag -- but god knows why, it's case insensitive
                && (strncasecmp(id, "!Scream!", 8) == 0 || strncasecmp(id, "BMOD2STM", 8) == 0)
        )) {
                return LOAD_UNSUPPORTED;
        }
        // and the next two bytes are the tracker version.
        // (XXX should this care about BMOD2STM? what is that anyway?)
        sprintf(song->tracker_id, "Scream Tracker %d.%02x", tmp[2], tmp[3]);

        slurp_seek(fp, 0, SEEK_SET);
        slurp_read(fp, song->title, 20);
        song->title[20] = '\0';
        slurp_seek(fp, 12, SEEK_CUR); // skip the tag and stuff

        song->initial_speed = (slurp_getc(fp) >> 4) ?: 1;
        npat = slurp_getc(fp);
        song->initial_global_volume = 2 * slurp_getc(fp);
        slurp_seek(fp, 13, SEEK_CUR); // junk

        if (npat > 64)
                return LOAD_FORMAT_ERROR;

        for (n = 1; n <= 31; n++) {
                struct stm_sample stmsmp;
                uint16_t blen;
                song_sample_t *sample = song->samples + n;

                slurp_read(fp, &stmsmp, sizeof(stmsmp));
                // the strncpy here is intentional -- ST2 doesn't show the '3' after the \0 bytes in the first
                // sample of pm_fract.stm, for example
                strncpy(sample->filename, stmsmp.name, 12);
                memcpy(sample->name, sample->filename, 12);
                blen = sample->length = bswapLE16(stmsmp.length);
                sample->loop_start = bswapLE16(stmsmp.loop_start);
                sample->loop_end = bswapLE16(stmsmp.loop_end);
                sample->c5speed = bswapLE16(stmsmp.c5speed);
                sample->volume = stmsmp.volume * 4; //mphack
                if (sample->loop_start < blen
                    && sample->loop_end != 0xffff
                    && sample->loop_start < sample->loop_end) {
                        sample->flags |= CHN_LOOP;
                        sample->loop_end = CLAMP(sample->loop_end, sample->loop_start, blen);
                }
        }

        slurp_read(fp, song->orderlist, 128);
        for (n = 0; n < 128; n++) {
                if (song->orderlist[n] >= 64)
                        song->orderlist[n] = ORDER_LAST;
        }

        if (lflags & LOAD_NOPATTERNS) {
                slurp_seek(fp, npat * 64 * 4 * 4, SEEK_CUR);
        } else {
                for (n = 0; n < npat; n++) {
                        song->patterns[n] = csf_allocate_pattern(64);
                        song->pattern_size[n] = song->pattern_alloc_size[n] = 64;
                        load_stm_pattern(song->patterns[n], fp);
                }
        }

        if (!(lflags & LOAD_NOSAMPLES)) {
                for (n = 1; n <= 31; n++) {
                        song_sample_t *sample = song->samples + n;
                        int align = (sample->length + 15) & ~15;

                        if (sample->length < 3) {
                                // Garbage?
                                sample->length = 0;
                        } else {
                                csf_read_sample(sample, SF_LE | SF_PCMS | SF_8 | SF_M,
                                        (const char *) (fp->data + fp->pos), sample->length);
                        }
                        slurp_seek(fp, align, SEEK_CUR);
                }
        }

        for (n = 0; n < 4; n++)
                song->channels[n].panning = ((n & 1) ? 64 : 0) * 4; //mphack
        for (; n < 64; n++)
                song->channels[n].flags |= CHN_MUTE;
        song->pan_separation = 64;
        song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

        return LOAD_SUCCESS;
}

