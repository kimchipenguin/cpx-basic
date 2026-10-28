/*
 * gemmain.c - Chipmunk BASIC as a GEM application (CHIPBAS.APP)
 *
 * Takes the place of CPX_MAIN.C: instead of XControl it is a normal
 * GEM program with a menu bar and a resizable window. The interpreter
 * (BASIC.C) runs as a coroutine as in the CPX (GCC/cpxmains.S): the
 * event loop continues it with ContBASIC() and it returns with
 * WaitBASIC() (waiting for a key) or BreakBASIC() (every 50 ms).
 * Output goes to the screen model in gemscrn.c, which the event loop
 * draws into the window.
 *
 * The menu commands for the BASIC are typed into the key buffer as
 * BASIC commands (e.g. "LOAD" opens the file selector), so they only
 * work at the ">" prompt and are disabled otherwise.
 */
#define ___MAIN_C___

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <osbind.h>
#include "glo.h"
#include "tokstr.h"

extern char ctrl, ctrlsq;			/* GCC/cpxmains.S */
extern int interactive;				/* CPX_IO.C: in FGets() */
extern int cmd_history;				/* CPX_IO.C: FGets() at the prompt */
extern int fptime;					/* BASIC.C */
extern void DATAfclose(int);
extern int basic_has_program(void);
extern void scr_cell(WORD *w, WORD *h);
extern int scr_resize(WORD cols, WORD rows, WORD cellw, WORD cellh);
extern int scr_dirty(WORD *y0, WORD *y1, WORD *x0, WORD *x1, WORD *scroll);
extern void scr_draw(WORD vh, const GRECT *work, const GRECT *clip, WORD y0, WORD y1);
extern int scr_set_view(long v);
extern WORD scr_view(void);
extern void scr_slider(WORD *size, WORD *pos);
extern int scr_set_slider(WORD pos);
extern void scr_set_colours(WORD back, WORD fore);

#define APP_NAME	"  Chipmunk BASIC"
#define WIN_KIND	(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE)
#define MIN_COLS	20
#define MIN_ROWS	5
#define MAX_COLS	255				/* lines are at most 255 characters */

static WORD vh;						/* VDI handle */
static WORD cellw, cellh;			/* system font cell */
static WORD win = -1;
static GRECT desk;
static int quit_req;
static WORD win_kind = WIN_KIND;		/* + SMALLER where possible */
static int iconified;
static GRECT before_icon;				/* window size before iconify */

/* the icon of the CPX (SOURCES/GCC/cpxhead.bin), 32 x 24 pixels */
static unsigned short icon_data[48] = {
	0x6e1d, 0xdc70,
	0xea11, 0x5423,
	0x2a1d, 0x5c20,
	0x2a11, 0x5823,
	0x2e11, 0xd470,
	0x0000, 0x0000,
	0x0000, 0x0000,
	0xee1d, 0xdd2e,
	0x2a15, 0x49a4,
	0xea1d, 0xc964,
	0x8a11, 0x8924,
	0xee11, 0x5d24,
	0x0000, 0x0000,
	0x0000, 0x0000,
	0xee12, 0xeae7,
	0x2a1a, 0x8a42,
	0xea16, 0xc442,
	0x2a12, 0x8a42,
	0xee12, 0xea47,
	0x0000, 0x0000,
	0x0000, 0x0000,
	0xae1e, 0xeee3,
	0xaa12, 0xa4a7,
	0xea10, 0xa4a1,
};
static MFDB icon_mfdb = { icon_data, 32, 24, 2, 0, 1, 0, 0, 0 };

/*--------------------------------------------------------------------
 * Menu bar, built here instead of loading a resource file
 *------------------------------------------------------------------*/

enum {
	MI_ABOUT = 1,
	MI_NEW, MI_OPEN, MI_MERGE, MI_SAVE, MI_QUIT,
	MI_RUN, MI_STOP, MI_CONT, MI_LIST, MI_CLS, MI_KEYWORDS,
	MI_COUNT
};

typedef struct {
	const char *text;			/* NULL ends the menu; "-" = separator */
	int id;						/* MI_... */
} MENUITEM;

static const char *titles[] = { " CHIPMUNK BASIC ", " File ", " Program ", NULL };

