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

#include "sndfile.h"

#include "cmixer.h"
#include "snd_fm.h"
#include "snd_gm.h"
#include "tables.h"

#include "util.h" /* for clamp/min */

#include <math.h>

/* --------------------------------------------------------------------------------------------------------- */
/* note/freq/period conversion functions */

int get_note_from_period(int period)
{
        int n;
        if (!period)
                return 0;
        for (n = 0; n <= 120; n++) {
                /* Essentially, this is just doing a note_to_period(n, 8363), but with less
                computation since there's no c5speed to deal with. */
                if (period >= (32 * period_table[n % 12] >> (n / 12)))
                        return n + 1;
        }
        return 120;
}

int get_period_from_note(int note, unsigned int c5speed, int linear)
{
        if (!note || note > 0xF0)
                return 0;
        note--;
        if (linear)
                return (period_table[note % 12] << 5) >> (note / 12);
        else if (!c5speed)
                return INT_MAX;
        else
                return _muldiv(8363, (period_table[note % 12] << 5), c5speed << (note / 12));
}


unsigned int get_freq_from_period(int period, unsigned int c5speed, int linear)
{
        if (period <= 0)
                return INT_MAX;
        return _muldiv(linear ? c5speed : 8363, 1712L << 8, (period << 8));
}


unsigned int transpose_to_frequency(int transp, int ftune)
{
        return (unsigned int) (8363.0 * pow(2, (transp * 128.0 + ftune) / 1536.0));
}

int frequency_to_transpose(unsigned int freq)
{
        return (int) (1536.0 * (log(freq / 8363.0) / log(2)));
}


unsigned long calc_halftone(unsigned long hz, int rel)
{
        return pow(2, rel / 12.0) * hz + 0.5;
}

/* --------------------------------------------------------------------------------------------------------- */
/* the full content of snd_fx.cpp follows. */


////////////////////////////////////////////////////////////
// Channels effects

void fx_note_cut(song_t *csf, uint32_t nchan)
{
        song_voice_t *chan = &csf->voices[nchan];
        // stop the current note:
        chan->flags |= CHN_FASTVOLRAMP;
        chan->length = 0;
        chan->period = 0; // keep instrument numbers from picking up old notes

        OPL_NoteOff(nchan);
        OPL_Touch(nchan, NULL, 0);
        GM_KeyOff(nchan);
        GM_Touch(nchan, 0);
}

void fx_key_off(song_t *csf, uint32_t nchan)
{
        song_voice_t *chan = &csf->voices[nchan];

        /*fprintf(stderr, "KeyOff[%d] [ch%u]: flags=0x%X\n",
                tick_count, (unsigned)nchan, chan->flags);*/
        OPL_NoteOff(nchan);
        GM_KeyOff(nchan);

        song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;

        /*if ((chan->flags & CHN_ADLIB)
        ||  (penv && penv->midi_channel_mask))
        {
                // When in AdLib / MIDI mode, end the sample
                chan->flags |= CHN_FASTVOLRAMP;
                chan->length = 0;
                chan->position    = 0;
                return;
        }*/

        chan->flags |= CHN_KEYOFF;
        //if ((!chan->ptr_instrument) || (!(chan->flags & CHN_VOLENV)))
        if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument && !(chan->flags & CHN_VOLENV)) {
                chan->flags |= CHN_NOTEFADE;
        }
        if (!chan->length)
                return;
        if ((chan->flags & CHN_SUSTAINLOOP) && chan->ptr_sample) {
                song_sample_t *psmp = chan->ptr_sample;
                if (psmp->flags & CHN_LOOP) {
                        if (psmp->flags & CHN_PINGPONGLOOP)
                                chan->flags |= CHN_PINGPONGLOOP;
                        else
                                chan->flags &= ~(CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
                        chan->flags |= CHN_LOOP;
                        chan->length = psmp->length;
                        chan->loop_start = psmp->loop_start;
                        chan->loop_end = psmp->loop_end;
                        if (chan->length > chan->loop_end) chan->length = chan->loop_end;
                } else {
                        chan->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
                        chan->length = psmp->length;
                }
        }
        if (penv && penv->fadeout && (penv->flags & ENV_VOLLOOP))
                chan->flags |= CHN_NOTEFADE;
}


static void fx_do_freq_slide(uint32_t flags, song_voice_t *chan, int32_t slide)
{
        // IT Linear slides
        if (!chan->period) return;
        if (flags & SONG_LINEARSLIDES) {
                if (slide < 0) {
                        uint32_t n = (-slide) >> 2;
                        if (n > 255)
                                n = 255;
                        chan->period = _muldivr(chan->period, linear_slide_down_table[n], 65536);
                } else {
                        uint32_t n = (slide) >> 2;
                        if (n > 255)
                                n = 255;
                        chan->period = _muldivr(chan->period, linear_slide_up_table[n], 65536);
                }
        } else {
                chan->period += slide;
        }
}

static void fx_fine_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if ((flags & SONG_FIRSTTICK) && chan->period && param) {
                if (flags & SONG_LINEARSLIDES) {
                        chan->period = _muldivr(chan->period, linear_slide_down_table[param & 0x0F], 65536);
                } else {
                        chan->period -= (int)(param * 4);
                }
        }
}

static void fx_fine_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if ((flags & SONG_FIRSTTICK) && chan->period && param) {
                if (flags & SONG_LINEARSLIDES) {
                        chan->period = _muldivr(chan->period, linear_slide_up_table[param & 0x0F], 65536);
                } else {
                        chan->period += (int)(param * 4);
                }
        }
}

static void fx_extra_fine_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if ((flags & SONG_FIRSTTICK) && chan->period && param) {
                if (flags & SONG_LINEARSLIDES) {
                        chan->period = _muldivr(chan->period, fine_linear_slide_down_table[param & 0x0F], 65536);
                } else {
                        chan->period -= (int)(param);
                }
        }
}

static void fx_extra_fine_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if ((flags & SONG_FIRSTTICK) && chan->period && param) {
                if (flags & SONG_LINEARSLIDES) {
                        chan->period = _muldivr(chan->period, fine_linear_slide_up_table[param & 0x0F], 65536);
                } else {
                        chan->period += (int)(param);
                }
        }
}

static void fx_reg_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if (!(flags & SONG_FIRSTTICK))
                fx_do_freq_slide(flags, chan, -(int)(param * 4));
}

static void fx_reg_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if (!(flags & SONG_FIRSTTICK))
                fx_do_freq_slide(flags, chan, (int)(param * 4));
}


static void fx_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if (param)
                chan->mem_pitchslide = param;
        else
                param = chan->mem_pitchslide;
        if (!(flags & SONG_COMPATGXX))
                chan->mem_portanote = param;

        switch (param & 0xf0) {
        case 0xe0:
                fx_extra_fine_portamento_up(flags, chan, param & 0x0F);
                break;
        case 0xf0:
                fx_fine_portamento_up(flags, chan, param & 0x0F);
                break;
        default:
                fx_reg_portamento_up(flags, chan, param);
                break;
        }
}

static void fx_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        if (param)
                chan->mem_pitchslide = param;
        else
                param = chan->mem_pitchslide;
        if (!(flags & SONG_COMPATGXX))
                chan->mem_portanote = param;

        switch (param & 0xf0) {
        case 0xe0:
                fx_extra_fine_portamento_down(flags, chan, param & 0x0F);
                break;
        case 0xf0:
                fx_fine_portamento_down(flags, chan, param & 0x0F);
                break;
        default:
                fx_reg_portamento_down(flags, chan, param);
                break;
        }
}

static void fx_tone_portamento(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        int delta;

        if (param)
                chan->mem_portanote = param;
        else
                param = chan->mem_portanote;
        if (!(flags & SONG_COMPATGXX))
                chan->mem_pitchslide = param;
        chan->flags |= CHN_PORTAMENTO;
        if (chan->period && chan->portamento_target && !(flags & SONG_FIRSTTICK)) {
                if (chan->period < chan->portamento_target) {
                        if (flags & SONG_LINEARSLIDES) {
                                uint32_t n = MIN(255, param);
                                delta = _muldivr(chan->period, linear_slide_up_table[n], 65536) - chan->period;
                                if (delta < 1) delta = 1;
                        } else {
                                delta = param * 4;
                        }
                        chan->period += delta;
                        if (chan->period > chan->portamento_target) {
                                chan->period = chan->portamento_target;
                                chan->portamento_target = 0;
                        }
                } else if (chan->period > chan->portamento_target) {
                        if (flags & SONG_LINEARSLIDES) {
                                uint32_t n = MIN(255, param);
                                delta = _muldivr(chan->period, linear_slide_down_table[n], 65536) - chan->period;
                                if (delta > -1) delta = -1;
                        } else {
                                delta = -param * 4;
                        }
                        chan->period += delta;
                        if (chan->period < chan->portamento_target) {
                                chan->period = chan->portamento_target;
                                chan->portamento_target = 0;
                        }
                }
        }
}

