#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "frontend.h"
#include "printer.h"

Printer::Printer(FILE *streamArg)
{
  indentLevel = 0;
  havePartialLine = false;
  lineMode = false;
  stringRemaining = 0;
  stringBuffer = NULL;
  charsInLine = 0;
  stream = streamArg;
}

Printer::Printer(char *buf, int maxLen)
{
  indentLevel = 0;
  havePartialLine = false;
  lineMode = false;
  stringRemaining = maxLen;
  stringBuffer = buf;
  charsInLine = 0;
  stream = NULL;
}

void Printer::putSingle(char c)
{
  if (stringBuffer) {
    if (stringRemaining>0) {
      *stringBuffer++ = c;
      stringRemaining--;
    }
  } else {
    fputc(c, stream);
  }
  
  charsInLine++;
  
  if (c=='\n')
    charsInLine = 0;
}  

void Printer::println(const char *fmt, ...)
{
  va_list vl;

  va_start(vl, fmt);
  
  if (!havePartialLine && !lineMode)
    for (int i=0;i<2*indentLevel;i++) 
      putSingle(' ');
      
  if (stringBuffer) {
    if (stringRemaining>0) {
      int printed = vsnprintf(stringBuffer, stringRemaining, fmt, vl);
      stringRemaining -= printed;
      stringBuffer += printed;
    }
  } else { 
    vfprintf(stream, fmt, vl);fflush(stream); 
  }
  
  if (!lineMode)
    putSingle('\n');
  
  havePartialLine = false;

  va_end(vl);
}

void Printer::flushLine()
{
  if (havePartialLine)
    println();
}  

void Printer::println()
{
  if (!lineMode)
    putSingle('\n');
    
  havePartialLine = false;
}  

void Printer::print(const char *fmt, ...)
{
  va_list vl;

  va_start(vl, fmt);
  if (!havePartialLine && !lineMode) {
    for (int i=0;i<2*indentLevel;i++) 
      putSingle(' ');
  }
      
  if (stringBuffer) {
    if (stringRemaining>0) {
      int printed = vsnprintf(stringBuffer, stringRemaining, fmt, vl);
      stringRemaining -= printed;
      stringBuffer += printed;
      charsInLine += printed;
    }
  } else {
    charsInLine += vfprintf(stream, fmt, vl);
  }
  
  havePartialLine = true;
  
  va_end(vl);
}

void Printer::flush()
{
  if (stream)
    fflush(stream);
    
  if (stringBuffer)
    *stringBuffer = 0;
}

void Printer::tabstop(int pos)
{
  while (charsInLine<pos)
    putSingle(' ');      
}

void warning(const char* fmt, ...)
{
  va_list vl;

  va_start(vl, fmt);
  vfprintf(stderr, "Warning: ", vl);
  vfprintf(stderr, fmt, vl);
  va_end(vl);
  fputc('\n', stderr);
}
