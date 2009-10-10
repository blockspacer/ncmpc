/* ncmpc (Ncurses MPD Client)
 * (c) 2004-2009 The Music Player Daemon Project
 * Project homepage: http://musicpd.org

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "list_window.h"
#include "config.h"
#include "options.h"
#include "charset.h"
#include "match.h"
#include "command.h"
#include "colors.h"
#include "screen_message.h"
#include "i18n.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern void screen_bell(void);

struct list_window *
list_window_init(WINDOW *w, unsigned width, unsigned height)
{
	struct list_window *lw;

	lw = g_malloc0(sizeof(*lw));
	lw->w = w;
	lw->cols = width;
	lw->rows = height;
	lw->range_selection = false;
	return lw;
}

void
list_window_free(struct list_window *lw)
{
	assert(lw != NULL);

	g_free(lw);
}

void
list_window_reset(struct list_window *lw)
{
	lw->selected = 0;
	lw->selected_start = 0;
	lw->selected_end = 0;
	lw->range_selection = false;
	lw->range_base = 0;
	lw->start = 0;
}

static void
list_window_check_selected(struct list_window *lw)
{
	if (lw->start + lw->rows > lw->length) {
		if (lw->length > lw->rows)
			lw->start = lw->length - lw->rows;
		else
			lw->start = 0;
	}

	if (lw->selected < lw->start)
		lw->selected = lw->start;

	if (lw->length == 0)
		lw->selected = 0;
	else if (lw->selected >= lw->length)
		lw->selected = lw->length - 1;

	if(lw->range_selection)
	{
		if (lw->length == 0) {
			lw->selected_start = 0;
			lw->selected_end = 0;
			lw->range_base = 0;
		} else {
			if (lw->selected_start >= lw->length)
				lw->selected_start = lw->length - 1;
			if (lw->selected_end >= lw->length)
				lw->selected_end = lw->length - 1;
			if (lw->range_base >= lw->length)
				lw->range_base = lw->length - 1;
		}

		if(lw->range_base > lw->selected_end)
			  lw->selected_end = lw->selected;
		if(lw->range_base < lw->selected_start)
			  lw->selected_start = lw->selected;
	}
	else
	{
		lw->selected_start = lw->selected;
		lw->selected_end = lw->selected;
	}
}

/**
 * Scroll after the cursor was moved, the list was changed or the
 * window was resized.
 */
static void
list_window_check_origin(struct list_window *lw)
{
	int start = lw->start;

	if ((unsigned) options.scroll_offset * 2 >= lw->rows)
		// Center if the offset is more than half the screen
		start = lw->selected - lw->rows / 2;
	else
		{
			if (lw->selected < lw->start + options.scroll_offset)
				start = lw->selected - options.scroll_offset;

			if (lw->selected >= lw->start + lw->rows - options.scroll_offset)
				{
					start = lw->selected - lw->rows + 1 + options.scroll_offset;
				}
		}

	if (start + lw->rows > lw->length)
		start = lw->length - lw->rows;

	if (start < 0 || lw->length == 0)
		start = 0;

	lw->start = start;
}

void
list_window_resize(struct list_window *lw, unsigned width, unsigned height)
{
	lw->cols = width;
	lw->rows = height;

	list_window_check_origin(lw);
}

void
list_window_set_length(struct list_window *lw, unsigned length)
{
	lw->length = length;

	list_window_check_selected(lw);
	list_window_check_origin(lw);
}

void
list_window_center(struct list_window *lw, unsigned n)
{
	if (n > lw->rows / 2)
		lw->start = n - lw->rows / 2;
	else
		lw->start = 0;

	if (lw->start + lw->rows > lw->length) {
		if (lw->rows < lw->length)
			lw->start = lw->length - lw->rows;
		else
			lw->start = 0;
	}
}

void
list_window_set_cursor(struct list_window *lw, unsigned i)
{
	lw->range_selection = false;
	lw->selected = i;
	lw->selected_start = i;
	lw->selected_end = i;

	list_window_check_selected(lw);
	list_window_check_origin(lw);
}

void
list_window_move_cursor(struct list_window *lw, unsigned n)
{
	lw->selected = n;
	if(lw->range_selection)
	{
		if(n >= lw->range_base)
		{
			lw->selected_end = n;
			lw->selected_start = lw->range_base;
		}
		if(n <= lw->range_base)
		{
			lw->selected_start = n;
			lw->selected_end = lw->range_base;
		}
	}
	else
	{
		lw->selected_start = n;
		lw->selected_end = n;
	}

	list_window_check_selected(lw);
	list_window_check_origin(lw);
}