// Implemented for IMF compatibility, can't actually save this in any formats
// sign should be 1 (up) or -1 (down)
static void fx_note_slide(uint32_t flags, song_voice_t *chan, uint32_t param, int sign)
{
        uint8_t x, y;
        if (flags & SONG_FIRSTTICK) {
                x = param & 0xf0;
                if (x)
                        chan->note_slide_speed = (x >> 4);
                y = param & 0xf;
                if (y)
                        chan->note_slide_step = y;
                chan->note_slide_counter = chan->note_slide_speed;
        } else {
                        if (--chan->note_slide_counter == 0) {
                                chan->note_slide_counter = chan->note_slide_speed;
                                // update it
                                chan->period = get_period_from_note
                                        (sign * chan->note_slide_step + get_note_from_period(chan->period),
                                         8363, 0);
                        }
        }
}



static void fx_vibrato(song_voice_t *p, uint32_t param)
{
        if (param & 0x0F)
                p->vibrato_depth = (param & 0x0F) * 4;
        if (param & 0xF0)
                p->vibrato_speed = (param >> 4) & 0x0F;
        p->flags |= CHN_VIBRATO;
}

static void fx_fine_vibrato(song_voice_t *p, uint32_t param)
{
        if (param & 0x0F)
                p->vibrato_depth = param & 0x0F;
        if (param & 0xF0)
                p->vibrato_speed = (param >> 4) & 0x0F;
        p->flags |= CHN_VIBRATO;
}


static void fx_panbrello(song_voice_t *chan, uint32_t param)
{
        unsigned int panpos = chan->panbrello_position & 0xFF;
        int pdelta;

        if (param & 0x0F)
                chan->panbrello_depth = param & 0x0F;
        if (param & 0xF0)
                chan->panbrello_speed = (param >> 4) & 0x0F;

        switch (chan->panbrello_type) {
        case VIB_SINE:
        default:
                pdelta = sine_table[panpos];
                break;
        case VIB_RAMP_DOWN:
                pdelta = ramp_down_table[panpos];
                break;
        case VIB_SQUARE:
                pdelta = square_table[panpos];
                break;
        case VIB_RANDOM:
                pdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        chan->panbrello_position += chan->panbrello_speed;
        pdelta = ((pdelta * (int)chan->panbrello_depth) + 2) >> 3;
        chan->panbrello_delta = pdelta;
}


static void fx_volume_up(song_voice_t *chan, uint32_t param)
{
        chan->volume += param * 4;
        if (chan->volume > 256)
                chan->volume = 256;
}

static void fx_volume_down(song_voice_t *chan, uint32_t param)
{
        chan->volume -= param * 4;
        if (chan->volume < 0)
                chan->volume = 0;
}

static void fx_volume_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        // Dxx     Volume slide down
        //
        // if (xx == 0) then xx = last xx for (Dxx/Kxx/Lxx) for this channel.
        if (param)
                chan->mem_volslide = param;
        else
                param = chan->mem_volslide;

        // Order of testing: Dx0, D0x, DxF, DFx
        if (param == (param & 0xf0)) {
                // Dx0     Set effect update for channel enabled if channel is ON.
                //         If x = F, then slide up volume by 15 straight away also (for S3M compat)
                //         Every update, add x to the volume, check and clip values > 64 to 64
                param >>= 4;
                if (param == 0xf || !(flags & SONG_FIRSTTICK))
                        fx_volume_up(chan, param);
        } else if (param == (param & 0xf)) {
                // D0x     Set effect update for channel enabled if channel is ON.
                //         If x = F, then slide down volume by 15 straight away also (for S3M)
                //         Every update, subtract x from the volume, check and clip values < 0 to 0
                if (param == 0xf || !(flags & SONG_FIRSTTICK))
                        fx_volume_down(chan, param);
        } else if ((param & 0xf) == 0xf) {
                // DxF     Add x to volume straight away. Check and clip values > 64 to 64
                param >>= 4;
                if (flags & SONG_FIRSTTICK)
                        fx_volume_up(chan, param);
        } else if ((param & 0xf0) == 0xf0) {
                // DFx     Subtract x from volume straight away. Check and clip values < 0 to 0
                param &= 0xf;
                if (flags & SONG_FIRSTTICK)
                        fx_volume_down(chan, param);
        }
}


static void fx_panning_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        int32_t slide = 0;
        if (param)
                chan->mem_panslide = param;
        else
                param = chan->mem_panslide;
        if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
                if (flags & SONG_FIRSTTICK) {
                        param = (param & 0xF0) >> 2;
                        slide = - (int)param;
                }
        } else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
                if (flags & SONG_FIRSTTICK) {
                        slide = (param & 0x0F) << 2;
                }
        } else {
                if (!(flags & SONG_FIRSTTICK)) {
                        if (param & 0x0F)
                                slide = (int)((param & 0x0F) << 2);
                        else
                                slide = -(int)((param & 0xF0) >> 2);
                }
        }
        if (slide) {
                slide += chan->panning;
                chan->panning = CLAMP(slide, 0, 256);
        }
        chan->flags &= ~CHN_SURROUND;
        chan->panbrello_delta = 0;
}


static void fx_tremolo(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        unsigned int trempos = chan->tremolo_position & 0xFF;
        int tdelta;

        if (param & 0x0F)
                chan->tremolo_depth = (param & 0x0F) << 2;
        if (param & 0xF0)
                chan->tremolo_speed = (param >> 4) & 0x0F;

        chan->flags |= CHN_TREMOLO;

        // don't handle on first tick if old-effects mode
        if ((flags & SONG_FIRSTTICK) && (flags & SONG_ITOLDEFFECTS))
                return;

        switch (chan->tremolo_type) {
        case VIB_SINE:
        default:
                tdelta = sine_table[trempos];
                break;
        case VIB_RAMP_DOWN:
                tdelta = ramp_down_table[trempos];
                break;
        case VIB_SQUARE:
                tdelta = square_table[trempos];
                break;
        case VIB_RANDOM:
                tdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        chan->tremolo_position = (trempos + 4 * chan->tremolo_speed) & 0xFF;
        tdelta = (tdelta * (int)chan->tremolo_depth) >> 5;
        chan->tremolo_delta = tdelta;
}


static void fx_retrig_note(song_t *csf, uint32_t nchan, uint32_t param)
{
        song_voice_t *chan = &csf->voices[nchan];

        //printf("Q%02X note=%02X tick%d  %d\n", param, chan->row_note, tick_count, chan->cd_retrig);
        if ((csf->flags & SONG_FIRSTTICK) && chan->row_note != NOTE_NONE) {
                chan->cd_retrig = param & 0xf;
        } else if (--chan->cd_retrig <= 0) {
                chan->cd_retrig = param & 0xf;
                param >>= 4;
                if (param) {
                        int vol = chan->volume;
                        if (retrig_table_1[param])
                                vol = (vol * retrig_table_1[param]) >> 4;
                        else
                                vol += (retrig_table_2[param]) << 2;
                        chan->volume = CLAMP(vol, 0, 256);
                        chan->flags |= CHN_FASTVOLRAMP;
                }

                uint32_t note = chan->new_note;
                int32_t period = chan->period;
                if (NOTE_IS_NOTE(note) && chan->length)
                        csf_check_nna(csf, nchan, 0, note, 1);
                csf_note_change(csf, nchan, note, 1, 1, 0);
                if (period && chan->row_note == NOTE_NONE)
                        chan->period = period;
                chan->position = chan->position_frac = 0;
        }
}


static void fx_channel_vol_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
        int32_t slide = 0;
        if (param)
                chan->mem_channel_volslide = param;
        else
                param = chan->mem_channel_volslide;
        if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
                if (flags & SONG_FIRSTTICK)
                        slide = param >> 4;
        } else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
                if (flags & SONG_FIRSTTICK)
                        slide = - (int)(param & 0x0F);
        } else {
                if (!(flags & SONG_FIRSTTICK)) {
                        if (param & 0x0F)
                                slide = -(int)(param & 0x0F);
                        else
                                slide = (int)((param & 0xF0) >> 4);
                }
        }
        if (slide) {
                slide += chan->global_volume;
                chan->global_volume = CLAMP(slide, 0, 64);
        }
}


