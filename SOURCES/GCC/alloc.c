/*
 * alloc.c - malloc() & co. for the GCC build of CPX-BASIC
 *
 * Replaces MiNTLib's allocator (MiNTLib's stdio uses these as well).
 * A CPX has no basepage of its own and must give back everything it
 * allocated when it is closed, which Pure C's _FreeAll() did. So the
 * memory comes from GEMDOS in big chunks that are all remembered, and
 * _FreeAll() returns them.
 *
 * The allocator itself is the classic first fit free list from
 * Kernighan & Ritchie.
 */
#include <stddef.h>
#include <string.h>
#include <osbind.h>

#define CHUNK_SIZE (32L * 1024)

typedef union header {
	struct {
		union header *next;	/* next free block */
		unsigned long units;	/* size of this block in units */
	} s;
	long align[2];
} Header;

typedef struct chunk {
	struct chunk *next;
	long pad;			/* keeps the first Header 8 bytes in */
} Chunk;

static Header base;
static Header *freep;
static Chunk *chunks;

void free(void *ap);

static Header *morecore(unsigned long nu)
{
	long bytes = (long)(nu * sizeof(Header)) + (long)sizeof(Chunk);
	Chunk *c = NULL;
	Header *up;

	if (bytes < CHUNK_SIZE) {
		c = (Chunk *)Malloc(CHUNK_SIZE);
		if (c != NULL)
			bytes = CHUNK_SIZE;
	}
	if (c == NULL)
		c = (Chunk *)Malloc(bytes);
	if (c == NULL || (long)c < 0)
		return NULL;
	c->next = chunks;
	chunks = c;
	up = (Header *)(c + 1);
	up->s.units = (bytes - sizeof(Chunk)) / sizeof(Header);
	free(up + 1);
	return freep;
}

void *malloc(size_t nbytes)
{
	Header *p, *prevp;
	unsigned long nunits;

	if (nbytes == 0)
		nbytes = 1;
	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
	if ((prevp = freep) == NULL) {
		base.s.next = freep = prevp = &base;
		base.s.units = 0;
	}
	for (p = prevp->s.next; ; prevp = p, p = p->s.next) {
		if (p->s.units >= nunits) {
			if (p->s.units == nunits)
				prevp->s.next = p->s.next;
			else {
				p->s.units -= nunits;
				p += p->s.units;
				p->s.units = nunits;
			}
			freep = prevp;
			return p + 1;
		}
		if (p == freep)
			if ((p = morecore(nunits)) == NULL)
				return NULL;
	}
}

void free(void *ap)
{
	Header *bp, *p;

	if (ap == NULL)
		return;
	bp = (Header *)ap - 1;
	for (p = freep; !(bp > p && bp < p->s.next); p = p->s.next)
		if (p >= p->s.next && (bp > p || bp < p->s.next))
			break;		/* freed block at start or end of arena */

	if (bp + bp->s.units == p->s.next) {
		bp->s.units += p->s.next->s.units;
		bp->s.next = p->s.next->s.next;
	} else
		bp->s.next = p->s.next;
	if (p + p->s.units == bp) {
		p->s.units += bp->s.units;
		p->s.next = bp->s.next;
	} else
		p->s.next = bp;
	freep = p;
}

void *realloc(void *ap, size_t nbytes)
{
	Header *bp;
	size_t old;
	void *n;

	if (ap == NULL)
		return malloc(nbytes);
	if (nbytes == 0) {
		free(ap);
		return NULL;
	}
	bp = (Header *)ap - 1;
	old = (bp->s.units - 1) * sizeof(Header);
	if (nbytes <= old)
		return ap;
	if ((n = malloc(nbytes)) == NULL)
		return NULL;
	memcpy(n, ap, old);
	free(ap);
	return n;
}

void *calloc(size_t n, size_t size)
{
	void *p = malloc(n * size);

	if (p != NULL)
		memset(p, 0, n * size);
	return p;
}

/* Pure C: give back all memory that malloc() ever got */
void _FreeAll(void)
{
	while (chunks != NULL) {
		Chunk *c = chunks;
		chunks = c->next;
		Mfree(c);
	}
	freep = NULL;
}