void
list_window_fetch_cursor(struct list_window *lw)
{
	if (lw->selected < lw->start + options.scroll_offset) {
		if (lw->start > 0)
			lw->selected = lw->start + options.scroll_offset;
		if (lw->range_selection) {
			if (lw->selected > lw->range_base) {
				lw->selected_end = lw->selected;
				lw->selected_start = lw->range_base;
			} else {
				lw->selected_start = lw->selected;
			}
		} else {
			lw->selected_start = lw->selected;
			lw->selected_end = lw->selected;
		}
	} else if (lw->selected > lw->start + lw->rows - 1 - options.scroll_offset) {
		if (lw->start + lw->rows < lw->length)
			lw->selected = lw->start + lw->rows - 1 - options.scroll_offset;
		if (lw->range_selection) {
			if (lw->selected < lw->range_base) {
				lw->selected_start = lw->selected;
				lw->selected_end = lw->range_base;
			} else {
				lw->selected_end = lw->selected;
			}
		} else {
			lw->selected_start = lw->selected;
			lw->selected_end = lw->selected;
		}
	}
}

static void
list_window_next(struct list_window *lw)
{
	if (lw->selected + 1 < lw->length)
		list_window_move_cursor(lw, lw->selected + 1);
	else if (options.list_wrap)
		list_window_move_cursor(lw, 0);
}

static void
list_window_previous(struct list_window *lw)
{
	if (lw->selected > 0)
		list_window_move_cursor(lw, lw->selected - 1);
	else if (options.list_wrap)
		list_window_move_cursor(lw, lw->length - 1);
}

static void
list_window_top(struct list_window *lw)
{
	if (lw->start == 0)
		list_window_move_cursor(lw, lw->start);
	else
		if ((unsigned) options.scroll_offset * 2 >= lw->rows)
			list_window_move_cursor(lw, lw->start + lw->rows / 2);
		else
			list_window_move_cursor(lw, lw->start + options.scroll_offset);
}

static void
list_window_middle(struct list_window *lw)
{
	if (lw->length >= lw->rows)
		list_window_move_cursor(lw, lw->start + lw->rows / 2);
	else
		list_window_move_cursor(lw, lw->length / 2);
}

static void
list_window_bottom(struct list_window *lw)
{
	if (lw->length >= lw->rows)
		if ((unsigned) options.scroll_offset * 2 >= lw->rows)
			list_window_move_cursor(lw, lw->start + lw->rows / 2);
		else
			if (lw->start + lw->rows == lw->length)
				list_window_move_cursor(lw, lw->length - 1);
			else
				list_window_move_cursor(lw, lw->start + lw->rows - 1 - options.scroll_offset);
	else
		list_window_move_cursor(lw, lw->length - 1);
}

static void
list_window_first(struct list_window *lw)
{
	list_window_move_cursor(lw, 0);
}

static void
list_window_last(struct list_window *lw)
{
	if (lw->length > 0)
		list_window_move_cursor(lw, lw->length - 1);
	else
		list_window_move_cursor(lw, 0);
}

static void
list_window_next_page(struct list_window *lw)
{
	if (lw->rows < 2)
		return;
	if (lw->selected + lw->rows < lw->length)
		list_window_move_cursor(lw, lw->selected + lw->rows - 1);
	else
		list_window_last(lw);
}

static void
list_window_previous_page(struct list_window *lw)
{
	if (lw->rows < 2)
		return;
	if (lw->selected > lw->rows - 1)
		list_window_move_cursor(lw, lw->selected - lw->rows + 1);
	else
		list_window_first(lw);
}

static void
list_window_scroll_up(struct list_window *lw, unsigned n)
{
	if (lw->start > 0) {
		if (n > lw->start)
			lw->start = 0;
		else
			lw->start -= n;

		list_window_fetch_cursor(lw);
	}
}

static void
list_window_scroll_down(struct list_window *lw, unsigned n)
{
	if (lw->start + lw->rows < lw->length)
	{
		if ( lw->start + lw->rows + n > lw->length - 1)
			lw->start = lw->length - lw->rows;
		else
			lw->start += n;

		list_window_fetch_cursor(lw);
	}
}

