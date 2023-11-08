#ifndef __context_h__
#define __context_h__

/* SourceContext: Remembers a specific position in the original source code that corresponds
   to some internal class, such as an AST node or an IL node. */

class SourceContext
{
public:
  const char *file, *line;
  int lineNo, pos;
  
  SourceContext(const char *file, const char *line, int lineNo, int pos)
    { this->file = file; this->line = line; this->lineNo = lineNo; this->pos = pos; };
  SourceContext()
    { this->file = "builtin"; this->line = "builtin"; this->lineNo = 1; this->pos = 0; };
    
  void showMessage(const char *error);
  SourceContext *clone()
    { return new SourceContext(file, line, lineNo, pos); };
};

#endif /* defined(__context_h__) */
