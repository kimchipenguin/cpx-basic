/*
 * gemscrn.c - the BASIC screen of the GEM application (CHIPBAS)
 *
 * Replaces the 256x176 bitmap screen of the CPX (GCC/cpx_bios.S). The
 * screen consists of
 *  - a grid of max_cx x max_cy characters with a reverse flag each,
 *    drawn with the VDI system font (v_gtext),
 *  - a monochrome graphics bitmap of the same size in pixels for PLOT,
 *    LINE and CIRCLE, copied with vrt_cpyfm, and
 *  - the scrollback: the last SB_MAX lines that scrolled off the top
 *    (text only). The window shows the live screen or, scrolled back
 *    with the slider, `view' lines further up.
 * As on the CPX, a character covers the graphics under its cell and
 * graphics scroll together with the text. Background and text colour
 * come from gemmain.c (Colors protocol).
 *
 * The functions called by CPX_IO.C and BASIC.C (PutChar, ScrollUp, cls,
 * plotpoint, ...) only change the grid and the bitmap and note the
 * changed area (rows and columns) and how often the whole window
 * contents scrolled up; gemmain.c draws that in its event loop
 * (scr_draw). The VDI is slow at text, so only what changed is drawn
 * again, and a scroll is a screen copy where the window is fully
 * visible.
 */
#include <stdlib.h>
#include <string.h>
#include "glo.h"

extern char ctrl, ctrlsq;		/* GCC/cpxmains.S */
extern WORD cx, cy, cm;			/* CPX_IO.C */

GLOBAL WORD max_cx, max_cy;		/* text area in characters */
GLOBAL char rev_mode;			/* !=0: reverse output (ESC p) */

static WORD cw, ch;				/* character cell in pixels */
static unsigned char *grid;		/* max_cy rows of max_cx characters */
static unsigned char *attr;		/* 1 = reverse */
static unsigned short *gfx;		/* graphics, 1 bit per pixel */
static long wpl;				/* words per bitmap line */
static WORD gfx_used;			/* something drawn since the last CLS */
static MFDB gfx_mfdb;
static WORD dirty_y0 = 0x7fff, dirty_y1 = -1;	/* changed rows (grid) */
static WORD dirty_x0 = 0x7fff, dirty_x1 = -1;	/* changed columns */
static WORD scrolled;			/* window contents moved up, in lines */

/* scrollback: a ring of lines, sb_first is the oldest */
#define SB_MAX 500
typedef struct {
	unsigned char *text;		/* len characters, then len attributes */
	WORD len;
} SBLINE;
static SBLINE sb[SB_MAX];
static WORD sb_first, sb_count;
static WORD view;				/* lines scrolled back, 0 = live screen */

/* window colours (VDI indexes): background and text/graphics */
static WORD back_col = G_WHITE, fore_col = G_BLACK;

void scr_set_colours(WORD back, WORD fore)
{
	back_col = back;
	fore_col = fore;
}

#define GFX_W ((long)max_cx * cw)
#define GFX_H ((long)max_cy * ch)

static void dirty_area(WORD y0, WORD y1, WORD x0, WORD x1)
{
	if (y0 < dirty_y0) dirty_y0 = y0;
	if (y1 > dirty_y1) dirty_y1 = y1;
	if (x0 < dirty_x0) dirty_x0 = x0;
	if (x1 > dirty_x1) dirty_x1 = x1;
	DirtyScreen = TRUE;
}

static void dirty(WORD y0, WORD y1)
{
	dirty_area(y0, y1, 0, max_cx - 1);
}

/*
 * The changed area since the last call in window rows (y0..y1) and
 * columns (x0..x1), and by how many lines the window contents moved up
 * before that area was changed. FALSE if nothing changed.
 */
int scr_dirty(WORD *y0, WORD *y1, WORD *x0, WORD *x1, WORD *scroll)
{
	int r = dirty_y0 <= dirty_y1 && dirty_x0 <= dirty_x1;

	*y0 = (dirty_y0 > 0 ? dirty_y0 : 0) + view;
	*y1 = dirty_y1 + view;
	if (*y1 >= max_cy)
		*y1 = max_cy - 1;
	*x0 = dirty_x0 > 0 ? dirty_x0 : 0;
	*x1 = dirty_x1 < max_cx ? dirty_x1 : max_cx - 1;
	*scroll = scrolled;
	dirty_y0 = dirty_x0 = 0x7fff;
	dirty_y1 = dirty_x1 = -1;
	scrolled = 0;
	DirtyScreen = FALSE;
	return (r && *y0 <= *y1) || *scroll;
}