static void fx_global_vol_slide(song_t *csf, song_voice_t *chan, uint32_t param)
{
        int32_t slide = 0;
        if (param)
                chan->mem_global_volslide = param;
        else
                param = chan->mem_global_volslide;
        if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
                if (csf->flags & SONG_FIRSTTICK)
                        slide = param >> 4;
        } else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
                if (csf->flags & SONG_FIRSTTICK)
                        slide = -(int)(param & 0x0F);
        } else {
                if (!(csf->flags & SONG_FIRSTTICK)) {
                        if (param & 0xF0)
                                slide = (int)((param & 0xF0) >> 4);
                        else
                                slide = -(int)(param & 0x0F);
                }
        }
        if (slide) {
                slide += csf->current_global_volume;
                csf->current_global_volume = CLAMP(slide, 0, 128);
        }
}


static void fx_pattern_loop(song_t *csf, song_voice_t *chan, uint32_t param)
{
        if (param) {
                if (chan->cd_patloop) {
                        if (!--chan->cd_patloop) {
                                // this should get rid of that nasty infinite loop for cases like
                                //     ... .. .. SB0
                                //     ... .. .. SB1
                                //     ... .. .. SB1
                                // it still doesn't work right in a few strange cases, but oh well :P
                                chan->patloop_row = csf->row + 1;
                                return; // don't loop!
                        }
                } else {
                        chan->cd_patloop = param;
                }
                csf->process_row = chan->patloop_row - 1;
        } else {
                chan->patloop_row = csf->row;
        }
}


static void fx_special(song_t *csf, uint32_t nchan, uint32_t param)
{
        song_voice_t *chan = &csf->voices[nchan];
        uint32_t command = param & 0xF0;
        param &= 0x0F;
        switch(command) {
        // S0x: Set Filter
        // S1x: Set Glissando Control
        case 0x10:
                chan->flags &= ~CHN_GLISSANDO;
                if (param) chan->flags |= CHN_GLISSANDO;
                break;
        // S2x: Set FineTune (no longer implemented)
        // S3x: Set Vibrato WaveForm
        case 0x30:
                chan->vib_type = param;
                break;
        // S4x: Set Tremolo WaveForm
        case 0x40:
                chan->tremolo_type = param;
                break;
        // S5x: Set Panbrello WaveForm
        case 0x50:
                chan->panbrello_type = param;
                break;
        // S6x: Pattern Delay for x ticks
        case 0x60:
                if (csf->flags & SONG_FIRSTTICK)
                        csf->tick_count += param;
                break;
        // S7x: Envelope Control
        case 0x70:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                switch(param) {
                case 0:
                case 1:
                case 2:
                        {
                                song_voice_t *bkp = &csf->voices[MAX_CHANNELS];
                                for (uint32_t i=MAX_CHANNELS; i<MAX_VOICES; i++, bkp++) {
                                        if (bkp->master_channel == nchan+1) {
                                                if (param == 1) {
                                                        fx_key_off(csf, i);
                                                } else if (param == 2) {
                                                        bkp->flags |= CHN_NOTEFADE;
                                                } else {
                                                        bkp->flags |= CHN_NOTEFADE;
                                                        bkp->fadeout_volume = 0;
                                                }
                                        }
                                }
                        }
                        break;
                case  3:        chan->nna = NNA_NOTECUT; break;
                case  4:        chan->nna = NNA_CONTINUE; break;
                case  5:        chan->nna = NNA_NOTEOFF; break;
                case  6:        chan->nna = NNA_NOTEFADE; break;
                case  7:        chan->flags &= ~CHN_VOLENV; break;
                case  8:        chan->flags |= CHN_VOLENV; break;
                case  9:        chan->flags &= ~CHN_PANENV; break;
                case 10:        chan->flags |= CHN_PANENV; break;
                case 11:        chan->flags &= ~CHN_PITCHENV; break;
                case 12:        chan->flags |= CHN_PITCHENV; break;
                }
                break;
        // S8x: Set 4-bit Panning
        case 0x80:
                if (csf->flags & SONG_FIRSTTICK) {
                        chan->flags &= ~CHN_SURROUND;
                        chan->panbrello_delta = 0;
                        chan->panning = (param << 4) + 8;
                        chan->flags |= CHN_FASTVOLRAMP;
                        chan->pan_swing = 0;
                }
                break;
        // S9x: Set Surround
        case 0x90:
                if (param == 1 && (csf->flags & SONG_FIRSTTICK)) {
                        chan->flags |= CHN_SURROUND;
                        chan->panbrello_delta = 0;
                        chan->panning = 128;
                }
                break;
        // SAx: Set 64k Offset
        // Note: don't actually APPLY the offset, and don't clear the regular offset value, either.
        case 0xA0:
                if (csf->flags & SONG_FIRSTTICK) {
                        chan->mem_offset = (param << 16) | (chan->mem_offset & ~0xf0000);
                }
                break;
        // SBx: Pattern Loop
        case 0xB0:
                if (csf->flags & SONG_FIRSTTICK)
                        fx_pattern_loop(csf, chan, param & 0x0F);
                break;
        // SCx: Note Cut
        case 0xC0:
                if (csf->flags & SONG_FIRSTTICK)
                        chan->cd_note_cut = param ?: 1;
                else if (--chan->cd_note_cut == 0)
                        fx_note_cut(csf, nchan);
                break;
        // SDx: Note Delay
        // SEx: Pattern Delay for x rows
        case 0xE0:
                if (csf->flags & SONG_FIRSTTICK) {
                        if (!csf->row_count) // ugh!
                                csf->row_count = param + 1;
                }
                break;
        // SFx: Set Active Midi Macro
        case 0xF0:
                chan->active_macro = param;
                break;
        }
}


// this is all brisby
void csf_midi_send(song_t *csf, const unsigned char *data, unsigned int len, uint32_t nchan, int fake)
{
        song_voice_t *chan = &csf->voices[nchan];
        int oldcutoff;
        const unsigned char *idata = data;
        unsigned int ilen = len;

        while (ilen > 4 && idata[0] == 0xF0 && idata[1] == 0xF0) {
                // impulse tracker filter control (mfg. 0xF0)
                switch (idata[2]) {
                case 0x00: // set cutoff
                        oldcutoff = chan->cutoff;
                        if (idata[3] < 0x80)
                                chan->cutoff = idata[3];
                        oldcutoff -= chan->cutoff;
                        if (oldcutoff < 0)
                                oldcutoff = -oldcutoff;
                        if (chan->volume > 0 || oldcutoff < 0x10
                            || !(chan->flags & CHN_FILTER)
                            || !(chan->left_volume|chan->right_volume)) {
                                setup_channel_filter(chan, !(chan->flags & CHN_FILTER), 256, csf->mix_frequency);
                        }
                        break;
                case 0x01: // set resonance
                        if (idata[3] < 0x80)
                                chan->resonance = idata[3];
                        setup_channel_filter(chan, !(chan->flags & CHN_FILTER), 256, csf->mix_frequency);
                        break;
                }
                idata += 4;
                ilen -= 4;
        }

        if (!fake && csf_midi_out_raw) {
                /* okay, this is kind of how it works.
                we pass buffer_count as here because while
                        1000 * ((8((buffer_size/2) - buffer_count)) / sample_rate)
                is the number of msec we need to delay by, libmodplug simply doesn't know
                what the buffer size is at this point so buffer_count simply has no
                frame of reference.

                fortunately, schism does and can complete this (tags: _schism_midi_out_raw )

                */
                csf_midi_out_raw(data, len, csf->buffer_count);
        }
}


static int _was_complete_midi(unsigned char *q, unsigned int len, int nextc)
{
        if (len == 0) return 0;
        if (*q == 0xF0) return (q[len-1] == 0xF7 ? 1 : 0);
        return ((nextc & 0x80) ? 1 : 0);
}

