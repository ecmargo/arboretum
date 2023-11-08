#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <errno.h>

#include "global.h"
#include "frontend.h"

extern FILE *yyin;
extern int yyparse();

InputContext *currentContext;
ASTVignette *astResult = NULL;
  
InputContext::InputContext(const char *filename)
  
{
  char *filenameBuf = (char*)malloc(strlen(filename) + 1);
  strcpy(filenameBuf, filename);
  this->errorcount = 0;
  this->warningcount = 0;
  this->lineno = 1;
  this->fail = false;
  this->isPrimaryError = false;
  this->filename = filenameBuf;
  this->pipe_end[0] = -1;
  this->pipe_end[1] = -1;
  this->cppPath = "/usr/bin/cpp";
  this->currentScope = NULL;
  this->options = NULL;
}
  
ASTVignette *InputContext::parse(FEScope *rootScope)
{
  currentScope = rootScope;

  openPipe();
  yyin = fdopen(pipe_end[0], "r");
  
  currentContext = this;
  
  if (yyparse())
    Error("Confused by previous errors...");
    
  closePipe();
  
  return astResult;
}

#define MAX_CPP_ARGC 50
  
void InputContext::openPipe()
 
{
  char cmd[3000];
  char *argv[MAX_CPP_ARGC];
  int argc = 0;
  
  sprintf(cmd, "%s %s", cppPath, filename);
  if (options) {
    strcat(cmd, " ");
    strcat(cmd, options);
  }

  for (argc=0; argc<MAX_CPP_ARGC; argc++) {
    argv[argc] = strtok((argc==0) ? cmd : NULL, " \t");
    if (!argv[argc])
      break;
  }    
    
  if (argc == MAX_CPP_ARGC)
    panic("too many preprocessor arguments (max %d)", MAX_CPP_ARGC);
    
  pipe(pipe_end);  
  
  cppPid = fork(); 
  switch (cppPid) {
    case 0: 
      close(1);
      close(pipe_end[0]);
      dup2(pipe_end[1], 1);
      execvp(argv[0], argv);
    case -1:
      panic("failed to fork cpp");          
  }
  
  close(pipe_end[1]);
}

void InputContext::closePipe()

{
  int cppExitStatus;

  close(pipe_end[0]);
  waitpid(cppPid, &cppExitStatus, 0);
  if (cppExitStatus!=0)
    panic("cpp returns error while parsing %s", filename);
}
  
void InputContext::ShowErrorPos()

{
  if (this->linebuf[strlen(this->linebuf) - 1] == '\n') 
    fprintf(stderr, "%s%*s\n", this->linebuf, 1+tokenPos, "^");
  else
    fprintf(stderr, "%s\n%*s\n", this->linebuf, 1+tokenPos, "^");
}

void InputContext::Error(const char *fmt, ...) 

{
  va_list vl;

//  if (!isPrimaryError)
//    return;
  
  fail = true;
  fprintf(stderr, "%s:%d:%d:error:", filename, lineno, 1+tokenPos);
  va_start(vl, fmt);
  vfprintf(stderr, fmt, vl);
  va_end(vl);
  fprintf(stderr,"\n");
  ShowErrorPos();
  ++errorcount;
  isPrimaryError = false;
}

void InputContext::Warning(const char *fmt, ...) 

{
  va_list vl;
  
  fprintf(stderr, "%s:%d:%d:warning:", filename, lineno, 1+tokenPos);
  va_start(vl, fmt);
  vfprintf(stderr, fmt, vl);
  va_end(vl);
  fprintf(stderr,"\n");
  ShowErrorPos();
  ++warningcount;
  isPrimaryError = false;
}

void InputContext::rememberError(const char *linebuf, int tokenPos)
{
  isPrimaryError = true;
  strcpy(this->linebuf, linebuf);
  this->tokenPos = tokenPos;
}

void SourceContext::showMessage(const char *error)
{
  fprintf(stderr, "%s:%d:%d: %s\n", file, lineNo, pos, error);
  if (line[strlen(line) - 1] == '\n') 
    fprintf(stderr, "%s%*s\n", line, 1+pos, "^");
  else
    fprintf(stderr, "%s\n%*s\n", line, 1+pos, "^");
}