/*--------------------------------------------------------------------
 * Scrollback
 *------------------------------------------------------------------*/

static SBLINE *sb_line(WORD i)		/* 0 = oldest */
{
	return &sb[(sb_first + i) % SB_MAX];
}

/* grid row y to the end of the scrollback; TRUE if the oldest line had
   to go for it */
static int sb_push(WORD y)
{
	unsigned char *g = grid + (long)y * max_cx, *a = attr + (long)y * max_cx;
	WORD len = max_cx;
	SBLINE *l;
	int dropped = FALSE;

	while (len > 0 && (g[len - 1] == ' ' || g[len - 1] == 0) && !a[len - 1])
		len--;
	if (sb_count == SB_MAX) {
		free(sb[sb_first].text);
		sb_first = (sb_first + 1) % SB_MAX;
		sb_count--;
		dropped = TRUE;
	}
	l = sb_line(sb_count);
	l->len = 0;
	l->text = NULL;
	if (len > 0 && (l->text = malloc(2L * len)) != NULL) {
		memcpy(l->text, g, len);
		memcpy(l->text + len, a, len);
		l->len = len;
	}
	sb_count++;
	return dropped;
}

/* the newest scrollback line back into grid row y */
static void sb_pop(WORD y)
{
	SBLINE *l = sb_line(--sb_count);
	WORD n = l->len < max_cx ? l->len : max_cx;

	memset(grid + (long)y * max_cx, ' ', max_cx);
	memset(attr + (long)y * max_cx, 0, max_cx);
	if (l->text != NULL) {
		memcpy(grid + (long)y * max_cx, l->text, n);
		memcpy(attr + (long)y * max_cx, l->text + l->len, n);
		free(l->text);
	}
	l->text = NULL;
}

/* sets the view (lines scrolled back); TRUE if it changed */
int scr_set_view(long v)
{
	if (v < 0) v = 0;
	if (v > sb_count) v = sb_count;
	if (v == view)
		return FALSE;
	view = (WORD)v;
	return TRUE;
}

WORD scr_view(void)
{
	return view;
}

/* slider size and position, 1..1000 as for WF_VSLSIZE/WF_VSLIDE */
void scr_slider(WORD *size, WORD *pos)
{
	long total = (long)sb_count + max_cy;

	*size = (WORD)(1000L * max_cy / total);
	*pos = sb_count ? (WORD)(1000L * (sb_count - view) / sb_count) : 1000;
}

/* view from a slider position 0..1000 */
int scr_set_slider(WORD pos)
{
	return scr_set_view(sb_count - (long)pos * sb_count / 1000);
}

/*--------------------------------------------------------------------
 * Size
 *------------------------------------------------------------------*/

void scr_cell(WORD *w, WORD *h)
{
	*w = cw;
	*h = ch;
}

/*
 * New size in characters. The text and the graphics keep their place
 * at the top left. If the cursor line would no longer fit, the lines
 * above it go into the scrollback; if the text reached the bottom and
 * the window grows, lines come back from the scrollback. Returns FALSE
 * without memory (the old screen is kept then).
 */