void csf_process_midi_macro(song_t *csf, uint32_t nchan, const char * macro, uint32_t param,
                        uint32_t note, uint32_t velocity, uint32_t use_instr)
{
/* this was all wrong. -mrsb */
        song_voice_t *chan = &csf->voices[nchan];
        song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE)
                        ? csf->instruments[use_instr ?: chan->last_instrument]
                        : NULL;
        unsigned char outbuffer[64];
        unsigned char cx;
        int mc, fake = 0;
        int saw_c;
        int i, j, x;

        saw_c = 0;
        if (!penv || penv->midi_channel_mask == 0) {
                /* okay, there _IS_ no real midi channel. forget this for now... */
                mc = 15;
                fake = 1;

        } else if (penv->midi_channel_mask >= 0x10000) {
                mc = (nchan-1) % 16;
        } else {
                mc = 0;
                while(!(penv->midi_channel_mask & (1 << mc))) ++mc;
        }

        for (i = j = x = 0, cx =0; i <= 32 && macro[i]; i++) {
                int c, cw;
                if (macro[i] >= '0' && macro[i] <= '9') {
                        c = macro[i] - '0';
                        cw = 1;
                } else if (macro[i] >= 'A' && macro[i] <= 'F') {
                        c = (macro[i] - 'A') + 10;
                        cw = 1;
                } else if (macro[i] == 'c') {
                        c = mc;
                        cw = 1;
                        saw_c = 1;
                } else if (macro[i] == 'n') {
                        c = (note-1);
                        cw = 2;
                } else if (macro[i] == 'v') {
                        c = velocity;
                        cw = 2;
                } else if (macro[i] == 'u') {
                        c = (chan->volume >> 1);
                        if (c > 127) c = 127;
                        cw = 2;
                } else if (macro[i] == 'x') {
                        c = chan->panning;
                        if (c > 127) c = 127;
                        cw = 2;
                } else if (macro[i] == 'y') {
                        c = chan->final_panning;
                        if (c > 127) c = 127;
                        cw = 2;
                } else if (macro[i] == 'a') {
                        if (!penv)
                                c = 0;
                        else
                                c = (penv->midi_bank >> 7) & 127;
                        cw = 2;
                } else if (macro[i] == 'b') {
                        if (!penv)
                                c = 0;
                        else
                                c = penv->midi_bank & 127;
                        cw = 2;
                } else if (macro[i] == 'z' || macro[i] == 'p') {
                        c = param & 0x7F;
                        cw = 2;
                } else {
                        continue;
                }
                if (j == 0 && cw == 1) {
                        cx = c;
                        j = 1;
                        continue;
                } else if (j == 1 && cw == 1) {
                        cx = (cx << 4) | c;
                        j = 0;
                } else if (j == 0) {
                        cx = c;
                } else if (j == 1) {
                        outbuffer[x] = cx;
                        x++;

                        cx = c;
                        j = 0;
                }
                // start of midi message
                if (_was_complete_midi(outbuffer, x, cx)) {
                        csf_midi_send(csf, outbuffer, x, nchan, saw_c && fake);
                        x = 0;
                }
                outbuffer[x] = cx;
                x++;
        }
        if (j == 1) {
                outbuffer[x] = cx;
                x++;
        }
        if (x) {
                // terminate sysex
                if (!_was_complete_midi(outbuffer, x, 0xFF)) {
                        if (*outbuffer == 0xF0) {
                                outbuffer[x] = 0xF7;
                                x++;
                        }
                }
                csf_midi_send(csf, outbuffer, x, nchan, saw_c && fake);
        }
}


////////////////////////////////////////////////////////////
// Length

unsigned int csf_get_length(song_t *csf)
{
        uint32_t elapsed=0, row=0, cur_pattern=0, next_pattern=0, pat=csf->orderlist[0];
        uint32_t speed=csf->initial_speed, tempo=csf->initial_tempo, next_row=0;
        uint32_t max_row = 0, max_pattern = 0;
        uint8_t instr[MAX_VOICES] = {0};
        uint8_t notes[MAX_VOICES] = {0};
        uint32_t patloop[MAX_VOICES] = {0};
        uint8_t vols[MAX_VOICES];
        uint8_t chnvols[MAX_VOICES];

        memset(vols, 0xFF, sizeof(vols));
        memset(chnvols, 64, sizeof(chnvols));
        for (uint32_t icv=0; icv<MAX_CHANNELS; icv++)
                chnvols[icv] = csf->channels[icv].volume;
        max_row = csf->process_row;
        max_pattern = csf->process_order;
        cur_pattern = next_pattern = 0;
        pat = csf->orderlist[0];
        row = next_row = 0;
        for (;;) {
                uint32_t speed_count = 0;
                row = next_row;
                cur_pattern = next_pattern;

                // Check if pattern is valid
                pat = csf->orderlist[cur_pattern];
                while (pat >= MAX_PATTERNS) {
                        // End of song ?
                        if (pat == 0xFF || cur_pattern >= MAX_ORDERS) {
                                goto EndMod;
                        } else {
                                cur_pattern++;
                                pat = (cur_pattern < MAX_ORDERS)
                                        ? csf->orderlist[cur_pattern]
                                        : 0xFF;
                        }
                        next_pattern = cur_pattern;
                }
                // Weird stuff?
                if ((pat >= MAX_PATTERNS) || (!csf->patterns[pat])) break;
                // Should never happen
                if (row >= csf->pattern_size[pat]) row = 0;
                // Update next position
                next_row = row + 1;
                if (next_row >= csf->pattern_size[pat]) {
                        next_pattern = cur_pattern + 1;
                        next_row = 0;
                }
                /* muahahaha */
                if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
                        if (csf->stop_at_order <= (signed) cur_pattern && csf->stop_at_row <= (signed) row)
                                goto EndMod;
                        if (csf->stop_at_time > 0) {
                                /* stupid api decision */
                                if (((elapsed+500) / 1000) >= csf->stop_at_time) {
                                        csf->stop_at_order = cur_pattern;
                                        csf->stop_at_row = row;
                                        goto EndMod;
                                }
                        }
                }

                if (!row) {
                        for (uint32_t ipck=0; ipck<MAX_CHANNELS; ipck++)
                                patloop[ipck] = elapsed;
                }
                song_voice_t *chan = csf->voices;
                song_note_t *p = csf->patterns[pat] + row * MAX_CHANNELS;
                for (uint32_t nchan=0; nchan<MAX_CHANNELS; p++,chan++, nchan++)
                if (*((uint32_t *)p)) {
                        uint32_t command = p->effect;
                        uint32_t param = p->param;
                        uint32_t note = p->note;
                        if (p->instrument) {
                                instr[nchan] = p->instrument;
                                notes[nchan] = 0;
                                vols[nchan] = 0xFF;
                        }
                        if (NOTE_IS_NOTE(note))
                                notes[nchan] = note;
                        if (p->voleffect == VOLFX_VOLUME)
                                vols[nchan] = p->volparam;
                        switch (command) {
                        case 0: break;
                        // Position Jump
                        case FX_POSITIONJUMP:
                                if (param <= cur_pattern)
                                        goto EndMod;
                                next_pattern = param;
                                next_row = 0;
                                break;
                        // Pattern Break
                        case FX_PATTERNBREAK:
                                next_row = param;
                                next_pattern = cur_pattern + 1;
                                break;
                        // Set Speed
                        case FX_SPEED:
                                if (param)
                                        speed = param;
                                break;
                        // Set Tempo
                        case FX_TEMPO:
                                if (param)
                                        chan->mem_tempo = param;
                                else
                                        param = chan->mem_tempo;
                                int d = (param & 0xf);
                                switch (param >> 4) {
                                default:
                                        tempo = param;
                                        break;
                                case 0:
                                        d = -d;
                                case 1:
                                        d = d * speed + tempo;
                                        tempo = CLAMP(d, 32, 255);
                                        break;
                                }
                                break;
                        // Pattern Delay
                        case FX_SPECIAL:
                                switch (param >> 4) {
                                case 0x6:
                                        speed_count = param & 0x0F;
                                        break;
                                case 0xb:
                                        if (param & 0x0F) {
                                                elapsed +=
                                                        (elapsed - patloop[nchan]) * (param & 0x0F);
                                        } else {
                                                patloop[nchan] = elapsed;
                                        }
                                        break;
                                case 0xe:
                                        speed_count = (param & 0x0F) * speed;
                                        break;
                                }
                                break;
                        }
                }
                speed_count += speed;
                elapsed += (2500 * speed_count) / tempo;
        }