static const MENUITEM menu_desk[] = {
	{ "  About Chipmunk BASIC... ", MI_ABOUT },
	{ "-", 0 },
	{ "1", 0 }, { "2", 0 }, { "3", 0 }, { "4", 0 }, { "5", 0 }, { "6", 0 },
	{ NULL, 0 }
};
static const MENUITEM menu_file[] = {
	{ "  New          ^N ", MI_NEW },
	{ "  Open...      ^O ", MI_OPEN },
	{ "  Merge...     ^M ", MI_MERGE },
	{ "  Save...      ^S ", MI_SAVE },
	{ "-", 0 },
	{ "  Quit         ^Q ", MI_QUIT },
	{ NULL, 0 }
};
static const MENUITEM menu_prog[] = {
	{ "  Run          ^R ", MI_RUN },
	{ "  Stop         ^C ", MI_STOP },
	{ "  Continue        ", MI_CONT },
	{ "  List         ^L ", MI_LIST },
	{ "  Clear screen    ", MI_CLS },
	{ "-", 0 },
	{ "  Keywords   Help ", MI_KEYWORDS },
	{ NULL, 0 }
};
static const MENUITEM *menus[] = { menu_desk, menu_file, menu_prog };

#define MAX_OBJ 48
static OBJECT menu[MAX_OBJ];
static char menu_text[MAX_OBJ][40];
static WORD obj_of[MI_COUNT];		/* MI_... -> object index */
static WORD title_of[MI_COUNT];		/* MI_... -> its menu title */
static WORD id_of[MAX_OBJ];			/* object index -> MI_... */

static WORD new_obj(WORD parent, WORD type, long spec, WORD x, WORD y, WORD w, WORD h)
{
	static WORD n;
	OBJECT *o = &menu[n];

	o->ob_next = o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = 0;
	o->ob_state = 0;
	o->ob_spec.index = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
	if (parent >= 0) {
		if (menu[parent].ob_head < 0)
			menu[parent].ob_head = n;
		else
			menu[menu[parent].ob_tail].ob_next = n;
		menu[parent].ob_tail = n;
		o->ob_next = parent;
	}
	return n++;
}

/* sizes in characters, as in a resource file; rsrc_obfix makes pixels */
static void build_menu(WORD screen_w)
{
	WORD root, bar, active, scr, t, i, x, n, w, box, o;
	WORD tx[4], tw[4];

	root = new_obj(-1, G_IBOX, 0L, 0, 0, 80, 25);
	bar = new_obj(root, G_BOX, 0x1100L, 0, 0, 80, 0x0201);
	active = new_obj(bar, G_IBOX, 0L, 2, 0, 0, 0x0301);
	for (t = 0, x = 0; titles[t]; t++) {
		tw[t] = (WORD)strlen(titles[t]);
		tx[t] = x;
		new_obj(active, G_TITLE, (long)titles[t], x, 0, tw[t], 0x0301);
		x += tw[t];
	}
	menu[active].ob_width = x;
	scr = new_obj(root, G_IBOX, 0L, 0, 0x0301, 80, 23);

	for (t = 0; titles[t]; t++) {
		const MENUITEM *m = menus[t];
		for (n = 0, w = 0; m[n].text; n++)
			if (m[n].text[0] != '-' && (WORD)strlen(m[n].text) > w)
				w = (WORD)strlen(m[n].text);
		if (t == 0 && w < 20)
			w = 20;				/* room for the accessory names */
		box = new_obj(scr, G_BOX, 0x00FF1100L, 2 + tx[t], 0, w, n);
		for (i = 0; i < n; i++) {
			char *s = menu_text[box + 1 + i];
			if (m[i].text[0] == '-') {
				memset(s, '-', w);
				s[w] = 0;
			} else if (t == 0 && m[i].id == 0)
				strcpy(s, "  Desk Accessory  ");
			else
				strcpy(s, m[i].text);
			o = new_obj(box, G_STRING, (long)s, 0, i, w, 1);
			if (m[i].text[0] == '-')
				menu[o].ob_state = OS_DISABLED;
			id_of[o] = m[i].id;
			if (m[i].id) {
				obj_of[m[i].id] = o;
				title_of[m[i].id] = 3 + t;
			}
		}
	}
	menu[o].ob_flags |= OF_LASTOB;

	for (i = 0; i <= o; i++)
		rsrc_obfix(menu, i);
	menu[root].ob_width = menu[bar].ob_width = screen_w;
	/* keep the menu boxes on the screen */
	for (i = menu[scr].ob_head; i != scr && i >= 0; i = menu[i].ob_next)
		if (menu[i].ob_x + menu[i].ob_width > screen_w)
			menu[i].ob_x = screen_w - menu[i].ob_width - 1;
}

/*--------------------------------------------------------------------
 * BASIC state and commands
 *------------------------------------------------------------------*/

/* BASIC waits for a line at its ">" prompt */
static int at_prompt(void)
{
	return interactive && cmd_history;
}

/* nothing to do for BASIC until the next event */
static int basic_idle(void)
{
	if (interactive && roff == woff)
		return TRUE;					/* waits for a key */
	return ctrl == 0 && ctrlsq == 0;	/* output stopped with Ctrl-S */
}

static void put_key(int key)
{
	KeyBuffer[woff++] = (BYTE)key;
	if (woff >= KB_SIZE)
		woff = 0;
}