static void
list_window_paint_row(WINDOW *w, unsigned y, unsigned width,
		      bool selected, bool highlight,
		      const char *text, const char *second_column)
{
	unsigned text_width = utf8_width(text);
	unsigned second_column_width;

#ifdef NCMPC_MINI
	second_column = NULL;
#endif /* NCMPC_MINI */

	if (second_column != NULL) {
		second_column_width = utf8_width(second_column) + 1;
		if (second_column_width < width)
			width -= second_column_width;
		else
			second_column_width = 0;
	} else
		second_column_width = 0;

	if (highlight)
		colors_use(w, COLOR_LIST_BOLD);
	else
		colors_use(w, COLOR_LIST);

	if (selected)
		wattron(w, A_REVERSE);

	waddstr(w, text);
	if (options.wide_cursor && text_width < width)
		whline(w, ' ', width - text_width);

	if (second_column_width > 0) {
		wmove(w, y, width);
		waddch(w, ' ');
		waddstr(w, second_column);
	}

	if (selected)
		wattroff(w, A_REVERSE);

	if (!options.wide_cursor && text_width < width) {
		if (second_column_width == 0)
			/* the cursor is at the end of the text; clear
			   the rest of this row */
			wclrtoeol(w);
		else
			/* there's a second column: clear the space
			   between the first and the second column */
			mvwhline(w, y, text_width, ' ', width - text_width);
	}
}

void
list_window_paint(const struct list_window *lw,
		  list_window_callback_fn_t callback,
		  void *callback_data)
{
	unsigned i;
	bool show_cursor = !lw->hide_cursor;
	bool highlight = false;

	show_cursor = show_cursor &&
		(!options.hardware_cursor || lw->range_selection);

	for (i = 0; i < lw->rows; i++) {
		const char *label;
		char *second_column = NULL;
		highlight = false;

		wmove(lw->w, i, 0);

		if (lw->start + i >= lw->length) {
			wclrtobot(lw->w);
			break;
		}

		label = callback(lw->start + i, &highlight, &second_column, callback_data);
		assert(label != NULL);

		list_window_paint_row(lw->w, i, lw->cols,
				      show_cursor &&
				      lw->start + i >= lw->selected_start &&
				      lw->start + i <= lw->selected_end,
				      highlight,
				      label, second_column);

		if (second_column != NULL)
			g_free(second_column);
	}

	if (options.hardware_cursor && lw->selected >= lw->start &&
	    lw->selected < lw->start + lw->rows) {
		curs_set(1);
		wmove(lw->w, lw->selected - lw->start, 0);
	}
}

bool
list_window_find(struct list_window *lw,
		 list_window_callback_fn_t callback,
		 void *callback_data,
		 const char *str,
		 bool wrap,
		 bool bell_on_wrap)
{
	bool h;
	unsigned i = lw->selected + 1;
	const char *label;

	assert(str != NULL);

	do {
		while (i < lw->length) {
			label = callback(i, &h, NULL, callback_data);
			assert(label != NULL);

			if (match_line(label, str)) {
				list_window_move_cursor(lw, i);
				return true;
			}
			if (wrap && i == lw->selected)
				return false;
			i++;
		}
		if (wrap) {
			if (i == 0) /* empty list */
				return 1;
			i=0; /* first item */
			if (bell_on_wrap) {
				screen_bell();
			}
		}
	} while (wrap);

	return false;
}

bool
list_window_rfind(struct list_window *lw,
		  list_window_callback_fn_t callback,
		  void *callback_data,
		  const char *str,
		  bool wrap,
		  bool bell_on_wrap)
{
	bool h;
	int i = lw->selected - 1;
	const char *label;

	assert(str != NULL);

	if (lw->length == 0)
		return false;

	do {
		while (i >= 0) {
			label = callback(i, &h, NULL, callback_data);
			assert(label != NULL);

			if (match_line(label, str)) {
				list_window_move_cursor(lw, i);
				return true;
			}
			if (wrap && i == (int)lw->selected)
				return false;
			i--;
		}
		if (wrap) {
			i = lw->length - 1; /* last item */
			if (bell_on_wrap) {
				screen_bell();
			}
		}
	} while (wrap);

	return false;
}

static bool
jump_match(const char *haystack, const char *needle)
{
#ifdef NCMPC_MINI
	bool jump_prefix_only = true;
#else
	bool jump_prefix_only = options.jump_prefix_only;
#endif

	assert(haystack != NULL);
	assert(needle != NULL);

	return jump_prefix_only
		? g_ascii_strncasecmp(haystack, needle, strlen(needle)) == 0
		: match_line(haystack, needle);
}