EndMod:
        return (elapsed+500) / 1000;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Effects

song_sample_t *csf_translate_keyboard(song_t *csf, song_instrument_t *penv, uint32_t note, song_sample_t *def)
{
        uint32_t n = penv->sample_map[note - 1];
        return (n && n < MAX_SAMPLES) ? &csf->samples[n] : def;
}

static void env_reset(song_voice_t *chan, int always)
{
        if (chan->ptr_instrument) {
                chan->flags |= CHN_FASTVOLRAMP;
                if (always) {
                        chan->vol_env_position = 0;
                        chan->pan_env_position = 0;
                        chan->pitch_env_position = 0;
                } else {
                        /* only reset envelopes with carry off */
                        if (!(chan->ptr_instrument->flags & ENV_VOLCARRY))
                                chan->vol_env_position = 0;
                        if (!(chan->ptr_instrument->flags & ENV_PANCARRY))
                                chan->pan_env_position = 0;
                        if (!(chan->ptr_instrument->flags & ENV_PITCHCARRY))
                                chan->pitch_env_position = 0;
                }
        }

        // this was migrated from csf_note_change, should it be here?
        chan->flags &= ~CHN_NOTEFADE;
        chan->fadeout_volume = 65536;
}

void csf_instrument_change(song_t *csf, song_voice_t *chan, uint32_t instr, int porta, int inst_column)
{
        int inst_changed = 0;

        if (instr >= MAX_INSTRUMENTS) return;
        song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? csf->instruments[instr] : NULL;
        song_sample_t *psmp = chan->ptr_sample;
        uint32_t note = chan->new_note;

        if (note == NOTE_NONE) {
                /* nothing to see here */
        } else if (NOTE_IS_CONTROL(note)) {
                /* nothing here either */
        } else if (penv) {
                if (NOTE_IS_CONTROL(penv->note_map[note-1]))
                        return;
                if (!(porta && penv == chan->ptr_instrument && chan->ptr_sample && chan->current_sample_data))
                        psmp = csf_translate_keyboard(csf, penv, note, NULL);
                chan->flags &= ~CHN_SUSTAINLOOP; // turn off sustain
        } else {
                psmp = csf->samples + instr;
        }

        // Update Volume
        if (inst_column && psmp) chan->volume = psmp->volume;
        // inst_changed is used for IT carry-on env option
        if (penv != chan->ptr_instrument || !chan->current_sample_data) {
                inst_changed = 1;
                chan->ptr_instrument = penv;
        }

        // Instrument adjust
        chan->new_instrument = 0;
        if (psmp) {
                psmp->played = 1;
                if (penv) {
                        penv->played = 1;
                        chan->instrument_volume = (psmp->global_volume * penv->global_volume) >> 7;
                        if (penv->flags & ENV_SETPANNING)
                                chan->panning = penv->panning;
                        chan->nna = penv->nna;
                } else {
                        chan->instrument_volume = psmp->global_volume;
                }
                if (psmp->flags & CHN_PANNING)
                        chan->panning = psmp->panning;
        }

        // Reset envelopes

        // Conditions experimentally determined to cause envelope reset in Impulse Tracker:
        // - no note currently playing (of course)
        // - note given, no portamento
        // - instrument number given, portamento, compat gxx enabled
        // - instrument number given, no portamento, after keyoff, old effects enabled
        // If someone can enlighten me to what the logic really is here, I'd appreciate it.
        // Seems like it's just a total mess though, probably to get XMs to play right.
        if (penv) {
                if ((
                        !chan->length
                ) || (
                        inst_column
                        && porta
                        && (csf->flags & SONG_COMPATGXX)
                ) || (
                        inst_column
                        && !porta
                        && (chan->flags & (CHN_NOTEFADE|CHN_KEYOFF))
                        && (csf->flags & SONG_ITOLDEFFECTS)
                )) {
                        env_reset(chan, inst_changed || (chan->flags & CHN_KEYOFF));
                } else if (!(penv->flags & ENV_VOLUME)) {
                        // XXX why is this being done?
                        chan->vol_env_position = 0;
                }

                chan->vol_swing = chan->pan_swing = 0;
                if (penv->vol_swing) {
                        /* this was wrong, and then it was still wrong.
                        (possibly it continues to be wrong even now?) */
                        double d = 2 * (((double) rand()) / RAND_MAX) - 1;
                        chan->vol_swing = d * penv->vol_swing / 100.0 * chan->volume;
                }
                if (penv->pan_swing) {
                        /* this was also wrong, and even more so */
                        double d = 2 * (((double) rand()) / RAND_MAX) - 1;
                        chan->pan_swing = d * penv->pan_swing * 4;
                }
        }

        // Invalid sample ?
        if (!psmp) {
                chan->ptr_sample = NULL;
                chan->instrument_volume = 0;
                return;
        }
        if (psmp == chan->ptr_sample && chan->current_sample_data && chan->length)
                return;

        // sample change: reset sample vibrato
        chan->autovib_depth = 0;
        chan->autovib_position = 0;

        if ((chan->flags & (CHN_KEYOFF | CHN_NOTEFADE)) && inst_column) {
                // Don't start new notes after ===/~~~
                chan->period = 0;
        } else {
                chan->period = get_freq_from_period(get_freq_from_period(chan->period, psmp->c5speed, 1),
                                                chan->c5speed, 1);
        }
        chan->flags &= ~(CHN_SAMPLE_FLAGS | CHN_KEYOFF | CHN_NOTEFADE
                           | CHN_VOLENV | CHN_PANENV | CHN_PITCHENV);
        chan->flags |= psmp->flags & CHN_SAMPLE_FLAGS;
        if (penv) {
                if (penv->flags & ENV_VOLUME)
                        chan->flags |= CHN_VOLENV;
                if (penv->flags & ENV_PANNING)
                        chan->flags |= CHN_PANENV;
                if (penv->flags & ENV_PITCH)
                        chan->flags |= CHN_PITCHENV;
                if ((penv->flags & ENV_PITCH) && (penv->flags & ENV_FILTER) && !chan->cutoff)
                        chan->cutoff = 0x7F;
                if (penv->ifc & 0x80)
                        chan->cutoff = penv->ifc & 0x7F;
                if (penv->ifr & 0x80)
                        chan->resonance = penv->ifr & 0x7F;
        }

        chan->ptr_sample = psmp;
        chan->length = psmp->length;
        chan->loop_start = psmp->loop_start;
        chan->loop_end = psmp->loop_end;
        chan->c5speed = psmp->c5speed;
        chan->current_sample_data = psmp->data;
        chan->position = 0;

        if (chan->flags & CHN_SUSTAINLOOP) {
                chan->loop_start = psmp->sustain_start;
                chan->loop_end = psmp->sustain_end;
                chan->flags |= CHN_LOOP;
                if (chan->flags & CHN_PINGPONGSUSTAIN)
                        chan->flags |= CHN_PINGPONGLOOP;
        }
        if ((chan->flags & CHN_LOOP) && chan->loop_end < chan->length)
                chan->length = chan->loop_end;
        /*fprintf(stderr, "length set as %d (from %d), ch flags %X smp flags %X\n",
            (int)chan->length,
            (int)psmp->length, chan->flags, psmp->flags);*/
}


