#define TOK_STR
#include "tokstr.h"

#ifndef NUM_EL
#define NUM_EL(a) (sizeof(a)/sizeof(*a))
#endif

char *tok_strings[]=
{
    "(",")","[","]"
    ,"=","<",">","<=",">=","<>"
    ,",",";",":"
    ,"?","#"

/* Schl《selworte mit Realzahlen als Argument */
    ,"+","-","*","/","^"
/* alphanumerische Funktionen */
    ,"SQU","SQR","SQRT"
    ,"SIN","COS","TAN","ASIN","ACOS","ATAN","ARCTAN"
    ,"SINH","COSH","TANH","ASINH","ACOSH","ATANH"
    ,"LOG","LN","LD","LG"
    ,"EXP","TEN","TWO"
    ,"ABS","SGN","STR$","RCP","INT","ROUND"
    ,"DMS$"
    ,"FAHRENHEIT","CELSIUS", "CEIL", "FLOOR"

/* Schl《selworte mit Intzahlen als Argument */
    ,"LABEL","NOT","FACT"
    ,"HEX$","OCT$","BIN$","CHR$"
    ,"PEEKL","PEEKW","PEEK","POKEL","POKEW","POKE"
	,"PLOT","LINE","CIRCLE","ELLIPSE", "MOVETO", "LINETO"

/* Schl《selworte mit String als Argument */
    ,"VAL","ASC","LEN"
    ,"DEG","FSEL$","MIRROR$","LOWER$","UPPER$"
	,"INBYTE","OUTBYTE"
	,"TRIM$","LTRIM$","RTRIM$","EXISTS","COOKIE"
	
/* Schl《selworte mit irgendeinem  oder ohne Argument */
    ,"REM","'","LET","PRINT","INPUT"
    ,"IF","END","STOP","CONTINUE","FOR","NEXT","WHILE","WEND"
    ,"GOSUB","RETURN","READ","DATA","RESTORE"
    ,"GOTOXY","LOCATE","POSITION","ON","DIM","DIMENSION"
    ,"LIST","RUN","NEW","LOAD","MERGE"
    ,"SAVE","BYE","DEL","RENUM"
    ,"FOPEN","FCLOSE","FINPUT","FPRINT","ERRNO"
    ,"SLEEP"
    ,"DEGREE","GRAD","RAD","CLS","HOME","CLEAR","USING$","USING"
    ,"RAND","PI","INKEY$","MID$"
    ,"INSTR","REPLACE$","REPEAT$"
    ,"AND","OR","XOR","MOD","THEN","ELSE","TO","STEP","GOTO"
	,"TRON","TROFF"
};

#ifdef __GNUC__
/* GCC doesn't allow sizeof in #if: fails if the count doesn't match */
typedef char tok_strings_count_check[NUM_EL(tok_strings)==NUM_TOKS ? 1 : -1];
#else
#if (NUM_EL(tok_strings)<NUM_TOKS)
#line 8
#error "not enough initializers in array 'tok_strings'"
#endif

#if (NUM_EL(tok_strings)>NUM_TOKS)
#line 8
#error "too many initializers in array 'tok_strings'"
#endif
#endif