/* types a command at the prompt (replacing what is typed there) */
static void command(const char *s)
{
	if (!at_prompt())
		return;
	put_key(KEY_LINE_KILL);
	while (*s)
		put_key(*s++);
	put_key('\r');
}

static void do_break(void)
{
	*((WORD *)&ctrl) = -1;				/* Ctrl-C: ctrl and ctrlsq */
}

/* Help: the keywords, as on the CPX (only at the prompt) */
static void show_keywords(void)
{
	char **str = tok_strings;
	int i, x = 0;

	if (!at_prompt())
		return;
	Putchar('\n');
	for (i = 0; i < NUM_TOKS; i++, str++) {
		if (x + (int)strlen(*str) >= MAX_CX) {
			if (x < MAX_CX)
				Putchar('\n');
			x = 0;
		}
		x += Printf("%s ", *str);
	}
	put_key(KEY_LINE_KILL);
	put_key('\r');
}

static void about(void)
{
	form_alert(1, "[1][Chipmunk BASIC 1.36 for GEM|"
		"Chipmunk BASIC: David Gillespie|"
		"CPX-BASIC: Oliver Teuber,|Jan Starzynski, Mia Jaap|"
		"Freeware under the GNU GPL][ OK ]");
}

static void ask_quit(void)
{
	if (!basic_has_program() ||
		form_alert(2, "[2][Quit Chipmunk BASIC?|"
			"The program in memory|will be lost.][Quit|Cancel]") == 1)
		quit_req = TRUE;
}

static void do_menu(int id)
{
	switch (id) {
	case MI_ABOUT:		about(); break;
	case MI_NEW:		command("NEW"); break;
	case MI_OPEN:		command("LOAD"); break;
	case MI_MERGE:		command("MERGE"); break;
	case MI_SAVE:		command("SAVE"); break;
	case MI_QUIT:		ask_quit(); break;
	case MI_RUN:		command("RUN"); break;
	case MI_STOP:		do_break(); break;
	case MI_CONT:		command("CONTINUE"); break;
	case MI_LIST:		command("LIST"); break;
	case MI_CLS:		command("CLS"); break;
	case MI_KEYWORDS:	show_keywords(); break;
	}
}

/* greys out what isn't possible right now */
static void update_menu(void)
{
	static int shown = -1;
	int prompt = at_prompt(), i;
	static const int prompt_only[] = {
		MI_NEW, MI_OPEN, MI_MERGE, MI_SAVE, MI_RUN, MI_CONT, MI_LIST,
		MI_CLS, MI_KEYWORDS, 0
	};

	if (prompt == shown)
		return;
	shown = prompt;
	for (i = 0; prompt_only[i]; i++)
		menu_ienable(menu, obj_of[prompt_only[i]], prompt);
	menu_ienable(menu, obj_of[MI_STOP], !prompt);
}

static void set_view(long v);

static void do_key(WORD kstate, WORD key)
{
	WORD scan = (key >> 8) & 0xff, ascii = key & 0xff;

	/* Shift+Up/Down: scroll back; any other key: back to the screen */
	if ((kstate & (K_LSHIFT | K_RSHIFT)) && (scan == 0x48 || scan == 0x50)) {
		set_view(scr_view() + (scan == 0x48 ? 1 : -1));
		return;
	}
	if (scr_view() > 0)
		set_view(0);

	if (scan == 0x62) {					/* Help */
		show_keywords();
		return;
	}
	if (kstate & K_CTRL) {
		switch (scan) {
		case 0x10: ask_quit(); break;			/* ^Q */
		case 0x2e: do_break(); break;			/* ^C */
		case 0x31: do_menu(MI_NEW); break;		/* ^N */
		case 0x18: do_menu(MI_OPEN); break;		/* ^O */
		case 0x32: do_menu(MI_MERGE); break;	/* ^M */
		case 0x13: do_menu(MI_RUN); break;		/* ^R */
		case 0x26: do_menu(MI_LIST); break;		/* ^L */
		case 0x1f:								/* ^S */
			if (at_prompt())
				do_menu(MI_SAVE);
			else								/* stop/go on output */
				ctrlsq = ctrlsq ? 0 : -1;
			break;
		}
		return;
	}
	if (interactive && ascii == 0) {
		if (scan == 0x48) ascii = KEY_HIST_UP;
		else if (scan == 0x50) ascii = KEY_HIST_DOWN;
		else if (scan == 0x4b) ascii = KEY_CUR_LEFT;
		else if (scan == 0x4d) ascii = KEY_CUR_RIGHT;
	}
	put_key(ascii);
}

/*--------------------------------------------------------------------
 * Window
 *------------------------------------------------------------------*/