void csf_note_change(song_t *csf, uint32_t nchan, int note, int porta, int retrig, int manual)
{
        // why would csf_note_change ever get a negative value for 'note'?
        if (note == NOTE_NONE || note < 0)
                return;
        song_voice_t *chan = &csf->voices[nchan];
        song_sample_t *pins = chan->ptr_sample;
        song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;
        if (penv && NOTE_IS_NOTE(note)) {
                pins = csf_translate_keyboard(csf, penv, note, pins);
                note = penv->note_map[note - 1];
                chan->flags &= ~CHN_SUSTAINLOOP; // turn off sustain
        }

        if (NOTE_IS_CONTROL(note)) {
                // hax: keep random sample numbers from triggering notes (see csf_instrument_change)
                // NOTE_OFF is a completely arbitrary choice - this could be anything above NOTE_LAST
                chan->new_note = NOTE_OFF;
                switch (note) {
                case NOTE_OFF:
                        fx_key_off(csf, nchan);
                        break;
                case NOTE_CUT:
                        fx_note_cut(csf, nchan);
                        break;
                case NOTE_FADE:
                default: // Impulse Tracker handles all unknown notes as fade internally
                        chan->flags |= CHN_NOTEFADE;
                        break;
                }
                return;
        }

        if (!pins)
                return;
        note = CLAMP(note, NOTE_FIRST, NOTE_LAST);
        chan->note = note;
        chan->new_instrument = 0;
        uint32_t period = get_period_from_note(note, chan->c5speed, csf->flags & SONG_LINEARSLIDES);
        if (period) {
                if (porta && chan->period) {
                        chan->portamento_target = period;
                } else {
                        chan->portamento_target = 0;
                        chan->period = period;
                }
                if (!porta || !chan->length) {
                        chan->ptr_sample = pins;
                        chan->current_sample_data = pins->data;
                        chan->length = pins->length;
                        chan->loop_end = pins->length;
                        chan->loop_start = 0;
                        chan->flags = (chan->flags & ~CHN_SAMPLE_FLAGS) | (pins->flags & CHN_SAMPLE_FLAGS);
                        if (chan->flags & CHN_SUSTAINLOOP) {
                                chan->loop_start = pins->sustain_start;
                                chan->loop_end = pins->sustain_end;
                                chan->flags &= ~CHN_PINGPONGLOOP;
                                chan->flags |= CHN_LOOP;
                                if (chan->flags & CHN_PINGPONGSUSTAIN) chan->flags |= CHN_PINGPONGLOOP;
                                if (chan->length > chan->loop_end) chan->length = chan->loop_end;
                        } else if (chan->flags & CHN_LOOP) {
                                chan->loop_start = pins->loop_start;
                                chan->loop_end = pins->loop_end;
                                if (chan->length > chan->loop_end) chan->length = chan->loop_end;
                        }
                        chan->position = chan->position_frac = 0;
                }
                if (chan->position >= chan->length)
                        chan->position = chan->loop_start;
        } else {
                porta = 0;
        }

        if (!porta)
                env_reset(chan, 0);

        chan->flags &= ~CHN_KEYOFF;
        // Enable Ramping
        if (!porta) {
                chan->vu_meter = 0x100;
                chan->strike = 4; /* this affects how long the initial hit on the playback marks lasts */
                chan->flags &= ~CHN_FILTER;
                chan->flags |= CHN_FASTVOLRAMP;
                if (!retrig) {
                        chan->autovib_depth = 0;
                        chan->autovib_position = 0;
                        chan->vibrato_position = 0;
                }
                chan->left_volume = chan->right_volume = 0;
                // Setup Initial Filter for this note
                if (penv) {
                        if (penv->ifr & 0x80)
                                chan->resonance = penv->ifr & 0x7F;
                        if (penv->ifc & 0x80)
                                chan->cutoff = penv->ifc & 0x7F;
                } else {
                        chan->vol_swing = chan->pan_swing = 0;
                }

                if (chan->cutoff < 0x7F)
                        setup_channel_filter(chan, 1, 256, csf->mix_frequency);
        }
        // Special case for MPT
        if (manual)
                chan->flags &= ~CHN_MUTE;

}


uint32_t csf_get_nna_channel(song_t *csf, uint32_t nchan)
{
        song_voice_t *chan = &csf->voices[nchan];
        // Check for empty channel
        song_voice_t *pi = &csf->voices[MAX_CHANNELS];
        for (uint32_t i=MAX_CHANNELS; i<MAX_VOICES; i++, pi++) {
                if (!pi->length) {
                        if (pi->flags & CHN_MUTE) {
                                if (pi->flags & CHN_NNAMUTE) {
                                        pi->flags &= ~(CHN_NNAMUTE|CHN_MUTE);
                                } else {
                                        /* this channel is muted; skip */
                                        continue;
                                }
                        }
                        return i;
                }
        }
        if (!chan->fadeout_volume) return 0;
        // All channels are used: check for lowest volume
        uint32_t result = 0;
        uint32_t vol = 64*65536;        // 25%
        int envpos = 0xFFFFFF;
        const song_voice_t *pj = &csf->voices[MAX_CHANNELS];
        for (uint32_t j=MAX_CHANNELS; j<MAX_VOICES; j++, pj++) {
                if (!pj->fadeout_volume) return j;
                uint32_t v = pj->volume;
                if (pj->flags & CHN_NOTEFADE)
                        v = v * pj->fadeout_volume;
                else
                        v <<= 16;
                if (pj->flags & CHN_LOOP) v >>= 1;
                if (v < vol || (v == vol && pj->vol_env_position > envpos)) {
                        envpos = pj->vol_env_position;
                        vol = v;
                        result = j;
                }
        }
        if (result) {
                /* unmute new nna channel */
                csf->voices[result].flags &= ~(CHN_MUTE|CHN_NNAMUTE);
        }
        return result;
}


void csf_check_nna(song_t *csf, uint32_t nchan, uint32_t instr, int note, int force_cut)
{
        song_voice_t *p;
        song_voice_t *chan = &csf->voices[nchan];
        song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;
        song_instrument_t *ptr_instrument;
        signed char *data;
        if (!NOTE_IS_NOTE(note))
                return;
        // Always NNA cut - using
        if (force_cut || !(csf->flags & SONG_INSTRUMENTMODE)) {
                if (!chan->length || (chan->flags & CHN_MUTE) || (!chan->left_volume && !chan->right_volume))
                        return;
                uint32_t n = csf_get_nna_channel(csf, nchan);
                if (!n) return;
                p = &csf->voices[n];
                // Copy Channel
                *p = *chan;
                p->flags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PORTAMENTO);
                p->tremolo_delta = 0;
                p->master_channel = nchan+1;
                p->n_command = 0;
                // Cut the note
                p->fadeout_volume = 0;
                p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
                // Stop this channel
                chan->length = chan->position = chan->position_frac = 0;
                chan->rofs = chan->lofs = 0;
                chan->left_volume = chan->right_volume = 0;
                OPL_NoteOff(nchan);
                OPL_Touch(nchan, NULL, 0);
                GM_KeyOff(nchan);
                GM_Touch(nchan, 0);
                return;
        }
        if (instr >= MAX_INSTRUMENTS) instr = 0;
        data = chan->current_sample_data;
        ptr_instrument = chan->ptr_instrument;
        if (instr && note) {
                ptr_instrument = (csf->flags & SONG_INSTRUMENTMODE) ? csf->instruments[instr] : NULL;
                if (ptr_instrument) {
                        uint32_t n = 0;
                        if (!NOTE_IS_CONTROL(note)) {
                                n = ptr_instrument->sample_map[note-1];
                                note = ptr_instrument->note_map[note-1];
                                if (n && n < MAX_SAMPLES)
                                        data = csf->samples[n].data;
                        }
                } else {
                        data = NULL;
                }
        }
        if (!penv) return;
        p = chan;
        for (uint32_t i=nchan; i<MAX_VOICES; p++, i++) {
                if (!((i >= MAX_CHANNELS || p == chan)
                      && ((p->master_channel == nchan+1 || p == chan)
                          && p->ptr_instrument)))
                        continue;
                int ok = 0;
                // Duplicate Check Type
                switch (p->ptr_instrument->dct) {
                case DCT_NOTE:
                        ok = (NOTE_IS_NOTE(note) && (int) p->note == note && ptr_instrument == p->ptr_instrument);
                        break;
                case DCT_SAMPLE:
                        ok = (data && data == p->current_sample_data);
                        break;
                case DCT_INSTRUMENT:
                        ok = (ptr_instrument == p->ptr_instrument);
                        break;
                }
                // Duplicate Note Action
                if (ok) {
                        switch(p->ptr_instrument->dca) {
                        case DCA_NOTECUT:
                                fx_note_cut(csf, i);
                                break;
                        case DCA_NOTEOFF:
                                fx_key_off(csf, i);
                                break;
                        case DCA_NOTEFADE:
                                p->flags |= CHN_NOTEFADE;
                                break;
                        }
                        if (!p->volume) {
                                p->fadeout_volume = 0;
                                p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
                        }
                }
        }
        if (chan->flags & CHN_MUTE)
                return;
        // New Note Action
        if (chan->volume && chan->length) {
                uint32_t n = csf_get_nna_channel(csf, nchan);
                if (n) {
                        p = &csf->voices[n];
                        // Copy Channel
                        *p = *chan;
                        p->flags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PORTAMENTO);
                        p->tremolo_delta = 0;
                        p->master_channel = nchan+1;
                        p->n_command = 0;
                        // Key Off the note
                        switch(chan->nna) {
                        case NNA_NOTEOFF:
                                fx_key_off(csf, n);
                                break;
                        case NNA_NOTECUT:
                                p->fadeout_volume = 0;
                        case NNA_NOTEFADE:
                                p->flags |= CHN_NOTEFADE;
                                break;
                        }
                        if (!p->volume) {
                                p->fadeout_volume = 0;
                                p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
                        }
                        // Stop this channel
                        chan->length = chan->position = chan->position_frac = 0;
                        chan->rofs = chan->lofs = 0;
                }
        }
}



