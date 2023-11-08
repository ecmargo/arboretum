%{

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include "frontend.h"
#include "parser.hh"

#define MAX_INCLUDE_DEPTH 10

#define LexError(x...) do { currentContext->rememberError(linebuf, tokenPos);currentContext->Error(x); } while (0)
#define LexWarning(x...) do { currentContext->rememberError(linebuf, tokenPos);currentContext->Warning(x); } while (0)

#define tokenAdvance    { tokenPos = nextPos; nextPos += yyleng;}
#define gotToken   	tokenAdvance
#define gotTokenAndWhite	{ tokenPos = nextPos;  \
			  for (char *s = yytext; *s != '\0'; s++)  \
				if (*s == '\t')   \
           				nextPos += 8 - (nextPos & 7); \
				else nextPos++;   \
			}

#define incNextPos	(nextPos++)

static int get_dec(char *);
static unsigned int get_hex(char *);
static unsigned int get_oct(char *);
static double get_float(char *);
static char get_chr_oct(char *);
static char get_chr_esc(char *);
static int nextchar = 0;
static int buflen;
static int nextPos = 0;
int tokenPos = 0;
char tempbuf[BUFSIZE];
const char *linebuf = "";

#ifdef FLEX_SCANNER 

#undef YY_INPUT
#define YY_INPUT(buffer, result, max_size) \
        (result = my_yyinput(buffer, max_size))
static int my_yyinput(char*, int);

#define YY_NO_UNPUT

#else

#undef input
#undef unput
#define input() (my_input())
#define unput(c) (my_unput(c))
static char my_input();
static void my_unput(char);

#endif 
%}

%option noyywrap

%%

";"		{gotToken; return SEMI;}
"{"		{gotToken; return LBRACE;}
"}"		{gotToken; return RBRACE;}
":"		{gotToken; return COLON;}
","		{gotToken; return COMMA;}
"="		{gotToken; return EQUAL;}
"=="            {gotToken; return BEQUAL;}
"->"		{gotToken; return ARROW;}
"=>"            {gotToken; return DBLARROW;}
"+"		{gotToken; return ADD;}
"-"		{gotToken; return SUB;}
"*"		{gotToken; return MUL;}
"/"		{gotToken; return DIV;}
"("		{gotToken; return LPAREN;}
")"		{gotToken; return RPAREN;}
"<"		{gotToken; return LT;}
">"		{gotToken; return GT;}
"["		{gotToken; return LBRACK;}
"]"		{gotToken; return RBRACK;}
"|"             {gotToken; return PIPE;}
"||"            {gotToken; return OR;}
"&&"            {gotToken; return AND;}
"!"             {gotToken; return BANG;}
true		{gotToken; return BTRUE;}
false		{gotToken; return BFALSE;}
for             {gotToken; return FOR;}
endfor          {gotToken; return ENDFOR;}
to              {gotToken; return TO;}
do              {gotToken; return DO;}
if              {gotToken; return IF;}
then            {gotToken; return THEN;}
else            {gotToken; return ELSE;}

[a-zA-Z][a-zA-Z0-9_]*	{gotToken;
	char *z = (char *) malloc(strlen(yytext) + 1);
	strcpy(z, yytext);
	yylval.id = z;
	return ID;
}

[1-9][0-9]*	{gotToken; 
	yylval.integer = get_dec(yytext);
	return LIT_INT;
}

0[xX][a-fA-F0-9]+	{gotToken; 
	yylval.integer = get_hex(yytext+2);   /* strip off the "0x" */
	return LIT_INT;
}

0[0-7]*	{gotToken; 
	yylval.integer = get_oct(yytext);
	return LIT_INT;
}

[0-9]+"."[0-9]*([eE][+-]?[0-9]+)? {gotToken;
	yylval.real = get_float(yytext);
	return LIT_REAL;
}

[0-9]+[eE][+-]?[0-9]+ {gotToken;
	yylval.real = get_float(yytext);
	return LIT_REAL;
}

