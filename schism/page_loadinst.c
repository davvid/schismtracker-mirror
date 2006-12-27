/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
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

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "sdlmain.h"

#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

/* --------------------------------------------------------------------------------------------------------- */
/* the locals */

static struct widget widgets_loadinst[1];
static char inst_cwd[PATH_MAX+1];

/* --------------------------------------------------------------------------------------------------------- */

/* files:
	file type       color   displayed title                 notes
	---------       -----   ---------------                 -----
	unchecked       4       <the filename>                  IT uses color 6 for these
	directory       5       "........Directory........"     dots are char 154 (same for libraries)
	sample          3       <the sample name>
	libraries       6       ".........Library........."     IT uses color 3. maybe use module name here?
	unknown         2       <the filename>                  any regular file that's not recognized
*/

static int top_file = 0;
static time_t directory_mtime;
static int _library_mode = 0;
static dmoz_filelist_t flist;
#define current_file flist.selected

static int slash_search_mode = -1;
static char slash_search_str[PATH_MAX];

/* get a color index from a dmoz_file_t 'type' field */
static inline int get_type_color(int type)
{
	if (type == TYPE_DIRECTORY)
		return 5;
	if (!(type & TYPE_EXT_DATA_MASK))
		return 4; /* unchecked */
	if (type & TYPE_BROWSABLE_MASK)
		return 6; /* library */
	if (type == TYPE_UNKNOWN)
		return 2;
	return 3; /* sample */
}


static void clear_directory(void)
{
	dmoz_free(&flist, NULL);
}

static int instgrep(dmoz_file_t *f)
{
	if ((f->type & TYPE_EXT_DATA_MASK) == 0)
		dmoz_fill_ext_data(f);

	if ((f->type & TYPE_EXT_DATA_MASK) == 0) return 0;
	return (f->type == TYPE_SAMPLE_MASK
	|| f->type == TYPE_UNKNOWN)
	? 0 : 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_reposition(void)
{
	if (current_file >= flist.num_files)
		current_file = flist.num_files-1;
	if (current_file < 0) current_file = 0;
	if (current_file < top_file)
		top_file = current_file;
	else if (current_file > top_file + 34)
		top_file = current_file - 34;
}

static void read_directory(void)
{
	struct stat st;

	clear_directory();
	
	if (stat(inst_cwd, &st) < 0)
		directory_mtime = 0;
	else
		directory_mtime = st.st_mtime;
	/* if the stat call failed, this will probably break as well, but
	at the very least, it'll add an entry for the root directory. */
	if (dmoz_read_ex(inst_cwd, &flist, NULL,
				dmoz_read_instrument_library) < 0)
		perror(inst_cwd);

	dmoz_filter_filelist(&flist,instgrep, &current_file, file_list_reposition);
        dmoz_cache_lookup(inst_cwd, &flist, 0);
	file_list_reposition();
}

/* return: 1 = success, 0 = failure
TODO: provide some sort of feedback if something went wrong. */
static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);
	struct stat buf;

	if (!ptr)
		return 0;

        dmoz_cache_update(inst_cwd, &flist, 0);

	if (stat(ptr, &buf) == 0 && S_ISDIR(buf.st_mode)) {
		strncpy(cfg_dir_instruments, ptr, PATH_MAX);
		cfg_dir_instruments[PATH_MAX] = 0;
	
	}
	strncpy(inst_cwd, ptr, PATH_MAX);
	inst_cwd[PATH_MAX] = 0;
	free(ptr);
	
	read_directory();
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_instrument_draw_const(void)
{
	draw_fill_chars(6, 13, 67, 47, 0);
	draw_thin_inner_box(50, 12, 61, 48, 0,0);
	draw_box(5, 12, 68, 48, BOX_THICK | BOX_INNER | BOX_INSET);

}

/* --------------------------------------------------------------------------------------------------------- */