int scr_resize(WORD cols, WORD rows, WORD cellw, WORD cellh)
{
	unsigned char *ng, *na;
	unsigned short *nb;
	long nwpl = ((long)cols * cellw + 15) / 16;
	WORD y, n, top = 0, pull = 0;

	ng = malloc((long)cols * rows);
	na = malloc((long)cols * rows);
	nb = calloc(nwpl * rows * cellh, 2);
	if (ng == NULL || na == NULL || nb == NULL) {
		free(ng);
		free(na);
		free(nb);
		return FALSE;
	}
	memset(ng, ' ', (long)cols * rows);
	memset(na, 0, (long)cols * rows);
	if (grid != NULL) {
		if (cy >= rows) {
			top = cy - rows + 1;		/* first old line that is kept */
			for (y = 0; y < top; y++)
				sb_push(y);
		} else if (rows > max_cy && cy == max_cy - 1) {
			pull = rows - max_cy;		/* lines from the scrollback */
			if (pull > sb_count)
				pull = sb_count;
		}
		n = cols < max_cx ? cols : max_cx;
		for (y = 0; y + pull < rows && y + top < max_cy; y++) {
			memcpy(ng + (long)(y + pull) * cols, grid + (long)(y + top) * max_cx, n);
			memcpy(na + (long)(y + pull) * cols, attr + (long)(y + top) * max_cx, n);
		}
		if (gfx_used && cellh == ch && cellw == cw) {
			long words = nwpl < wpl ? nwpl : wpl;
			long lines = (long)((rows - pull) < (max_cy - top) ? (rows - pull) : (max_cy - top)) * ch;
			long l, skip = (long)top * ch, down = (long)pull * ch;
			for (l = 0; l < lines; l++)
				memcpy(nb + (l + down) * nwpl, gfx + (l + skip) * wpl, words * 2);
		}
		cy += pull - top;
		free(grid);
		free(attr);
		free(gfx);
	}
	grid = ng;
	attr = na;
	gfx = nb;
	wpl = nwpl;
	max_cx = cols;
	max_cy = rows;
	cw = cellw;
	ch = cellh;
	for (y = pull - 1; y >= 0; y--)	/* the newest line at the bottom */
		sb_pop(y);
	gfx_mfdb.fd_addr = gfx;
	gfx_mfdb.fd_w = (short)(wpl * 16);
	gfx_mfdb.fd_h = (short)GFX_H;
	gfx_mfdb.fd_wdwidth = (short)wpl;
	gfx_mfdb.fd_stand = 0;
	gfx_mfdb.fd_nplanes = 1;
	if (cx >= max_cx) cx = max_cx - 1;
	if (cy >= max_cy) cy = max_cy - 1;
	if (cy < 0) cy = 0;
	view = 0;
	scrolled = 0;
	dirty(0, max_cy - 1);
	return TRUE;
}

/* clears the graphics under a character cell */
static void clear_cell(WORD x, WORD y)
{
	long px = (long)x * cw, py = (long)y * ch, l;
	WORD i;

	if (!gfx_used)
		return;
	if ((px & 7) == 0 && (cw & 7) == 0) {
		unsigned char *p = (unsigned char *)gfx + py * wpl * 2 + px / 8;
		for (l = 0; l < ch; l++, p += wpl * 2)
			memset(p, 0, cw / 8);
	} else {
		for (l = py; l < py + ch; l++)
			for (i = 0; i < cw; i++)
				gfx[l * wpl + ((px + i) >> 4)] &= ~(0x8000U >> ((px + i) & 15));
	}
}

/*--------------------------------------------------------------------
 * The functions CPX_IO.C and BASIC.C call (were in CPX_BIOS.S)
 *------------------------------------------------------------------*/

VOID PutChar(WORD x, WORD y, WORD ch_)
{
	/* Ctrl-S: wait until Ctrl-S again or Ctrl-C (both flags 0 = wait) */
	while (ctrl == 0 && ctrlsq == 0)
		WaitBASIC();
	if (x < 0 || x >= max_cx || y < 0 || y >= max_cy || (ch_ & 0xff00))
		return;
	grid[(long)y * max_cx + x] = (unsigned char)ch_;
	attr[(long)y * max_cx + x] = rev_mode != 0;
	clear_cell(x, y);
	dirty_area(y, y, x, x);
}

/* the cursor is drawn by scr_draw(); only its cell has to be redrawn */
VOID DrawCursor(WORD x, WORD y)
{
	if (y >= 0 && y < max_cy && x >= 0 && x < max_cx)
		dirty_area(y, y, x, x);
}

/* clears from x0 to x1 in a line (always normal, not reverse) */
void delxy(int line, int x0, int x1)
{
	WORD x;

	if (line < 0 || line >= max_cy)
		return;
	if (x0 < 0) x0 = 0;
	for (x = x0; x <= x1 && x < max_cx; x++) {
		grid[(long)line * max_cx + x] = ' ';
		attr[(long)line * max_cx + x] = 0;
		clear_cell(x, line);
	}
	dirty(line, line);
}

void clline(int line)
{
	delxy(line, 0, max_cx - 1);
}

void cllines(int from, int to)
{
	for (; from <= to; from++)
		clline(from);
}

void cls(void)
{
	memset(grid, ' ', (long)max_cx * max_cy);
	memset(attr, 0, (long)max_cx * max_cy);
	memset(gfx, 0, wpl * 2 * GFX_H);
	gfx_used = FALSE;
	dirty(0, max_cy - 1);
}