static void handle_effect(song_t *csf, uint32_t nchan, uint32_t cmd, uint32_t param, int porta, int firsttick)
{
        song_voice_t *chan = csf->voices + nchan;

        switch (cmd) {
        case FX_NONE:
                break;

        // Set Volume
        case FX_VOLUME:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                chan->volume = (param < 64) ? param*4 : 256;
                chan->flags |= CHN_FASTVOLRAMP;
                break;

        case FX_PORTAMENTOUP:
                fx_portamento_up(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_PORTAMENTODOWN:
                fx_portamento_down(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_VOLUMESLIDE:
                fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_TONEPORTAMENTO:
                fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_TONEPORTAVOL:
                fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, 0);
                break;

        case FX_VIBRATO:
                fx_vibrato(chan, param);
                break;

        case FX_VIBRATOVOL:
                fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                fx_vibrato(chan, 0);
                break;

        case FX_SPEED:
                if ((csf->flags & SONG_FIRSTTICK) && param) {
                        csf->tick_count = param;
                        csf->current_speed = param;
                }
                break;

        case FX_TEMPO:
                if (csf->flags & SONG_FIRSTTICK) {
                        if (param)
                                chan->mem_tempo = param;
                        else
                                param = chan->mem_tempo;
                        if (param >= 0x20)
                                csf->current_tempo = param;
                } else {
                        param = chan->mem_tempo; // this just got set on tick zero

                        switch (param >> 4) {
                        case 0:
                                csf->current_tempo -= param & 0xf;
                                if (csf->current_tempo < 32)
                                        csf->current_tempo = 32;
                                break;
                        case 1:
                                csf->current_tempo += param & 0xf;
                                if (csf->current_tempo > 255)
                                        csf->current_tempo = 255;
                                break;
                        }
                }
                break;

        case FX_OFFSET:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                if (param)
                        chan->mem_offset = (chan->mem_offset & ~0xff00) | (param << 8);
                if (NOTE_IS_NOTE(chan->row_note)) {
                        // when would position *not* be zero if there's a note but no portamento?
                        if (porta)
                                chan->position = chan->mem_offset;
                        else
                                chan->position += chan->mem_offset;
                        if (chan->position > chan->length) {
                                chan->position = (csf->flags & SONG_ITOLDEFFECTS) ? chan->length : 0;
                        }
                }
                break;

        case FX_ARPEGGIO:
                chan->n_command = FX_ARPEGGIO;
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                if (param)
                        chan->mem_arpeggio = param;
                break;

        case FX_RETRIG:
                if (param)
                        chan->mem_retrig = param & 0xFF;
                fx_retrig_note(csf, nchan, chan->mem_retrig);
                break;

        case FX_TREMOR:
                // Tremor logic lifted from DUMB, which is the only player that actually gets it right.
                // I *sort of* understand it.
                if (csf->flags & SONG_FIRSTTICK) {
                        if (!param)
                                param = chan->mem_tremor;
                        else if (!(csf->flags & SONG_ITOLDEFFECTS)) {
                                if (param & 0xf0) param -= 0x10;
                                if (param & 0x0f) param -= 0x01;
                        }
                        chan->mem_tremor = param;
                        chan->cd_tremor |= 128;
                }

                if ((chan->cd_tremor & 128) && chan->length) {
                        if (chan->cd_tremor == 128)
                                chan->cd_tremor = (chan->mem_tremor >> 4) | 192;
                        else if (chan->cd_tremor == 192)
                                chan->cd_tremor = (chan->mem_tremor & 0xf) | 128;
                        else
                                chan->cd_tremor--;
                }

                chan->n_command = FX_TREMOR;

                break;

        case FX_GLOBALVOLUME:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                if (param <= 128)
                        csf->current_global_volume = param;
                break;

        case FX_GLOBALVOLSLIDE:
                fx_global_vol_slide(csf, chan, param);
                break;

        case FX_PANNING:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                chan->flags &= ~CHN_SURROUND;
                chan->panbrello_delta = 0;
                chan->panning = param;
                chan->pan_swing = 0;
                chan->flags |= CHN_FASTVOLRAMP;
                break;

        case FX_PANNINGSLIDE:
                fx_panning_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_TREMOLO:
                fx_tremolo(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_FINEVIBRATO:
                fx_fine_vibrato(chan, param);
                break;

        case FX_SPECIAL:
                fx_special(csf, nchan, param);
                break;

        case FX_KEYOFF:
                if ((csf->current_speed - csf->tick_count) == param)
                        fx_key_off(csf, nchan);
                break;

        case FX_CHANNELVOLUME:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                // FIXME rename global_volume to channel_volume in the channel struct
                if (param <= 64) {
                        chan->global_volume = param;
                        chan->flags |= CHN_FASTVOLRAMP;
                }
                break;

        case FX_CHANNELVOLSLIDE:
                fx_channel_vol_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
                break;

        case FX_PANBRELLO:
                fx_panbrello(chan, param);
                break;

        case FX_SETENVPOSITION:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                chan->vol_env_position = param;
                chan->pan_env_position = param;
                chan->pitch_env_position = param;
                if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument) {
                        song_instrument_t *penv = chan->ptr_instrument;
                        if ((chan->flags & CHN_PANENV)
                            && (penv->pan_env.nodes)
                            && ((int)param > penv->pan_env.ticks[penv->pan_env.nodes-1])) {
                                chan->flags &= ~CHN_PANENV;
                        }
                }
                break;

        case FX_POSITIONJUMP:
                if (csf->flags & SONG_FIRSTTICK) {
                        if (!(csf->mix_flags & SNDMIX_NOBACKWARDJUMPS) || csf->process_order < param)
                                csf->process_order = param - 1;
                        csf->process_row = PROCESS_NEXT_ORDER;
                }
                break;

        case FX_PATTERNBREAK:
                if (csf->flags & SONG_FIRSTTICK) {
                        csf->break_row = param;
                        csf->process_row = PROCESS_NEXT_ORDER;
                }
                break;

        case FX_MIDI:
                if (!(csf->flags & SONG_FIRSTTICK))
                        break;
                if (param < 0x80) {
                        csf_process_midi_macro(csf, nchan,
                                csf->midi_config.sfx[chan->active_macro],
                                param, 0, 0, 0);
                } else {
                        csf_process_midi_macro(csf, nchan,
                                csf->midi_config.zxx[param & 0x7F],
                                0, 0, 0, 0);
                }
                break;

        case FX_NOTESLIDEUP:
                fx_note_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param, 1);
                break;
        case FX_NOTESLIDEDOWN:
                fx_note_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param, -1);
                break;
        }
}