"\""(([^\"]*)((\\.)*))*"\"" {gotToken;
	char *z = (char *) malloc(strlen(yytext) + 1);
	strcpy(z, yytext+1);
        z[strlen(z)-1] = 0;
	yylval.str = z;
	return LIT_STRING;
}

"'"."'" {gotToken;
	yylval.chr = yytext[1];
	return LIT_CHR;
}

"'"\\([0-3]?[0-7]?[0-7])"'" {gotToken;
	yylval.chr = get_chr_oct(yytext + 2);
	return LIT_CHR;
}

"'"\\."'"	{gotToken; 
	yylval.chr = get_chr_esc(yytext + 2);
	return LIT_CHR;
}

"'"\\"x"[a-fA-F0-9]?[a-fA-F0-9]"'"	{gotToken;
	yylval.chr = get_chr_esc(yytext+2); 
	return LIT_CHR;
}

"//".*	{gotToken; }

"/*" {  gotToken;
	int prevchar = 0, currchar = yyinput();
	incNextPos;
	for (;;) {
        	if (currchar == EOF)
 			break;
 		if (currchar == '\n') {
			currentContext->NextLine();
			nextPos = 0;
			tokenPos = 0;
		}
		else if (currchar == '\t')
			nextPos += (8 - 1) - (nextPos-1) & 7;
		else if (prevchar == '/' && currchar == '*') {
			tokenPos = nextPos-2; 
                 	LexWarning("Nested comment");
 		}
		else if (prevchar == '*' && currchar == '/')
			break;
		prevchar = currchar;
		currchar = yyinput();
		incNextPos;
	}
}

^[ \t]*#.* {gotTokenAndWhite;
	/*
	 * Parse `#line' directives.  (The token `line' is optional.)
	 * (White space before the '#' is optional.)
	 */
	int i = 0;
	/* Skip over whitespace before the initial `#'. */
	for (;
	     ((i < yyleng)
	      && ((yytext[i] == ' ') || (yytext[i] == '\t')));
	     ++i)
		/* Do nothing. */ ;
	
	/* Skip over '#' and any whitespace. */
	for (++i;
	     ((i < yyleng)
	      && ((yytext[i] == ' ') || (yytext[i] == '\t')));
	     ++i)
		/* Do nothing. */ ;
	/* Skip over the (optional) token `line' and subsequent whitespace. */
	if ((i < yyleng)
	    && !strncmp(&(yytext[i]), "line", (sizeof("line") - 1))) {
		i += sizeof("line") - 1; /* `sizeof' counts terminating NUL */
		for (;
		     ((i < yyleng) &&
		      ((yytext[i] == ' ') || (yytext[i] == '\t')));
		     ++i)
			/* Do nothing. */ ;
	}
	
	/* Parse the line number and subsequent filename. */
	if ((i < yyleng)
	    && isdigit(yytext[i])) {
		int filename_start, filename_end, lineno;
                char *infilename;
		
		lineno = get_dec(yytext + i) - 1;
		
		for (filename_start = 0; (i < yyleng); ++i)
			if (yytext[i] == '\"') {
				filename_start = i + 1;
				break;
			}
		if (filename_start)
			for (i = filename_start, filename_end = 0;
			     (i < yyleng);
			     ++i)
				if (yytext[i] == '\"') {
					filename_end = i;
					break;
				}
		if (filename_start && filename_end) {
			infilename =
				(char *)
				malloc(sizeof(char)
					   * ((filename_end - filename_start)
					      + 1 /* for terminating NUL */));
			strncpy(infilename,
				&(yytext[filename_start]),
				(filename_end - filename_start));
			infilename[filename_end - filename_start] = 0;
                        currentContext->setPosition(infilename, lineno);
		}
	}
}

<<EOF>> { yyterminate(); } 

[ \t\f\r]*	{gotTokenAndWhite; }

\n	{ currentContext->NextLine();
	  tokenPos = nextPos = 0;
}

.	{gotToken; LexError("Illegal character: '%c'", yytext);}

%%

static int 
ishexdigit(char c)
{
	if (c >= '0' && c <= '9')
		return 1;
	if (c >= 'a' && c <= 'f')
		return 1;
	if (c >= 'A' && c <= 'F')
		return 1;
	return 0;
}

static int 
get_dec(char *s) 
{
	unsigned int res = 0, lastres = 0;
        char is_negative = 0;
        
        if (*s=='-') { is_negative=1;s++; } 
          else if (*s=='+') s++;
          
	while (*s >= '0' && *s <= '9' && lastres <= res) {
		lastres = res;
		res = res * 10 + *s - '0';
		s++;
	}
	if (lastres > res)
		LexError("Decimal overflow");
                
        if (is_negative) res=-res;
                
	return res;
}

/* converts a hex number string to an int.  Note: 's' must not contain "0x" */
static unsigned int 
get_hex(char *s) 
{
	unsigned int res = 0, lastres = 0;
	while (((*s >= '0' && *s <= '9') ||
	        (*s >= 'a' && *s <= 'f') ||
	        (*s >= 'A' && *s <= 'F')) &&
	       (lastres <= res)) {
		lastres = res;
		if (*s >= '0' && *s <= '9') 
			res = res * 16 + *s - '0';
		else 
			res = res * 16 + ((*s & 0x1f) + 9);
		s++;
	}
	if (lastres > res)
		LexError("Hex overflow");
	return res;
}
			
static unsigned int 
get_oct(char *s) 
{
	unsigned int res = 0, lastres = 0;
	while (*s >= '0' && *s < '8' && lastres <= res) {
		lastres = res;
		res = res * 8 + *s - '0';
		s++;
	}
	if (lastres > res)
		LexError("Octal overflow");
	return res;
}
			
static double 
get_float(char *s)
{
	double div = 0;
	int exp = 0, sign = 1;
	int mantissa = 1;
	double res = 0;
	while (1) {
	  if (*s >= '0' && *s <= '9') {
	    if (mantissa) {
	      if (div)
		res += ((double)(*s - '0') / div);
	      else
		res = res * 10 + (*s - '0');
	      div *= 10;
	    } else 
	      exp = exp * 10 + (*s - '0');
	  } else if (*s == '.') {
	    div = 10;
	  } else if (*s == 'e' || *s == 'E') {
	    mantissa = 0;
	  } else if (*s == '-') {
	    sign = -1;
	  } else if (*s == '+') {
	    sign = 1;
	  } else {
	    res *= pow(10, sign * exp);
	    return res;
    	  }
	  s++;
	}
}

static char 
get_chr_oct(char *s) 
{
	int ret = 0;
	int pos = 0;
	while (s[pos] >= '0' && s[pos] <= '7' && pos < 3)
		ret = ret * 8 + s[pos++] - '0';
	return (char) ret;
}

static char 
get_chr_esc(char *s)
{
	switch (*s) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case 'b':
		return '\b';
	case 'r':
		return '\r';
	case 'f':
		return '\f';
	case 'a':
		return '\a';
	case '\\':
		return '\\';
	case '\?':
		return '?';
	case '\'':
		return '\'';
	case '"':
		return '"';
	case 'x': {
		char str[3];
		int i;
		for (i = 0; 
		     s[i + 1] != '\0' && ishexdigit(s[i + 1]) && i < 2;
		     i++)
			str[i] = s[i+1];
		str[i] = 0;
		return get_hex(str);
	}
	break;
	default:
		if (s[1] >= '0' && s[1] <= '7') {
			int i;
			for (i = 1; i <= 3 && s[i] >= '0' && s[i] <= '7'; i++)
				{}
			char save = s[i];
			s[i] = '\0';
			char out = (char)get_oct(&s[1]);
			s[i] = save;
			return out;
		} else {
			LexError("Invalid escape sequence: %s", s);
			return s[1];
		}

		break;
	}
}

#ifdef FLEX_SCANNER

static int my_yyinput(char* dest, int size) {
  	if (!linebuf[nextchar]) {
    		if (fgets(tempbuf, BUFSIZE, yyin)) {
	 	 	buflen = strlen(tempbuf);
    			nextchar = 0;
		} else {
			dest[0] = '\0';
			return 0;
		}
          char *newbuf = (char*)malloc(buflen+1);
          strncpy(newbuf, tempbuf, buflen);
          newbuf[buflen] = 0;
          linebuf = newbuf;
  	}
  	size--; /* reserve room for NULL terminating char */
  	if (buflen - nextchar < size)
	    	size = buflen - nextchar;
  	strncpy(dest, &linebuf[nextchar], size);
  	dest[size] = 0; /* terminate with null */
  	nextchar += size;
  	return size;
}

#else

static char my_input() {
	if (!linebuf[nextchar]) {
    		fgets(tempbuf, BUFSIZE, yyin);
    		nextchar = 0;
                if (debug&DEBUG_CPP)
                  printf("I: %s", tempbuf);
                linebuf = (char*)malloc(strlen(tempbuf) + 1);
                strcpy(linebuf, tempbuf);
 	}
  	return linebuf[nextchar++];
}

static void my_unput(char c) {
  	if (nextchar <= 0) 
    		panic("Not able to unput characters from previous lines");
  	else
    		linebuf[--nextchar] = c;
}

#endif
