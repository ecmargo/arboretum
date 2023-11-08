#ifndef __frontend_h__
#define __frontend_h__

#define BUFSIZE 500

#include <unistd.h>

#include "ast.h"

enum SymbolClass { SYM_LETBINDING, SYM_FUNCTION };

class FEScope
{
protected:
  struct symbolRecord {
    struct symbolRecord *next;
    SymbolClass cls;
    char *name;
  } *symbols;

  FEScope *parentScope;

  bool symbolExistsLocally(const char *identifier, SymbolClass cls);
  
public:
  FEScope(FEScope *parentScope, SourceContext *context)
    { this->parentScope = parentScope; this->symbols = NULL; };
    
  bool symbolExists(const char *identifier, SymbolClass cls);
  bool symbolExists(const char *identifier)
    { return symbolExists(identifier, SYM_LETBINDING) || symbolExists(identifier, SYM_FUNCTION); };
  void add(SymbolClass cls, const char *id);
  FEScope *getParentScope() { return parentScope; };
};

class FEParams
{
protected:
  ASTExpression *expr[10];
  int num;

public:
  FEParams() { num = 0; }
  void add(ASTExpression *exprArg)
    { assert(num<10); expr[num++] = exprArg; }
  int getNum()
    { return num; }
  ASTExpression *get(int idx) 
    { return expr[idx]; }
};

class InputContext
{
protected:
  FEScope *currentScope;
  const char *cppPath;
  FILE *input;
  bool fail;
  bool isPrimaryError;
  int errorcount;
  int warningcount;
  int pipe_end[2];
  pid_t cppPid;
  const char *options;

  void ShowErrorPos();

public:
  const char *filename;
  char linebuf[BUFSIZE];
  int lineno;
  int tokenPos;

  InputContext(const char *filename);

  void Error(const char *fmt, ...);
  void Warning(const char *fmt, ...);
  bool hasErrors() { return fail; };
  FILE *getFile() { return input; }
  int getErrorCount() { return errorcount; }
  int getWarningCount() { return warningcount; }
  void NextLine() { lineno++; }
  void rememberError(const char *linebuf, int tokenPos);
  const char *getFilename() { return filename; }
  void setPosition(const char *filename, int lineno) { this->filename = filename; this->lineno = lineno; }
  void setOptions(const char *options) { this->options = options; };
  void update(InputContext *c) { errorcount += c->errorcount; warningcount += c->warningcount; if (c->fail) fail=true; }
  void openPipe();
  void closePipe();
  void setCppPath(const char *cppPath) { this->cppPath = cppPath; }
  ASTVignette *parse(FEScope *rootScope);
  FEScope *getCurrentScope() { return currentScope; }
  void setCurrentScope(FEScope *scope) { currentScope = scope; }
};

extern InputContext *currentContext;

#endif /* defined(__frontend_h__) */