static void handle_voleffect(song_t *csf, song_voice_t *chan, uint32_t volcmd, uint32_t vol,
        int firsttick, int start_note)
{
        /* A few notes, paraphrased from ITTECH.TXT:
                Ex/Fx/Gx are shared with Exx/Fxx/Gxx; Ex/Fx are 4x the 'normal' slide value
                Gx is linked with Ex/Fx if Compat Gxx is off, just like Gxx is with Exx/Fxx
                Gx values: 1, 4, 8, 16, 32, 64, 96, 128, 255
                Ax/Bx/Cx/Dx values are used directly (i.e. D9 == D09), and are NOT shared with Dxx
                        (value is stored into mem_vc_volslide and used by A0/B0/C0/D0)
                Hx uses the same value as Hxx and Uxx, and affects the *depth*
                        so... hxx = (hx | (oldhxx & 0xf0))  ???

        Additionally: volume and panning are handled on the start tick, not
        the first tick of the row (that is, SDx alters their behavior) */

        switch (volcmd) {
        case VOLFX_NONE:
                break;

        case VOLFX_VOLUME:
                if (start_note) {
                        if (vol > 64) vol = 64;
                        chan->volume = vol << 2;
                        chan->flags |= CHN_FASTVOLRAMP;
                }
                break;

        case VOLFX_PANNING:
                if (start_note) {
                        if (vol > 64) vol = 64;
                        chan->panning = vol << 2;
                        chan->pan_swing = 0;
                        chan->flags |= CHN_FASTVOLRAMP;
                        chan->flags &= ~CHN_SURROUND;
                        chan->panbrello_delta = 0;
                }
                break;

        case VOLFX_PORTAUP: // Fx
                if (firsttick) {
                        if (vol)
                                chan->mem_pitchslide = 4 * vol;
                        if (!(csf->flags & SONG_COMPATGXX))
                                chan->mem_portanote = chan->mem_pitchslide;
                } else {
                        fx_reg_portamento_up(csf->flags, chan, chan->mem_pitchslide);
                }
                break;

        case VOLFX_PORTADOWN: // Ex
                if (firsttick) {
                        if (vol)
                                chan->mem_pitchslide = 4 * vol;
                        if (!(csf->flags & SONG_COMPATGXX))
                                chan->mem_portanote = chan->mem_pitchslide;
                } else {
                        fx_reg_portamento_down(csf->flags, chan, chan->mem_pitchslide);
                }
                break;

        case VOLFX_TONEPORTAMENTO: // Gx
                fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan,
                        vc_portamento_table[vol & 0x0F]);
                break;

        case VOLFX_VOLSLIDEUP: // Cx
                if (firsttick) {
                        if (vol)
                                chan->mem_vc_volslide = vol;
                } else {
                        fx_volume_up(chan, chan->mem_vc_volslide);
                }
                break;

        case VOLFX_VOLSLIDEDOWN: // Dx
                if (firsttick) {
                        if (vol)
                                chan->mem_vc_volslide = vol;
                } else {
                        fx_volume_down(chan, chan->mem_vc_volslide);
                }
                break;

        case VOLFX_FINEVOLUP: // Ax
                if (firsttick) {
                        if (vol)
                                chan->mem_vc_volslide = vol;
                        else
                                vol = chan->mem_vc_volslide;
                        fx_volume_up(chan, vol);
                }
                break;

        case VOLFX_FINEVOLDOWN: // Bx
                if (firsttick) {
                        if (vol)
                                chan->mem_vc_volslide = vol;
                        else
                                vol = chan->mem_vc_volslide;
                        fx_volume_down(chan, vol);
                }
                break;

        case VOLFX_VIBRATODEPTH: // Hx
                fx_vibrato(chan, vol);
                break;

        case VOLFX_VIBRATOSPEED: // $x (FT2 compat.)
                fx_vibrato(chan, vol << 4);
                break;

        case VOLFX_PANSLIDELEFT: // <x (FT2)
                fx_panning_slide(csf->flags, chan, vol);
                break;

        case VOLFX_PANSLIDERIGHT: // >x (FT2)
                fx_panning_slide(csf->flags, chan, vol << 4);
                break;
        }
}

/* firsttick is only used for SDx at the moment */
void csf_process_effects(song_t *csf, int firsttick)
{
        song_voice_t *chan = csf->voices;
        for (uint32_t nchan=0; nchan<MAX_CHANNELS; nchan++, chan++) {
                chan->n_command=0;

                uint32_t instr = chan->row_instr;
                uint32_t volcmd = chan->row_voleffect;
                uint32_t vol = chan->row_volparam;
                uint32_t cmd = chan->row_effect;
                uint32_t param = chan->row_param;
                int porta = (cmd == FX_TONEPORTAMENTO
                               || cmd == FX_TONEPORTAVOL
                               || volcmd == VOLFX_TONEPORTAMENTO);
                int start_note = csf->flags & SONG_FIRSTTICK;

                chan->flags &= ~CHN_FASTVOLRAMP;

                // set instrument before doing anything else
                if (instr) chan->new_instrument = instr;

                /* Have to handle SDx specially because of the way the effects are structured.
                In a PERFECT world, this would be very straightforward:
                  - Handle the effect column, and set flags for things that should happen
                    (portamento, volume slides, arpeggio, vibrato, tremolo)
                  - If note delay counter is set, stop processing that channel
                  - Trigger all notes if it's their start tick
                  - Handle volume column.
                The obvious implication of this is that all effects are checked only once, and
                volumes only need to be set for notes once. Additionally this helps for separating
                the mixing code from the rest of the interface (which is always good, especially
                for hardware mixing...)
                Oh well, the world is not perfect. */

                if (cmd == FX_SPECIAL) {
                        if (param)
                                chan->mem_special = param;
                        else
                                param = chan->mem_special;
                        if (param >> 4 == 0xd) {
                                // Ideally this would use SONG_FIRSTTICK, but Impulse Tracker has a bug here :)
                                if (firsttick) {
                                        chan->cd_note_delay = (param & 0xf) ?: 1;
                                        continue; // notes never play on the first tick with SDx, go away
                                }
                                if (--chan->cd_note_delay > 0)
                                        continue; // not our turn yet, go away
                                start_note = (chan->cd_note_delay == 0);
                        }
                }

                // Handles note/instrument/volume changes
                if (start_note) {
                        uint32_t note = chan->row_note;
                        if (instr && note == NOTE_NONE) {
                                if (csf->flags & SONG_INSTRUMENTMODE) {
                                        if (chan->ptr_sample)
                                                chan->volume = chan->ptr_sample->volume;
                                } else {
                                        if (instr < MAX_SAMPLES)
                                                chan->volume = csf->samples[instr].volume;
                                }
                        }
                        // Invalid Instrument ?
                        if (instr >= MAX_INSTRUMENTS) instr = 0;
                        // Note Cut/Off => ignore instrument
                        if ((NOTE_IS_CONTROL(note)) || (note != NOTE_NONE && !porta)) {
                                /* This is required when the instrument changes (KeyOff is not called) */
                                /* Possibly a better bugfix could be devised. --Bisqwit */
                                OPL_NoteOff(nchan);
                                OPL_Touch(nchan, NULL, 0);
                                GM_KeyOff(nchan);
                                GM_Touch(nchan, 0);
                        }

                        if (NOTE_IS_CONTROL(note)) {
                                instr = 0;
                        } else if (NOTE_IS_NOTE(note)) {
                                chan->new_note = note;
                                // New Note Action ? (not when paused!!!)
                                if (!porta)
                                        csf_check_nna(csf, nchan, instr, note, 0);
                        }
                        // Instrument Change ?
                        if (instr) {
                                song_sample_t *psmp = chan->ptr_sample;
                                csf_instrument_change(csf, chan, instr, porta, 1);
                                OPL_Patch(nchan, csf->samples[instr].adlib_bytes);

                                if((csf->flags & SONG_INSTRUMENTMODE) && csf->instruments[instr])
                                        GM_DPatch(nchan, csf->instruments[instr]->midi_program,
                                                csf->instruments[instr]->midi_bank,
                                                csf->instruments[instr]->midi_channel_mask);

                                chan->new_instrument = 0;
                                // Special IT case: portamento+note causes sample change -> ignore portamento
                                if (psmp != chan->ptr_sample && NOTE_IS_NOTE(note)) {
                                        porta = 0;
                                }
                        }
                        // New Note ?
                        if (note != NOTE_NONE) {
                                if (!instr && chan->new_instrument && NOTE_IS_NOTE(note)) {
                                        csf_instrument_change(csf, chan, chan->new_instrument, porta, 0);
                                        if ((csf->flags & SONG_INSTRUMENTMODE)
                                            && csf->instruments[chan->new_instrument]) {
                                                OPL_Patch(nchan, csf->samples[chan->new_instrument].adlib_bytes);
                                                GM_DPatch(nchan, csf->instruments[chan->new_instrument]->midi_program,
                                                        csf->instruments[chan->new_instrument]->midi_bank,
                                                        csf->instruments[chan->new_instrument]->midi_channel_mask);
                                        }
                                        chan->new_instrument = 0;
                                }
                                csf_note_change(csf, nchan, note, porta, 0, 0);
                        }
                }

                handle_voleffect(csf, chan, volcmd, vol, firsttick, start_note);
                handle_effect(csf, nchan, cmd, param, porta, firsttick);
        }
}