static void _common_set_page(void)
{
	struct stat st;
	
	if (!inst_cwd[0]) {
		strcpy(inst_cwd, cfg_dir_instruments);
	}

	/* if we have a list, the directory didn't change, and the mtime is the same, we're set */
	if (flist.num_files > 0
	    && (status.flags & DIR_SAMPLES_CHANGED) == 0
	    && stat(inst_cwd, &st) == 0
	    && st.st_mtime == directory_mtime) {
		return;
	}

	change_dir(inst_cwd);

	status.flags &= ~DIR_INSTRUMENTS_CHANGED;

	*selected_widget = 0;
	slash_search_mode = -1;
}
static void load_instrument_set_page(void)
{
	_library_mode = 0;
	_common_set_page();
}
static void library_instrument_set_page(void)
{
	_library_mode = 1;
	_common_set_page();
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_draw(void)
{
	int n, pos, fg, bg, i;
	char buf[8];
	char sbuf[32];
	dmoz_file_t *file;
	
	/* there's no need to have if (files) { ... } like in the load-module page,
	   because there will always be at least "/" in the list */
	if (top_file < 0) top_file = 0;
	if (current_file < 0) current_file = 0;
	for (n = top_file, pos = 13; n < flist.num_files && pos < 48; n++, pos++) {
		file = flist.files[n];
		
		if (n == current_file && ACTIVE_PAGE.selected_widget == 0) {
			fg = 0;
			bg = 3;
		} else {
			fg = get_type_color(file->type);
			bg = 0;
		}

		draw_text(numtostr(3, n, (unsigned char *) buf), 2, pos, 0, 2);
		draw_text_len((unsigned char *) (file->title ? file->title : ""),
						25, 6, pos, fg, bg);
		draw_char(168, 31, pos, 2, bg);
		draw_text_len((unsigned char *) (file->base ? file->base : ""),
						18, 32, pos, fg, bg);

		if (file->base && slash_search_mode > -1) {
			if (strncasecmp(file->base,slash_search_str,slash_search_mode) == 0) {
				for (i = 0 ; i < slash_search_mode; i++) {
					if (tolower(((unsigned)file->base[i]))
					!= tolower(((unsigned)slash_search_str[i]))) break;
					draw_char(file->base[i], 32+i, pos, 3,1);
				}
			}
		}

		if (file->sampsize > 1) {
			sprintf(sbuf, "%u Samples", file->sampsize);
			draw_text_len((unsigned char *) sbuf, 10, 51, pos, fg, bg);
		} else if (file->sampsize == 1) {
			draw_text((unsigned char *) "1 Sample  ", 51, pos, fg, bg);
		} else if (file->type & TYPE_MODULE_MASK) {
			draw_text((unsigned char *) "\x9a\x9a""Module\x9a\x9a", 51, pos, fg, bg);
		} else {
			draw_text((unsigned char *) "          ", 51, pos, fg, bg);
		}
		if (file->filesize > 1048576) {
			sprintf(sbuf, "%lum", (long)(file->filesize / 1048576));
		} else if (file->filesize > 1024) {
			sprintf(sbuf, "%luk", (long)(file->filesize / 1024));
		} else if (file->filesize > 0) {
			sprintf(sbuf, "%lu", (long)(file->filesize));
		} else {
			*sbuf = 0;
		}
		draw_text_len((unsigned char *) sbuf, 6, 62, pos, fg, bg);
	}
	
	/* draw the info for the current file (or directory...) */
	
	while (pos < 48)
		draw_char(168, 31, pos++, 2, 0);
}

/* on the file list, that is */
static void do_enable_inst(UNUSED void *d)
{
	song_set_instrument_mode(1);
	main_song_changed_cb();
	set_page(PAGE_INSTRUMENT_LIST);
	memused_songchanged();
}
static void dont_enable_inst(UNUSED void *d)
{
	set_page(PAGE_INSTRUMENT_LIST);
}
static void reposition_at_slash_search(void)
{
	dmoz_file_t *f;
	int i, j, b, bl;

	if (slash_search_mode < 0) return;
	bl = b = -1;
	for (i = 0; i < flist.num_files; i++) {
		f = flist.files[i];
		if (!f || !f->base) continue;
		for (j = 0; j < slash_search_mode; j++) {
			if (tolower(((unsigned)f->base[j]))
			!= tolower(((unsigned)slash_search_str[j]))) break;
		}
		if (bl < j) {
			bl = j;
			b = i;
		}
	}
	if (bl > -1) {
		current_file = b;
		file_list_reposition();
	}
}
static void handle_enter_key(void)
{
	dmoz_file_t *file;
	int cur = instrument_get_current();
	
	if (current_file < 0 || current_file >= flist.num_files) return;
	file = flist.files[current_file];
        dmoz_cache_update(inst_cwd, &flist, 0);

	if (file->type & TYPE_BROWSABLE_MASK) {
		change_dir(file->path);
		status.flags |= NEED_UPDATE;
	} else if (file->type & TYPE_INST_MASK) {
		if (_library_mode) return;
		if (file->instnum > -1) {
			song_load_instrument_ex(cur, NULL,
					file->path, file->instnum);
		} else {
			song_load_instrument(cur, file->path);
		}
		if (!song_is_instrument_mode()) {
			dialog_create(DIALOG_YES_NO,
				"Enable instrument mode?",
				do_enable_inst, dont_enable_inst, 0, NULL);
		} else {
			set_page(PAGE_INSTRUMENT_LIST);
		}
		memused_songchanged();
	}

	/* TODO */
}

static int file_list_handle_key(struct key_event * k)
{
	int new_file = current_file;

	new_file = CLAMP(new_file, 0, flist.num_files - 1);

	if (k->mouse) {
		if (k->x >= 6 && k->x <= 67 && k->y >= 13 && k->y <= 47) {
			if (k->mouse == MOUSE_SCROLL_UP) {
				new_file--;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				new_file++;
			} else {
				new_file = top_file + (k->y - 13);
			}
		}
	} else if (slash_search_mode > -1) {
		int c = unicode_to_ascii(k->unicode);
		if (k->sym == SDLK_RETURN || k->sym == SDLK_ESCAPE) {
			if (!k->state) return 1;
			slash_search_mode = -1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->sym == SDLK_BACKSPACE) {
			if (k->state) return 1;
			slash_search_mode--;
			status.flags |= NEED_UPDATE;
			reposition_at_slash_search();
			return 1;
		} else if (c >= 32) {
			if (k->state) return 1;
			if (slash_search_mode < PATH_MAX) {
				slash_search_str[ slash_search_mode ] = c;
				slash_search_mode++;
				reposition_at_slash_search();
				status.flags |= NEED_UPDATE;
			}
			return 1;
		}
	}
	
	switch (k->sym) {
	case SDLK_UP:
		new_file--;
		break;
	case SDLK_DOWN:
		new_file++;
		break;
	case SDLK_PAGEUP:
		new_file -= 35;
		break;
	case SDLK_PAGEDOWN:
		new_file += 35;
		break;
	case SDLK_HOME:
		new_file = 0;
		break;
	case SDLK_END:
		new_file = flist.num_files - 1;
		break;
	case SDLK_RETURN:
		if (!k->state) return 0;
		handle_enter_key();
		return 1;
	case SDLK_ESCAPE:
		if (k->state && NO_MODIFIER(k->mod))
			set_page(PAGE_INSTRUMENT_LIST);
		return 1;
	case SDLK_SLASH:
		if (k->orig_sym == SDLK_SLASH) {
			if (status.flags & CLASSIC_MODE) return 0;
			if (!k->state) return 0;
			slash_search_mode = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		}
	default:
		if (!k->mouse) return 0;
	}
	
	if (k->mouse == MOUSE_CLICK) {
		if (!k->state) return 0;
	} else if (k->mouse == MOUSE_DBLCLICK) {
		handle_enter_key();
		return 1;
	} else {
		if (k->state) return 0;
	}

	new_file = CLAMP(new_file, 0, flist.num_files - 1);
	if (new_file < 0) new_file = 0;
	if (new_file != current_file) {
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void load_instrument_handle_key(struct key_event * k)
{
	if (!k->state) return;
	if (k->sym == SDLK_ESCAPE && NO_MODIFIER(k->mod))
		set_page(PAGE_INSTRUMENT_LIST);
}

/* --------------------------------------------------------------------------------------------------------- */

void load_instrument_load_page(struct page *page)
{
	clear_directory();

	page->title = "Load Instrument";
	page->draw_const = load_instrument_draw_const;
	page->set_page = load_instrument_set_page;
	page->handle_key = load_instrument_handle_key;
	page->total_widgets = 1;
	page->widgets = widgets_loadinst;
	page->help_index = HELP_GLOBAL;
	inst_cwd[0] = 0;
	create_other(widgets_loadinst + 0, 0, file_list_handle_key, file_list_draw);
	widgets_loadinst[0].accept_text = 1;
}
void library_instrument_load_page(struct page *page)
{
	clear_directory();

	page->title = "Instrument Library (Ctrl-F4)";
	page->draw_const = load_instrument_draw_const;
	page->set_page = library_instrument_set_page;
	page->handle_key = load_instrument_handle_key;
	page->total_widgets = 1;
	page->widgets = widgets_loadinst;
	page->help_index = HELP_GLOBAL;
	inst_cwd[0] = 0;
	create_other(widgets_loadinst + 0, 0, file_list_handle_key, file_list_draw);
	widgets_loadinst[0].accept_text = 1;
}
