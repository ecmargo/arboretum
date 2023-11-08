#ifndef __printer_h__
#define __printer_h__

class Printer
{
private:
  void putSingle(char c);

protected:
  char *stringBuffer;
  int indentLevel;
  int stringRemaining;
  int charsInLine;
  bool lineMode;
  bool havePartialLine;
  FILE *stream;
  
  void flushLine();
  void tabstop(int pos);
  
public:
  Printer(FILE *stream);
  Printer(char *buf, int maxLen);
  void setLineMode(bool state) { lineMode = state; };
  void println(const char *fmt, ...);
  void print(const char *fmt, ...);
  void println();
  void indent(int offset) { indentLevel += offset; };
  void flush();
  bool bufferFull() { return (stringRemaining<1); };
};
#endif /* defined(__printer_h__) */