/* deletes line `line', the lines below move up */
void ScrollUpR(int line)
{
	long n = (long)(max_cy - 1 - line) * max_cx;

	if (line < 0 || line >= max_cy)
		return;
	if (line == 0) {
		/* the top line goes into the scrollback. If the user looks at
		   older lines, the view stays where it is (unless the oldest
		   line had to go); else the window contents move up, which
		   gemmain.c can do with a screen copy. */
		if (!sb_push(0) && view > 0)
			view++;
		else
			scrolled++;
		/* what changed before moves up with the lines */
		if (dirty_y0 <= dirty_y1) {
			if (dirty_y0 > 0) dirty_y0--;
			if (--dirty_y1 < dirty_y0)
				dirty_y0 = 0x7fff, dirty_y1 = -1;
		}
	}
	if (n > 0) {
		memmove(grid + (long)line * max_cx, grid + (long)(line + 1) * max_cx, n);
		memmove(attr + (long)line * max_cx, attr + (long)(line + 1) * max_cx, n);
		if (gfx_used)
			memmove(gfx + (long)line * ch * wpl, gfx + (long)(line + 1) * ch * wpl,
				(long)(max_cy - 1 - line) * ch * wpl * 2);
	}
	clline(max_cy - 1);
	if (line > 0)
		dirty(line, max_cy - 1);
}

void ScrollUp(void)
{
	ScrollUpR(0);
}

/* inserts an empty line at `line', the lines below move down */
void ScrollDownR(int line)
{
	long n = (long)(max_cy - 1 - line) * max_cx;

	if (line < 0 || line >= max_cy)
		return;
	if (n > 0) {
		memmove(grid + (long)(line + 1) * max_cx, grid + (long)line * max_cx, n);
		memmove(attr + (long)(line + 1) * max_cx, attr + (long)line * max_cx, n);
		if (gfx_used)
			memmove(gfx + (long)(line + 1) * ch * wpl, gfx + (long)line * ch * wpl,
				(long)(max_cy - 1 - line) * ch * wpl * 2);
	}
	clline(line);
	dirty(line, max_cy - 1);
}

void ScrollDown(void)
{
	ScrollDownR(0);
}

/* (0,0) is the lower left corner, as on the CPX */
void plotpoint(int x, int y)
{
	long py;

	if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H)
		return;
	py = GFX_H - 1 - y;
	gfx[py * wpl + (x >> 4)] |= 0x8000U >> (x & 15);
	gfx_used = TRUE;
	dirty_area((WORD)(py / ch), (WORD)(py / ch), (WORD)(x / cw), (WORD)(x / cw));
}

