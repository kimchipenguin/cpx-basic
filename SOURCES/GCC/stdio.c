/*
 * stdio.c - the few file functions CPX-BASIC uses, directly on GEMDOS
 *
 * Replaces MiNTLib's fopen() & co. for two reasons:
 *  - Pure C opens files without "b" in text mode: CR LF in the file is
 *    '\n' in memory and the other way round. MiNTLib under TOS treats
 *    every file as binary, so LOAD would keep a '\r' at the end of each
 *    line and SAVE would write LF only.
 *  - MiNTLib's open() brings in stat, user/group and time zone code,
 *    which roughly doubles the size of the CPX.
 *
 * Text mode as in Pure C: '\r' is dropped and ^Z ends the file when
 * reading, '\n' is written as CR LF. ftell()/fseek() work with the
 * position in the file, so DATAfclose()/DATAfopen() can continue where
 * they stopped.
 *
 * The FILE pointers handed out point to struct sfile; stdin/stdout are
 * never used for files in CPX-BASIC (glo.h makes them NULL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osbind.h>

#undef putc			/* MiNTLib has macros for these */
#undef fputc

#define DEFAULT_BUFSIZ 1024

struct sfile {
	short handle;
	char writing;		/* opened with "w" or "a" */
	char text;		/* no "b": text mode */
	char eof, err;
	unsigned char *buf;
	long size;		/* buffer size */
	long len;		/* valid bytes in buf (reading) */
	long pos;		/* current index in buf */
	long bufpos;		/* position of buf[0] in the file */
};

#define SF(fp) ((struct sfile *)(fp))

FILE *fopen(const char *name, const char *mode)
{
	struct sfile *f;
	long h;

	if (*mode == 'r')
		h = Fopen(name, 0);
	else if (*mode == 'w')
		h = Fcreate(name, 0);
	else if (*mode == 'a') {
		h = Fopen(name, 1);
		if (h < 0)
			h = Fcreate(name, 0);
		else if (Fseek(0L, (short)h, 2) < 0) {
			Fclose((short)h);
			h = -1;
		}
	} else
		return NULL;
	if (h < 0)
		return NULL;

	f = malloc(sizeof(*f));
	if (f != NULL) {
		memset(f, 0, sizeof(*f));
		f->buf = malloc(DEFAULT_BUFSIZ);
	}
	if (f == NULL || f->buf == NULL) {
		free(f);
		Fclose((short)h);
		return NULL;
	}
	f->handle = (short)h;
	f->writing = *mode != 'r';
	f->text = strchr(mode, 'b') == NULL;
	f->size = DEFAULT_BUFSIZ;
	if (f->writing)
		f->bufpos = Fseek(0L, f->handle, 1);
	return (FILE *)f;
}

int setvbuf(FILE *fp, char *buf, int type, size_t size)
{
	struct sfile *f = SF(fp);
	unsigned char *n;

	(void)buf;
	(void)type;
	if (f->pos != 0 || f->len != 0 || size == 0)
		return -1;		/* only before the first access */
	n = malloc(size);
	if (n == NULL)
		return -1;
	free(f->buf);
	f->buf = n;
	f->size = (long)size;
	return 0;
}

static int flush(struct sfile *f)
{
	if (f->writing && f->pos > 0) {
		long n = Fwrite(f->handle, f->pos, f->buf);
		if (n != f->pos)
			f->err = 1;
		if (n > 0)
			f->bufpos += n;
		f->pos = 0;
	}
	return f->err ? EOF : 0;
}

int fclose(FILE *fp)
{
	struct sfile *f = SF(fp);
	int r;

	if (f == NULL)
		return EOF;
	r = flush(f);
	if (Fclose(f->handle) < 0)
		r = EOF;
	free(f->buf);
	free(f);
	return r;
}

static int putbyte(struct sfile *f, int c)
{
	if (f->pos >= f->size && flush(f) != 0)
		return EOF;
	f->buf[f->pos++] = (unsigned char)c;
	return c & 0xff;
}

int putc(int c, FILE *fp)
{
	struct sfile *f = SF(fp);

	if (f == NULL || !f->writing)
		return EOF;
	if (f->text && c == '\n' && putbyte(f, '\r') == EOF)
		return EOF;
	return putbyte(f, c);
}

int fputc(int c, FILE *fp)
{
	return putc(c, fp);
}

static int getbyte(struct sfile *f)
{
	if (f->eof)
		return EOF;
	if (f->pos >= f->len) {
		f->bufpos += f->len;
		f->pos = 0;
		f->len = Fread(f->handle, f->size, f->buf);
		if (f->len <= 0) {
			if (f->len < 0)
				f->err = 1;
			f->len = 0;
			f->eof = 1;
			return EOF;
		}
	}
	return f->buf[f->pos++];
}

char *fgets(char *s, int n, FILE *fp)
{
	struct sfile *f = SF(fp);
	char *p = s;
	int c;

	if (f == NULL || f->writing || n <= 0)
		return NULL;
	while (p < s + n - 1) {
		c = getbyte(f);
		if (c == EOF)
			break;
		if (f->text) {
			if (c == '\r')
				continue;
			if (c == 0x1a) {	/* ^Z: end of a text file */
				f->eof = 1;
				break;
			}
		}
		*p++ = (char)c;
		if (c == '\n')
			break;
	}
	if (p == s)
		return NULL;
	*p = 0;
	return s;
}

long ftell(FILE *fp)
{
	struct sfile *f = SF(fp);

	return f->bufpos + f->pos;
}

int fseek(FILE *fp, long offset, int whence)
{
	struct sfile *f = SF(fp);
	long r;

	if (whence == SEEK_CUR) {
		offset += ftell(fp);
		whence = SEEK_SET;
	}
	if (flush(f) != 0)
		return -1;
	r = Fseek(offset, f->handle, whence);
	if (r < 0)
		return -1;
	f->bufpos = r;
	f->pos = f->len = 0;
	f->eof = 0;
	return 0;
}

/*
 * MiNTLib's vfprintf() (needed by sprintf() for the floating point
 * formats) references the stream layer for real files, which reaches
 * the OS only through these hooks. They pull in open(), fstat() and
 * with them time zone and passwd code. Real files are never MiNTLib
 * streams here (fopen() is the one above), so the hooks can't be
 * reached; these stubs keep the rest out of the CPX.
 */
void __stdio_init_stream(FILE *fp) { (void)fp; }
int __stdio_fileno(void *cookie) { (void)cookie; return -1; }
int __stdio_close(void *cookie) { (void)cookie; return -1; }
long __stdio_read(void *c, char *b, size_t n) { (void)c; (void)b; (void)n; return -1; }
long __stdio_write(void *c, const char *b, size_t n) { (void)c; (void)b; (void)n; return -1; }
long __stdio_text_read(void *c, char *b, size_t n) { (void)c; (void)b; (void)n; return -1; }
long __stdio_text_write(void *c, const char *b, size_t n) { (void)c; (void)b; (void)n; return -1; }
int __stdio_seek(void *c, long *pos, int whence) { (void)c; (void)pos; (void)whence; return -1; }