static void work_area(GRECT *r)
{
	wind_get(win, WF_WORKXYWH, &r->g_x, &r->g_y, &r->g_w, &r->g_h);
}

static int intersect(const GRECT *a, GRECT *b)
{
	WORD x1 = a->g_x > b->g_x ? a->g_x : b->g_x;
	WORD y1 = a->g_y > b->g_y ? a->g_y : b->g_y;
	WORD x2 = a->g_x + a->g_w < b->g_x + b->g_w ? a->g_x + a->g_w : b->g_x + b->g_w;
	WORD y2 = a->g_y + a->g_h < b->g_y + b->g_h ? a->g_y + a->g_h : b->g_y + b->g_h;

	b->g_x = x1;
	b->g_y = y1;
	b->g_w = x2 - x1;
	b->g_h = y2 - y1;
	return b->g_w > 0 && b->g_h > 0;
}

static int intersect(const GRECT *a, GRECT *b);

/* the iconified window: the CPX icon, "BASIC" below it if there's room */
static void draw_icon(const GRECT *work, const GRECT *clip)
{
	static WORD colors[2] = { G_BLACK, G_WHITE };
	static MFDB screen;
	WORD pxy[8], d, h, x, y;
	GRECT ic;

	pxy[0] = clip->g_x;
	pxy[1] = clip->g_y;
	pxy[2] = clip->g_x + clip->g_w - 1;
	pxy[3] = clip->g_y + clip->g_h - 1;
	vs_clip(vh, 1, pxy);
	vswr_mode(vh, MD_REPLACE);
	vsf_interior(vh, FIS_SOLID);
	vsf_color(vh, G_WHITE);
	vr_recfl(vh, pxy);

	h = work->g_h >= 24 + 2 + cellh ? 24 + 2 + cellh : 24;
	x = work->g_x + (work->g_w - 32) / 2;
	y = work->g_y + (work->g_h - h) / 2;

	/* vrt_cpyfm doesn't clip: copy only the visible part */
	ic.g_x = x;
	ic.g_y = y;
	ic.g_w = 32;
	ic.g_h = 24;
	if (intersect(clip, &ic)) {
		pxy[0] = ic.g_x - x;
		pxy[1] = ic.g_y - y;
		pxy[2] = pxy[0] + ic.g_w - 1;
		pxy[3] = pxy[1] + ic.g_h - 1;
		pxy[4] = ic.g_x;
		pxy[5] = ic.g_y;
		pxy[6] = ic.g_x + ic.g_w - 1;
		pxy[7] = ic.g_y + ic.g_h - 1;
		vrt_cpyfm(vh, MD_REPLACE, pxy, &icon_mfdb, &screen, colors);
	}
	if (h > 24) {
		vswr_mode(vh, MD_TRANS);
		vst_color(vh, G_BLACK);
		vst_alignment(vh, 1, 5, &d, &d);	/* centred, top */
		v_gtext(vh, work->g_x + work->g_w / 2, y + 24 + 2, "BASIC");
		vst_alignment(vh, 0, 5, &d, &d);
		vswr_mode(vh, MD_REPLACE);
	}
	vs_clip(vh, 0, pxy);
}

static void update_slider(void);

