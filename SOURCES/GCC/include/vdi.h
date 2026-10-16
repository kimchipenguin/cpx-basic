/* vdi.h - Pure C's VDI.H for the GCC build: GEMlib has it all in gem.h */
/* p2c.h #defines Char, which is a parameter name in GEMlib's mt_gem.h */
#pragma push_macro("Char")
#undef Char
#include <gem.h>
#pragma pop_macro("Char")
#include <portab.h>

/* Pure C's colour names; GEMlib calls them G_WHITE, G_BLACK, ... */
#ifndef WHITE
#define WHITE	G_WHITE
#define BLACK	G_BLACK
#endif