/* Bresenham, as the CPX version */
void line(int x0, int y0, int x1, int y1)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy, e2;

	for (;;) {
		plotpoint(x0, y0);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

/*--------------------------------------------------------------------
 * Drawing (called by gemmain.c, inside wind_update/M_OFF)
 *------------------------------------------------------------------*/

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

static void fill(WORD vh, const GRECT *r, WORD color, WORD mode)
{
	WORD pxy[4];

	vswr_mode(vh, mode);
	vsf_color(vh, color);
	pxy[0] = r->g_x;
	pxy[1] = r->g_y;
	pxy[2] = r->g_x + r->g_w - 1;
	pxy[3] = r->g_y + r->g_h - 1;
	vr_recfl(vh, pxy);
}

/* v_gtext in pieces: the VDI takes only so many characters at once */
static void text(WORD vh, WORD x, WORD y, const unsigned char *s, WORD n)
{
	char buf[81];
	WORD i, k;

	while (n > 0) {
		k = n > 80 ? 80 : n;
		for (i = 0; i < k; i++)
			buf[i] = s[i] ? (char)s[i] : ' ';
		buf[k] = 0;
		v_gtext(vh, x, y, buf);
		s += k;
		x += k * cw;
		n -= k;
	}
}

/*
 * Draws window rows y0..y1 into the work area `work', clipped to
 * `clip' (already inside the work area and the screen). Window row r
 * shows grid row r - view, or a scrollback line if that is negative.
 */
void scr_draw(WORD vh, const GRECT *work, const GRECT *clip, WORD y0, WORD y1)
{
	WORD colors[2];
	MFDB screen = { 0 };
	WORD pxy[8], y, x, x0, c0, c1, n, gy, len;
	GRECT r, area;
	unsigned char row[512], rattr[512];
	const unsigned char *g, *a;

	pxy[0] = clip->g_x;
	pxy[1] = clip->g_y;
	pxy[2] = clip->g_x + clip->g_w - 1;
	pxy[3] = clip->g_y + clip->g_h - 1;
	vs_clip(vh, 1, pxy);
	vsf_interior(vh, FIS_SOLID);
	vsf_perimeter(vh, 0);
	colors[0] = fore_col;			/* graphics: set pixels */
	colors[1] = back_col;

	for (y = y0; y <= y1 && y < max_cy; y++) {
		r.g_x = work->g_x;
		r.g_y = work->g_y + y * ch;
		r.g_w = (WORD)GFX_W;
		r.g_h = ch;
		area = *clip;
		if (!intersect(&r, &area))
			continue;

		gy = y - view;				/* grid row, < 0: scrollback */
		if (gy >= 0) {
			g = grid + (long)gy * max_cx;
			a = attr + (long)gy * max_cx;
			len = max_cx;
		} else {
			SBLINE *l = sb_line(sb_count + gy);
			g = l->text;
			a = l->text + l->len;
			len = l->len;
		}

		/* background: graphics or white */
		if (gy >= 0 && gfx_used) {
			pxy[0] = area.g_x - work->g_x;
			pxy[1] = area.g_y - work->g_y - view * ch;
			pxy[2] = pxy[0] + area.g_w - 1;
			pxy[3] = pxy[1] + area.g_h - 1;
			pxy[4] = area.g_x;
			pxy[5] = area.g_y;
			pxy[6] = area.g_x + area.g_w - 1;
			pxy[7] = area.g_y + area.g_h - 1;
			vrt_cpyfm(vh, MD_REPLACE, pxy, &gfx_mfdb, &screen, colors);
		} else
			fill(vh, &area, back_col, MD_REPLACE);

		/* the columns inside the clipping area */
		c0 = (area.g_x - work->g_x) / cw;
		c1 = (area.g_x + area.g_w - 1 - work->g_x) / cw;
		if (c1 >= max_cx) c1 = max_cx - 1;
		if (c1 - c0 >= (WORD)sizeof(row))
			c1 = c0 + (WORD)sizeof(row) - 1;
		for (x = c0; x <= c1; x++) {
			row[x - c0] = x < len && g[x] ? g[x] : ' ';
			rattr[x - c0] = x < len ? a[x] : 0;
		}
		n = c1 - c0 + 1;

		/* normal characters: transparent over the background, without
		   the spaces at both ends (the VDI is slow at text) */
		{
			unsigned char plain[512];
			WORD i, first, last;
			for (i = 0; i < n; i++)
				plain[i] = rattr[i] ? ' ' : row[i];
			for (first = 0; first < n && plain[first] == ' '; first++)
				;
			for (last = n; last > first && plain[last - 1] == ' '; last--)
				;
			if (last > first) {
				vswr_mode(vh, MD_TRANS);
				vst_color(vh, fore_col);
				text(vh, r.g_x + (c0 + first) * cw, r.g_y, plain + first, last - first);
			}
		}

		/* reverse runs: black bar, white characters */
		for (x = 0; x < n; ) {
			if (!rattr[x]) {
				x++;
				continue;
			}
			x0 = x;
			while (x < n && rattr[x])
				x++;
			area.g_x = r.g_x + (c0 + x0) * cw;
			area.g_y = r.g_y;
			area.g_w = (x - x0) * cw;
			area.g_h = ch;
			fill(vh, &area, fore_col, MD_REPLACE);
			vswr_mode(vh, MD_TRANS);
			vst_color(vh, back_col);
			text(vh, area.g_x, area.g_y, row + x0, x - x0);
		}

		/* cursor: inverted cell */
		if (!cm && gy == cy && cx >= 0 && cx < max_cx) {
			area.g_x = r.g_x + cx * cw;
			area.g_y = r.g_y;
			area.g_w = cw;
			area.g_h = ch;
			fill(vh, &area, G_BLACK, MD_XOR);
		}
	}

	/* the rest of the work area right of and below the text area */
	if (y1 >= max_cy - 1) {
		r.g_x = work->g_x;
		r.g_y = work->g_y + (WORD)GFX_H;
		r.g_w = work->g_w;
		r.g_h = work->g_h - (WORD)GFX_H;
		area = *clip;
		if (intersect(&r, &area))
			fill(vh, &area, back_col, MD_REPLACE);
	}
	r.g_x = work->g_x + (WORD)GFX_W;
	r.g_y = work->g_y + y0 * ch;
	r.g_w = work->g_w - (WORD)GFX_W;
	r.g_h = (y1 - y0 + 1) * ch;
	area = *clip;
	if (intersect(&r, &area))
		fill(vh, &area, back_col, MD_REPLACE);

	vswr_mode(vh, MD_REPLACE);
	vs_clip(vh, 0, pxy);
}
