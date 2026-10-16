/* aes.h - Pure C's AES.H for the GCC build: GEMlib has it all in gem.h */
/* p2c.h #defines Char, which is a parameter name in GEMlib's mt_gem.h */
#pragma push_macro("Char")
#undef Char
#include <gem.h>
#pragma pop_macro("Char")
#include <portab.h>