static void iconify(WORD *msg)
{
	wind_get(win, WF_CURRXYWH, &before_icon.g_x, &before_icon.g_y,
		&before_icon.g_w, &before_icon.g_h);
	wind_set(win, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
	iconified = TRUE;
}

/* back to the size before iconify (`r' from WM_UNICONIFY, or NULL
   when we noticed the double click ourselves) */
static void uniconify(const GRECT *r)
{
	GRECT to = before_icon;

	if (r != NULL && r->g_w > 0 && r->g_h > 0 &&
		(r->g_w != before_icon.g_w || r->g_h != before_icon.g_h)) {
		/* some AES give the icon's size here: only take the place */
		to.g_x = r->g_x;
		to.g_y = r->g_y;
	} else if (r != NULL && r->g_w > 0)
		to = *r;
	wind_set(win, WF_UNICONIFY, to.g_x, to.g_y, to.g_w, to.g_h);
	iconified = FALSE;
	update_slider();
}

/* draws rows y0..y1 where `area' (screen coordinates) is visible */
static void redraw(const GRECT *area, WORD y0, WORD y1)
{
	GRECT work, r, clip;

	work_area(&work);
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
	while (r.g_w > 0 && r.g_h > 0) {
		clip = *area;
		if (intersect(&r, &clip) && intersect(&desk, &clip)) {
			if (iconified)
				draw_icon(&work, &clip);
			else
				scr_draw(vh, &work, &clip, y0, y1);
		}
		wind_get(win, WF_NEXTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
}

static void redraw_all(void)
{
	GRECT work;

	work_area(&work);
	redraw(&work, 0, MAX_CY - 1);
}

/* the vertical slider shows where the view is in the scrollback */
static void update_slider(void)
{
	static WORD shown_size = -1, shown_pos = -1;
	WORD size, pos;

	if (iconified)
		return;
	scr_slider(&size, &pos);
	if (size != shown_size)
		wind_set(win, WF_VSLSIZE, shown_size = size, 0, 0, 0);
	if (pos != shown_pos)
		wind_set(win, WF_VSLIDE, shown_pos = pos, 0, 0, 0);
}

/* look at older lines (v lines back) or, with 0, at the screen */
static void set_view(long v)
{
	if (scr_set_view(v)) {
		redraw_all();
		update_slider();
	}
}

/*
 * Scrolls the window contents up by `lines' text lines with a screen
 * copy. Only possible where the whole window is visible: on top and
 * inside the desktop. FALSE if not done.
 */
static int scroll_window(WORD lines)
{
	static MFDB screen;		/* fd_addr NULL: the screen */
	GRECT work, r, d;
	WORD pxy[8];
	int done = FALSE;

	work_area(&work);
	wind_update(BEG_UPDATE);
	wind_get(win, WF_FIRSTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
	d = work;
	if (r.g_x == work.g_x && r.g_y == work.g_y && r.g_w == work.g_w &&
		r.g_h == work.g_h && intersect(&desk, &d) && d.g_w == work.g_w &&
		d.g_h == work.g_h) {
		wind_get(win, WF_NEXTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
		if (r.g_w <= 0 || r.g_h <= 0) {
			pxy[0] = work.g_x;
			pxy[1] = work.g_y + lines * cellh;
			pxy[2] = work.g_x + work.g_w - 1;
			pxy[3] = work.g_y + MAX_CY * cellh - 1;
			pxy[4] = work.g_x;
			pxy[5] = work.g_y;
			pxy[6] = pxy[2];
			pxy[7] = pxy[3] - lines * cellh;
			graf_mouse(M_OFF, NULL);
			vro_cpyfm(vh, S_ONLY, pxy, &screen, &screen);
			graf_mouse(M_ON, NULL);
			done = TRUE;
		}
	}
	wind_update(END_UPDATE);
	return done;
}

/* what BASIC has changed since the last time */
static void update_screen(void)
{
	WORD y0, y1, x0, x1, scroll;
	GRECT work, r;

	if (!DirtyScreen || !scr_dirty(&y0, &y1, &x0, &x1, &scroll) || iconified)
		return;
	if (scroll > 0 && (scroll >= MAX_CY || !scroll_window(scroll))) {
		redraw_all();
		return;
	}
	if (y0 > y1 || x0 > x1)
		return;
	work_area(&work);
	r.g_x = work.g_x + x0 * cellw;
	r.g_y = work.g_y + y0 * cellh;
	r.g_w = (x1 == MAX_CX - 1 ? work.g_w : (x1 + 1) * cellw) - x0 * cellw;
	r.g_h = (y1 == MAX_CY - 1 ? work.g_h : (y1 + 1) * cellh) - y0 * cellh;
	redraw(&r, y0, y1);
}

/* window size from outer coordinates; the work area is made a
   multiple of the character cell */
static void set_window(GRECT *outer)
{
	GRECT work;
	WORD cols, rows;

	wind_calc(WC_WORK, win_kind, outer->g_x, outer->g_y, outer->g_w, outer->g_h,
		&work.g_x, &work.g_y, &work.g_w, &work.g_h);
	cols = work.g_w / cellw;
	rows = work.g_h / cellh;
	if (cols < MIN_COLS) cols = MIN_COLS;
	if (rows < MIN_ROWS) rows = MIN_ROWS;
	if (cols > MAX_COLS) cols = MAX_COLS;
	work.g_w = cols * cellw;
	work.g_h = rows * cellh;
	wind_calc(WC_BORDER, win_kind, work.g_x, work.g_y, work.g_w, work.g_h,
		&outer->g_x, &outer->g_y, &outer->g_w, &outer->g_h);
	wind_set(win, WF_CURRXYWH, outer->g_x, outer->g_y, outer->g_w, outer->g_h);
	if (cols != MAX_CX || rows != MAX_CY)
		scr_resize(cols, rows, cellw, cellh);
	redraw_all();
	update_slider();
}

/* iconify needs appl_getinfo() (AES 4.0 and up, or "?AGI") */
static int can_iconify(void)
{
	WORD o1, o2, o3, o4;

	if (aes_global[0] < 0x400 && appl_find("?AGI") != 0)
		return FALSE;
	if (!appl_getinfo(AES_WINDOW, &o1, &o2, &o3, &o4))
		return FALSE;
	return (o1 & 0x80) != 0;
}

/*--------------------------------------------------------------------
 * Window colour: the Colors protocol (Holger Weets, as in Pong). A colour
 * program sends COLOR_ID with the mouse position (msg[3], msg[4]) and a
 * VDI colour index (msg[5]); dropped on the window it becomes the
 * background. Kept per colour depth, so a colour from 256 colours can't
 * make the text unreadable in monochrome. White = the standard again.
 *------------------------------------------------------------------*/

#define COLOR_ID	0xC010
#define MAX_PLANES	32

static WORD ncolors, planes;			/* of the screen */
static WORD colours[MAX_PLANES + 1];	/* background per depth, -1 = white */

/* light colours get black text, dark ones white */
static int light_colour(WORD index)
{
	WORD rgb[3];

	if (index == G_WHITE)
		return TRUE;
	if (index == G_BLACK)
		return FALSE;
	vq_color(vh, index, 1, rgb);
	return (3L * rgb[0] + 6L * rgb[1] + rgb[2]) / 10 > 500;
}

static void apply_colours(void)
{
	WORD back = G_WHITE;

	if (planes >= 1 && planes <= MAX_PLANES && colours[planes] >= 0)
		back = colours[planes];
	scr_set_colours(back, light_colour(back) ? G_BLACK : G_WHITE);
}

/* a colour dropped on the window */
static void drop_colour(WORD mx, WORD my, WORD index)
{
	GRECT work;

	if (win < 0 || iconified || wind_find(mx, my) != win)
		return;
	work_area(&work);
	if (mx < work.g_x || mx >= work.g_x + work.g_w ||
		my < work.g_y || my >= work.g_y + work.g_h)
		return;
	if (index < 0 || (planes <= 8 && index >= ncolors))
		return;
	if (planes < 1 || planes > MAX_PLANES)
		return;
	colours[planes] = index == G_WHITE ? -1 : index;
	apply_colours();
	redraw_all();
}

/*--------------------------------------------------------------------
 * Window position and colours, kept in CHIPBAS.INF in the folder the
 * program was started from (plain text: "CHIPBAS 1", "screen w h",
 * "window x y w h", then "colour planes index" for each colour depth)
 *------------------------------------------------------------------*/

static char inf_name[200];
static WORD screen_w, screen_h;

static void inf_init(void)
{
	char *p = inf_name;

	*p++ = (char)('A' + Dgetdrv());
	*p++ = ':';
	if (Dgetpath(p, 0) < 0)
		*p = 0;
	p += strlen(p);
	if (p[-1] != '\\')
		*p++ = '\\';
	strcpy(p, "CHIPBAS.INF");
}

/* the next number in the text, -1 if there is none */
static long next_number(const char **t)
{
	const char *s = *t;
	long v = 0;

	while (*s && (*s < '0' || *s > '9'))
		s++;
	if (!*s)
		return -1;
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	*t = s;
	return v;
}

/* the saved window rectangle, if it was saved in this screen size */
static int load_position(GRECT *r)
{
	char buf[512];
	const char *t = buf;
	long h, n, v[7];
	int i;

	h = Fopen(inf_name, 0);
	if (h < 0)
		return FALSE;
	n = Fread((short)h, sizeof(buf) - 1, buf);
	Fclose((short)h);
	if (n <= 0 || strncmp(buf, "CHIPBAS 1", 9) != 0)
		return FALSE;
	buf[n] = 0;
	t = buf + 9;
	for (i = 0; i < 6; i++)
		if ((v[i] = next_number(&t)) < 0)
			return FALSE;
	if (v[0] != screen_w || v[1] != screen_h)
		return FALSE;
	r->g_x = (WORD)v[2];
	r->g_y = (WORD)v[3];
	r->g_w = (WORD)v[4];
	r->g_h = (WORD)v[5];
	return r->g_w > 0 && r->g_h > 0;
}

/* the colours from CHIPBAS.INF ("colour planes index" lines) */
static void load_colours(void)
{
	char buf[512];
	const char *t;
	long h, n, p, i;

	for (p = 0; p <= MAX_PLANES; p++)
		colours[p] = -1;
	h = Fopen(inf_name, 0);
	if (h < 0)
		return;
	n = Fread((short)h, sizeof(buf) - 1, buf);
	Fclose((short)h);
	if (n <= 0)
		return;
	buf[n] = 0;
	for (t = buf; (t = strstr(t, "colour ")) != NULL; ) {
		t += 7;
		p = next_number(&t);
		i = next_number(&t);
		if (p >= 1 && p <= MAX_PLANES && i >= 0)
			colours[p] = (WORD)i;
	}
}

static void save_position(void)
{
	char buf[512];
	GRECT r;
	long h;
	int p;

	if (iconified)
		r = before_icon;
	else
		wind_get(win, WF_CURRXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
	h = Fcreate(inf_name, 0);
	if (h < 0)
		return;
	sprintf(buf, "CHIPBAS 1\r\nscreen %d %d\r\nwindow %d %d %d %d\r\n",
		screen_w, screen_h, r.g_x, r.g_y, r.g_w, r.g_h);
	for (p = 1; p <= MAX_PLANES; p++)
		if (colours[p] >= 0)
			sprintf(buf + strlen(buf), "colour %d %d\r\n", p, colours[p]);
	Fwrite((short)h, strlen(buf), buf);
	Fclose((short)h);
}

/* the saved rectangle made to fit: on the desktop, whole characters */
static int saved_window(GRECT *outer, WORD *cols, WORD *rows)
{
	GRECT work;

	if (!load_position(outer))
		return FALSE;
	if (outer->g_w > desk.g_w) outer->g_w = desk.g_w;
	if (outer->g_h > desk.g_h) outer->g_h = desk.g_h;
	if (outer->g_x + outer->g_w > desk.g_x + desk.g_w)
		outer->g_x = desk.g_x + desk.g_w - outer->g_w;
	if (outer->g_y + outer->g_h > desk.g_y + desk.g_h)
		outer->g_y = desk.g_y + desk.g_h - outer->g_h;
	if (outer->g_x < desk.g_x) outer->g_x = desk.g_x;
	if (outer->g_y < desk.g_y) outer->g_y = desk.g_y;
	wind_calc(WC_WORK, win_kind, outer->g_x, outer->g_y, outer->g_w, outer->g_h,
		&work.g_x, &work.g_y, &work.g_w, &work.g_h);
	*cols = work.g_w / cellw;
	*rows = work.g_h / cellh;
	if (*cols < MIN_COLS || *rows < MIN_ROWS)
		return FALSE;
	if (*cols > MAX_COLS) *cols = MAX_COLS;
	wind_calc(WC_BORDER, win_kind, work.g_x, work.g_y, *cols * cellw, *rows * cellh,
		&outer->g_x, &outer->g_y, &outer->g_w, &outer->g_h);
	return TRUE;
}

static int open_window(void)
{
	GRECT work, outer;
	WORD cols, rows;
	int saved;

	if (can_iconify())
		win_kind |= SMALLER;
	saved = saved_window(&outer, &cols, &rows);

	/* largest work area on the desktop, at most 80 x 25 characters */
	wind_calc(WC_WORK, win_kind, desk.g_x, desk.g_y, desk.g_w, desk.g_h,
		&work.g_x, &work.g_y, &work.g_w, &work.g_h);
	cols = work.g_w / cellw;
	rows = work.g_h / cellh;
	if (cols > 80) cols = 80;
	if (rows > 25) rows = 25;
	if (saved) {
		/* the position from CHIPBAS.INF (cols/rows from there) */
	} else if (cols < MIN_COLS || rows < MIN_ROWS) {
		form_alert(1, "[3][Chipmunk BASIC needs a screen|"
			"of at least 20 x 5 characters.][ OK ]");
		return FALSE;
	}
	if (!saved)	{
		/* the saved size is set by saved_window() */
		work.g_w = cols * cellw;
		work.g_h = rows * cellh;
		wind_calc(WC_BORDER, win_kind, work.g_x, work.g_y, work.g_w, work.g_h,
			&outer.g_x, &outer.g_y, &outer.g_w, &outer.g_h);
		outer.g_x = desk.g_x + (desk.g_w - outer.g_w) / 2;
		outer.g_y = desk.g_y + (desk.g_h - outer.g_h) / 2;
		if (outer.g_y < desk.g_y)
			outer.g_y = desk.g_y;
	}
	if (!scr_resize(cols, rows, cellw, cellh))
		return FALSE;

	win = wind_create(win_kind, desk.g_x, desk.g_y, desk.g_w, desk.g_h);
	if (win < 0) {
		form_alert(1, "[3][No more windows available.][ OK ]");
		return FALSE;
	}
	wind_set_str(win, WF_NAME, APP_NAME + 2);
	wind_open(win, outer.g_x, outer.g_y, outer.g_w, outer.g_h);
	update_slider();
	return TRUE;
}

static void do_message(WORD *msg)
{
	GRECT r;

	if ((unsigned short)msg[0] == COLOR_ID) {
		drop_colour(msg[3], msg[4], msg[5]);
		return;
	}
	switch (msg[0]) {
	case MN_SELECTED:
		do_menu(id_of[msg[4]]);
		menu_tnormal(menu, msg[3], 1);
		break;
	case WM_REDRAW:
		if (msg[3] == win)
			redraw((GRECT *)&msg[4], 0, MAX_CY - 1);
		break;
	case WM_TOPPED:
		if (msg[3] == win)
			wind_set(win, WF_TOP, 0, 0, 0, 0);
		break;
	case WM_CLOSED:
		if (msg[3] == win)
			ask_quit();
		break;
	case WM_MOVED:
		if (msg[3] == win)
			wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
		break;
	case WM_SIZED:
		if (msg[3] == win)
			set_window((GRECT *)&msg[4]);
		break;
	case WM_FULLED:
		if (msg[3] == win) {
			GRECT cur, full;
			wind_get(win, WF_CURRXYWH, &cur.g_x, &cur.g_y, &cur.g_w, &cur.g_h);
			wind_get(win, WF_FULLXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
			if (cur.g_w >= full.g_w - cellw && cur.g_h >= full.g_h - cellh)
				wind_get(win, WF_PREVXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
			else
				r = full;
			set_window(&r);
		}
		break;
	case WM_ARROWED:
		if (msg[3] == win)
			switch (msg[4]) {
			case WA_UPPAGE: set_view(scr_view() + (MAX_CY - 1)); break;
			case WA_DNPAGE: set_view(scr_view() - (MAX_CY - 1)); break;
			case WA_UPLINE: set_view(scr_view() + 1); break;
			case WA_DNLINE: set_view(scr_view() - 1); break;
			}
		break;
	case WM_VSLID:
		if (msg[3] == win && scr_set_slider(msg[4])) {
			redraw_all();
			update_slider();
		}
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (msg[3] == win && !iconified)
			iconify(msg);
		break;
	case WM_UNICONIFY:
		if (msg[3] == win && iconified)
			uniconify((GRECT *)&msg[4]);
		break;
	case AP_TERM:
		quit_req = TRUE;
		break;
	}
}

/*--------------------------------------------------------------------
 * Event loop
 *------------------------------------------------------------------*/

static void event_loop(void)
{
	WORD msg[8], mx, my, mb, ks, key, clicks, ev;
	long last = clock();

	StartBASIC();					/* prints the start-up text */
	update_screen();
	for (;;) {
		update_menu();
		/* while iconified also double clicks: not every AES turns them
		   into WM_UNICONIFY */
		ev = evnt_multi(MU_MESAG | MU_KEYBD | MU_TIMER | (iconified ? MU_BUTTON : 0),
			2, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			msg, basic_idle() ? 100L : 0L,
			&mx, &my, &mb, &ks, &key, &clicks);
		if (ev & MU_MESAG)
			do_message(msg);
		if (ev & MU_KEYBD)
			do_key(ks, key);
		if ((ev & MU_BUTTON) && iconified && clicks >= 2 &&
			wind_find(mx, my) == win)
			uniconify(NULL);
		if (quit_req)
			break;

		/* files of FOPEN are closed 10 ticks after the last access */
		if (clock() - last >= CLOCKS_PER_SEC / 20) {
			last = clock();
			if (fptime > 0) {
				fptime--;
				DATAfclose(0);
			}
		}

		if (!basic_idle() && ContBASIC() == 'STOP')
			break;					/* BYE */
		update_screen();
	}
}

int main(void)
{
	WORD work_in[11], work_out[57], attrib[10], d, i;

	if (appl_init() < 0)
		return 1;
	graf_mouse(ARROW, NULL);

	vh = graf_handle(&d, &d, &d, &d);
	for (i = 0; i < 10; i++)
		work_in[i] = 1;
	work_in[10] = 2;
	v_opnvwk(work_in, &vh, work_out);
	if (vh <= 0) {
		form_alert(1, "[3][Chipmunk BASIC can't open|a VDI workstation.][ OK ]");
		appl_exit();
		return 1;
	}
	vst_alignment(vh, 0, 5, &d, &d);	/* left, top */
	vqt_attributes(vh, attrib);
	cellw = attrib[8];
	cellh = attrib[9];

	wind_get(0, WF_WORKXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
	screen_w = work_out[0] + 1;
	screen_h = work_out[1] + 1;
	ncolors = work_out[13];
	{
		WORD ext[57];
		vq_extnd(vh, 1, ext);
		planes = ext[4];
	}
	build_menu(screen_w);

	/* under MultiTOS/MagiC: our name in the application list, and we
	   understand AP_TERM */
	if (aes_global[1] != 1)
		menu_register(aes_global[2], APP_NAME);
	if (aes_global[0] >= 0x400)
		shel_write(SWM_NEWMSG, NM_APTERM, 0, NULL, NULL);

	inf_init();
	load_colours();
	apply_colours();
	if (open_window()) {
		menu_bar(menu, 1);
		event_loop();
		menu_bar(menu, 0);
		save_position();
		wind_close(win);
		wind_delete(win);
	}
	DATAfclose(1);
	v_clsvwk(vh);
	appl_exit();
	return 0;
}