bool
list_window_jump(struct list_window *lw,
		 list_window_callback_fn_t callback,
		 void *callback_data,
		 const char *str)
{
	bool h;
	const char *label;

	assert(str != NULL);

	for (unsigned i = 0; i < lw->length; ++i) {
		label = callback(i, &h, NULL, callback_data);
		assert(label != NULL);

		if (label[0] == '[')
			label++;

		if (jump_match(label, str)) {
			list_window_move_cursor(lw, i);
			return true;
		}
	}
	return false;
}

/* perform basic list window commands (movement) */
bool
list_window_cmd(struct list_window *lw, command_t cmd)
{
	switch (cmd) {
	case CMD_LIST_PREVIOUS:
		list_window_previous(lw);
		break;
	case CMD_LIST_NEXT:
		list_window_next(lw);
		break;
	case CMD_LIST_TOP:
		list_window_top(lw);
		break;
	case CMD_LIST_MIDDLE:
		list_window_middle(lw);
		break;
	case CMD_LIST_BOTTOM:
		list_window_bottom(lw);
		break;
	case CMD_LIST_FIRST:
		list_window_first(lw);
		break;
	case CMD_LIST_LAST:
		list_window_last(lw);
		break;
	case CMD_LIST_NEXT_PAGE:
		list_window_next_page(lw);
		break;
	case CMD_LIST_PREVIOUS_PAGE:
		list_window_previous_page(lw);
		break;
	case CMD_LIST_RANGE_SELECT:
		if(lw->range_selection)
		{
			screen_status_printf(_("Range selection disabled"));
			list_window_set_cursor(lw, lw->selected);
		}
		else
		{
			screen_status_printf(_("Range selection enabled"));
			lw->range_base = lw->selected;
			lw->range_selection = true;
		}
		break;
	case CMD_LIST_SCROLL_UP_LINE:
		list_window_scroll_up(lw, 1);
		break;
	case CMD_LIST_SCROLL_DOWN_LINE:
		list_window_scroll_down(lw, 1);
		break;
	case CMD_LIST_SCROLL_UP_HALF:
		list_window_scroll_up(lw, (lw->rows - 1) / 2);
		break;
	case CMD_LIST_SCROLL_DOWN_HALF:
		list_window_scroll_down(lw, (lw->rows - 1) / 2);
		break;
	default:
		return false;
	}

	return true;
}

bool
list_window_scroll_cmd(struct list_window *lw, command_t cmd)
{
	switch (cmd) {
	case CMD_LIST_SCROLL_UP_LINE:
	case CMD_LIST_PREVIOUS:
		if (lw->start > 0)
			lw->start--;
		break;

	case CMD_LIST_SCROLL_DOWN_LINE:
	case CMD_LIST_NEXT:
		if (lw->start + lw->rows < lw->length)
			lw->start++;
		break;

	case CMD_LIST_FIRST:
		lw->start = 0;
		break;

	case CMD_LIST_LAST:
		if (lw->length > lw->rows)
			lw->start = lw->length - lw->rows;
		else
			lw->start = 0;
		break;

	case CMD_LIST_NEXT_PAGE:
		lw->start += lw->rows - 1;
		if (lw->start + lw->rows > lw->length) {
			if (lw->length > lw->rows)
				lw->start = lw->length - lw->rows;
			else
				lw->start = 0;
		}
		break;

	case CMD_LIST_PREVIOUS_PAGE:
		if (lw->start > lw->rows)
			lw->start -= lw->rows;
		else
			lw->start = 0;
		break;

	case CMD_LIST_SCROLL_UP_HALF:
		if (lw->start > (lw->rows - 1) / 2)
			lw->start -= (lw->rows - 1) / 2;
		else
			lw->start = 0;
		break;

	case CMD_LIST_SCROLL_DOWN_HALF:
		lw->start += (lw->rows - 1) / 2;
		if (lw->start + lw->rows > lw->length) {
			if (lw->length > lw->rows)
				lw->start = lw->length - lw->rows;
			else
				lw->start = 0;
		}
		break;

	default:
		return false;
	}

	return true;
}

#ifdef HAVE_GETMOUSE
bool
list_window_mouse(struct list_window *lw, unsigned long bstate, int y)
{
	assert(lw != NULL);

	/* if the even occurred above the list window move up */
	if (y < 0) {
		if (bstate & BUTTON3_CLICKED)
			list_window_first(lw);
		else
			list_window_previous_page(lw);
		return true;
	}

	/* if the even occurred below the list window move down */
	if ((unsigned)y >= lw->length) {
		if (bstate & BUTTON3_CLICKED)
			list_window_last(lw);
		else
			list_window_next_page(lw);
		return true;
	}

	return false;
}
#endif
